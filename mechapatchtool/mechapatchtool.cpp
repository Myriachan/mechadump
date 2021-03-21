#define _CRT_SECURE_NO_DEPRECATE
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cinttypes>
#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "cipher.h"
#include "keys.hpp"
#include "util.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::size_t;


const char* g_patchcryptInFilename = nullptr;


static_assert(sizeof(uint8_t) == 1, "unexpected uint8_t size");


// Useful value
static constexpr uint8_t c_zeroIV[8] = {};


[[noreturn]] void FatalErrorWithFilenameV(const char* filename, const char* format, va_list args)
{
	std::fprintf(stderr, "patchcrypt: %s: ", filename);
	std::vfprintf(stderr, format, args);
	std::fprintf(stderr, "\n");

	std::fflush(stderr);
	std::exit(1);
}


[[noreturn]] void FatalError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	FatalErrorWithFilenameV(g_patchcryptInFilename, format, args);
	va_end(args);
}


std::vector<uint8_t> LoadFile(const char* filename)
{
	do
	{
		std::FILE* file = std::fopen(filename, "rb");
		if (!file)
			break;

		if (std::fseek(file, 0, SEEK_END))
			break;

		long lengthLong = std::ftell(file);

		if (std::fseek(file, 0, SEEK_SET) || (lengthLong < 0))
			break;

		size_t length = static_cast<size_t>(lengthLong);

		std::vector<uint8_t> data;
		data.resize(length);

		if (std::fread(data.data(), 1, length, file) != length)
			break;

		std::fclose(file);
		return data;

	} while (false);

	FatalError("%s", strerror(errno));
	std::exit(1);
}


int SaveFile(const char* filename, const std::vector<uint8_t>& data)
{
	do 
	{
		std::FILE* file = std::fopen(filename, "wb");
		if (!file)
			break;

		if (std::fwrite(data.data(), 1, data.size(), file) != data.size())
			break;

		std::fclose(file);
		return 0;

	} while (false);

	FatalError("%s", strerror(errno));
	return 1;
}


// List of Thumb replacement instructions for doing fixups.
struct ThumbReplacements
{
	ThumbReplacements();

	std::vector<uint16_t> m_universal;
	std::vector<uint16_t> m_byRegister[16];
};


// Builds a list of Thumb opcodes that have no side effects other than destroying
// flags and the registers in registerMask.
ThumbReplacements::ThumbReplacements()
{
	// "movs low, low" is always allowed because we allow destroying flags.
	for (unsigned reg = 0u; reg < 8u; ++reg)
	{
		unsigned opcode = 0b0001110'000'000000;
		opcode |= reg << 3;
		opcode |= reg;
		m_universal.push_back(static_cast<uint16_t>(opcode));
	}

	// "mov high, high" is always allowed for any register except PC.
	// Notably, "nop" is actually "mov r8, r8".
	for (unsigned reg = 8u; reg < 15u; ++reg)
	{
		unsigned opcode = 0b01000110'00000000;
		opcode |= ((reg & 8) << 4) | (reg & 7);
		opcode |= reg << 3;
		m_universal.push_back(static_cast<uint16_t>(opcode));
	}

	// Stuff for low registers.
	for (unsigned reg = 0u; reg < 8u; ++reg)
	{
		std::vector<uint16_t>& list = m_byRegister[reg];
		unsigned opcode00;

		// movs reg, #any8
		opcode00 = 0b00100'000'00000000;
		opcode00 |= reg << 8;
		for (unsigned b = 0; b < 0x100; ++b)
		{
			list.push_back(static_cast<uint16_t>(opcode00 | b));
		}

		// adds reg, #any8
		opcode00 = 0b00110'000'00000000;
		opcode00 |= reg << 8;
		for (unsigned b = 0; b < 0x100; ++b)
		{
			list.push_back(static_cast<uint16_t>(opcode00 | b));
		}

		// subs reg, #any8
		opcode00 = 0b00111'000'00000000;
		opcode00 |= reg << 8;
		for (unsigned b = 0; b < 0x100; ++b)
		{
			list.push_back(static_cast<uint16_t>(opcode00 | b));
		}

		// lsls reg, anyreg, #any5
		opcode00 = 0b00000'00000'000'000;
		opcode00 |= reg;
		for (unsigned shift = 0; shift < 32; ++shift)
		{
			for (unsigned otherreg = 0; otherreg < 8; ++otherreg)
			{
				list.push_back(static_cast<uint16_t>(opcode00 | (shift << 6) | (otherreg << 3)));
			}
		}

		// lsrs reg, anyreg, #any5
		opcode00 = 0b00001'00000'000'000;
		opcode00 |= reg;
		for (unsigned shift = 0; shift < 32; ++shift)  // shift 0 means 32 for LSRS
		{
			for (unsigned otherreg = 0; otherreg < 8; ++otherreg)
			{
				list.push_back(static_cast<uint16_t>(opcode00 | (shift << 6) | (otherreg << 3)));
			}
		}

		// asrs reg, anyreg, #any5
		opcode00 = 0b00010'00000'000'000;
		opcode00 |= reg;
		for (unsigned shift = 0; shift < 32; ++shift)  // shift 0 means 32 for ASRS
		{
			for (unsigned otherreg = 0; otherreg < 8; ++otherreg)
			{
				list.push_back(static_cast<uint16_t>(opcode00 | (shift << 6) | (otherreg << 3)));
			}
		}
	}

	// That was probably overkill.  We'll find a match >99% of the time from the above... for one register.
}


