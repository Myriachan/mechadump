#include "common.hpp"

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>

#include <libcdvd.h>

#include "cipher.h"
#include "keys.hpp"
#include "util.h"

#include "sysinfo.hpp"


bool GetMechaconVersion(int& major, int& minor, std::string& refreshDate)
{
	uint8_t params[1];
	
	uint8_t resultVersion[3];
	params[0] = 0x00;
	if (!sceCdApplySCmd(0x03, params, sizeof(params), resultVersion, sizeof(resultVersion)))
		return false;
	
	uint8_t resultDate[6];
	params[0] = 0xFD;
	if (!sceCdApplySCmd(0x03, params, sizeof(params), resultDate, sizeof(resultDate)))
		return false;
	if (resultDate[0] != 0x00)
		return false;
	
	major = resultVersion[1];
	minor = resultVersion[2];
	
	char dateString[64];
	std::snprintf(dateString, sizeof(dateString), "20%02x/%02x/%02x %02x:%02x",
		resultDate[1], resultDate[2], resultDate[3], resultDate[4], resultDate[5]);
	refreshDate = dateString;
	
	return true;
}


bool CheckNVMBlockChecksum(const uint8_t* block, size_t blockSize)
{
	assert(blockSize >= 2u);
	
	unsigned sum = 0;
	for (size_t i = 0; i < blockSize - 2; ++i)
		sum += block[i];
	
	return static_cast<uint8_t>(~sum) == block[blockSize - 1];
}


bool DecodeRegionFlags(const uint8_t* eeprom, uint32_t& regionFlags)
{
	static constexpr uint8_t c_zeroIV[8] = {};
	
	// Validate the EEPROM block checksums.
	if (!CheckNVMBlockChecksum(&eeprom[0x1C6], 0x0A))
		return false;
	if (!CheckNVMBlockChecksum(&eeprom[0x1D0], 0x0A))
		return false;
	
	// Derive the key that encrypts the region code.
	uint8_t regionKey[8];
	uint8_t globalFlagsKey[8];
	write_be_uint64(globalFlagsKey, g_globalFlagsKey);
	cipherCbcEncrypt(regionKey, &eeprom[0x1C6], 8, globalFlagsKey, 1, c_zeroIV);
	
	// Decrypt the region value.
	uint8_t decrypted[8];
	cipherCbcDecrypt(decrypted, &eeprom[0x1D0], 8, regionKey, 1, c_zeroIV);
	
	// Validate internal checksum on the region.
	uint32_t regionCode = read_le_uint32(&decrypted[0]);
	uint16_t regionRandom = read_le_uint16(&decrypted[4]);
	uint16_t regionChecksum = read_le_uint16(&decrypted[6]);
	
	unsigned regionCorrect = (0u + regionRandom + (regionCode & 0xFFFF) + (regionCode >> 16)) & 0xFFFF;
	if (regionChecksum != regionCorrect)
		return false;
	
	regionFlags = regionCode;
	return true;
}


std::string GetModelString()
{
	char output[17];
	uint8_t params[1];
	uint8_t result[9];

	params[0] = 0;
	if (!sceCdApplySCmd(0x17, params, sizeof(params), result, sizeof(result)))
		return std::string();
	if (result[0] != 0x00)
		return std::string();

	std::memcpy(&output[0], &result[1], sizeof(result) - 1);

	params[0] = 8;
	if (!sceCdApplySCmd(0x17, params, sizeof(params), result, sizeof(result)))
		return std::string();
	if (result[0] != 0x00)
		return std::string();

	std::memcpy(&output[8], &result[1], sizeof(result) - 1);

	output[16] = '\0';
	return output;
}


std::vector<uint8_t> MakeEmptyPatchset()
{
	static constexpr uint8_t c_zeroIV[8] = {};
	
	uint8_t encryptedZeros[8];
	
	uint8_t mechaPatchKey[8];
	write_be_uint64(mechaPatchKey, g_mechaPatchKey);
	cipherCbcEncrypt(encryptedZeros, c_zeroIV, 8, mechaPatchKey, 1, c_zeroIV);
	
	std::vector<uint8_t> result;
	result.reserve(0xE0);
	result.insert(result.end(), encryptedZeros + 0, encryptedZeros + 8);
	result.insert(result.end(), encryptedZeros + 0, encryptedZeros + 8);
	for (int i = 0; i < 0xD0; ++i)
		result.emplace_back(0xFF);
	return result;
}
