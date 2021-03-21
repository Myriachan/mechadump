OUTPUT_FORMAT("elf32-littlearm")
ENTRY(_start)
__phys = 0x00010000;
SECTIONS
{
	.text __phys : AT(__phys) {
		__code = .;
		*(.text.crt0)
		*(.text)
		. = ALIGN(4);
	}
	.rodata : AT(__phys + (__rodata - __code))
	{
		__rodata = .;
		*(.rodata)
		. = ALIGN(4);
	}
	.data : AT(__phys + (__data - __code))
	{
		__data = .;
		*(.data)
		. = ALIGN(4);
	}
	.got : AT(__phys + (__got - __code))
	{
		__got = .;
		*(.got)
		. = ALIGN(4);
		__got_end = .;
	}
	.got.plt : AT(__phys + (__got_plt - __code))
	{
		__got_plt = .;
		*(.got.plt)
		. = ALIGN(4);
	}
	.bss : AT(__phys + (__bss - __code))
	{
		__bss = .;
		*(.bss)
		. = ALIGN(4);
		__bss_end = .;
	}
	__end = .;
}
