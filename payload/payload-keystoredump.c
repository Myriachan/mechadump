#include <stddef.h>
#include <stdint.h>


__attribute__((__target__("thumb")))
void payload_start(int dummy0, int dummy1, int dummy2, uint32_t parameter)
{
	union
	{
		uint8_t m_bytes[8];
		uint16_t m_halfWords[4];
	} keyData;

	// Unlock the keystore mechanism.
	volatile uint8_t* unlockReg1 = (volatile uint8_t*) 0x94255ABC;
	volatile uint8_t* unlockReg2 = (volatile uint8_t*) 0x94255EF0;
	volatile uint8_t* unlockReg3 = (volatile uint8_t*) 0xFFFFF234;
	volatile uint8_t* unlockReg4 = (volatile uint8_t*) 0xFFFFF678;

	*unlockReg1 = 0xC9;
	*unlockReg2 = 0xCB;
	*unlockReg3 = 0x5D;
	*unlockReg4 = 0x58;
	
	// Select key to use.
	volatile uint16_t* keystoreOffset = (volatile uint16_t*) 0x03004F00;
	*keystoreOffset = (uint16_t) parameter;

	// Read key out.
	volatile uint16_t* keystoreData = (volatile uint16_t*) 0x03004F08;
	keyData.m_halfWords[0] = *keystoreData;
	keyData.m_halfWords[1] = *keystoreData;
	keyData.m_halfWords[2] = *keystoreData;
	keyData.m_halfWords[3] = *keystoreData;

	// Lock the keystore mechanism.
	*unlockReg1 = 0x9C;
	*unlockReg2 = 0xBC;
	*unlockReg3 = 0xD5;
	*unlockReg4 = 0x85;

	// Return the result.
	volatile uint8_t* scmdReply = (volatile uint8_t*) 0x036000D9;
	*scmdReply = 0x00;
	for (int i = 0; i < sizeof(keyData.m_bytes); ++i)
	{
		*scmdReply = keyData.m_bytes[i];
	}
}
