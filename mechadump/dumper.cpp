#include "common.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <vector>

#include <libcdvd.h>
#include <unistd.h>

#include "dumper.hpp"
#include "util.h"


bool IsBackDoorFunctioning()
{
	// Issue 03:A4 with a zero response to the challenge.
	// The original code will just return a 0; ours will return A4.
	uint8_t request[9]{ 0xA4 };
	uint8_t reply[1];
	if (!sceCdApplySCmd(0x03, request, sizeof(request), reply, sizeof(reply)))
		return false;
	
	return reply[0] == 0xA4;
}

bool BackDoorReadU32(uint32_t *value, uint32_t address)
{
	uint32_t params[4];
	params[0] = 0x000000A4;
	params[1] = 0;
	params[2] = address;
	params[3] = 0;

	uint8_t result[5];
	memset(result, 0xCC, sizeof(result));
	if (!sceCdApplySCmd(0x03, params, sizeof(params), result, sizeof(result)))
		return false;
	if (result[0] != 0x81)
		return false;

	params[0] = 0x44434DA4;
	memset(result, 0xCC, sizeof(result));
	if (!sceCdApplySCmd(0x03, params, 9, result, sizeof(result)))
		return false;
	if (result[0] != 0x42)
		return false;

	memcpy(value, &result[1], sizeof(*value));
	return true;
}

bool BackDoorExecuteU32(uint8_t *output16, uint32_t address, uint32_t r3)
{
	uint32_t params[4];
	params[0] = 0x000000A4;
	params[1] = 1;
	params[2] = address;
	params[3] = r3;

	uint8_t result[5];
	memset(result, 0xCC, sizeof(result));
	if (!sceCdApplySCmd(0x03, params, sizeof(params), result, sizeof(result)))
		return false;
	if (result[0] != 0x81)
		return false;

	params[0] = 0x44434DA4;
	memset(result, 0xCC, sizeof(result));
	if (!sceCdApplySCmd(0x03, params, 9, output16, 16))
		return false;
	if (result[0] == 0xA4)
		return false;

	return true;
}

// Dump Mechacon memory slowly using the back door to read 4 bytes at a time.
std::vector<uint32_t> DumpMechaconMemory(uint32_t address, uint32_t size, DebugOutput& debug)
{
	std::vector<uint32_t> emptyVector;
	
	if (!IsBackDoorFunctioning())
	{
		debug.Printf("Back door not found\n");
		return emptyVector;
	}
	
	if ((size | address) % sizeof(uint32_t))
	{
		debug.Printf("DumpMechaconMemory: Misaligned address or size\n");
		return emptyVector;
	}
	
	std::vector<uint32_t> result;
	result.resize(size / sizeof(uint32_t));
	
	for (uint32_t index = 0; index < size / sizeof(uint32_t); ++index)
	{
		uint32_t target = address + (index * sizeof(uint32_t));
		if (!BackDoorReadU32(&result[index], target))
		{
			debug.Printf("Failed to read address %08" PRIX32 "\n", target);
			return emptyVector;
		}
	}
	
	return result;
}

std::vector<uint32_t> DumpMechaconRAM(DebugOutput& debug)
{
	return DumpMechaconMemory(MECHACON_RAM_START, MECHACON_RAM_SIZE, debug);
}

bool UploadFakeMagicGateHeader(const void* data, size_t size, DebugOutput& debug)
{
	if (size > 0x7F0)
	{
		debug.Printf("Size %08X too large for MG header\n", static_cast<unsigned>(size));
		return false;
	}
	
	// Say we're sending a larger header than we actually are, so it doesn't actually start
	// processing the header.
	uint8_t headerRequest[5] =
	{
		0x00,  // header code
		static_cast<uint8_t>((size + 0x10)),
		static_cast<uint8_t>((size + 0x10) >> 8),
		0x00,  // key index?
		0x00,  // another key index?
	};
	uint8_t reply[1];
	
	if (!sceCdApplySCmd(0x90, headerRequest, sizeof(headerRequest), reply, sizeof(reply)))
	{
		debug.Printf("sceCdApplySCmd failed for command 0x90\n");
		return false;
	}
	
	if (reply[0] != 0x00)
	{
		debug.Printf("Command 0x90 failed with code %02X\n", reply[0]);
		return false;
	}
	
	const uint8_t* ptr = static_cast<const uint8_t*>(data);
	while (size > 0)
	{
		size_t current = (size > 0x10) ? 0x10 : size;
		if (!sceCdApplySCmd(0x8D, ptr, current, reply, sizeof(reply)))
		{
			debug.Printf("sceCdApplySCmd failed for command 0x8D\n");
			return false;
		}

		if (reply[0] != 0x00)
		{
			debug.Printf("Command 0x8D failed with code %02X\n", reply[0]);
			return false;
		}
		
		ptr += current;
		size -= current;
	}
	
	return true;
}

