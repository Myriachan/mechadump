.section ".text.crt0"
.syntax unified


.globl _start
.thumb
.align 2
_start:
	push {r0-r3}

	// Find our slide value.
	adr r0, .mypool
	ldr r1, =.mypool
	subs r0, r0, r1

	// Check whether we've relocated already.
	ldr r1, =__relocated
	adds r1, r1, r0
	ldr r2, [r1]
	cmp r2, #0
	bne .already_relocated
	movs r2, #1
	str r2, [r1]

	// Zero out the .bss section.
	ldr r1, =__bss
	adds r1, r1, r0
	ldr r2, =__bss_end
	adds r2, r2, r0
	movs r3, #0
.bss_clear_loop:
	cmp r1, r2
	bcs .bss_clear_done
	str r3, [r1]
	adds r1, r1, #4
	b .bss_clear_loop
.bss_clear_done:

	// Adjust the pointers in the GOT.
	ldr r1, =__got
	adds r1, r1, r0
	ldr r2, =__got_end
	adds r2, r2, r0
.got_fixup_loop:
	cmp r1, r2
	bcs .got_fixup_done
	ldr r3, [r1]
	adds r3, r3, r0
	str r3, [r1]
	adds r1, r1, #4
	b .got_fixup_loop
.got_fixup_done:

.already_relocated:
	// We need a register that isn't r0...r3.  r12 is expressly designated
	// as destroyable by linker-generated thunk code, so use that.
	ldr r1, =payload_start+1
	adds r1, r1, r0
	mov r12, r1
	pop {r0-r3}
	bx r12

.align 2
.mypool:
.pool


.section ".data"
.align 2
__relocated:
	.word 0