// Checks whether a "row" (16 bytes) is WriteConfig compatible.
bool IsWriteConfigCompatible(const uint8_t* row)
{
	unsigned sum = 0;
	for (int i = 0; i < 15; ++i)
	{
		sum += row[i];
	}

	return row[15] == static_cast<uint8_t>(sum);
}


void VerifyPatchData(const std::vector<uint8_t>& input, bool& isNonzero, bool& writeConfigCompatible, bool& blockChecksumOK, bool& addressChecksumOK)
{
	// Check WriteConfig compatibility.
	writeConfigCompatible = true;
	for (int row = 0; row < 0xE; ++row)
	{
		if (!IsWriteConfigCompatible(&input[row * 0x10]))
		{
			writeConfigCompatible = false;
			break;
		}
	}

	// Initialize the key.  The DES code we're using always does CBC mode, so turn it into ECB by doing
	// a single block decrypt with a zero IV.
	uint8_t key[8];
	write_be_uint64(key, g_mechaPatchKey);

	// Check for a null patch list.
	uint8_t patchList[16];
	std::memcpy(patchList, input.data(), sizeof(patchList));

	cipherCbcDecrypt(&patchList[0], &patchList[0], 8, key, 1, c_zeroIV);
	cipherCbcDecrypt(&patchList[8], &patchList[8], 8, key, 1, c_zeroIV);

	isNonzero = false;
	for (int i = 0; i < 16; ++i)
	{
		if (patchList[i] != 0)
		{
			isNonzero = true;
			break;
		}
	}

	// Check the two blocks' checksums.
	blockChecksumOK = true;
	for (int block = 0; block < 2; ++block)
	{
		int offset = block * 0x70;
		unsigned sum = 0;
		for (int i = 0; i < 0x6E; ++i)  // byte 0x6E not counted
		{
			sum += input[offset + i];
		}

		if (input[offset + 0x6F] != static_cast<uint8_t>(~sum))
		{
			blockChecksumOK = false;
		}
	}

	// Check the patch address checksum.  It's a wrapping checksum.
	uint32_t addressSum = 0;
	for (int i = 0; i < 4; ++i)
	{
		addressSum = static_cast<uint32_t>(addressSum + read_le_uint32(&patchList[i * sizeof(uint32_t)]));
	}
	addressSum = static_cast<uint32_t>(~addressSum);
	uint32_t allegedAddressSum = read_le_uint32(&input[0xDA]);
	addressChecksumOK = allegedAddressSum == addressSum;
}


std::vector<uint8_t> DecryptUnpackPatchData(const std::vector<uint8_t>& encrypted)
{
	assert(encrypted.size() == 0xE0);

	std::vector<uint8_t> decrypted;
	decrypted.resize(0xD8);

	// Combine the two blocks.
	std::memcpy(decrypted.data(), encrypted.data(), 0x6E);
	std::memcpy(decrypted.data() + 0x6E, encrypted.data() + 0x70, 0x6A);

	// Initialize the key.  The DES code we're using always does CBC mode, so turn it into ECB by doing
	// a single block decrypt with a zero IV.
	uint8_t key[8];
	write_be_uint64(key, g_mechaPatchKey);

	// Decrypt the blocks.
	for (int i = 0; i < 0xD8; i += 8)
	{
		cipherCbcDecrypt(decrypted.data() + i, decrypted.data() + i, 8, key, 1, c_zeroIV);
	}

	return decrypted;
}


