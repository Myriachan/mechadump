#include "common.hpp"

#include <cassert>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <debug.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <libcdvd.h>
#include <libmc.h>
#include <libmtap.h>
#include <libpad.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <unistd.h>

#include "configexploit.hpp"
#include "dumper.hpp"
#include "knowndumps.hpp"
#include "sysinfo.hpp"

#include "crc32.h"


constexpr int c_versionMajor = 1;
constexpr int c_versionMinor = 2;


extern "C" unsigned int size_irx_fileXio;
extern "C" unsigned char irx_fileXio[];
extern "C" unsigned int size_irx_padman;
extern "C" unsigned char irx_padman[];
extern "C" unsigned int size_irx_sio2man;
extern "C" unsigned char irx_sio2man[];
extern "C" unsigned int size_irx_iomanX;
extern "C" unsigned char irx_iomanX[];
extern "C" unsigned int size_irx_mtapman;
extern "C" unsigned char irx_mtapman[];
extern "C" unsigned int size_irx_mcman;
extern "C" unsigned char irx_mcman[];
extern "C" unsigned int size_irx_mcserv;
extern "C" unsigned char irx_mcserv[];
extern "C" unsigned int size_irx_usbd;
extern "C" unsigned char irx_usbd[];
extern "C" unsigned int size_irx_usbhdfsd;
extern "C" unsigned char irx_usbhdfsd[];

extern "C" unsigned int size_patch_irq_hook;
extern "C" unsigned char patch_irq_hook[];
extern "C" unsigned int size_patch_cdprotect_hook;
extern "C" unsigned char patch_cdprotect_hook[];

extern "C" unsigned int size_payload_fastdump;
extern "C" unsigned char payload_fastdump[];
extern "C" unsigned int size_payload_writenvm;
extern "C" unsigned char payload_writenvm[];
extern "C" unsigned int size_payload_keystoredump;
extern "C" unsigned char payload_keystoredump[];

const std::vector<uint8_t> patch_empty_vector = MakeEmptyPatchset();


enum NvmState
{
	NVM_STATE_EMPTY,
	NVM_STATE_OTHER,
	NVM_STATE_IRQ_HOOK,
	NVM_STATE_CDPROTECT_HOOK,
};

alignas(64) char g_pad0Buffer[256];

alignas(16) uint8_t g_eeprom[0x400];

int g_mechaconMajor = -1;
int g_mechaconMinor = -1;
bool g_systemIsDEX = false;
std::string g_refreshDate;
std::string g_modelString;


class SimpleDebugOutput : public DebugOutput
{
public:
	virtual void VPrintf(const char* format, va_list args)
	{
		std::vprintf(format, args);
		std::fflush(stdout);
	}
};


// HACK: Hide the silly cursor the debug drawing does.
extern "C" uint8_t msx[];
void HackToHideCursor()
{
	std::memset(&msx[219 * 8], 0, 8);
}


void ResetEverything(bool isPS2Link)
{
	if (!isPS2Link)
	{
		// Initialize connection to IOP.
		SifInitRpc(0);
		
		// Reset the IOP with the default ROM modules.
		while (!SifIopReset("", 0))
			;
		while (!SifIopSync())
			;
		SifInitRpc(0);
	
		// Patch the kernel as needed.
		sbv_patch_enable_lmb();
		sbv_patch_disable_prefix_check();
		sbv_patch_fileio();
		
		// Prepare to load modules.
		SifLoadFileInit();
	}
	
	struct IopModule
	{
		bool m_loadWhenPS2Link;
		const void* m_nameOrPointer;
		const unsigned int* m_size;
	};
	static const IopModule s_iopModules[] =
	{
		{ false, "rom0:CDVDMAN", nullptr },
		{ false, irx_iomanX, &size_irx_iomanX },
		{ false, irx_fileXio, &size_irx_fileXio },
		{ true, irx_sio2man, &size_irx_sio2man },
		{ true, irx_mtapman, &size_irx_mtapman },
		{ true, irx_padman, &size_irx_padman },
		{ true, irx_mcman, &size_irx_mcman },
		{ true, irx_mcserv, &size_irx_mcserv },
		{ true, irx_usbd, &size_irx_usbd },
		{ true, irx_usbhdfsd, &size_irx_usbhdfsd },
	};

	for (const IopModule& module : s_iopModules)
	{
		if (isPS2Link && !module.m_loadWhenPS2Link)
			continue;
		if (module.m_size)
		{
			SifExecModuleBuffer(const_cast<void*>(module.m_nameOrPointer), *module.m_size, 0, nullptr,
				nullptr);
		}
		else
		{
			SifLoadModule(static_cast<const char*>(module.m_nameOrPointer), 0, nullptr);
		}
	}
}


