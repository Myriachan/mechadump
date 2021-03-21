#pragma once

#include <climits>

#include "sha256.hpp"


#define STRING_CONCAT_(x, y) x ## y
#define STRING_CONCAT(x, y) STRING_CONCAT_(x, y)


template <int Part>
constexpr std::uint64_t CompileTimeSHA256Part(std::uint64_t data)
{
	using namespace sha256_internal;

	sha256_word state[8]{};
	sha256_init(state);

	sha256_word input[16]
	{
		static_cast<sha256_word>(data >> 32),
		static_cast<sha256_word>(data & 0xFFFFFFFF),
		0x80000000,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, sizeof(std::uint64_t) * CHAR_BIT
	};

	sha256_transform(state, input);

	return (static_cast<std::uint64_t>(state[Part * 2]) << 32) + state[Part * 2 + 1];
}

template <std::uint64_t Key>
struct CompileTimeSHA256
{
private:
	enum : std::uint64_t
	{
		ab = CompileTimeSHA256Part<0>(Key),
		cd = CompileTimeSHA256Part<1>(Key),
		ef = CompileTimeSHA256Part<2>(Key),
		gh = CompileTimeSHA256Part<3>(Key),
	};
public:
	enum : std::uint32_t
	{
		a = static_cast<uint32_t>(ab >> 32),
		b = static_cast<uint32_t>(ab & 0xFFFFFFFF),
		c = static_cast<uint32_t>(cd >> 32),
		d = static_cast<uint32_t>(cd & 0xFFFFFFFF),
		e = static_cast<uint32_t>(ef >> 32),
		f = static_cast<uint32_t>(ef & 0xFFFFFFFF),
		g = static_cast<uint32_t>(gh >> 32),
		h = static_cast<uint32_t>(gh & 0xFFFFFFFF),
	};
};


constexpr std::uint64_t g_desParityMask = 0x01010101'01010101;

constexpr std::uint64_t g_mechaPatchKey = STRING_CONCAT(0x, MECHA_PATCH_KEY) & ~g_desParityMask;
constexpr std::uint64_t g_globalFlagsKey = STRING_CONCAT(0x, GLOBAL_FLAGS_KEY) & ~g_desParityMask;

static_assert(
	CompileTimeSHA256<g_mechaPatchKey>::a == 0x2B69BA04 &&
	CompileTimeSHA256<g_mechaPatchKey>::b == 0x87A716C4 &&
	CompileTimeSHA256<g_mechaPatchKey>::c == 0xFF66452A &&
	CompileTimeSHA256<g_mechaPatchKey>::d == 0x816910B7 &&
	CompileTimeSHA256<g_mechaPatchKey>::e == 0xB6CD2541 &&
	CompileTimeSHA256<g_mechaPatchKey>::f == 0x898A24C5 &&
	CompileTimeSHA256<g_mechaPatchKey>::g == 0x9CADB82D &&
	CompileTimeSHA256<g_mechaPatchKey>::h == 0xF306EF29,
	"incorrect MECHA_PATCH_KEY");
static_assert(
	CompileTimeSHA256<g_globalFlagsKey>::a == 0x720CB04F &&
	CompileTimeSHA256<g_globalFlagsKey>::b == 0x58712834 &&
	CompileTimeSHA256<g_globalFlagsKey>::c == 0x9053B65E &&
	CompileTimeSHA256<g_globalFlagsKey>::d == 0xEDAB6607 &&
	CompileTimeSHA256<g_globalFlagsKey>::e == 0x85F5989D &&
	CompileTimeSHA256<g_globalFlagsKey>::f == 0x7788F311 &&
	CompileTimeSHA256<g_globalFlagsKey>::g == 0xE799B490 &&
	CompileTimeSHA256<g_globalFlagsKey>::h == 0x3053EABA,
	"incorrect GLOBAL_FLAGS_KEY");