std::vector<uint8_t> EncryptPackPatchData(const std::vector<uint8_t>& decrypted, bool writeConfigFixup6andD)
{
	std::vector<uint8_t> encrypted;
	encrypted.resize(0xE0);

	// Compute the header checksum.
	uint32_t addressSum = 0;
	for (int i = 0; i < 16; i += 4)
	{
		addressSum = static_cast<uint32_t>(addressSum + read_le_uint32(decrypted.data() + i));
	}
	addressSum = static_cast<uint32_t>(~addressSum);

	// Copy the decrypted data, padding out to the maximum size.
	assert(decrypted.size() <= 0xD8);
	std::memcpy(encrypted.data(), decrypted.data(), decrypted.size());
	std::memset(encrypted.data() + decrypted.size(), 0, 0xE0 - decrypted.size());

	// Initialize the key.  The DES code we're using always does CBC mode, so turn it into ECB by doing
	// a single block decrypt with a zero IV.
	uint8_t key[8];
	write_be_uint64(key, g_mechaPatchKey);

	// Encrypt the blocks.  We'll deal with the block split after this..
	for (int i = 0; i < 0xD8; i += 8)
	{
		cipherCbcEncrypt(encrypted.data() + i, encrypted.data() + i, 8, key, 1, c_zeroIV);
	}

	// Split the blocks.
	std::memmove(encrypted.data() + 0x70, encrypted.data() + 0x6E, 0x6E);

	// Header checksum.
	write_le_uint32(encrypted.data() + 0xDA, addressSum);

	// Block checksums.
	for (int block = 0; block < 2; ++block)
	{
		int offset = block * 0x70;
		unsigned sum = 0;
		for (int i = 0; i < 0x6E; ++i)  // byte 0x6E not counted
		{
			sum += encrypted[offset + i];
		}

		encrypted[offset + 0x6F] = static_cast<uint8_t>(~sum);

		// Unused bytes.  If requested, set to the value that makes a WriteConfig checksum pass
		// for the final row.
		if (writeConfigFixup6andD)
		{
			unsigned row6Sum = 0x00;
			for (int i = 0x60; i < 0x6E; ++i)
			{
				row6Sum += encrypted[offset + i];
			}
			encrypted[offset + 0x6E] = static_cast<uint8_t>(encrypted[offset + 0x6F] - row6Sum);
		}
		else
		{
			encrypted[offset + 0x6E] = 0x00;
		}
	}

	return encrypted;
}


// Helper routine to extract patch data from an EEPROM dump.
// If it's already extracted, this does nothing.
bool ExtractPatchDataFromEEPROM(std::vector<uint8_t>& dump)
{
	if (dump.size() == 0xE0)
	{
		// Already extracted.
		return true;
	}
	else if (dump.size() == 0x400)
	{
		// Presumably an EEPROM dump.
		dump.erase(dump.begin(), dump.begin() + 0x320);
		return true;
	}
	else
	{
		return false;
	}
}


// Verifies that a patch file's checksums are valid.
int VerifyPatchFile(const char* inFilename)
{
	g_patchcryptInFilename = inFilename;
	std::vector<uint8_t> input = LoadFile(inFilename);

	if (!ExtractPatchDataFromEEPROM(input))
	{
		FatalError("file is not a patch file");
	}

	assert(input.size() == 0xE0);

	bool isNonzero;
	bool writeConfigCompatible;
	bool blockChecksumOK;
	bool addressChecksumOK;
	VerifyPatchData(input, isNonzero, writeConfigCompatible, blockChecksumOK, addressChecksumOK);

	std::printf("Patch has data: %s\n", isNonzero ? "YES" : "NO");
	std::printf("WriteConfig exploit compatible: %s\n", writeConfigCompatible ? "YES" : "NO");
	std::printf("Block checksums OK: %s\n", blockChecksumOK ? "YES" : "NO");
	std::printf("Address checksum OK: %s\n", addressChecksumOK ? "YES" : "NO");

	// isNonzero isn't relevant for success
	return (writeConfigCompatible && blockChecksumOK && addressChecksumOK) ? 0 : 1;
}


