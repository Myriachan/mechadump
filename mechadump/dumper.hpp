#pragma once

#include <functional>
#include <vector>

typedef bool DumpMechaconCallback(size_t dumped, size_t total);

bool IsBackDoorFunctioning();
std::vector<uint8_t> DumpMechaconROMFastWithPayload(const void* payload, size_t payloadSize,
	std::function<DumpMechaconCallback> callback, DebugOutput& debug);
std::vector<uint8_t> DumpMechaconKeystoreWithPayload(const void* payload, size_t payloadSize,
	DebugOutput& debug);
bool RestoreNVMConfigAndPatchData(const void* payload, size_t payloadSize, const uint8_t* configData,
	DebugOutput& debug);
bool ResetMechaconAndPowerOff();