bool GetPressedButtons(int& buttons)
{
	if (padGetState(0, 0) != PAD_STATE_STABLE)
		return false;
	
	struct padButtonStatus status;
	if (!padRead(0, 0, &status))
		return false;
	
	buttons = status.btns ^ 0xFFFF;
	return true;
}


// Wait for either X or O.
void WaitForXorOConfirm()
{
	int buttons;
	for (;;)
	{
		if (!GetPressedButtons(buttons))
			continue;
		if (!(buttons & (PAD_CIRCLE | PAD_CROSS)))
			break;
	}
	
	for (;;)
	{
		if (!GetPressedButtons(buttons))
			continue;
		if (buttons & (PAD_CIRCLE | PAD_CROSS))
			break;
	}
}


void WarningScreen()
{
	scr_setbgcolor(0xFF000080);
	scr_clear();
	
	scr_setXY(0, 5);
	scr_printf("          MechaDump v%d.%02d\n", c_versionMajor, c_versionMinor);
	scr_printf("          Dumps Dragon-series Mechacon chips.\n");
	scr_printf(" \n");
	scr_printf("          *** WARNING ***\n");
	scr_printf("          THIS PROGRAM IS DANGEROUS, AND HAS A GOOD CHANCE OF\n");
	scr_printf("          BRICKING YOUR PS2, REQUIRING SOLDERING TO RECOVER.\n");
	scr_printf(" \n");
	scr_printf("          USE AT YOUR OWN RISK.\n");
	
	sleep(10);

	scr_printf(" \n");
	scr_printf("          Press X or O to continue.\n");
	
	WaitForXorOConfirm();
}


std::string RegionFlagsToString(uint32_t regionCode)
{
	static const struct
	{
		int m_bit;
		const char* m_string;
	} s_bits[] = {
		{ 20, "Internal" },
		{ 19, "Prototype" },
		{ 18, "Arcade" },
		{ 17, "QA" },
		{ 16, "DEX" },
		{ 7, "LatinAmerica" },
		{ 6, "China" },
		{ 5, "Russia" },
		{ 4, "Asia" },
		{ 3, "AustraliaNZ" },
		{ 2, "Europe" },
		{ 1, "NorthAmerica" },
		{ 0, "Japan" },
	};
	
	std::string response;
	for (auto&& entry : s_bits)
	{
		if (regionCode & (uint32_t(1) << entry.m_bit))
		{
			if (!response.empty())
				response.append(" ");
			response.append(entry.m_string);
		}
	}
	return response;
}


NvmState GetNVMState()
{
	if (std::memcmp(&g_eeprom[0x320], patch_irq_hook, 0xE0) == 0)
		return NVM_STATE_IRQ_HOOK;
	else if (std::memcmp(&g_eeprom[0x320], patch_cdprotect_hook, 0xE0) == 0)
		return NVM_STATE_CDPROTECT_HOOK;
	else if (std::memcmp(&g_eeprom[0x320], patch_empty_vector.data(), 0xE0) == 0)
		return NVM_STATE_EMPTY;
	else
		return NVM_STATE_OTHER;
}


