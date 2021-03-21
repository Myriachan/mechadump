#include <cstring>

#include "knowndumps.hpp"


struct KnownDump
{
	constexpr KnownDump(const KnownDump&) = default;
	constexpr KnownDump& operator =(const KnownDump&) = default;
	
	constexpr KnownDump(const char* hexString);
	
	sha256::digest m_digest;
	
private:
	static constexpr uint8_t ByteFromHexNibble(char ch);
	static constexpr uint8_t ByteFromHexString(const char* hexString, size_t index);
};


constexpr uint8_t KnownDump::ByteFromHexNibble(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return static_cast<uint8_t>(ch - '0');
	if ((ch >= 'A') && (ch <= 'F'))
		return static_cast<uint8_t>(ch - 'A' + 10);
	if ((ch >= 'a') && (ch <= 'f'))
		return static_cast<uint8_t>(ch - 'a' + 10);
	
	// Make constexpr barf
	return *reinterpret_cast<char*>(ch) * 0;
}


constexpr uint8_t KnownDump::ByteFromHexString(const char* hexString, size_t index)
{
	return static_cast<uint8_t>(
		(ByteFromHexNibble(hexString[index * 2]) << 4) +
		ByteFromHexNibble(hexString[index * 2 + 1]));
}


constexpr KnownDump::KnownDump(const char* hexString)
:	m_digest{
		ByteFromHexString(hexString, 0x00), ByteFromHexString(hexString, 0x01),
		ByteFromHexString(hexString, 0x02), ByteFromHexString(hexString, 0x03),
		ByteFromHexString(hexString, 0x04), ByteFromHexString(hexString, 0x05),
		ByteFromHexString(hexString, 0x06), ByteFromHexString(hexString, 0x07),
		ByteFromHexString(hexString, 0x08), ByteFromHexString(hexString, 0x09),
		ByteFromHexString(hexString, 0x0A), ByteFromHexString(hexString, 0x0B),
		ByteFromHexString(hexString, 0x0C), ByteFromHexString(hexString, 0x0D),
		ByteFromHexString(hexString, 0x0E), ByteFromHexString(hexString, 0x0F),
		ByteFromHexString(hexString, 0x10), ByteFromHexString(hexString, 0x11),
		ByteFromHexString(hexString, 0x12), ByteFromHexString(hexString, 0x13),
		ByteFromHexString(hexString, 0x14), ByteFromHexString(hexString, 0x15),
		ByteFromHexString(hexString, 0x16), ByteFromHexString(hexString, 0x17),
		ByteFromHexString(hexString, 0x18), ByteFromHexString(hexString, 0x19),
		ByteFromHexString(hexString, 0x1A), ByteFromHexString(hexString, 0x1B),
		ByteFromHexString(hexString, 0x1C), ByteFromHexString(hexString, 0x1D),
		ByteFromHexString(hexString, 0x1E), ByteFromHexString(hexString, 0x1F)
	}
{
}


extern const KnownDump g_knownDumps[] =
{
	// * Mexican "fat" PS2 SCPH-50011 has a version that calls itself 5.06.0, but has a different date.
	//   It came after 5.12.0, so it probably should've been numbered 5.16.0.
	// * If 5.08 exists, it probably is an early DESR-5000 PSX with CXR706080-701GG.
	// * If 6.08 exists, it probably is a late SCPH-750xx or early SCPH-770xx with CXR716080-105GG.
	"81347ef6021bfc786139e4af88feac61af4ca07d79eca0ae8df32f4f08e4256f", // 5.00.0         CXR706080-101GG
	"0c9edfabfce9e2eaa981c618ada9fd5ffdda7e7a6bad2e62e35404be7954f750", // 5.02.0         CXR706080-102GG
	"80d660afffb7c56e1f41c8e7dfe88d301f87cbb98780cbd267370c806d39c544", // 5.04.0         CXR706080-103GG
	"6dbb29570ad2c7f9e713eec8f1ca8627245c24afd18ff22e264907e470cb6d09", // 5.06.0         CXR706080-104GG
	"37f2f9d900046d54a04803b818f04dde165e2e1984b3389a4cd13e77bac422f2", // 5.06.0 Mexico  CXR706080-106GG
	"36af871458150ccf14e411b59bded46b4f068158b412b8ebedf997b0fdf6bb47", // 5.10.1 PSX     CXR706080-702GG
	"8614f586091bf67d9f9d80d8594da9639e8c0b60e7e209892b5020884685b863", // 5.12.0         CXR706080-105GG
	"6ab6e70d40b6bc17b52ddfc6612a5cf7287a895f101006ebc7aa3d144b85de54", // 5.14.1 PSX     CXR706080-703GG
	"c4b7cc053da6b17d1ab8a6938c38e2ed16fce811dee4d2fe7507508581ecc6dc", // 6.00.0         CXR716080=101GG
	"d4a840191960705cce4aa0a939c8fc387fae40bc3e050ebeeb48e21aec20dd37", // 6.02.0         CXR716080-102GG
	"ebe1175fdaaef195cd776d3afa63c6b4c995cad7114d14d14ab17c2c04020ea2", // 6.04.0         CXR716080-103GG
	"f435866772a4fb9c4fdb68d3317bb36d72d8999d2efa210f55ae75320bfff616", // 6.06.0         CXR716080-104GG
	"68a23e873f6aabdcfd49ace2f9143ebaedf57c37a73ad86e72db3ded1ebc94d4", // 6.10.0         CXR716080-106GG
	"18371ba394e6339d861cfe19db7db7c9e44c8067114888421ee4a35feaaeba1b", // 6.12.0         CXR726080-301GB
};


bool IsKnownDump(const sha256::digest& dig)
{
	for (const KnownDump& dump : g_knownDumps)
	{
		if (std::memcmp(dig, dump.m_digest, sizeof(dig)) == 0)
			return true;
	}
	return false;
}
