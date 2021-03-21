#include <stddef.h>
#include <stdint.h>


enum
{
	MECHACON_ROM_START = 0x00000000,
	MECHACON_ROM_SIZE = 0x44000,
	MECHACON_EEPROM_SIZE = 0x400,
};


// Address of the WriteNVM SCMD handler in ROM, saved for efficiency.
int (*scmd_0B_WriteNVM)(unsigned wordOffset, uint16_t value) = (int (*)(unsigned, uint16_t)) -1;


// The prolog code of WriteNVM, so we can find it in any version.
static const union
{
	uint8_t m_bytes[12];
	uint32_t m_words[3];
} c_pattern_WriteNVM = {
	{
		0xF0, 0xB5,  // push {r4-r7,lr}
		0x05, 0x1C,  // movs r5, r0
		0x0E, 0x1C,  // movs r6, r1
		0x01, 0x27,  // movs r7, #1
		0x00, 0x21,  // movs r1, #0
		0x01, 0x20,  // movs r0, #1
	}
};


__attribute__((__target__("thumb")))
void payload_start(int dummy0, int dummy1, int dummy2, uint32_t parameter)
{
	volatile uint8_t* scmdReply = (volatile uint8_t*) 0x036000D9;

	// Search for WriteNVM in ROM if we haven't yet.
	if (scmd_0B_WriteNVM == (int (*)(unsigned, uint16_t)) -1)
	{
		// NOTE: This code needs the -fno-isolate-erroneous-paths-dereference
		// GCC command line parameter, because otherwise it will assume that
		// reading null will crash and skip this code.
		const uint32_t* romEnd = (const uint32_t*) (MECHACON_ROM_START +
			MECHACON_ROM_SIZE - sizeof(c_pattern_WriteNVM));
		const uint32_t* search;
		uint32_t firstWord = c_pattern_WriteNVM.m_words[0];
		for (search = (const uint32_t*) MECHACON_ROM_START; search <= romEnd; ++search)
		{
			if (*search == firstWord)
			{
				if ((search[1] == c_pattern_WriteNVM.m_words[1]) &&
					(search[2] == c_pattern_WriteNVM.m_words[2]))
				{
					goto Found;
				}
			}
		}
		
		*scmdReply = 0x86;
		return;
		
	Found:
		// Don't forget to OR with 1 because it's a Thumb function.
		scmd_0B_WriteNVM = (int (*)(unsigned, uint16_t)) (((uintptr_t) search) | 1);
	}
	
	// Act the same way as the real WriteNVM: use a big-endian address
	// and value.  The back door doesn't know that the parameter is bytes
	// rather than a uint32_t, so put it back and parse it that way.
	union
	{
		uint32_t m_word;
		uint8_t m_bytes[4];
	} converter;
	converter.m_word = parameter;
	
	unsigned wordOffset = (((unsigned) converter.m_bytes[0]) << 8) + converter.m_bytes[1];
	uint16_t value = (uint16_t) ((((unsigned) converter.m_bytes[2]) << 8) + converter.m_bytes[3]);

	// Address out of range?
	if (wordOffset > MECHACON_EEPROM_SIZE / sizeof(uint16_t))
	{
		*scmdReply = 0x80;
		return;
	}

	// Execute and return the result.
	int result = (*scmd_0B_WriteNVM)(wordOffset, value);
	*scmdReply = (uint8_t) result;
}