bool CheckAndWaitForBackdoor()
{
	if (!IsBackDoorFunctioning())
	{
		NvmState nvmState = GetNVMState();
		if (nvmState == NVM_STATE_CDPROTECT_HOOK)
		{
			scr_setbgcolor(0xFF800000);
			scr_clear();
			scr_setXY(0, 5);
			scr_printf("          Back door not yet active.  Insert a legitimate disk\n");
			scr_printf("          for this PS2's region to activate it.\n");
			scr_printf("          Hold Triangle to cancel.\n");
			
			for (;;)
			{
				int buttons = 0;
				if (GetPressedButtons(buttons) && (buttons & PAD_TRIANGLE))
					return false;

				if (IsBackDoorFunctioning())
					break;

				sleep(1);
			}

			sceCdStop();
			sceCdSync(0);
		}
		else if (nvmState == NVM_STATE_IRQ_HOOK)
		{
			scr_setbgcolor(0xFF000080);
			scr_clear();
			scr_setXY(0, 5);
			scr_printf("          Back door not working for some reason!\n");
			scr_printf("          Press X or O to return to the main menu.\n");
			WaitForXorOConfirm();
			return false;
		}
	}
	return true;
}


bool SysinfoScreen()
{
	scr_setbgcolor(0xFF800000);
	scr_clear();
	scr_setXY(0, 5);
	
	scr_printf("          Retrieving system information...\n");
	
	// Read EEPROM.
	if (!ReadNVM(g_eeprom, 0, sizeof(g_eeprom)))
	{
		scr_setbgcolor(0xFF0000FF);
		scr_printf("          Could not dump EEPROM.  Cannot proceed.\n");
		return false;
	}
	
	// Model string...simple stuff
	g_modelString = GetModelString();
	scr_printf("          Model: %s\n", g_modelString.c_str());
	
	// Mechacon version stuff.
	if (!GetMechaconVersion(g_mechaconMajor, g_mechaconMinor, g_refreshDate))
	{
		scr_printf("          Could not retrieve Mechacon version!\n");
		return false;
	}
	
	g_systemIsDEX = static_cast<bool>(g_mechaconMinor & 1);
	g_mechaconMinor &= ~1;
	
	if (g_mechaconMajor < 5)
	{
		scr_setbgcolor(0xFF000080);
		scr_printf("          This PS2 is incompatible with this program.\n");
		scr_printf("          This program is only for consoles with 'Dragon' Mechacons:\n");
		scr_printf("          the 50000, 70000, 75000, 77000, 90000, PSX and Bravia series.\n");
		return false;
	}
	
	scr_printf("          Mechacon version: %d.%02d  %s\n", g_mechaconMajor, g_mechaconMinor,
		g_systemIsDEX ? "DEX" : "CEX");
	scr_printf("          Mechacon build date: %s\n", g_refreshDate.c_str());

	// Decode the region.
	uint32_t regionFlags;
	if (DecodeRegionFlags(g_eeprom, regionFlags))
		scr_printf("          Region code: %08" PRIX32 "  %s\n", regionFlags, RegionFlagsToString(
			regionFlags).c_str());
	else
		scr_printf("          Region code: UNKNOWN\n");
	
	// Compute patch data CRC.
	uint32_t patchCRC = crc32(&g_eeprom[0x320], 0xE0, 0);
	scr_printf("          Patchset CRC32: %08" PRIX32 "\n", patchCRC);

	scr_printf(" \n");
	scr_printf("          This system is compatible.\n");
	scr_printf("          Press X or O to continue.\n");
	
	WaitForXorOConfirm();
	
	return true;
}


std::string MakeDumpFilename()
{
	std::string result;
	result.reserve(64);
	result.append("mechacon-");

	char versionPart[20];
	std::snprintf(versionPart, sizeof(versionPart), "%d.%02d", g_mechaconMajor, g_mechaconMinor);
	result.append(versionPart);
	
	result.append("-");
	for (char ch : g_refreshDate)
	{
		if (ch == ' ')
			ch = '-';
		if ((ch < '0') || (ch > '9'))
			ch = '_';
		result.push_back(ch);
	}
	
	result.append(".bin");
	return result;
}


