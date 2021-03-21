#pragma once

#include <vector>


bool GetMechaconVersion(int& major, int& minor, std::string& refreshDate);
bool DecodeRegionFlags(const uint8_t* eeprom, uint32_t& regionFlags);
std::string GetModelString();
std::vector<uint8_t> MakeEmptyPatchset();
