# MechaDump
MechaDump is a program to dump the firmware from "Dragon"-series PS2 Mechacon chips: SCPH-500xx, all slim systems, and all "PSX" systems.

**WARNING: THIS PROGRAM IS DANGEROUS.**  It can _easily_ "brick" your PS2 if something goes wrong, requiring soldering and a Raspberry Pi to fix it.

**FOLLOW DIRECTIONS.  USE AT YOUR OWN RISK.**

## Using MechaDump
Please follow these directions.  Bad things could happen if something goes wrong.

1. Verify that you have a compatible PS2.  It needs to be an SCPH-500xx, a "slim", or a "PSX" (the DVR PS2).  Earlier consoles are not supported.
1. You'll need a PS2 controller.  PS1 controllers don't work yet.
1. Set up a way to run PS2 ELF files.
   1. Free McBoot on most consoles - only some SCPH-900xx systems aren't supported
   1. OpenTuna or Fortuna - not as convenient, but works on FMCB-incompatible SCPH-900xx systems
1. Copy mechadump.elf to a USB stick (or SD card with USB adaptor) that works with your PS2.
1. Run mechadump.elf from the USB stick on your PS2.
1. Accept the warning.
1. Proceed past the system information screen.
1. At the main menu, **BACK UP YOUR EEPROM**.
1. After EEPROM is backed up, select "Install Backdoor".
1. From here, you have two choices:
   1. "CD Protect Hook" back door.  This option is safer, but requires a legitimate PS2 disc for your PS2's region, and a working PS2 CD drive to read it.
   1. "IRQ Hook" back door.  This is more dangerous, but doesn't require a legitimate PS2 disc or a working PS2 CD drive.
1. Once it is installed, you need to _remove power from the machine_.  Turning off the normal way _will not work_.  On the "fat" system (SCPH-500xx), you can flip the power
switch in the back.  On other systems, you'll need to pull the power plug.
1. Turn your system back on and return to mechadump.elf.
1. At the main menu, this time select "Dump Mechacon ROM", which should no longer be grayed out.
   1. If using "CD Protect Hook", you may be prompted to insert a legitimate disc.  Do it when asked.
1. This will take a bit (~5 minutes).
1. When it's done, return to the MechaDump main menu and select "Restore EEPROM".
1. After the restore, your system will automatically turn off, and will be back to normal.  You don't need to remove power again.
1. You're done!

**DO NOT** leave your system in the "back door" state.  Restore your EEPROM before using your PS2 normally again.  Failure to do this could result in reliability issues:
Sony fixed bugs in the Mechacon at the factory, and MechaDump's back door disables these patches.  Restoring EEPROM will reinstall the patches.

## Building
Compiling MechaDump is easy...once you get the prerequisites, which is the hard part.  We need 3 different versions of GCC: one for the machine doing the compile, one for the PS2
Emotion Engine, and one for the Mechacon, which is a 32-bit ARM chip (ARM7TDMI).

### Prerequisites
You need:
* A Linux system.
  * Ubuntu for Windows works; it's what I use!
* GCC and G++ for your Linux system.
* The homebrew PS2 SDK installed in Linux.
  * https://github.com/ps2dev/ps2sdk
* An ARM32 GCC and G++ installed in Linux.
  * The Ubuntu package `gcc-arm-none-eabi` will work.
  * The 3DS homebrew SDK might work, but I haven't tried.  https://www.3dbrew.org/wiki/Setting_up_Development_Environment
* Two secret keys, which aren't provided here, but are easy to find online.
  * `MECHA_PATCH_KEY`
  * `GLOBAL_FLAGS_KEY`

### Make
You need to tell the Makefile the names of your PS2 GCC and your ARM GCC.  You do this by setting the Makefile variables `EE_PREFIX` and `ARM_PREFIX` to the part
of the name of GCC for these targets before `gcc`, including any final dashes.  For example, if your R5900 (EE) compiler for PS2 is named `meow-ee-gcc`, you use
`EE_PREFIX=meow-ee-`.  Likewise for your ARM32 compiler.

You also need to provide the keys using `MECHA_PATCH_KEY=` and `GLOBAL_FLAGS_KEY=` parameters.  There is also an optional `HOST_PREFIX` if you need to use a special `gcc` for
your Linux system itself.  (If you're crazy and building from a Raspberry Pi, `ARM_PREFIX=` will work because the host GCC is also ARM32.)

Here is an example compile string based on my setup:

```
make EE_PREFIX=mips64r5900el-ps2-elf- ARM_PREFIX=arm-none-eabi- MECHA_PATCH_KEY=0123456789abcdef GLOBAL_FLAGS_KEY=fedcba9876543210
```