struct MenuOption
{
	const char* m_text;
	bool m_enabled;
};


void DrawMenuOption(int x, int y, const MenuOption& option, bool highlighted)
{
	if (!option.m_enabled)
		scr_setbgcolor(0xFF404040);
	else if (highlighted)
		scr_setbgcolor(0xFF000080);
	else
		scr_setbgcolor(0xFF800000);

	const char* prefix = "   ";
	if (highlighted)
		prefix = "-> ";

	scr_setXY(x, y);
	scr_printf("%s%s", prefix, option.m_text);
}


size_t ShowMenu(int x, int y, const std::vector<MenuOption>& options)
{
	// Find first enabled option.
	size_t currentOption = options.size();
	for (size_t i = 0; i < options.size(); ++i)
	{
		if (options[i].m_enabled)
		{
			currentOption = i;
			break;
		}
	}
	assert(currentOption < options.size());
	
	// Any buttons pressed at menu start are ignored until released.
	int ignoreButtons = 0;
	GetPressedButtons(ignoreButtons);

	// Draw initial menu.
	for (size_t i = 0; i < options.size(); ++i)
	{
		DrawMenuOption(x, y + static_cast<int>(i), options[i], currentOption == i);
	}
		
	// Main loop.
	for (;;)
	{
		// Wait for button presses.
		int buttons = 0;
		for (;;)
		{
			if (GetPressedButtons(buttons))
			{
				// Un-ignore any buttons that are now released.
				ignoreButtons &= buttons;
				
				// Ignore any buttons that were held at menu start.
				buttons &= ~ignoreButtons;
				
				break;
			}
		}
		
		bool changed = false;

		// Ignore up+down together.
		if ((buttons & (PAD_UP | PAD_DOWN)) == (PAD_UP | PAD_DOWN))
			buttons &= ~(PAD_UP | PAD_DOWN);

		if (buttons & PAD_UP)
		{
			for (;;)
			{
				if (currentOption > 0)
					--currentOption;
				else
					currentOption = options.size() - 1;
				if (options[currentOption].m_enabled)
					break;
			}
			ignoreButtons |= PAD_UP;
			changed = true;
		}
		else if (buttons & PAD_DOWN)
		{
			for (;;)
			{
				++currentOption;
				if (currentOption >= options.size())
					currentOption = 0;
				if (options[currentOption].m_enabled)
					break;
			}
			ignoreButtons |= PAD_DOWN;
			changed = true;
		}

		// Avoids flicker from constant non-vsync redraw.
		if (!changed && !(buttons & (PAD_CIRCLE | PAD_CROSS)))
			continue;

		// Update menu.
		for (size_t i = 0; i < options.size(); ++i)
		{
			DrawMenuOption(x, y + static_cast<int>(i), options[i], currentOption == i);
		}

		// If selected, return.
		if (buttons & (PAD_CIRCLE | PAD_CROSS))
		{
			scr_setbgcolor(0xFF800000);
			return currentOption;
		}
	}
}


bool Stage_BackupNVM()
{
	scr_setbgcolor(0xFF800000);
	scr_clear();
	
	scr_setXY(10, 5);
	scr_printf("EEPROM Backup");
	scr_setXY(10, 7);
	scr_printf("Are you sure you want to back up your EEPROM to USB?  This will");
	scr_setXY(10, 8);
	scr_printf("overwrite any previous backup on your USB stick.");
	
	std::vector<MenuOption> menuOptions;
	menuOptions.emplace_back(MenuOption{ "NO", true });
	menuOptions.emplace_back(MenuOption{ "YES", true });
	
	if (ShowMenu(10, 11, menuOptions) != 1)
		return false;
	
	scr_setbgcolor(0xFF800000);
	scr_clear();
	scr_setXY(10, 5);
	scr_printf("Backing up EEPROM to USB stick...\n");
	
	bool success = false;
	std::FILE* file = std::fopen("mass:/mechadump_eeprom_backup.bin", "wb");
	if (file)
	{
		success = std::fwrite(g_eeprom, sizeof(g_eeprom), 1, file) == 1;
		std::fclose(file);
	}
	
	if (!success)
	{
		scr_setXY(10, 7);
		scr_setbgcolor(0xFF000080);
		scr_printf("BACKUP FAILED ");
		scr_setbgcolor(0xFF800000);
		scr_printf("- no USB stick inserted?\n");
		scr_setXY(10, 8);
		scr_printf("Press X or O to continue.\n");
		WaitForXorOConfirm();
		return false;
	}
	
	scr_setXY(10, 7);
	scr_printf("Backup successful.\n");
	scr_setXY(10, 8);
	scr_printf("Press X or O to continue.\n");
	WaitForXorOConfirm();
	return true;
}