bool UploadAndFindCode(const void* data, size_t size, uint32_t& address, DebugOutput& debug)
{
	// Memorize the address and reuse it if possible.
	static uint32_t s_previousAddress = uint32_t(-1);
	
	if (size < 16)
	{
		debug.Printf("Payload is empty\n");
		return false;
	}
	if (size % sizeof(uint32_t))
	{
		debug.Printf("Size not a multiple of uint32\n");
		return false;
	}
	
	// Flush any existing data.
	debug.Printf("Flushing MagicGate header buffer...\n");
	
	std::vector<uint8_t> zeros;
	zeros.resize(0x7F0);
	
	if (!UploadFakeMagicGateHeader(zeros.data(), zeros.size(), debug))
	{
		debug.Printf("Flush failed\n");
		return false;
	}
	
	debug.Printf("Uploading payload to find...\n");
	
	// Upload real code.
	if (!UploadFakeMagicGateHeader(data, size, debug))
	{
		debug.Printf("Upload failed\n");
		return false;
	}

	// If we already succeeded before, reuse the address if we can.
	if (s_previousAddress != uint32_t(-1))
	{
		debug.Printf("Attempting to reuse address %08" PRIX32 "...\n", s_previousAddress);
		std::vector<uint32_t> check = DumpMechaconMemory(s_previousAddress, size, debug);
		if ((check.size() * sizeof(uint32_t) == size) && (std::memcmp(check.data(), data, size) == 0))
		{
			debug.Printf("Found again at %08" PRIX32 "\n", s_previousAddress);
			address = s_previousAddress;
			return true;
		}
	}

	debug.Printf("Dumping RAM...\n");
	
	std::vector<uint32_t> ram = DumpMechaconRAM(debug);
	if (ram.empty())
	{
		debug.Printf("RAM dump failed\n");
		return false;
	}

	debug.Printf("Searching for payload...\n");

	// Find our payload.
	uint32_t firstUint32;
	std::memcpy(&firstUint32, data, sizeof(firstUint32));
	
	uint32_t maxPossible = ram.size() - ((size + sizeof(ram[0]) - 1) / sizeof(ram[0]));
	
	for (size_t index = 0; index <= maxPossible; ++index)
	{
		if (ram[index] == firstUint32)
		{
			if (std::memcmp(ram.data() + index, data, size) == 0)
			{
				address = MECHACON_RAM_START + (index * sizeof(ram[0]));
				debug.Printf("Found uploaded code at %08" PRIX32 "\n", address);
				s_previousAddress = address;
				return true;
			}
		}
	}
	
	debug.Printf("Could not find code in RAM\n");
	return false;
}

void DumpPatchRegistersUsingPayload(const void* payload, size_t size, DebugOutput& debug)
{
	static const uint32_t s_registers[]
	{
		(0x00 + 0x03880000) | 0x10000000,
		(0x00 + 0x03880004),
		(0x00 + 0x03880008),
		(0x10 + 0x03880000) | 0x10000000,
		(0x10 + 0x03880004),
		(0x10 + 0x03880008),
		(0x20 + 0x03880000) | 0x10000000,
		(0x20 + 0x03880004),
		(0x20 + 0x03880008),
		(0x30 + 0x03880000) | 0x10000000,
		(0x30 + 0x03880004),
		(0x30 + 0x03880008),
	};
	
	uint32_t address;
	if (!UploadAndFindCode(payload, size, address, debug))
	{
		debug.Printf("Could not upload payload\n");
		return;
	}
	debug.Printf("Found code at %08" PRIX32 "\n", address);
	
	for (uint32_t target : s_registers)
	{
		uint8_t reply[16];
		std::memset(reply, 0xCC, sizeof(reply));
		
		if (!BackDoorExecuteU32(reply, address | 1, target))
		{
			debug.Printf("Could not execute payload for %08" PRIX32 "\n", target);
			return;
		}
		
		if (target & 0x10000000)
		{
			if (reply[0] != 0x6A)
			{
				debug.Printf("Unexpected result %02X for %08" PRIX32 "\n", reply[0], target);
				return;
			}
			
			debug.Printf("%08" PRIX32 " = %02X\n", target, reply[1]);
		}
		else
		{
			if (reply[0] != 0x6B)
			{
				debug.Printf("Unexpected result %02X for %08" PRIX32 "\n", reply[0], target);
				return;
			}
			
			debug.Printf("%08" PRIX32 " = %02X%02X%02X%02X\n", target,
				reply[4], reply[3], reply[2], reply[1]);
		}
	}
}

