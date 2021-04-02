# MechaDump
MechaDump is a program to dump the firmware from "Dragon"-series PS2 Mechacon chips: SCPH-500xx, all slim systems, and all "PSX" systems.  It also dumps the "keystore" of secret keys.

**WARNING: THIS PROGRAM IS DANGEROUS.**  It can _easily_ "brick" your PS2 if something goes wrong, requiring soldering and a Raspberry Pi to fix it.

**FOLLOW DIRECTIONS.  USE AT YOUR OWN RISK.**

## Using MechaDump
Please follow these directions.  Bad things could happen if something goes wrong.

1. Verify that you have a compatible PS2.  It needs to be an SCPH-500xx, a "slim", or a "PSX" (the DVR PS2).  Earlier consoles are not supported.
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
1. The dump is done!  You will find "mechacon-_version_-_date_.bin" and "mechacon-keystore.bin" on your USB stick.

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
  * The 3DS homebrew SDK might work, but I haven't tried.  [3DBrew: Setting up Development Environment](https://www.3dbrew.org/wiki/Setting_up_Development_Environment)
* Two secret keys, which aren't provided here, but are easy to find online.
  * `MECHA_PATCH_KEY`
  * `GLOBAL_FLAGS_KEY`

### Make
You need to tell the Makefile the names of your PS2 GCC and your ARM GCC.  You do this by setting the Makefile variables `EE_PREFIX` and `ARM_PREFIX` to the part
of the name of GCC for these targets before `gcc`, including any final dashes.  For example, if your R5900 (EE) compiler for PS2 is named `meow-ee-gcc`, you use
`EE_PREFIX=meow-ee-`.  Likewise for your ARM32 compiler.  For a standard PS2SDK setup, `EE_PREFIX` is `mips64r5900el-ps2-elf-`.