// Does not return if successful.
bool Stage_RestoreNVM()
{
	SimpleDebugOutput debug;
	
	if (!CheckAndWaitForBackdoor())
		return false;
	
	scr_setbgcolor(0xFF800000);
	scr_clear();
	
	scr_setXY(10, 5);
	scr_printf("EEPROM Restore");
	scr_setXY(10, 7);
	scr_printf("Are you sure you want to restore your EEPROM from USB?");
	
	std::vector<MenuOption> menuOptions;
	menuOptions.emplace_back(MenuOption{ "NO", true });
	menuOptions.emplace_back(MenuOption{ "YES", true });
	
	if (ShowMenu(10, 11, menuOptions) != 1)
		return false;
	
	scr_setXY(10, 5);
	scr_printf("Restoring EEPROM patch data from USB stick...\n");
	
	uint8_t eepromConfigToRestore[0x200];
	bool success = false;
	std::FILE* file = std::fopen("mass:/mechadump_eeprom_backup.bin", "rb");
	if (file)
	{
		std::fseek(file, 0x200, SEEK_SET);
		success = std::fread(eepromConfigToRestore, 0x200, 1, file) == 1;
		std::fclose(file);
	}
	
	scr_setbgcolor(0xFF800000);
	scr_clear();
	scr_setXY(10, 5);
	scr_printf("Restoring EEPROM from backup...\n");
	
	if (!success)
	{
		scr_setXY(10, 7);
		scr_setbgcolor(0xFF000080);
		scr_printf("RESTORE FAILED ");
		scr_setbgcolor(0xFF800000);
		scr_printf("- no USB stick inserted?\n");
		scr_setXY(10, 8);
		scr_printf("Press X or O to continue.\n");
		WaitForXorOConfirm();
		return false;
	}

	if (!RestoreNVMConfigAndPatchData(payload_writenvm, size_payload_writenvm, eepromConfigToRestore,
		debug))
	{
		scr_setXY(10, 7);
		scr_setbgcolor(0xFF000080);
		scr_printf("RESTORE FAILED ");
		scr_setbgcolor(0xFF800000);
		scr_printf("THIS IS REALLY BAD.\n");
		scr_setXY(10, 8);
		scr_printf("Press X or O to continue.\n");
		WaitForXorOConfirm();
		return false;
	}
	
	scr_setXY(10, 7);
	scr_printf("Restore successful.\n");
	scr_setXY(10, 8);
	scr_printf("Press X or O to power off system.\n");
	WaitForXorOConfirm();

	ResetMechaconAndPowerOff();
	scr_setbgcolor(0xFF800000);
	scr_clear();
	scr_setXY(10, 7);
	scr_setbgcolor(0xFF000080);
	scr_printf("Reboot failed ");
	scr_setbgcolor(0xFF800000);
	scr_printf("Unplug your system, wait for the red light to turn off, then plug in.\n");
	for (;;)
		sleep(1);
}


