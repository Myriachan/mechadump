OUTPUT_FORMAT("elf32-littlearm")
ENTRY(_start)
phys = 0x00010000;
SECTIONS
{
	.text phys : AT(phys) {
		code = .;
		*(.text)
		*(.rodata)
		. = ALIGN(4);
	}
	.rodata : AT(phys + (rodata - code))
	{
		rodata = .;
		*(.rodata)
		. = ALIGN(4);
	}
	.data : AT(phys + (data - code))
	{
		data = .;
		*(.data)
		. = ALIGN(4);
	}
	.bss : AT(phys + (bss - code))
	{
		bss = .;
		*(.bss)
		. = ALIGN(4);
	}
	end = .;
}
