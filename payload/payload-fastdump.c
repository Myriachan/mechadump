#include <stddef.h>
#include <stdint.h>

typedef struct PatchRegisterSet_
{
	uint8_t m_enable;
	uint8_t m_dummy[3];
	uint32_t m_target;
	uint32_t m_value;
	uint32_t m_dummy2;
} PatchRegisterSet;
_Static_assert(sizeof(PatchRegisterSet) == 0x10, "bad PatchRegisterSet type");
_Static_assert(offsetof(PatchRegisterSet, m_enable) == 0x00, "bad PatchRegisterSet type");
_Static_assert(offsetof(PatchRegisterSet, m_target) == 0x04, "bad PatchRegisterSet type");
_Static_assert(offsetof(PatchRegisterSet, m_value) == 0x08, "bad PatchRegisterSet type");

typedef struct PatchRegisters_
{
	PatchRegisterSet m_sets[4];
	uint32_t m_dummy[4];
	uint16_t m_lockControl;
} PatchRegisters;
_Static_assert(offsetof(PatchRegisters, m_sets) == 0x00, "bad PatchRegisters type");
_Static_assert(offsetof(PatchRegisters, m_lockControl) == 0x50, "bad PatchRegisters type");


static uint32_t DisableInterrupts();
static void RestoreInterrupts(uint32_t oldCPSR);


__attribute__((__target__("thumb")))
void payload_start(int dummy0, int dummy1, int dummy2, const uint8_t* address)
{
	(void) dummy0;
	(void) dummy1;
	(void) dummy2;
	
	// Don't let us messing with the patch registers cause problems.
	uint32_t oldCPSR = DisableInterrupts();
	
	// Disable patches so that we get a clean dump.
	volatile PatchRegisters* patchRegs = (volatile PatchRegisters*) 0x03880000;
	patchRegs->m_lockControl = 0xE669;
	patchRegs->m_lockControl = 0xCF7E;
	
	// Save old patch configuration while we disable.
	uint8_t patchEnables[4];
	for (int i = 0; i < 4; ++i)
	{
		patchEnables[i] = patchRegs->m_sets[i].m_enable;
		patchRegs->m_sets[i].m_enable = 0x00;
	}
	
	// Copy data to local buffer and checksum.
	uint8_t copy_buffer[14];
	unsigned checksum = 0;
	for (int i = 0; i < 14; ++i)
	{
		uint8_t b = address[i];
		copy_buffer[i] = b;
		checksum += b;
	}
	
	// The address itself is part of the checksum.
	for (int i = 0; i < sizeof(uint32_t); ++i)
	{
		checksum += (uint8_t) (((uintptr_t) address) >> (i * 8));
	}
	
	// Send a response code, checksum, and the 14 bytes back.
	volatile uint8_t* out_reg = (volatile uint8_t*) 0x036000D9;
	*out_reg = 0x69;
	*out_reg = (uint8_t) ~checksum;
	
	for (int i = 0; i < 14; ++i)
	{
		*out_reg = copy_buffer[i];
	}

	// Restore patch configuration.
	for (int i = 0; i < 4; ++i)
	{
		patchRegs->m_sets[i].m_enable = patchEnables[i];
	}
	patchRegs->m_lockControl = 0x78C7;
	patchRegs->m_lockControl = 0xF6D7;

	// Restore CPSR before returning.
	RestoreInterrupts(oldCPSR);
}

__attribute__((__noinline__, __target__("arm")))
static uint32_t DisableInterrupts()
{
	uint32_t oldCPSR;
	__asm__(
		"mrs %0, CPSR\n\t"
		: "=r"(oldCPSR));

	__asm__ volatile(
		"msr CPSR_c, %0\n\t"
		:
		: "r"(oldCPSR | 0x000000C0));

	return oldCPSR;
}

__attribute__((__noinline__, __target__("arm")))
static void RestoreInterrupts(uint32_t oldCPSR)
{
	__asm__ volatile(
		"msr CPSR_c, %0\n\t"
		:
		: "r"(oldCPSR));
}