// Does not return if successful.  Returns on failure.
void Stage_InstallHackChosen(NvmState which)
{
	const unsigned char* patch;
	switch (which)
	{
		case NVM_STATE_IRQ_HOOK:
			patch = patch_irq_hook;
			break;
		case NVM_STATE_CDPROTECT_HOOK:
			patch = patch_cdprotect_hook;
			break;
		default:
			assert(false);
			ExecOSD(0, nullptr);
			for (;;)
				sleep(1);
	}
	
	scr_setbgcolor(0xFF800000);
	scr_clear();
	
	scr_setXY(10, 5);
	scr_printf("Installing back door...\n");
	
	int errorCode = -1;
	bool result = WriteConfigExploit(patch, errorCode);

	if (result)
	{
		scr_setXY(10, 8);
		scr_printf("SUCCESS!  Back door installed.\n");
		scr_setXY(10, 10);
		scr_printf("Your system is now frozen.\n");
		scr_setXY(10, 11);
		if (g_mechaconMajor < 6)
			scr_printf("Please UNPLUG or TURN POWER SUPPLY SWITCH OFF,\n");
		else
			scr_printf("Please UNPLUG your PS2, then\n");
		scr_setXY(10, 12);
		scr_printf("wait for red light to go out, then power on\n");
		scr_setXY(10, 13);
		scr_printf("and run this program again.\n");
		for (;;)
			sleep(1);
	}
	
	scr_setbgcolor(0xFF000080);
	scr_setXY(10, 8);
	scr_printf("FAILED TO INSTALL!  ");
	scr_setXY(10, 9);
	scr_printf("Error code: %d  ", errorCode);
	scr_setXY(10, 11);
	scr_printf("Press X or O to return to the menu.");
	WaitForXorOConfirm();
}


// Does not return if successful.  Returns on failure.
void Stage_InstallHack()
{
	scr_setbgcolor(0xFF800000);
	scr_clear();
	
	scr_setXY(10, 5);
	scr_printf("Install Backdoor");
	scr_setXY(10, 7);
	scr_printf("Which backdoor payload do you want to use?");
	scr_setXY(10, 8);

	std::vector<MenuOption> menuOptions;
	menuOptions.emplace_back(MenuOption{ "Cancel", true });
	menuOptions.emplace_back(MenuOption{ "CD Protect Hook (safer, but requires a legitimate game disc)",
		true });
	menuOptions.emplace_back(MenuOption{ "IRQ Hook (riskier, but doesn't need drive at all)", true });

	size_t choice = ShowMenu(10, 11, menuOptions);
	NvmState whichHack = NVM_STATE_EMPTY;
	switch (choice)
	{
		case 0:
			return;
		case 1:
			whichHack = NVM_STATE_CDPROTECT_HOOK;
			break;
		case 2:
			whichHack = NVM_STATE_IRQ_HOOK;
			break;
		default:
			assert(false);
			return;
	}

	scr_setbgcolor(0xFF800000);
	scr_clear();
	
	scr_setXY(10, 5);
	scr_printf("Install Backdoor");
	scr_setXY(10, 7);
	scr_printf("Are you sure you want to install the backdoor?");
	scr_setXY(10, 8);
	scr_setbgcolor(0xFF000080);
	scr_printf("IT MIGHT BRICK YOUR SYSTEM!");
	scr_setbgcolor(0xFF800000);
	scr_printf("  (Recoverable with soldering.)\n");

	menuOptions.clear();
	menuOptions.emplace_back(MenuOption{ "NO", true });
	menuOptions.emplace_back(MenuOption{ "YES", true });

	if (ShowMenu(10, 11, menuOptions) != 1)
		return;

	Stage_InstallHackChosen(whichHack);
}


