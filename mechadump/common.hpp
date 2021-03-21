#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::size_t;
using std::ptrdiff_t;


enum : uint32_t
{
	MECHACON_ROM_START = 0x00000000,
	MECHACON_ROM_SIZE = 0x44000,
	MECHACON_RAM_START = 0x02000000,
	MECHACON_RAM_SIZE = 0x4000,
	KEYSTORE_SIZE = 0x400,
};

class DebugOutput
{
public:
	virtual ~DebugOutput() {}
	virtual void VPrintf(const char* format, va_list args) = 0;
	
	void Printf(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		VPrintf(format, args);
		va_end(args);
	}
};