// Convenient subroute for dumping ROM.
bool Dump14Bytes(uint8_t* output, uint32_t romAddress, uint32_t payloadAddress, DebugOutput& debug)
{
	// Ask for data.
	uint8_t reply[16];
	if (!BackDoorExecuteU32(reply, payloadAddress, romAddress))
	{
		debug.Printf("Dump14Bytes: BackDoorExecuteU32 failed at %08" PRIX32 "\n", romAddress);
		return false;
	}
	
	if (reply[0] != 0x69)
	{
		debug.Printf("Dump14Bytes: Bad reply %02X from function\n", reply[0]);
		return false;
	}
	
	// The checksum includes the ROM address we're reading.
	unsigned checksum = 0;
	for (unsigned i = 0; i < sizeof(uint32_t); ++i)
	{
		checksum += static_cast<uint8_t>(romAddress >> (i * 8));
	}
	
	// Checksum the data.
	for (unsigned i = 0; i < 14; ++i)
	{
		checksum += reply[i + 2];
	}
	
	checksum = static_cast<uint8_t>(~checksum);
	
	// Verify checksum.
	if (reply[1] != checksum)
	{
		debug.Printf("Dump14Bytes: Invalid checksum %02X (correct=%02X) at %08" PRIX32 "\n",
			reply[1], checksum, romAddress);
		return false;
	}
	
	// Success; copy and return.
	std::memcpy(output, &reply[2], 14);
	return true;
}


std::vector<uint8_t> DumpMechaconROMFastWithPayload(const void* payload, size_t payloadSize,
	std::function<DumpMechaconCallback> callback, DebugOutput& debug)
{
	std::vector<uint8_t> emptyVector;
	
	debug.Printf("Fastdump: Starting payload upload\n");
	
	if (!IsBackDoorFunctioning())
	{
		debug.Printf("Fastdump: No back door found?\n");
		return emptyVector;
	}
	
	uint32_t payloadAddress;
	if (!UploadAndFindCode(payload, payloadSize, payloadAddress, debug))
	{
		debug.Printf("Fastdump: UploadAndFindCode failed\n");
		return emptyVector;
	}
	payloadAddress |= 1;
	
	std::vector<uint8_t> rom;
	rom.resize(MECHACON_ROM_SIZE);
	static_assert(MECHACON_ROM_SIZE > 14u, "bad MECHACON_ROM_SIZE constant");
	
	debug.Printf("Fastdump: Starting ROM dump\n");
	
	// Dump 14 bytes at a time.
	constexpr uint32_t mechaconEnd = MECHACON_ROM_START + MECHACON_ROM_SIZE;
	
	uint32_t prevPage = uint32_t(-1);
	uint32_t address;
	for (address = MECHACON_ROM_START; mechaconEnd - address >= 14; address += 14)
	{
		uint32_t atPage = address & ~uint32_t(0xFFF);
		if (atPage != prevPage)
		{
			prevPage = atPage;
			debug.Printf("Fastdump: Dump at %08" PRIX32 " so far\n", atPage);
			if (!callback(atPage - MECHACON_ROM_START, MECHACON_ROM_SIZE))
			{
				debug.Printf("Cancelled.\n");
				return emptyVector;
			}
		}
		
		if (!Dump14Bytes(rom.data() + address - MECHACON_ROM_START, address, payloadAddress, debug))
		{
			debug.Printf("Fastdump: Failed at %08" PRIX32 "\n", address);
			return emptyVector;
		}
	}
	
	// Handle the last part.
	if (mechaconEnd > address)
	{
		uint8_t lastBlock[14];
		if (!Dump14Bytes(lastBlock, mechaconEnd - 14, payloadAddress, debug))
		{
			debug.Printf("Fastdump: Failed dumping last bytes\n");
			return emptyVector;
		}
		
		std::memcpy(rom.data() + address - MECHACON_ROM_START,
			&lastBlock[14 - (mechaconEnd - address)],
			mechaconEnd - address);
	}
	
	debug.Printf("Fastdump: Success\n");
	callback(MECHACON_ROM_SIZE, MECHACON_ROM_SIZE);
	return rom;
}


