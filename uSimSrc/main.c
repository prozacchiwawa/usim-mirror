/* uSim main.c
 * Copyright (C) 2000, Tsurishaddai Williamson, tsuri@earthlink.net
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**********************************************************************/

#include "ustdio.h"
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include "file.h"

#include "memory.h"
#include "system.h"
#include "cpu.h"
#include "monitor.h"
#include "bdev.h"
#include "cdev.h"

/**********************************************************************/
#pragma mark *** MAIN ***

#define kMaxRom (64 * 1024)

#define kMaxRam (64 * 1024)

const char *gFOpenPath[] = {
	"",
	":ASM:",
	"ASM/",
	"ASM\\",
	":CPM:",
	"CPM/",
	"CPM\\",
	":src:",
	"src/",
	"src\\",
	":uSimSrc:",
	"uSimSrc/",
	"uSimSrc\\",
	0
};

static const char *gWindowTitle = CPU " " kProgram " " kVersion;

static const char *gAboutTitle = "About " kProgram "...";

static const char *gAboutText = 
	kProgram " " kVersion " -- " CPU " microprocessor simulator\n"
	"\n"
	kCopyright "\n"
	"\n"
	kProgram " is licensed via the GNU GENERAL PUBLIC LICENSE\n"
    kProgram " comes with ABSOLUTELY NO WARRANTY.\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under certain conditions; see license.txt for details.\n"
	"\n"
	"To contact the author:\n"
	"  Tsurishaddai Williamson\n"
	"  tsuri@earthlink.net\n"
	"\n"
	"Note: Use <ESCAPE><ESCAPE> to enter the monitor.\n"
	"      Use <ESCAPE><ESCAPE><ESCAPE> to quit.";

int main(void)
{
	Byte *rom = 0;
	Byte *ram = 0;
	Byte *bitBucket = 0;

	/* Open the console. */
	if (!ConsoleOpen(gWindowTitle, gAboutTitle, gAboutText))
		goto error;

	/* Prepare the memory system. */
	if ((rom = malloc(kMaxRom)) == 0) {
		SystemMessage("?MALLOC [rom]\n");
		goto error;
	}
	if ((ram = malloc(kMaxRam)) == 0) {
		SystemMessage("?MALLOC [ram]\n");
		goto error;
	}
	if ((bitBucket = malloc(kBankSize)) == 0) {
		SystemMessage("?MALLOC [bitBucket]\n");
		goto error;
	}
	if (!MemoryOpen(rom, kMaxRom, ram, kMaxRam, bitBucket)) {
		SystemMessage("?MEMORY\n");
		goto error;
	}

	/* Prepare the I/O ports. */
	SetupSystemPorts();

	/* Issue the BOOT command. */
	MonitorCommand("BOOT");

	/* Run the CPU until halted. */
	Cpu();

	/* All done, deallocate everything and exit. */
error:
	BDevUnmount(0);
	CDevDetach(0);
	MonitorClose();
	MemoryClose();
	if (bitBucket != 0)
		free(bitBucket);
	if (ram != 0)
		free(ram);
	if (rom != 0)
		free(rom);
	ConsoleClose();

	return 0;

}