bool Stage_DumpMechaconROM()
{
	SimpleDebugOutput debug;
	
	scr_setbgcolor(0xFF800000);
	scr_clear();
	
	scr_setXY(10, 5);
	scr_printf("Dump Mechacon ROM");
	scr_setXY(10, 7);
	scr_printf("Are you sure you want to dump your Mechacon ROM to USB?  This will");
	scr_setXY(10, 8);
	scr_printf("overwrite any previous Mechacon ROM dump on your USB stick.");

	std::vector<MenuOption> menuOptions;
	menuOptions.emplace_back(MenuOption{ "NO", true });
	menuOptions.emplace_back(MenuOption{ "YES", true });
	
	if (ShowMenu(10, 11, menuOptions) != 1)
		return false;
	
	if (!IsBackDoorFunctioning())
	{
		if (!CheckAndWaitForBackdoor())
			return false;
	}
	
	scr_setbgcolor(0xFF800000);
	scr_clear();
	scr_setXY(0, 5);
	scr_printf("          Dumping Mechacon...  (this can take 15 seconds to start)\n");
	scr_printf("\n");
	scr_printf("          |                                                  |\n");
	scr_printf("\n");
	scr_printf("          Hold triangle to cancel.\n");

	// Dump keystore.  This will take 15 seconds if we don't know where the payload gets
	// uploaded to.  However, once done, we'll remember, so the firmware dump won't wait.
	std::vector<uint8_t> keystore = DumpMechaconKeystoreWithPayload(payload_keystoredump,
		size_payload_keystoredump, debug);
	bool cancelled = false;
	{
		int buttons = 0;
		if (GetPressedButtons(buttons) && (buttons & PAD_TRIANGLE))
		{
			cancelled = true;
			return false;
		}
	}
	
	// Dump main firmware.
	std::vector<uint8_t> rom = DumpMechaconROMFastWithPayload(payload_fastdump, size_payload_fastdump,
		[&](size_t current, size_t total) -> bool
		{
			int buttons = 0;
			if (GetPressedButtons(buttons) && (buttons & PAD_TRIANGLE))
			{
				cancelled = true;
				return false;
			}

			float percentage = 100.0f * static_cast<float>(current) / static_cast<float>(total);
			int chars = static_cast<int>(percentage) / 2;
			if (chars < 0)
				chars = 0;
			else if (chars > 50)
				chars = 50;
			// scr_print will print an extra space, so subtract 1.
			if (chars >= 1)
			{
				char spaces[51];
				std::memset(spaces, ' ', 50);
				spaces[chars - 1] = '\0';
				
				scr_setXY(10 + 1, 7);
				scr_setbgcolor(0xFF000080);
				scr_printf("%s", spaces);
				scr_setbgcolor(0xFF800000);
			}
			return true;
		}, debug);

	if (cancelled)
		return false;
	
	scr_setXY(0, 9);
	scr_printf("          Saving...                        \n");

	bool success = false;
	std::string dumpPath;
	if (!keystore.empty() && !rom.empty())
	{
		bool successKeystore = false;
		std::FILE* keystoreFile = std::fopen("mass:/mechacon-keystore.bin", "wb");
		if (keystoreFile)
		{
			if (std::fwrite(keystore.data(), 1, keystore.size(), keystoreFile) == keystore.size())
				successKeystore = true;
			std::fclose(keystoreFile);
		}
		
		bool successROM = false;
		dumpPath = std::string("mass:/") + MakeDumpFilename();
		std::FILE* romFile = std::fopen(dumpPath.c_str(), "wb");
		if (romFile)
		{
			if (std::fwrite(rom.data(), 1, rom.size(), romFile) == rom.size())
				successROM = true;
			std::fclose(romFile);
		}
		
		success = successKeystore && successROM;
	}

	if (success)
		scr_printf("          Success!  File dumped as %s\n", dumpPath.c_str());
	else
		scr_printf("          FAILED!  Could not write file to USB\n");

	// Is this a known version?
	sha256::digest digest;
	sha256 sha256;
	sha256.reset();
	sha256.process(rom.data(), rom.size());
	sha256.finish(digest);
	debug.Printf("Firmware digest: ");
	for (size_t i = 0; i < sizeof(digest); ++i)
	{
		debug.Printf("%02x", digest[i]);
	}
	debug.Printf("\n");
	
	if (!IsKnownDump(digest))
	{
		scr_printf("          ");
		scr_setbgcolor(0xFF000080);
		scr_printf("Unknown Version!!");
		scr_setbgcolor(0xFF800000);
		scr_printf("  Please send it.\n");
		scr_printf("\n");
	}
	
	scr_printf("          Press X or O to continue.\n");
	WaitForXorOConfirm();
	return success;
}