std::vector<uint8_t> DumpMechaconKeystoreWithPayload(const void* payload, size_t payloadSize,
	DebugOutput& debug)
{
	std::vector<uint8_t> emptyVector;
	
	debug.Printf("Keystoredump: Starting payload upload\n");
	
	if (!IsBackDoorFunctioning())
	{
		debug.Printf("Keystoredump: No back door found?\n");
		return emptyVector;
	}
	
	uint32_t payloadAddress;
	if (!UploadAndFindCode(payload, payloadSize, payloadAddress, debug))
	{
		debug.Printf("Keystoredump: UploadAndFindCode failed\n");
		return emptyVector;
	}
	payloadAddress |= 1;
	
	std::vector<uint8_t> keystore;
	keystore.resize(KEYSTORE_SIZE);
	
	debug.Printf("Keystoredump: Starting keystore dump\n");
	
	// Dump 8 bytes (one "key") at a time.
	for (uint32_t keyHalfwordOffset = 0; keyHalfwordOffset < KEYSTORE_SIZE / 2; keyHalfwordOffset += 4)
	{
		uint8_t reply[16];
		if (!BackDoorExecuteU32(reply, payloadAddress, keyHalfwordOffset))
		{
			debug.Printf("Could not execute payload for %04" PRIX32 "\n", keyHalfwordOffset);
			return emptyVector;
		}
		
		if (reply[0] != 0x00)
		{
			debug.Printf("Keystoredump: Payload failed\n");
			return emptyVector;
		}
		
		std::memcpy(keystore.data() + (keyHalfwordOffset * 2), &reply[1], 8);
	}

	debug.Printf("Keystoredump: Success\n");
	return keystore;
}


bool RestoreNVMConfigAndPatchData(const void* payload, size_t payloadSize, const uint8_t* configData,
	DebugOutput& debug)
{
	debug.Printf("RestoreNVMConfigAndPatchData: Starting payload upload\n");
	
	if (!IsBackDoorFunctioning())
	{
		debug.Printf("RestoreNVMConfigAndPatchData: No back door found?\n");
		return false;
	}
	
	uint32_t payloadAddress;
	if (!UploadAndFindCode(payload, payloadSize, payloadAddress, debug))
	{
		debug.Printf("RestoreNVMConfigAndPatchData: UploadAndFindCode failed\n");
		return false;
	}
	payloadAddress |= 1;

	for (uint16_t wordOffset = 0x200 / 2; wordOffset < 0x400 / 2; ++wordOffset)
	{
	Retry:
		uint8_t reply[16]{};
		
		// WriteNVM uses big-endian for who knows what reason.
		unsigned char command[4];
		write_be_uint16(&command[0], wordOffset);
		command[2] = configData[wordOffset * 2 + 1 - 0x200];
		command[3] = configData[wordOffset * 2 + 0 - 0x200];
		uint32_t r3 = read_le_uint32(command);
		
		if (!BackDoorExecuteU32(reply, payloadAddress, r3))
		{
			debug.Printf("Could not execute payload for %04" PRIX16 "\n", wordOffset);
			return false;
		}
		
		// Result 1 indicates busy.
		if (reply[0] == 0x01)
			goto Retry;
		
		// Other indicates error.
		if (reply[0] != 0x00)
		{
			debug.Printf("RestoreNVMConfigAndPatchData: failed at %04" PRIX16 " with code %02X\n",
				wordOffset, reply[0]);
			return false;
		}
	}
	
	debug.Printf("RestoreNVMConfigAndPatchData: succeeded, in theory?\n");
	return true;
}


// Does not return if successful.
bool ResetMechaconAndPowerOff()
{
	if (!IsBackDoorFunctioning())
		return false;

	uint8_t reply[16]{};
	if (!BackDoorExecuteU32(reply, 0, 0))
		return false;

	for (;;)
		sleep(1);
}