// Decrypts a patch file.
int DecryptPatchFile(const char* inFilename, const char* outFilename)
{
	g_patchcryptInFilename = inFilename;
	std::vector<uint8_t> input = LoadFile(inFilename);

	if (!ExtractPatchDataFromEEPROM(input))
	{
		FatalError("file is not a patch file");
	}

	assert(input.size() == 0xE0);

	std::vector<uint8_t> output = DecryptUnpackPatchData(input);

	return SaveFile(outFilename, output);
}


// Encrypts a patch file.
int EncryptPatchFile(const char* inFilename, const char* outFilename)
{
	g_patchcryptInFilename = inFilename;
	std::vector<uint8_t> input = LoadFile(inFilename);

	assert(input.size() == 0xD8);

	std::vector<uint8_t> output = EncryptPackPatchData(input, false);

	return SaveFile(outFilename, output);
}



struct Fixup
{
	uint32_t m_type;
	uint32_t m_offset;

	bool operator<(const Fixup& other) const
	{
		return m_offset < other.m_offset;
	}
};

enum FixupType : uint32_t
{
	FixupType_AnyUint8 = 0,
	FixupType_AnyUint16 = 1,
	FixupType_PatchAddress = 2,
	FixupType_ThumbRegDestroy = 3,
	FixupType_NumFixupTypes
};
static const unsigned char s_fixupSizeByType[] =
{
	sizeof(uint8_t), // FixupType_AnyUint8
	sizeof(uint16_t), // FixupType_AnyUint16
	sizeof(uint32_t), // FixupType_PatchAddress
	sizeof(uint16_t), // FixupType_ThumbRegDestroy
};
static_assert(sizeof(s_fixupSizeByType) == FixupType_NumFixupTypes, "s_fixupSizeByType is wrong size");

uint32_t FixupAnyUint8_CountPossible(const uint8_t*)
{
	return uint32_t(uint8_t(-1)) + 1;
}
void FixupAnyUint8_Apply(uint32_t counter, uint8_t* overwrite, const uint8_t*)
{
	assert(counter <= uint8_t(-1));
	*overwrite = static_cast<uint8_t>(counter);
}

uint32_t FixupAnyUint16_CountPossible(const uint8_t*)
{
	return uint32_t(uint16_t(-1)) + 1;
}
void FixupAnyUint16_Apply(uint32_t counter, uint8_t* overwrite, const uint8_t*)
{
	assert(counter <= uint16_t(-1));
	write_le_uint16(overwrite, static_cast<uint16_t>(counter));
}

enum : uint32_t
{
	// Address range we don't care about messing with in a patch.
	PATCHADDRESS_FIXUP_START = 0x44000, // past the end
	PATCHADDRESS_FIXUP_SIZE = 0x1000,
};
uint32_t FixupPatchAddress_CountPossible(const uint8_t*)
{
	return PATCHADDRESS_FIXUP_SIZE / 4;
}
void FixupPatchAddress_Apply(uint32_t counter, uint8_t* overwrite, const uint8_t*)
{
	assert(counter < (PATCHADDRESS_FIXUP_SIZE / 4));
	write_le_uint32(overwrite, PATCHADDRESS_FIXUP_START + (counter * 4));
}

static ThumbReplacements s_thumbReplacements;
uint32_t FixupThumbRegDestroy_CountPossible(const uint8_t* original)
{
	unsigned regList = read_le_uint16(original);
	size_t count = s_thumbReplacements.m_universal.size();
	for (int i = 0; i < 16; ++i)
	{
		if (regList & (1u << i))
		{
			count += s_thumbReplacements.m_byRegister[i].size();
		}
	}
	return static_cast<uint32_t>(count);
}
void FixupThumbRegDestroy_Apply(uint32_t counter, uint8_t* overwrite, const uint8_t* original)
{
	unsigned regList = read_le_uint16(original);
	for (int i = -1; i < 16; ++i)
	{
		const std::vector<uint16_t>* current;
		if (i < 0)
		{
			current = &s_thumbReplacements.m_universal;
		}
		else if (regList & (1u << i))
		{
			current = &s_thumbReplacements.m_byRegister[i];
		}
		else
		{
			continue;
		}

		if (counter < current->size())
		{
			write_le_uint16(overwrite, (*current)[counter]);
			return;
		}
		else
		{
			counter -= static_cast<uint32_t>(current->size());
		}
	}
	// counter too large
	assert(false);
}