[[noreturn]] void MainMenu()
{
	for (;;)
	{
		NvmState nvmState = GetNVMState();

		bool allowNVMBackup = false;
		bool allowNVMRestore = false;
		bool allowInstallHack = false;
		bool allowDumpMechacon = false;

		bool backdoorInstalled = false;
		bool backdoorFunctioning = IsBackDoorFunctioning();
		
		const char* nvmStateString;
		switch (nvmState)
		{
			case NVM_STATE_CDPROTECT_HOOK:
				nvmStateString = "Backdoored (CD Protect)";
				allowNVMBackup = false;
				backdoorInstalled = true;
				break;
			case NVM_STATE_IRQ_HOOK:
				nvmStateString = "Backdoored (IRQ Handler)";
				allowNVMBackup = false;
				backdoorInstalled = true;
				break;
			case NVM_STATE_EMPTY:
				nvmStateString = "Empty";
				allowNVMBackup = true;
				backdoorInstalled = false;
				break;
			case NVM_STATE_OTHER:
			default:
				nvmStateString = "Default";
				allowNVMBackup = true;
				backdoorInstalled = false;
				break;
		}

		allowInstallHack = true;
		allowNVMRestore = backdoorFunctioning || backdoorInstalled;
		allowDumpMechacon = backdoorFunctioning || backdoorInstalled;

		std::FILE* backup = std::fopen("mass:/mechadump_eeprom_backup.bin", "rb");
		if (backup)
			std::fclose(backup);
		else
			allowNVMRestore = false;

		scr_setbgcolor(0xFF800000);
		scr_clear();

		scr_setXY(10, 5);
		scr_printf("Patch state: %s", nvmStateString);
		scr_setXY(39, 10);
		scr_setbgcolor(0xFF800080);
		scr_printf(" MAIN MENU");

		std::vector<MenuOption> menuOptions;
		menuOptions.emplace_back(MenuOption{ "Back Up EEPROM", allowNVMBackup });
		menuOptions.emplace_back(MenuOption{ "Restore EEPROM", allowNVMRestore });
		menuOptions.emplace_back(MenuOption{ "Install Backdoor", allowInstallHack });
		menuOptions.emplace_back(MenuOption{ "Dump Mechacon ROM", allowDumpMechacon });
		menuOptions.emplace_back(MenuOption{ "Exit", true });

		size_t choice = ShowMenu(10, 13, menuOptions);

		switch (choice)
		{
			case 0:
				if (allowNVMBackup)
					Stage_BackupNVM();
				break;
			case 1:
				if (allowNVMRestore)
					Stage_RestoreNVM();
				break;
			case 2:
				if (allowInstallHack)
					Stage_InstallHack();
				break;
			case 3:
				if (allowDumpMechacon)
					Stage_DumpMechaconROM();
				break;
			case 4:
				ExecOSD(0, nullptr);
				for (;;)
					sleep(1);
			default:
				break;
		}
	}
}


int main(int argc, char** argv)
{
	assert(size_patch_irq_hook == 0xE0);
	assert(size_patch_cdprotect_hook == 0xE0);
	
	init_scr();
	HackToHideCursor();
	scr_setXY(0, 5);
	scr_printf("          Loading...\n");
	
	ResetEverything(!argv[0]);
	scr_printf("          Past ResetEverything\n");

	mcInit(MC_TYPE_XMC);
	scr_printf("          Past mcInit\n");
	mtapInit();
	scr_printf("          Past mtapInit\n");
	padInit(0);
	scr_printf("          Past padInit\n");
	mtapPortOpen(0);
	scr_printf("          Past mtapPortOpen\n");
	padPortOpen(0, 0, g_pad0Buffer);
	scr_printf("          Past padPortOpen\n");
	sceCdInit(SCECdINoD);
	scr_printf("          Past sceCdInit\n");

	WarningScreen();
	
	if (!SysinfoScreen())
	{
		SleepThread();
		return 0;
	}

	MainMenu();
}
