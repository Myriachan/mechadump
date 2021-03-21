.section ".text"
.syntax unified
.align 4

// Offset 0x00
.thumb
.globl _start
_start:
	// Addresses to patch...
	.word 0x00000164
	.word 0x00000168
	.word 0x0000016C
fixup_row_0:
	.word 0

// Offset 0x10
.arm
replacement:
	mov r0, #0
	ldr r0, [r0, #0x28]
	ldr pc, [r0, #0x78]
fixup_row_1:
	.word 0xFFFFFFFF

	//---- ROW 2
// Offset 0x20
// This offset is normally the SVC dispatch addresses, but because the initial
// patch set doesn't patch in an SVC, we can put an extra 16 bytes of code.
//
// This function is called from the patch code we placed at 0x00000200.
.arm
initialize_thunk:
	adr r0, initialize + 1
	bx r0
.thumb
initialize:
	push {r2-r3}
	
fixup_row_2:
	// Safe to destroy r0,r1,r2,r3,flags
	.hword (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)
	
	// Search the ROM for the SCMD 03 dispatch table.
	ldr r1, constant_000001A3
	ldr r2, constant_000009A4
	//---- ROW 3
	movs r0, #0
	// DEBUGGING ONLY - reduces search size
	//ldr r0, constant_00038000
search_loop:
	ldmia r0!, {r3}
	cmp r3, r1
	bne search_loop
	ldr r3, [r0, #4]
	cmp r3, r2
	bne search_loop
	
fixup_row_3:
	// Safe to destroy r1,r2,r3,flags
	.hword (1 << 1) | (1 << 2) | (1 << 3)
	//---- ROW 4
fixup_row_4:
	// Safe to destroy r1,r2,r3,flags
	.hword (1 << 1) | (1 << 2) | (1 << 3)
	
	// Unlock patch registers.
	ldr r2, constant_CF7EE669
	ldr r1, constant_03880050
	strh r2, [r1]
	lsrs r2, r2, #16
	strh r2, [r1]
	
	// Disable patches in patch registers.
	subs r1, r1, #0x10
	movs r2, #0
	//---- ROW 5
disable_patch_loop:
	subs r1, r1, #0x10
	strb r2, [r1]
	lsls r3, r1, #24
	bne disable_patch_loop

fixup_row_5:
	// Safe to destroy r2,r3
	.hword (1 << 2) | (1 << 3)

	// Hook the SCMD 03:A4 handler.
	adr r2, scmd_03_A4_handler
	// While we have scmd_03_A4_handler's address, write the address of the
	// original handler for scmd_03_A4_handler to use.
	ldr r3, [r0]   // must this be done before patch register write?
	str r3, [r2, #original_scmd_03_A4_handler - scmd_03_A4_handler]
	//---- ROW 6 (SHORT ROW - AND NO FIXUP REQUIRED)
	// Now we finish the writes to the patch registers.
	str r0, [r1, #0x04]
	adds r2, r2, #1
	str r2, [r1, #0x08]
	
	// Enable this patch.
	movs r0, #1
	strb r0, [r1]

	// Lock patch registers.
	adds r1, r1, #0x50
	ldr r2, constant_F6D778C7
	//---- ROW 7
	strh r2, [r1]
	lsrs r2, r2, #16
	strh r2, [r1]

	// Resume with code we patched out.
	movs r0, #0x164 / 2
	adds r0, r0
fixup_row_7:
	.hword (1 << 1) | (1 << 2) | (1 << 3)
	// Restore critical registers.
	pop {r2-r3}
	// Return.
	bx r0

	//---- ROW 8 
	// This halfword is wasted due to alignment, so we might as well use it as
	// a branch target for the below code.
execute:
	bx r2


// *** We just aligned.  Cause assembler error if not so we can correct for this.
//.align 2
.thumb
scmd_03_A4_handler:
	// Read address of 03:A4 plus 4 written there by initialize.
	ldr r0, original_scmd_03_A4_handler
	subs r0, r0, #1
	// r0 contains the address of the original SCMD 03:A4 handler.
	// Get the address of the SCMD parameter buffer.  +4 is where the actual
	// parameters are located.  While we're at it, grab the constant
	// 0x036000D0 (SCMD result FIFO, but minus 9).
	ldr r1, [r0, #0x78]   // 0x036000D0
	ldr r0, [r0, #0x70]   // SCMD input buffer (+4 = parameters)

fixup_row_8:
	// Safe to destroy r2,r3
	.hword (1 << 2) | (1 << 3)
	
	// 03:A4 takes exactly one parameter, A4, or will fail.  However, nothing
	// clears the parameter buffer, so rely on that to get extra parameters.
	// Check for magic bytes "MCD" after A4.
	ldr r2, [r0, #4]
		
	ldr r3, constant_44434DA4
	//---- ROW 9
	cmp r2, r3
	bne error

fixup_row_9:
	// Safe to destroy r2,r3
	.hword (1 << 2) | (1 << 3)

	ldr r2, [r0, #8]
	cmp r2, #1
	ldr r2, [r0, #12]
	ldr r3, [r0, #16]
	beq execute          // 1 = execute
	//---- ROW A
	bcs write            // 2 = write
	ldr r2, [r2]         // 0 = load
	b done
	
fixup_row_A:
	// Between code blocks; anything can be here.
	.hword 0xFFFF
	
write:
	str r3, [r2]
	
done:
	// r0 contains a RAM address (0x0200????), so we can shift it right to use
	// to count to 4 instead of initializing it.
	movs r3, #0x42  // success code
error:  // R3's low byte will be 0xA4
	strb r3, [r1, #0x09]
result_loop:
	strb r2, [r1, #0x09]   // 0x036000D9 (SCMD result FIFO)
	//---- ROW B
	lsrs r2, r2, #8
	lsrs r0, r0, #8
	bne result_loop
	
fixup_row_B:
	// Safe to destroy r0,r1,r2,r3
	.hword (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)
	
	// An ordinary return works just fine.
	bx lr

// We should be aligned here; cause an error if not so we can correct it.
// However, in terms of the checksum system, we're misaligned.
//.align 2
constant_000001A3:
	.word 0x000001A3
constant_000009A4:
	.word 0x000009A4
	//---- ROW C
original_scmd_03_A4_handler:
fixup_row_C:
	// Overwritten with address of 03:A4 handler.  We can also use this field
	// for the checksum brute-force.
	.word 0xFFFFFFFF
constant_CF7EE669:
	.word 0xCF7EE669
	//---- ROW D
constant_03880050:
	.word 0x03880050
constant_F6D778C7:
	.word 0xF6D778C7
constant_44434DA4:
	.word 0x44434DA4


// WriteConfig checksum bypass information.
.align 2
fixup_list_start:
	// 1 = uint16 (or uint32) that can take any value possible
	// 2 = special handling for an unneeded patch address
	// 3 = thumb instruction allowed to modify flags and any specified register from uint16
	.word 2, fixup_row_0 - _start
	.word 1, fixup_row_1 - _start
	.word 3, fixup_row_2 - _start
	.word 3, fixup_row_3 - _start
	.word 3, fixup_row_4 - _start
	.word 3, fixup_row_5 - _start
	// Row 6 does not require brute force help, because it has the first 0x70-block checksum.
	.word 3, fixup_row_7 - _start
	.word 3, fixup_row_8 - _start
	.word 3, fixup_row_9 - _start
	.word 1, fixup_row_A - _start
	.word 3, fixup_row_B - _start
	.word 1, fixup_row_C - _start

	// The final uint32 is how big the primary payload area is.
	.word fixup_list_start - _start