struct FixupHandler
{
	uint32_t(*m_countPossible)(const uint8_t* original);
	void(*m_apply)(uint32_t counter, uint8_t* overwrite, const uint8_t* original);
};
static const FixupHandler s_fixupHandlers[] =
{
	{ FixupAnyUint8_CountPossible, FixupAnyUint8_Apply },
	{ FixupAnyUint16_CountPossible, FixupAnyUint16_Apply },
	{ FixupPatchAddress_CountPossible, FixupPatchAddress_Apply },
	{ FixupThumbRegDestroy_CountPossible, FixupThumbRegDestroy_Apply },
};
static_assert(sizeof(s_fixupHandlers) / sizeof(s_fixupHandlers[0]) == FixupType_NumFixupTypes, "s_fixupHandlers is wrong size");


int FixupPatchFile(const char* inFilename, const char* outFilename)
{
	g_patchcryptInFilename = inFilename;
	std::vector<uint8_t> input = LoadFile(inFilename);

	if (input.size() < sizeof(uint32_t) * 2)
		FatalError("file too small");

	uint32_t payloadLength = read_le_uint32(input.data() + input.size() - sizeof(uint32_t));
	if (payloadLength > input.size() - sizeof(uint32_t))
		FatalError("invalid payload length");
	if (payloadLength < 0x10)
		FatalError("payload too short; must be at least 16 bytes");
	if (payloadLength > 0xD8)
		FatalError("payload too long; must be 0xD8 bytes or shorter");

	// Determine number of fixups.
	size_t fixupByteLength = input.size() - payloadLength - sizeof(uint32_t);
	if (fixupByteLength % (sizeof(uint32_t) * 2) != 0)
		FatalError("non-integer number of fixups");

	size_t fixupLength = fixupByteLength / (sizeof(uint32_t) * 2);

	std::vector<Fixup> fixups;
	fixups.reserve(fixupLength);

	// Parse and validate fixup table.
	for (size_t i = 0; i < fixupLength; ++i)
	{
		const uint8_t* p = input.data() + payloadLength + (i * sizeof(uint32_t) * 2);
		uint32_t type = read_le_uint32(p);
		uint32_t offset = read_le_uint32(p + sizeof(uint32_t));
		uint32_t size = s_fixupSizeByType[type];

		if (type >= FixupType_NumFixupTypes)
			FatalError("bad fixup type %" PRIu32, type);
		if (offset >= payloadLength)
			FatalError("fixup out of range at offset %02" PRIX32, offset);
		if (payloadLength - offset < size)
			FatalError("fixup at offset 0x%02" PRIX32 " goes past the end of payload", offset);
		if ((offset >= 0x60) && (offset < 0x6E))
			FatalError("fixup at offset 0x%02" PRIX32 " is on row 6", offset);
		if ((offset >= 0x6E) && ((offset & 0xF) >= 0xE))
			FatalError("fixup at offset 0x%02" PRIX32 " not allowed at +0xE row offsets after 0x60 for crypto reasons", offset);
		if (i > 0)
		{
			if (offset < fixups[i - 1].m_offset)
				FatalError("fixup at offset 0x%02" PRIX32 " is out of order", offset);
			if (offset < fixups[i - 1].m_offset + s_fixupSizeByType[fixups[i - 1].m_type])
				FatalError("fixup at offset 0x%02" PRIX32 " overlaps previous fixup", offset);
		}

		fixups.emplace_back(Fixup{ type, offset });
	}

	// For any unused bytes, put a single-byte fixup.
	for (unsigned i = payloadLength; i < 0xD8; ++i)
	{
		fixups.emplace_back(Fixup{ FixupType_AnyUint8, i });
	}

	// Buffer for sketching out alternatives.
	std::vector<uint8_t> sketch = input;
	sketch.resize(payloadLength);
	sketch.resize(0xD8);

	// Chain of counters for up to 16 fixups.
	std::vector<uint32_t> rowCounters;
	rowCounters.reserve(16);
	std::vector<uint32_t> rowPossible;
	rowPossible.reserve(16);

	// Initialize the key.  The DES code we're using always does CBC mode, so turn it into ECB by doing
	// a single block decrypt with a zero IV.
	uint8_t key[8];
	write_be_uint64(key, g_mechaPatchKey);

	// Handle the rows.
	for (unsigned row = 0x0; row <= 0xD; ++row)
	{
		// Rows 6 and D are handled by EncryptPackPatchData.
		if ((row == 0x6) || (row == 0xD))
			continue;

		// For row 7 and later, two of our bytes are part of the previous encryption block, but affect
		// our checksum too. So we need to encrypt the previous block to know what our target is.
		uint8_t previousBlock[8]{};
		unsigned rowFixupSize = 0x10;
		if (row >= 0x7)
		{
			cipherCbcEncrypt(previousBlock, sketch.data() + (row * 0x10) - 8, 8, key, 1, c_zeroIV);
			rowFixupSize = 0xE;
		}

		// Find fixups relevant to this row.
		auto fixupBegin = std::lower_bound(fixups.begin(), fixups.end(), Fixup{ 0, row * 0x10 });
		auto fixupEnd = std::upper_bound(fixups.begin(), fixups.end(), Fixup{ 0, row * 0x10 + rowFixupSize - 1 });

		// Fixup loop.  The loop is designed so that a null fixup set works if it already matches.
		rowCounters.resize(static_cast<size_t>(fixupEnd - fixupBegin));

		// Determine number of options for each fixup.
		rowPossible.resize(rowCounters.size());
		for (size_t row = 0; row < rowCounters.size(); ++row)
		{
			rowCounters[row] = 0;
			rowPossible[row] = (*s_fixupHandlers[fixupBegin[row].m_type].m_countPossible)(input.data() + fixupBegin[row].m_offset);
			if (rowPossible[row] == 0)
				FatalError("fixup at offset %02" PRIX32 " has no possible values", fixupBegin[row].m_offset);
		}

		for (;;)
		{
			// Set the data bytes according to their counters.
			for (size_t row = 0; row < rowCounters.size(); ++row)
			{
				(*s_fixupHandlers[fixupBegin[row].m_type].m_apply)(rowCounters[row], sketch.data() + fixupBegin[row].m_offset,
					input.data() + fixupBegin[row].m_offset);
			}

			// Encrypt the block.
			uint8_t encrypted[16];
			cipherCbcEncrypt(&encrypted[0], sketch.data() + (row * 0x10), 8, key, 1, c_zeroIV);
			cipherCbcEncrypt(&encrypted[8], sketch.data() + (row * 0x10) + 8, 8, key, 1, c_zeroIV);

			if (row >= 0x7)
			{
				// The block to check for a match is misaligned.
				uint8_t checkBlock[16];
				std::memcpy(&checkBlock[0], &previousBlock[6], 2);
				std::memcpy(&checkBlock[2], &encrypted[0], 14);
				if (IsWriteConfigCompatible(checkBlock))
					break;
			}
			else
			{
				if (IsWriteConfigCompatible(encrypted))
					break;
			}

			// Increment counters.
			bool overflow = true;
			for (size_t row = 0; row < rowCounters.size(); ++row)
			{
				++(rowCounters[row]);
				if (rowCounters[row] == rowPossible[row])
				{
					// Carry
					rowCounters[row] = 0;
					continue;
				}
				overflow = false;
				break;
			}
			if (overflow)
				FatalError("ran out of possibilities for match on row %X", row);
		}
	}

	// Encrypt the data, which also handles rows 6 and D.
	std::vector<uint8_t> encrypted = EncryptPackPatchData(sketch, true);

	bool isNonzero;
	bool writeConfigCompatible;
	bool blockChecksumOK;
	bool addressChecksumOK;
	VerifyPatchData(encrypted, isNonzero, writeConfigCompatible, blockChecksumOK, addressChecksumOK);

	std::printf("--- Fixup verification ---\n");
	std::printf("Patch has data: %s\n", isNonzero ? "YES" : "NO");
	std::printf("WriteConfig exploit compatible: %s\n", writeConfigCompatible ? "YES" : "NO");
	std::printf("Block checksums OK: %s\n", blockChecksumOK ? "YES" : "NO");
	std::printf("Address checksum OK: %s\n", addressChecksumOK ? "YES" : "NO");

	if (!isNonzero || !writeConfigCompatible || !blockChecksumOK || !addressChecksumOK)
		FatalError("fixup process failed to verify");

	return SaveFile(outFilename, encrypted);
}