You also need to provide the keys using `MECHA_PATCH_KEY=` and `GLOBAL_FLAGS_KEY=` parameters.  There is also an optional `HOST_PREFIX` if you need to use a special `gcc` for
your Linux system itself.  (If you're crazy and building from a Raspberry Pi, `ARM_PREFIX=` will work because the host GCC is also ARM32.)

Here is an example compile string based on my setup, using bogus keys:

```
make EE_PREFIX=mips64r5900el-ps2-elf- ARM_PREFIX=arm-none-eabi- MECHA_PATCH_KEY=0123456789abcdef GLOBAL_FLAGS_KEY=fedcba9876543210
```

## How It Works
### Background
It appears that a recurring problem Sony had with pre-50000 Mechacon chips is that they would find problems, and have to make a new mask ROM in order to fix them.  That is very expensive and slow.

In the new Dragon redesign of the Mechacon, Sony added the ability to patch the Mechacon code by writing the patches to EEPROM.  Then the factory and repair facilities could update these patches through "test mode" (a serial port connected to Mechacon).

Naturally, Sony encrypted the patches.  But, since this project exists, you can correctly guess that that didn't quite work out as planned.

The patches were encrypted with DES in ECB mode, with no MAC.  The use of ECB meant that any block of 8 zero bytes would always encrypt to the same pattern.  When we noticed the repeating pattern, we guessed that they were using ECB mode, and these were encrypted blocks of zeros.  That guess turned out correct.  We hoped that Sony had used single-DES and not double- or triple-DES, and it turns out they did.  Brute-forcing single-DES is actually rather cheap; 2<sup>56</sup> isn't as big a number as it used to be.

### Exploiting
Sony intended that only by connecting to the "test mode" serial port could patches be updated or the region code be changed, so that at the least you'd have to open your system in order to hack it even if the crypto was compromised.  But there are bugs in the Mechacon.

In the Dragon series Mechacons, because there is security-critical data in EEPROM, the "WriteNVM" S-command no longer allows software to write to anywhere within EEPROM.  Only certain regions are writable, and patches are not one of them.

Certain system settings are stored in EEPROM by OSDSYS, like video mode.  EEPROM bytes 0x200 through 0x31F are reserved for software-writable configuration in blocks of 16 bytes.  Software can write to these areas by using the "OpenConfig" and "WriteConfig" S-commands.  The patch data in EEPROM is 0x320-0x3FF, right after the config area.  OpenConfig takes a length parameter that wasn't sufficiently validated, so we can write past the end into the patch data - oops.

Two complications happen.  The first is that the WriteConfig buffer wraps every 256 bytes because the index value is a uint8.  This just complicates the write exploit a bit.  The regular config gets trashed, so we have to restore it after writing the patch data.  No big deal.

The other complication is considerably more annoying: WriteConfig requires that the 16th byte of each block of 16 bytes be a checksum of the previous 15 bytes.  And even better, the patches themselves have their own checksum data.  We have to simultaneously meet all the checksums...all with DES-encrypted patch data.

That required some engineering.  We made a way for our patch code to have useless instructions that could be brute-forced to match the required checksum bytes.  A table in the compiled patch says which bytes are changeable for this purpose.  The tool `mechapatchtool` in this project does the brute-forcing to find a useless harmless instruction to put at various places such that when encrypted, it happens to have  a valid checksum.

### Patching
The patch mechanism has four "channels".  Each channel allows replacing a uint32 of ROM with some other value.  So, we can change up to 16 bytes of the ROM.  The patch EEPROM data is loaded to a RAM address, and typically, the Sony ROM patches trigger jumps to this RAM area.

These patches are loaded from EEPROM during Mechacon startup.  Because Mechacon is the chip that watches for you to press the power button, it's running from the moment you plug in it.  This is why you have to unplug the system in order to reload patches.

Because this tool is specifically for dumping the Mechacon, we have a problem: how do we know what ROM addresses to patch, and how do we know where in RAM our patches have been loaded to?

This was a difficult problem that took a lot of trial and error until we managed to get our first dump blind.  Once we had the first dump, we could analyze the patching mechanism to understand what was happening.  Then we got a second dump of a different version.

Now we could see what would change between versions.  It turns out that near the beginning of ROM are two routines that are sufficiently large enough, safe enough to patch, can be triggered reliably, and change locations very little among ROM versions.  These two routines are the IRQ handler and one of the functions involved in the disc protection.  Hence the two options available.

The Mechacon has a system call (SVC) mechanism designed to facilitate patching.  If an SVC instruction occurs, the SVC handler jumps to an address written to the patch data.  Many Sony patches use this by putting an SVC instruction somewhere, then having code that gets executed as a hook on the patched address.

The SVC handler has a pointer to the RAM area at which patch EEPROM is loaded so it can do this.  The SVC handler itself moves among Mechacon versions... but the SVC vector at 0x00000028 has a pointer to the SVC handler.  The SVC handler doesn't change at all through the lifetime of the Dragon Mechacon, allowing us to use its pointer to the RAM location to find ourselves.

The code to find and read the SVC handler then jump to the RAM address is only 6 Thumb instructions, or 12 bytes, which fit in the 16 bytes of ROM patching we can do.  (Actually, we can only patch 12 bytes, because one of the patches needs to be a bogus location to match the 16-byte block checksum requirement of WriteConfig.)

### Hooking
Now we can execute arbitrary code, but what can we do with it that lets us dump the Mechacon back to PS2 software?

We search ROM for a certain pattern to find the handler for the S command 0x03 with subcommand 0xA4.  03:A4 is some region-related back door Sony has, and no OS nor game software is going to use this, so we just take it over.  From dumps, we know where the patch hardware registers are, so can change the patches at runtime to hook this function once we find it.

Now we have code that can be executed on demand by PS2 software, receive a request, and send a result.  So we make a simple back door that allows reading, writing, and executing arbitrary addresses.

And all this has to fit in 0xC0 bytes of code, including dummy instructions used to brute-force row checksums.  It took quite a bit of size optimization to fit everything needed.

## Credits
* AKuHAK
* asmblur
* balika011
* krHACKen
* l\_oliveira
* Mathieulh
* Myria
* V3ditata (for dumping the rare Latin American Mechacon version for us!)
* Others who wish to remain anonymous