[[noreturn]] void SyntaxError()
{
	std::printf(
		"Syntax:\n"
		"    Apply fixups to a compiled patch file:\n"
		"        mechapatchtool --fixup --in=infile --out=outfile\n"
		"    Verify the checksums on an encrypted patch file:\n"
		"        mechapatchtool --verify --in=infile\n"
		"    Decrypt an encrypted patch dump, either alone or in an EEPROM dump:\n"
		"        mechapatchtool --decrypt --in=infile --out=outfile\n"
		"    Encrypt a patch dump:\n"
		"        mechapatchtool --encrypt --in=infile --out=outfile\n"
		"\n"
		"Filenames can also be separate parameters (and don't use equal signs).\n");
	std::exit(1);
}


int main(int, char** argv)
{
	// Parameter declarations.
	bool cmdFixup = false;
	bool cmdVerify = false;
	bool cmdDecrypt = false;
	bool cmdEncrypt = false;
	const char* inFilename = nullptr;
	const char* outFilename = nullptr;

	struct ParamInfo
	{
		const char* m_name;
		bool m_isCommand;
		size_t m_strlen;
		bool* m_bool;
		const char** m_string;
	};
	ParamInfo paramInfo[] =
	{
		{ "--fixup", true, 0, &cmdFixup, nullptr },
		{ "--verify", true, 0, &cmdVerify, nullptr },
		{ "--decrypt", true, 0, &cmdDecrypt, nullptr },
		{ "--encrypt", true, 0, &cmdEncrypt, nullptr },
		{ "--in", false, 0, nullptr, &inFilename },
		{ "--out", false, 0, nullptr, &outFilename },
	};
	for (ParamInfo& param : paramInfo)
	{
		param.m_strlen = std::strlen(param.m_name);
	}

	// Evaluate parameters.
	if (!argv[0])
		SyntaxError();

	bool haveCommand = false;

	for (++argv; *argv; ++argv)
	{
		const char* arg = *argv;
		size_t length = std::strlen(arg);

		for (const ParamInfo& param : paramInfo)
		{
			if (param.m_bool)
			{
				if ((length == param.m_strlen) && (std::memcmp(arg, param.m_name, length) == 0))
				{
					if (haveCommand && param.m_isCommand)
						SyntaxError();
					*param.m_bool = true;
					if (param.m_isCommand)
						haveCommand = true;
					goto found;
				}
			}

			if (param.m_string)
			{
				size_t tempLength = length;
				const char* equals = static_cast<const char*>(std::memchr(arg, '=', tempLength));
				if (equals)
					tempLength = static_cast<size_t>(equals - arg);

				if ((tempLength == param.m_strlen) && (std::memcmp(arg, param.m_name, tempLength) == 0))
				{
					if (*param.m_string)
						SyntaxError();
					*param.m_string = equals ? equals + 1 : *++argv;
					if (!*param.m_string)
						SyntaxError();
					goto found;
				}
			}
		}

		SyntaxError();

	found:;
	}

	if (!haveCommand)
		SyntaxError();

	if (cmdFixup)
	{
		if (!inFilename || !outFilename)
			SyntaxError();

		return FixupPatchFile(inFilename, outFilename);
	}
	else if (cmdVerify)
	{
		if (!inFilename || outFilename)
			SyntaxError();

		return VerifyPatchFile(inFilename);
	}
	else if (cmdDecrypt)
	{
		if (!inFilename || !outFilename)
			SyntaxError();

		return DecryptPatchFile(inFilename, outFilename);
	}
	else if (cmdEncrypt)
	{
		if (!inFilename || !outFilename)
			SyntaxError();

		return EncryptPatchFile(inFilename, outFilename);
	}
	else
	{
		assert(false);
	}
}
