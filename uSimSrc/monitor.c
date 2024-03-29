/* uSim monitor.c
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

#undef GENERATE_ROM
#undef CALCULATE_ROM
#define kRomFile "ROM.H"
#undef CPM_IN_ROM

/* To generate the rom file:
 * 1) #define GENERATE_ROM
 * 2) Execute main().
 * 3) Move the rom file to the source directory.
 * 4) #undef GENERATE_ROM
 */

#ifdef Z80
#ifdef CPM_IN_ROM
#define ROM_SOURCE "BOOT64.Z80 CCP64.ASM BDOS64.ASM BIOS64.Z80"
#else
#define ROM_SOURCE "BOOT64.Z80"
#endif
#else
#ifdef CPM_IN_ROM
#define ROM_SOURCE "BOOT64.ASM CCP64.ASM BDOS64.ASM BIOS64.ASM"
#else
#define ROM_SOURCE "BOOT64.ASM"
#endif
#endif

#ifdef GENERATE_ROM
#define CALCULATE_ROM
#endif

#include "ustdio.h"
#include "digits.h"
#include "string.h"
#include "file.h"
#include <ctype.h>
#include <stdarg.h>

#include "memory.h"
#include "system.h"
#include "cpu.h"
#include "dasm.h"
#include "asm.h"
#include "ddt.h"
#include "monitor.h"
#include "bdev.h"
#include "cdev.h"
#include "clock.h"

/**********************************************************************/
#pragma mark *** SYSTEM ROM ***

#ifndef CALCULATE_ROM

#pragma mark gRomCode
static const Byte gRomCode[] = {
#include kRomFile
};

#endif

/**********************************************************************/
#pragma mark *** SYSTEM MESSAGE ***

int gMonitorActive;

/* SystemMessage() displays system messages. */
int SystemMessage(const char *format, ...)
{
	va_list ap;
	int result;

	/* If not already in the system monitor... */
	if (!gMonitorActive)
		/* Clean the line and use the system text color. */
		printf(kConsoleCleanLine kConsoleColorSystem);

	/* Do the printf(). */
	va_start(ap, format);
	result = vprintf(format, ap);
	va_end(ap);

	/* If not already in the system monitor... */
	if (!gMonitorActive)
		/* Restore the text color. */
		printf(kConsoleColorReset);

	/* All done, return the printf() result. */
	return result;

}

/**********************************************************************/
#pragma mark *** MONITOR STATE ***

static CpuStatePtr gCpuStatePtr;

static int gPageCount;

static Word gAddress;

static unsigned gTraceCount;
static unsigned gTraceDisplay;
#define kMaxHistory 8
static unsigned gHistoryEnable;
static CpuState gHistory[kMaxHistory];

#define kMaxBreak 4
static unsigned gMaxBreak;
static Word gBreakAddress[kMaxBreak];

#define kMaxTBreak 2
static unsigned gMaxTBreak;
static Word gTBreakAddress[kMaxTBreak];

static void MonitorHelp(char *command);

static FILE *gTraceFile;
static char gTraceFileName[256];

/* CloseTraceFile() closes the trace file. */
static void CloseTraceFile(void)
{

	if (gTraceFile != 0)  {
		fclose(gTraceFile);
		gTraceFile = 0;
		printf("CLOSED [%s]\n", gTraceFileName);
	}

}

/* OpenTraceFile() opens the trace file. */
static int OpenTraceFile(char *traceFileName)
{

	CloseTraceFile();

	strcpy(gTraceFileName, traceFileName);

	if (gTraceFileName[0] == '+') {
		strcpy(gTraceFileName, &gTraceFileName[1]);
		gTraceFile = fopen(gTraceFileName, "a");
		if (gTraceFile == 0) {
			printf("?ERROR\n");
			goto error;
		}
	}
	else if (gTraceFileName[0] == '-') {
		strcpy(gTraceFileName, &gTraceFileName[1]);
		gTraceFile = fopen(gTraceFileName, "w");
		if (gTraceFile == 0) {
			printf("?ERROR\n");
			goto error;
		}
	}
	else if ((gTraceFile = fopen(gTraceFileName, "r")) != 0) {
		fclose(gTraceFile);
		gTraceFile = 0;
		printf("?EXISTS\n");
	}
	else {
		gTraceFile = fopen(gTraceFileName, "w");
		if (gTraceFile == 0) {
			printf("?ERROR\n");
			goto error;
		}
	}

	if (gTraceFile != 0)
		printf("OPENED [%s]\n", gTraceFileName);

	return 1;

error:
	return 0;

}

static FILE *gCommandFile;

/* CloseCommandFile() closes the command file. */
static void CloseCommandFile(void)
{

	if (gCommandFile != 0) {
		fclose(gCommandFile);
		gCommandFile = 0;
	}

}

/* OpenCommandFile() opens a command file. */
static int OpenCommandFile(const char *file)
{

	CloseCommandFile();

	if ((gCommandFile = FOpenPath(file, "r")) == 0)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** MONITOR COMMANDS ***

/* ABOUT() implements the ABOUT command. */
static int ABOUT(int argc, char **argv)
{
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Display the About Text. */
	printf(kConsoleAbout);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* ASSEMBLE() implements the ASSEMBLE command. */
static int ASSEMBLE(int argc, char **argv)
{
	char sourceFileBuffer[kMaxFileName];
	const char *sourceFile = 0;
	char objectFileBuffer[kMaxFileName];
	const char *objectFile = 0;
	char listFileBuffer[kMaxFileName];
	const char *listFile = 0;
	char *biasStr = 0;
	Word bias = 0;
	int generateListFile = 0;
	int stripZeros = 0;
	int debug = 0;
	int kompile = 0;
	int pageCount;
	int i;

	/* Parse command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'B':
			if (biasStr != 0)
				goto usage;
			if (strlen(token) > 2)
				token = &token[2];
			else if (++i >= argc)
				goto usage;
			else
				token = argv[i];
			biasStr = token;
			if (!StringToShort(biasStr, (short *)&bias))
				goto usage;
			break;
		case 'D':
			if (debug)
				goto usage;
			debug++;
			break;
		case 'K':
			if (kompile)
				goto usage;
			kompile++;
			break;
		case 'L':
			if (generateListFile)
				goto usage;
			generateListFile++;
			break;
		case 'S':
			if (stripZeros)
				goto usage;
			stripZeros++;
			break;
		default:
			goto usage;
		}
	}
	/* <CODEFILE> is required. */
	if (i >= argc)
		goto usage;
	sourceFile = argv[i++];
	/* <HEXFILE> is optional. */
	if (i < argc)
		objectFile = argv[i++];
	/* No more arguments allowed. */
	if (i < argc)
		goto usage;

	/* The default 8080 source file extension is ".ASM". */
	/* The default Z80 source file extension is ".Z80". */
	if (strlen(FileExtension(sourceFile)) != 0)
		sprintf(sourceFileBuffer,
		        "%s.%s",
		        FileName(sourceFile),
		        FileExtension(sourceFile));
	else {
#ifdef Z80
		sprintf(sourceFileBuffer, "%s.Z80", FileName(sourceFile));
#else
		sprintf(sourceFileBuffer, "%s.ASM", FileName(sourceFile));
#endif
	}
	sourceFile = sourceFileBuffer;

	/* The default object file extension is ".HEX". */
	if (objectFile == 0) {
		if (strcmp(FileExtension(sourceFile), "HEX")) {
			sprintf(objectFileBuffer,
			        "%s.HEX",
			        FileName(sourceFile));
			objectFile = objectFileBuffer;
		}
	}

	/* The default list file extension is ".LST". */
	if (generateListFile) {
		sprintf(listFileBuffer, "%s.LST", FileName(sourceFile));
		listFile = listFileBuffer;
	}

	/* Do the assembly. */
	pageCount = Assemble(0,
	                     0,
	                     0,
	                     sourceFile,
	                     listFile,
	                     kompile ? 0 : objectFile,
	                     0,
	                     0,
	                     0,
	                     bias,
	                     0,
	                     debug);
	if (pageCount == 0)
		goto error;
	printf("%d PAGES\n", pageCount);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* ATTACH() implements the ATTACH command. */
static int ATTACH(int argc, char **argv)
{
	char _cDev[5];
	char *cDev = 0;
	char *name = 0;
	char *result;
	int i;

	/* Parse command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* <CDEV> is optional. */
	if (i < argc)
		cDev = argv[i++];
	/* <NAME> is optional. */
	if (i < argc)
		name = argv[i++];
	/* No more arguments allowed. */
	if (i < argc)
		goto usage;
	/* <CDEV> and <NAME> might not be seperated by a space. */
	if ((cDev != 0) && (name == 0)) {
		if ((cDev[3] == ':') && (cDev[4] != 0)) {
			strncpy(_cDev, cDev, 4);
			_cDev[4] = 0;
			name = &cDev[4];
			cDev = _cDev;
		}
	}

	/* Show attach state for all cDev's. */
	if (cDev == 0)
		ShowCDevAttach(0);
	/* Validate the cDev. */
	else if ((result = CDevAttach(cDev, 0)) == 0) {
		printf("?NODEV [%s]\n", cDev);
		goto error;
	}
	/* Show attach state for this cDev. */
	else if (name == 0)
		ShowCDevAttach(cDev);
	/* Attach this cDev. */
	else if ((result = CDevAttach(cDev, name)) == 0) {
		printf("?ATTACH [%s]\n", name);
		goto error;
	}
	/* Show attach state for this cDev. */
	else
		ShowCDevAttach(cDev);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* BOOT() implements the BOOT command. */
static int BOOT(int argc, char **argv)
{
	int i;
	FILE *f;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Execute monitor commands from the kBOOTBAT file. */
	if ((f = FOpenPath(kBOOTBAT, "r")) != 0) {
		fclose(f);
		if (!MonitorCommand(kBOOTBAT))
			goto error;
	}
	/* Otherwise: RESET, LOAD and GO */
	else {
		MonitorCommand("ABOUT");
		printf("?%s", kBOOTBAT);
		MonitorCommand("RESET");
		MonitorCommand("LOAD");
		MonitorCommand("GO");
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;


}

/* BREAK() implements the BREAK command. */
static int BREAK(int argc, char **argv)
{
	char *addressStr = 0;
	int removeBreak = 0;
	Word address;
	int n;
	int i;

	/* Display all breakpoints. */
	if (argc == 1) {
		if (gMaxBreak == 0)
			printf("?NOBREAK\n");
		else for (i = 0; i < gMaxBreak; i++)
			printf("%04X\n", gBreakAddress[i] & 0xFFFF);
	}
	/* Clear all breakpoints. */
	else if ((argc == 2) && !strcmp(argv[1], ".")) {
		for (i = 0; i < gMaxBreak; i++)
			printf("-%04X\n", gBreakAddress[i] & 0xFFFF);
		gMaxBreak = 0;
	}
	/* Add or remove breakpoints. */
	else {
		/* Verify all of the address strings. */
		for (n = 1; n < argc; n++) {
			addressStr = argv[n];
			if ((*addressStr == '-') || (*addressStr == '+'))
				*addressStr++;
			if (!StringToShort(addressStr, (short *)&address))
				goto usage;
		}
		/* Process the addresses. */
		for (n = 1; n < argc; n++) {
			addressStr = argv[n];
			/* Add or remove? */
			removeBreak = 0;
			if (*addressStr == '-') {
				removeBreak = 1;
				addressStr++;
			}
			else if (*addressStr == '+')
				*addressStr++;
			if (!StringToShort(addressStr, (short *)&address))
				goto usage;
			/* See if the address is already in the list... */
			for (i = 0; i < gMaxBreak; i++)
				if (gBreakAddress[i] == address)
					break;
			/* The address is not already in the list. */
			if (i == gMaxBreak) {
				if (removeBreak)
					/* Remove: already not there. */
					printf("-%04X\n", address & 0xFFFF);
				else {
					/* Add: add at the end of the list. */
					if (gMaxBreak == kMaxBreak) {
						printf("?NOSPACE\n");
						goto error;
					}
					else {
						gBreakAddress[gMaxBreak++] = address;
						printf("+%04X\n", address & 0xFFFF);
					}
				}
			}
			/* The address is already in the list. */
			else {
				if (removeBreak) {
					/* Remove: remove from the list. */
					gMaxBreak--;
					while (i++ < gMaxBreak)
						gBreakAddress[i - 1] = gBreakAddress[i];
					printf("-%04X\n", address & 0xFFFF);
				}
				else
					/* Add: already in the list. */
					printf("+%04X\n", address & 0xFFFF);
			}
		}
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* CLOCK() implements the CLOCK command. */
static int CLOCK(int argc, char **argv)
{
	char *timeValueStr = 0;
	long timeValue;
	TimeOfDay timeOfDay;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* <NEW VALUE> is optional. */
	if (i < argc) {
		timeValueStr = argv[i++];
		if (!StringToLong(timeValueStr, &timeValue))
			goto usage;
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Reset if a value was specified. */
	if (timeValueStr != 0)
		ResetClock(timeValue);
 
 	/* Show the current time of day. */
	GetTimeOfDay(&timeOfDay);
	printf("CLOCK=%ld, DAYS=%d, HOURS=%d, MINUTES=%d, SECONDS=%d\n",
	       GetClock(),
	       timeOfDay.days,
	       timeOfDay.hours,
	       timeOfDay.minutes,
	       timeOfDay.seconds);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* COPY() implements the COPY command. */
static int COPY(int argc, char **argv)
{
	char _bDev[3];
	char *bDev = 0;
	BDevPtr bDevPtr;
	int readOnly = 0;
	char *file = 0;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'R':
			if (readOnly)
				goto usage;
			readOnly = 1;
			break;
		default:
			goto usage;
		}
	}
	/* <DISK> is required. */
	if (i >= argc)
		goto usage;
	bDev = argv[i++];
	/* <DISK> and <FILE> might not be seperated by a space. */
	if (i < argc)
		file = argv[i++];
	else if ((bDev[1] == ':') && (bDev[2] != 0)) {
		strncpy(_bDev, bDev, 2);
		_bDev[2] = 0;
		file = &bDev[2];
		bDev = _bDev;
	}
	/* <FILENAME> is required. */
	if (file == 0)
		goto usage;
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Copy this bDev. */
	if ((bDevPtr = BDevMount(bDev, 0, 0)) == 0) {
		printf("?NODISK [%s]\n", bDev);
		goto error;
	}
	if (BDevStatus(bDevPtr, 0) != kBDevStatusClosed) {
		if (readOnly) {
			if (!AsciiDiskCopy(bDevPtr, file))
				goto error;
		}
		else {
			if (!FileDiskCopy(bDevPtr, file))
				goto error;
		}
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* DETACH() implements the DETACH command. */
static int DETACH(int argc, char **argv)
{
	char *cDev = 0;
	char *result;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* <PHYSDEV> is required. */
	if (i >= argc)
		goto usage;
	cDev = argv[i++];
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Detach this cDev. */
	if ((result = CDevAttach(cDev, 0)) == 0) {
		printf("?NODEV [%s]\n", cDev);
		goto error;
	}
	if (strlen(result) != 0)
		CDevDetach(cDev);

	/* Confirm that this cDev has been detached. */
	ShowCDevAttach(cDev);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* DISPLAY() implements the DISPLAY command. */
static int DISPLAY(int argc, char **argv)
{
	Byte (*rdByte)(Word address) = RdByte;
	char *addressStr = 0;
	Word address = gAddress;
	char *endAddressStr = 0;
	Word endAddress;
	int i;
	int j;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'R':
			if (rdByte == RdRom)
				goto usage;
			rdByte = RdRom;
			break;
		default:
			goto usage;
		}
	}
	/* <ADDRESS> is optional. */
	if (i < argc) {
		addressStr = argv[i++];
		if (!StringToShort(addressStr, (short *)&address))
			goto usage;
	}
	/* <ENDADDRESS> is optional. */
	if (i < argc) {
		endAddressStr = argv[i++];
		if (!StringToShort(endAddressStr, (short *)&endAddress))
			goto usage;
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Display 256 bytes of memory if <ENDADDRESS> not specified. */
	if (endAddressStr == 0)
		endAddress = address + 256 - 1;

	/* Display memory from here to there. */
	for (;;) {
		if (ConsoleInput(0))
			break;
		printf("%04X", address & 0xFFFF);
		for (j = 0; j < 16; j++) {
			if ((address + j) <= endAddress)
				printf(" %02X", rdByte(address + j) & 0xFF);
			else
				printf("   ");
		}
		printf("  ");
		for (j = 0; j < 16; j++) {
			char c = rdByte(address + j);
			if ((address + j) <= endAddress)
				printf("%c", ((c >= ' ') && (c <= '~')) ? c : '.');
			if ((address + j) == endAddress)
				break;
			else if (j >= 15)
				break;
		}
		printf("\n");
		address += j;
		if (address++ == endAddress)
			break;
	}

	/* Remember the last address. */
	gAddress = address;

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* DDTHELP() helps with DDT command syntax. */
static int DDTHELP(int argc, char **argv)
{
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	MonitorHelp(argv[0]);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* EXAMINE() implements the EXAMINE command. */
static int EXAMINE(int argc, char **argv)
{
	char buffer[256];
	Word *wordPtr = 0;
	Byte *bytePtr = 0;
	Byte *flagPtr = 0;
	Byte flagBit = 0;
	char *name = 0;
	int i;

	i = 1;
	/* <REGISTER> is optional. */
	if (i < argc)
		name = argv[i++];
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Display the Cpu State if no register name is specified. */
	if (name == 0) {
		for (i = gHistoryEnable - 1; i > 0; i--) {
			DissasembleCpuState(buffer, &gHistory[i - 1]);
			puts(buffer);
		}
		DissasembleCpuState(buffer, gCpuStatePtr);
		puts(buffer);
	}
	/* Enable trace history if "+". */
	else if (!strcmp(name, "+")) {
		for (i = 0; i < kMaxHistory; i++)
			MemoryZero(&gHistory[i], sizeof(CpuState));
		gHistoryEnable = 1;
	}
	/* Disable trace history if "-". */
	else if (!strcmp(name, "-"))
		gHistoryEnable = 0;
	/* Otherwise set a value to the specified register or flag. */
	else {
		if (!strcmp(name, "C")) {
			flagPtr = &gCpuStatePtr->af.byte.low;
			flagBit = CARRY;
		}
		if (!strcmp(name, "Z")) {
			flagPtr = &gCpuStatePtr->af.byte.low;
			flagBit = ZERO;
		}
		if (!strcmp(name, "M")) {
			flagPtr = &gCpuStatePtr->af.byte.low;
			flagBit = SIGN;
		}
		if (!strcmp(name, "E")) {
			flagPtr = &gCpuStatePtr->af.byte.low;
			flagBit = PARITY;
		}
		if (!strcmp(name, "I")) {
			flagPtr = &gCpuStatePtr->af.byte.low;
			flagBit = HALFCARRY;
		}
		if (!strcmp(name, "N")) {
			flagPtr = &gCpuStatePtr->af.byte.low;
			flagBit = SUBTRACT;
		}
		if (!strcmp(name, "A"))
			bytePtr = &gCpuStatePtr->af.byte.high;
		if (!strcmp(name, "B"))
			wordPtr = &gCpuStatePtr->bc.word;
		if (!strcmp(name, "D"))
			wordPtr = &gCpuStatePtr->de.word;
		if (!strcmp(name, "H"))
			wordPtr = &gCpuStatePtr->hl.word;
		if (!strcmp(name, "S"))
			wordPtr = &gCpuStatePtr->sp.word;
		if (!strcmp(name, "P"))
			wordPtr = &gCpuStatePtr->pc.word;
#ifdef Z80
		if (!strcmp(name, "X"))
			wordPtr = &gCpuStatePtr->ix.word;
		if (!strcmp(name, "Y"))
			wordPtr = &gCpuStatePtr->iy.word;
		if (!strcmp(name, "CC")) {
			flagPtr = &gCpuStatePtr->prime.af.byte.low;
			flagBit = CARRY;
		}
		if (!strcmp(name, "ZZ")) {
			flagPtr = &gCpuStatePtr->prime.af.byte.low;
			flagBit = ZERO;
		}
		if (!strcmp(name, "MM")) {
			flagPtr = &gCpuStatePtr->prime.af.byte.low;
			flagBit = SIGN;
		}
		if (!strcmp(name, "EE")) {
			flagPtr = &gCpuStatePtr->prime.af.byte.low;
			flagBit = PARITY;
		}
		if (!strcmp(name, "II")) {
			flagPtr = &gCpuStatePtr->prime.af.byte.low;
			flagBit = HALFCARRY;
		}
		if (!strcmp(name, "NN")) {
			flagPtr = &gCpuStatePtr->prime.af.byte.low;
			flagBit = SUBTRACT;
		}
		if (!strcmp(name, "AA"))
			bytePtr = &gCpuStatePtr->prime.af.byte.high;
		if (!strcmp(name, "BB"))
			wordPtr = &gCpuStatePtr->prime.bc.word;
		if (!strcmp(name, "DD"))
			wordPtr = &gCpuStatePtr->prime.de.word;
		if (!strcmp(name, "HH"))
			wordPtr = &gCpuStatePtr->prime.hl.word;
#endif
		/* Set a 16-bit register. */
		if (wordPtr != 0) {
			printf("%s %04X ", name, *wordPtr & 0xFFFF);
			if ((gets(buffer) != 0) && (strlen(buffer) > 0)) {
				if (!DigitsToShort(buffer, (short *)wordPtr, 16))
					printf("?\n");
			}
		}
		/* Set an 8-bit register. */
		if (bytePtr != 0) {
			printf("%s %02X ", name, *bytePtr & 0xFFFF);
			if ((gets(buffer) != 0) && (strlen(buffer) > 0)) {
				if (!DigitsToChar(buffer, (char *)bytePtr, 16))
					printf("?\n");
			}
		}
		/* Set a 1-bit flag. */
		if (flagPtr != 0) {
			printf("%s%s ", name, (*flagPtr & flagBit) ? "1" : "0");
			if ((gets(buffer) != 0) && (strlen(buffer) > 0)) {
				if (!DigitsToInt(buffer, &i, 2))
					printf("?\n");
				else if (i == 0)
					*flagPtr &= ~flagBit;
				else
					*flagPtr |= flagBit;
			}
		}
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* FILL() implements the FILL command. */
static int FILL(int argc, char **argv)
{
	void (*wrByte)(Word address, Byte value) = WrByte;
	Word address;
	Word finish;
	Byte byte;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'R':
			if (wrByte == WrRom)
				goto usage;
			wrByte = WrRom;
			break;
		default:
			goto usage;
		}
	}
	/* <ADDRESS> is required. */
	if (i >= argc)
		goto usage;
	if (!StringToShort(argv[i++], (short *)&address))
		goto usage;
	/* <ENDADDRESS> is required. */
	if (i >= argc)
		goto usage;
	if (!StringToShort(argv[i++], (short *)&finish))
		goto usage;
	/* <BYTE> is required. */
	if (i >= argc)
		goto usage;
	if (!StringToChar(argv[i++], (char *)&byte))
		goto usage;
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Fill bytes from start to finish. */
	do
		wrByte(address, byte);
	while (address++ != finish);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* FORMAT() implements the FORMAT command. */
static int FORMAT(int argc, char **argv)
{
	int i;
	char *fileName = 0;
	long siz = -1;
	long spt = -1;
	long bls = -1;
	long drm = -1;
	long off = -1;
	long skf = -1;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		long *valuePtr;
		switch (token[1]) {
		case 'B': valuePtr = &bls; break;
		case 'F': valuePtr = &drm; break;
		case 'O': valuePtr = &off; break;
		case 'S': valuePtr = &spt; break;
		case 'X': valuePtr = &skf; break;
		default:
			goto usage;
		}
		if (*valuePtr != -1)
			goto usage;
		if (strlen(token) > 2)
			token = &token[2];
		else if (++i >= argc)
			goto usage;
		else
			token = argv[i];
		if (!StringToLong(token, valuePtr))
			goto usage;
	}
	/* <FILE> is required. */
	if (i >= argc)
		goto usage;
	fileName = argv[i++];
	/* <SIZE> is optional. */
	if ((i < argc) && (!StringToLong(argv[i++], &siz)))
		goto usage;
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Format the disk. */
	(void)FileDiskFormat(fileName, siz, spt, bls, drm, off, skf);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* GO() implements the GO command. */
static int GO(int argc, char **argv)
{
	char *addressStr = 0;
	Word address;
	int i;
	int maxTBreak = 0;
	short tBreakAddress[kMaxTBreak];

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'B':
			if (strlen(token) > 2)
				token = &token[2];
			else if (++i >= argc)
				goto usage;
			else
				token = argv[i];
			if (maxTBreak >= kMaxTBreak)
				goto usage;
			if (!StringToShort(token, &tBreakAddress[maxTBreak]))
				goto usage;
			maxTBreak++;
			break;
		default:
			goto usage;
		}
	}
	/* <ADDRESS> is optional. */
	if (i < argc) {
		addressStr = argv[i++];
		if (!StringToShort(addressStr, (short *)&address))
			goto usage;
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Set the PC if an address was specified. */
	if (addressStr != 0)
		gCpuStatePtr->pc.word = address;

	/* Set the temporary breakpoints if any were specified. */
	for (gMaxTBreak = 0; gMaxTBreak < maxTBreak; gMaxTBreak++) {
		printf("+%04X\n", tBreakAddress[gMaxTBreak] & 0xFFFF);
		gTBreakAddress[gMaxTBreak] = tBreakAddress[gMaxTBreak];
	}

	printf("%04X\n", gCpuStatePtr->pc.word & 0xFFFF);

	/* Exit the System Monitor. */
	SetSystemFlags(0, kSystemMonitor);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* HELP() implements the HELP command. */
static int HELP(int argc, char **argv)
{
	char *command = 0;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* <COMMAND> is optional. */
	if (i < argc)
		command = argv[i++];
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Help with the command. */
	MonitorHelp(command);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* IMPORT() implements the IMPORT command. */
static int IMPORT(int argc, char **argv)
{
	char internalFileBuffer[kMaxFileName];
	char *internalFile = 0;
	char *externalFile = 0;
	int textFile = 0;
	int binaryFile = 0;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'T':
			if (textFile)
				goto usage;
			if (binaryFile)
				goto usage;
			textFile++;
			break;
		case 'B':
			if (binaryFile)
				goto usage;
			if (textFile)
				goto usage;
			binaryFile++;
			break;
		default:
			goto usage;
		}
	}
	/* <COMFILE> is required. */
	if (i >= argc)
		goto usage;
	internalFile = argv[i++];
	/* <CODEFILE> is optional. */
	if (!textFile && !binaryFile && (i < argc))
		externalFile = argv[i++];
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Allow a shortcut for common text files. */
	if (!textFile && !binaryFile) {
		const char *fileExtension = FileExtension(internalFile);
		textFile |= !strcmp(fileExtension, "ASM");
		textFile |= !strcmp(fileExtension, "Z80");
		textFile |= !strcmp(fileExtension, "HEX");
		textFile |= !strcmp(fileExtension, "LIB");
		textFile |= !strcmp(fileExtension, "TXT");
	}

	/* Import a text file.  Requires PIP.COM. */
	if (textFile) {
		if (!MonitorCommand("ATTACH PTR: %s", internalFile))
			goto error;
		ConsolePushInput("PIP %s=RDR:\r", internalFile);
		SetSystemFlags(0, kSystemMonitor);
	}
	/* Import a binary file.  Requires GETFILE.COM. */
	else if (binaryFile) {
		ConsolePushInput("GETFILE %s\r", internalFile);
		SetSystemFlags(0, kSystemMonitor);
	}
	/* Import a program. */
	else {
		if (strlen(FileExtension(internalFile)) != 0)
			sprintf(internalFileBuffer,
			        "%s.%s",
			        FileName(internalFile),
			        FileExtension(internalFile));
		else {
			sprintf(internalFileBuffer,
			        "%s.COM",
			        FileName(internalFile));
		}
		internalFile = internalFileBuffer;
		if (externalFile == 0) {
			if (!MonitorCommand("LOAD %s.HEX", FileName(internalFile)))
				goto error;
		}
		else {
			if (!MonitorCommand("LOAD %s", externalFile))
				goto error;
		}
		ConsolePushInput("SAVE %d %s\r", gPageCount, internalFile);
		SetSystemFlags(0, kSystemMonitor);
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* KEY() implements the KEY command. */
static int KEY(int argc, char **argv)
{
	int i;
	int key;
	unsigned x;
	unsigned y;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Clear the screen. */
	printf(kConsoleClearScreen);

	/* Show the keys... */
	for (;;) {

		key = getchar();

		printf("\\%03o 0x%02X %3d ", key & 0xFF, key & 0xFF, key);

		if (key > 0) {
			if (iscntrl(key) && (key != 0x7F))
				printf("^%c ", '@' + key);
			if (isprint(key))
				printf("'%c' ", key);
		}

		switch (key) {
		case kConsoleQuit:    printf("QUIT");       break;
		case kConsoleMonitor: printf("MONITOR");    break;
		case F9KEY:           printf("F9");         break;
		case F8KEY:           printf("F8");         break;
		case F7KEY:           printf("F7");         break;
		case F6KEY:           printf("F6");         break;
		case F5KEY:           printf("F5");         break;
		case F4KEY:           printf("F4");         break;
		case F3KEY:           printf("F3");         break;
		case F2KEY:           printf("F2");         break;
		case F1KEY:           printf("F1");         break;
		case HELPKEY:         printf("HELP");       break;
		case ESCAPEKEY:       printf("ESCAPE");     break;
		case RETURNKEY:       printf("RETURN");     break;
		case DELETEKEY:       printf("DELETE");     break;
		case BACKSPACEKEY:    printf("BACKSPACE");  break;
		case TABKEY:          printf("TAB");        break;
		case UPARROWKEY:      printf("UPARROW");    break;
		case DOWNARROWKEY:    printf("DOWNAROW");   break;
		case LEFTARROWKEY:    printf("LEFTARROW");  break;
		case RIGHTARROWKEY:   printf("RIGHTARROW"); break;
		case REFRESHKEY:      printf("REFRESH");    break;
		case INSERTKEY:       printf("INSERT");     break;
		case PAGEUPKEY:       printf("PAGEUP");     break;
		case PAGEDOWNKEY:     printf("PAGEDOWN");   break;
		case MOUSEKEY:
			ConsoleMouse(&x, &y);
			printf("MOUSE %d,%d", x, y);
			break;
		default:
			if (key < 0)
				printf("???");
			break;
		}

		printf("\n");

		if (key == kConsoleQuit)
			break;

		if (key == kConsoleMonitor)
			break;

	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* LIST() implements the LIST command. */
static int LIST(int argc, char **argv)
{
	Byte (*rdByte)(Word address) = RdByte;
	char *addressStr = 0;
	Word address = gAddress;
	char *outputFile = 0;
	char buffer[256];
	Byte operation[4];
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'R':
			if (rdByte == RdRom)
				goto usage;
			rdByte = RdRom;
			break;
		default:
			goto usage;
		}
	}
	/* <ADDRESS> is optional. */
	if (i < argc) {
		addressStr = argv[i++];
		if (!StringToShort(addressStr, (short *)&address))
			goto usage;
	}
	/* <OUTPUTFILE> is optional. */
	if (i < argc)
		outputFile = argv[i++];
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Disassemble to a file if one is specified. */
	if (outputFile != 0)
		Disassemble(outputFile, address, rdByte);
	/* Otherwise display the next 16 instructions. */
	else {
		for (i = 0; i < 16; i++) {
			printf("%04X  ", address & 0xFFFF);
			operation[0] = rdByte(address + 0);
			operation[1] = rdByte(address + 1);
			operation[2] = rdByte(address + 2);
			operation[3] = rdByte(address + 3);
			DisassembleInstruction(buffer,
			                       address,
			                       operation,
			                       &address,
			                       0,
			                       0);
			printf("%s\n", buffer);
		}
	}

	/* Remember the last address. */
	gAddress = address;

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* LOAD() implements the LOAD command. */
static int LOAD(int argc, char **argv)
{
	char buffer[256];
	char codeFileBuffer[kMaxFileName];
	const char *addressStr = 0;
	const char *codeFile = 0;
	void (*wrByte)(Word address, Byte value) = WrByte;
	char *biasStr = 0;
	int zeroMemory = 0;
	int stripZeros = 0;
	int debug = 0;
	Word bias = 0;
	Word address = 0;
	int i;
	int n;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'B':
			if (biasStr != 0)
				goto usage;
			if (strlen(token) > 2)
				token = &token[2];
			else if (++i >= argc)
				goto usage;
			else
				token = argv[i];
			biasStr = token;
			if (!StringToShort(biasStr, (short *)&bias))
				goto usage;
			break;
		case 'D':
			if (debug)
				goto usage;
			debug++;
			break;
		case 'R':
			if (wrByte == WrRom)
				goto usage;
			wrByte = WrRom;
			break;
		case 'S':
			if (stripZeros)
				goto usage;
			stripZeros++;
			break;
		case 'Z':
			if (zeroMemory)
				goto usage;
			zeroMemory++;
			break;
		default:
			goto usage;
		}
	}
	/* <ADDRESS> is optional */
	if (i < argc) {
		addressStr = argv[i++];
		if (!StringToShort(addressStr, (short *)&address)) {
			addressStr = 0;
			i--;
		}
	}
	/* No other arguments are allowed if <ADDRESS> specified. */
	if ((addressStr != 0) && (i < argc))
		goto usage;

	/* If a single address is specified, go interactive. */
	if (addressStr != 0) {
		/* Write to ROM if "-R" specified. */
		/* Zero memory if "-Z" specified. */
		/* Strip zeros if "-S" specified. */
		/* Debug if "-D" specified. */
		/* Bias if "-B" specified. */
		if (zeroMemory) {
			if (wrByte == WrRom)
				ZeroRom();
			else
				ZeroRam();
		}
		for (;;) {
			printf("%04X  ", address & 0xFFFF);
			if (gets(buffer) == 0)
				break;
			if (strlen(buffer) == 0)
				break;
			if (!strcmp(buffer, "."))
				break;
			gPageCount = Assemble(buffer,
			                      0,
			                      0,
			                      0,
			                      0,
			                      0,
			                      0,
			                      wrByte,
			                      &address,
			                      bias,
			                      stripZeros,
			                      debug);
			if (!gPageCount) {
				printf("?\n");
				goto error;
			}
		}
	}
	/* If object files are specified, load them. */
	else if (i < argc) {
		/* Write to ROM if "-R" specified. */
		/* Zero memory if "-Z" specified. */
		/* Strip zeros if "-S" specified. */
		/* Debug if "-D" specified. */
		/* Bias if "-B" specified. */
		if (zeroMemory) {
			if (wrByte == WrRom)
				ZeroRom();
			else
				ZeroRam();
		}
		gPageCount = 0;
		while (i < argc) {
			/* The default object file extension is ".HEX". */
			codeFile = argv[i++];
			if (strlen(FileExtension(codeFile)) != 0)
				sprintf(codeFileBuffer,
				        "%s.%s",
				        FileName(codeFile),
				        FileExtension(codeFile));
			else
				sprintf(codeFileBuffer, "%s.HEX", FileName(codeFile));
			codeFile = codeFileBuffer;
			n = Assemble(0,
			             0,
			             0,
			             codeFile,
			             0,
			             0,
			             0,
			             wrByte,
			             0,
			             bias,
			             stripZeros,
			             debug);
			if (n == 0)
				goto error;
			if (n > gPageCount)
				gPageCount = n;
		}
		printf("%d PAGES\n", gPageCount);
	}
	/* If no object files are specified, load the internal ROM. */
	else {
		if (zeroMemory)
			goto usage;
		if (wrByte == WrRom)
			goto usage;
		if (bias != 0)
			goto usage;
		if (stripZeros)
			goto usage;
		if (debug)
			goto usage;
#ifdef CALCULATE_ROM
		if (!MonitorCommand("LOAD -R -Z %s", ROM_SOURCE))
			goto error;
#ifdef GENERATE_ROM
		if (!MonitorCommand("UNLOAD -C -R -S %s", kRomFile))
			goto error;
#endif
#else
		ZeroRom();
		gPageCount = Assemble(0,
		                      0,
		                      gRomCode,
		                      0,
		                      0,
		                      0,
		                      0,
		                      WrRom,
		                      0,
		                      0,
		                      0,
		                      0);
		if (gPageCount == 0)
			goto error;
		printf("%d PAGES\n", gPageCount);
#endif
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* MAP() implements the MAP command. */
static int MAP(int argc, char **argv)
{
	char *thisBankStr = 0;
	long thisBank;
	long maxThisBank;
	char *thatBankStr = 0;
	long thatBank;
	long maxThatBank;
	int physical = 0;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'P':
			if (physical)
				goto usage;
			physical++;
			break;
		default:
			goto usage;
		}
	}
	/* Note limits to thisBank and thatBank. */
	if (physical) {
		maxThisBank = MaxRamBank();
		maxThatBank = kMaxBank;
	}
	else {
		maxThisBank = kMaxBank;
		maxThatBank = MaxRamBank();
	}
	/* <BANK> is optional. */
	if (i < argc) {
		thisBankStr = argv[i++];
		if (!StringToLong(thisBankStr, &thisBank))
			goto usage;
		else if (thisBank < 0)
			goto usage;
		else if (thisBank >= maxThisBank)
			goto usage;
	}
	/* <VALUE> is optional. */
	if (i < argc) {
		thatBankStr = argv[i++];
		if (!StringToLong(thatBankStr, &thatBank))
			goto usage;
		else if (thatBank < 0)
			goto usage;
		else if (thatBank >= maxThatBank)
			goto usage;
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Show the entire map if <BANK> is not specified. */
	if (thisBankStr == 0) {
		for (thisBank = 0; thisBank < maxThisBank; thisBank++) {
			if (physical)
				ShowPhysicalMemoryMap(thisBank);
			else
				ShowLogicalMemoryMap(thisBank);
		}
	}
	else {
		/* Set the bank if <VALUE> is specified. */
		if (thatBankStr != 0) {
			if (physical)
				WrBank(thatBank, thisBank);
			else
				WrBank(thisBank, thatBank);
		}
		/* Show the specific map entry if <BANK> is specified. */
		if (physical)
			ShowPhysicalMemoryMap(thisBank);
		else
			ShowLogicalMemoryMap(thisBank);
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* MOUNT() implements the MOUNT command. */
static int MOUNT(int argc, char **argv)
{
	char _bDev[3];
	char *bDev = 0;
	char *file = 0;
	BDevPtr bDevPtr = 0;
	int showFCB = 0;
	int showALV = 0;
	int showDIR = 0;
	char *fcbNumberStr = 0;
	int readOnly = 0;
	int verbose = 0;
	int eraseSystemTracks = 0;
	long fcbNumber = -1;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'A':
			if (showALV)
				goto usage;
			showALV++;
			break;
		case 'D':
			if (showDIR)
				goto usage;
			showDIR++;
			break;
		case 'F':
			if (showFCB)
				goto usage;
			showFCB++;
			break;
		case 'N':
			if (fcbNumberStr != 0)
				goto usage;
			if (strlen(token) > 2)
				fcbNumberStr = &token[2];
			else if (++i >= argc)
				goto usage;
			else
				fcbNumberStr = argv[i];
			break;
		case 'R':
			if (readOnly)
				goto usage;
			readOnly++;
			break;
		case 'V':
			if (verbose)
				goto usage;
			verbose++;
			break;
		case 'Z':
			if (eraseSystemTracks)
				goto usage;
			eraseSystemTracks++;
			break;
		default:
			goto usage;
		}
	}
	/* <DISK> is optioal. */
	if (i < argc)
		bDev = argv[i++];
	/* <FILE> is optional. */
	if (i < argc)
		file = argv[i++];
	/* <DISK> and <FILE> might not be seperated by a space. */
	if ((bDev != 0) && (file == 0)) {
		if ((bDev[1] == ':') && (bDev[2] != 0)) {
			strncpy(_bDev, bDev, 2);
			_bDev[2] = 0;
			file = &bDev[2];
			bDev = _bDev;
		}
	}
	if ((fcbNumberStr != 0) && !showFCB)
		goto usage;
	if (fcbNumberStr != 0) {
		if (!StringToLong(fcbNumberStr, &fcbNumber))
			goto usage;
		if (fcbNumber < 0)
			goto usage;
	}
	if (showDIR) {
		if (readOnly || verbose || showFCB || showALV || (bDev == 0))
			goto usage;
	}
	else if (showFCB) {
		if (readOnly || verbose || showALV || (bDev == 0))
			goto usage;
	}
	else if (showALV) {
		if (readOnly || verbose || (bDev == 0))
			goto usage;
	}
	else if (file == 0) {
		if (readOnly || (verbose && (bDev == 0)))
			goto usage;
	}
	else {
		if (bDev == 0)
			goto usage;
	}
	if (eraseSystemTracks) {
		if (readOnly || verbose || showFCB || showALV || showDIR)
			goto usage;
	}
	if (bDev != 0) {
		if ((bDevPtr = BDevMount(bDev, 0, 0)) == 0) {
			printf("?NODISK [%s]\n", bDev);
			goto error;
		}
		if (showDIR || showFCB || showALV || eraseSystemTracks)
			if (BDevStatus(bDevPtr, 0) == kBDevStatusClosed) {
				printf("?NOMOUNT [%s]\n", bDev);
				goto error;
			}
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Erase System Tracks. */
	if (eraseSystemTracks)
		EraseSystemTracks(bDevPtr);
	/* Show directory for this bDev. */
	else if (showDIR)
		ShowBDevDIR(bDev, fcbNumber, file);
	/* Show FCB for this bDev. */
	else if (showFCB)
		ShowBDevFCB(bDev, fcbNumber, file);
	/* Show ALV for this bDev. */
	else if (showALV)
		ShowBDevALV(bDev, fcbNumber, file);
	/* Show mount state for all bDev's. */
	else if (bDev == 0)
		ShowBDevMount(0, verbose);
	/* Show mount state for this bDev, or all bDev's. */
	else if (file == 0)
		ShowBDevMount(bDev, verbose);
	/* Mount this bDev. */
	else if ((bDevPtr = BDevMount(bDev, file, readOnly)) == 0) {
		printf("?MOUNT [%s]\n", file);
		goto error;
	}
	/* Show mount state for this bDev. */
	else
		ShowBDevMount(bDev, verbose);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* MOVE() implements the MOVE command. */
static int MOVE(int argc, char **argv)
{
	void (*wrByte)(Word address, Byte value) = WrByte;
	Byte (*rdByte)(Word address) = RdByte;
	Word address;
	Word finish;
	Word dest;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'R':
			if (wrByte == WrRom)
				goto usage;
			wrByte = WrRom;
			rdByte = RdRom;
			break;
		default:
			goto usage;
		}
	}
	/* <ADDRESS> is required. */
	if (i >= argc)
		goto usage;
	if (!StringToShort(argv[i++], (short *)&address))
		goto usage;
	/* <ENDADDRESS> is required. */
	if (i >= argc)
		goto usage;
	if (!StringToShort(argv[i++], (short *)&finish))
		goto usage;
	/* <DESTADDRESS> is required. */
	if (i >= argc)
		goto usage;
	if (!StringToShort(argv[i++], (short *)&dest))
		goto usage;
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Move bytes from start to finish. */
	do
		wrByte(dest++, RdByte(address));
	while (address++ != finish);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* PRINT() implements the PRINT command. */
static int PRINT(int argc, char **argv)
{
	long result;
	int i;
	char buffer[256];
	char *b;
	char *a;

	/* Gather the arguments back into a single buffer. */
	b = buffer;
	for (i = 1; i < argc; i++) {
		a = argv[i];
		*b++ = ' ';
		while (*a != 0)
			*b++ = *a++;
		*b = 0;
	}

	/* Evaluate the expression, display the results. */
	if (!CalculateExpression(buffer, &result)) {
		printf("?ERROR\n");
		goto error;
	}
	else {

		/* Negative signed decimal. */
		if (result < 0)
			printf("%ldD\n", result);

		/* Unsigned decimal. */
		printf("%luD\n", result);

		/* Unsigned octal. */
		printf("%oQ\n", result);

		/* 1, 2 or 4 byte unsigned hexadecimal. */
		if (IsChar(result))
			printf("%s%02XH\n",
			       ((result & 0xF0) > 0x90) ? "0" : "",
			       result & 0xFF);
		else if (IsShort(result))
			printf("%s%04XH\n",
			       ((result & 0xF000) > 0x9000) ? "0" : "",
			       result & 0xFFFF);
		else
			printf("%s%04X$%04XH\n",
			       ((result & 0xF0000000) > 0x90000000) ? "0" : "",
			       (result >> 16) & 0xFFFF,
			       result & 0xFFFF);

		/* 1, 2, or 4 byte binary. */
		if (IsChar(result))
			i = 8;
		else if (IsShort(result))
			i = 16;
		else
			i = 32;
		while (i-- > 0) {
			printf("%c", (result & (1 << i)) ? '1' : '0');
			if (((i % 8) == 0) && (i != 0))
				printf("$");
		}
		printf("B\n");

		/* Number each bit for the binary number. */
		if (IsChar(result))
			i = 8;
		else if (IsShort(result))
			i = 16;
		else
			i = 32;
		while (i-- > 0) {
			printf("%d", i / 10);
			if (((i % 8) == 0) && (i != 0))
				printf(" ");
		}
		printf("\n");
		if (IsChar(result))
			i = 8;
		else if (IsShort(result))
			i = 16;
		else
			i = 32;
		while (i-- > 0) {
			printf("%d", i % 10);
			if (((i % 8) == 0) && (i != 0))
				printf(" ");
		}
		printf("\n");

	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* QUIT() implements the QUIT command. */
static int QUIT(int argc, char **argv)
{
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Set the kSystemHalt, clear all other flags. */
	SetSystemFlags(kSystemHalt, ~kSystemHalt);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* RESET() implements the RESET command. */
static int RESET(int argc, char **argv)
{
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Clear the Cpu State. */
	MemoryZero(gCpuStatePtr, sizeof(CpuState));
	gAddress = 0;

	/* Zero RAM, reset the MAP. */
	MemoryReset();

	/* Unmount all block devices. */
	BDevUnmount(0);

	/* Unmount all character devices. */
	CDevDetach(0);

	/* Attach TTY: to the Console. */
	CDevAttach("TTY:", "CONSOLE");

	/* Set the default System ID. */
	ResetSystemID();

	/* Reset the Clock. */
	ResetClock(0);

	/* Clear the kSystemReset and kSystemHalt flags. */
	SetSystemFlags(0, kSystemReset | kSystemHalt);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* SET() implements the SET command. */
static int SET(int argc, char **argv)
{
	char buffer[256];
	Byte (*rdByte)(Word address) = RdByte;
	void (*wrByte)(Word address, Byte value) = WrByte;
	char *addressStr = 0;
	Word address;
	char *valueStr = 0;
	WordBytes value;
	int isShort;
	int i;
	int n;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'R':
			if (rdByte == RdRom)
				goto usage;
			rdByte = RdRom;
			wrByte = WrRom;
			break;
		default:
			goto usage;
		}
	}
	/* <ADDRESS> is required. */
	if (i >= argc)
		goto usage;
	addressStr = argv[i++];
	if (!StringToShort(addressStr, (short *)&address))
		goto usage;
	/* <VALUE> is optional. */
	if (i < argc) {
		valueStr = argv[i++];
		if ((n = StringToShort(valueStr, (short *)&value.word)) == 0)
			goto usage;
		isShort = n > 2;
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* If a value was specified, write it. */
	if (valueStr != 0) {
		wrByte(address++, value.byte.low);
		if (isShort)
			wrByte(address++, value.byte.high);
	}
	/* Otherwise, go interactive. */
	else {
		for (;;) {
			printf("%04X %02X ", address & 0xFFFF, rdByte(address) & 0xFF);
			if (gets(buffer) == 0)
				break;
			if (strlen(buffer) == 0)
				break;
			if (!strcmp(buffer, "."))
				break;
			if (!DigitsToChar(buffer, (char *)&value.byte.low, 16)) {
				printf("?\n");
				break;
			}
			wrByte(address++, value.byte.low);
		}
	}

	/* Remember the last address. */
	gAddress = address;

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* SYSID() implements the SYSID command. */
static int SYSID(int argc, char **argv)
{
	char *systemIDStr = 0;
	long systemID;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* <NEW VALUE> is optional. */
	if (i < argc) {
		systemIDStr = argv[i++];
		if (!StringToLong(systemIDStr, &systemID))
			goto usage;
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Set the System ID if a <NEW VALUE> is specified. */
	if (systemIDStr != 0)
		SetSystemID(systemID);

	/* Show the current System ID. */
	printf("%ld\n", GetSystemID());

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* TRACE() implements the TRACE command. */
static int TRACE(int argc, char **argv)
{
	char *traceFileName = 0;
	char *countStr = 0;
	long count;
	int disable = 0;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* <COUNT> is optonal. */
	if (i < argc) {
		countStr = argv[i++];
		if (!strcmp(countStr, ".")) {
			countStr = 0;
			disable = 1;
		}
		else if (!StringToLong(countStr, &count)) {
			countStr = 0;
			i--;
		}
	}
	/* <OUTPUT FILE> is optional. */
	if (!disable && (i < argc)) {
		traceFileName = argv[i++];
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Disable? */
	if (disable) {
		CloseTraceFile();
		gTraceDisplay = 0;
	}
	/* Enable? */
	else {
		/* Enable tracing. */
		gTraceCount = (countStr == 0) ? 1 : count;
		gTraceDisplay = 1;
		/* Open Trace File? */
		if (traceFileName != 0)
			if (!OpenTraceFile(traceFileName))
				gTraceCount = 0;
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* UNMOUNT() implements the UNMOUNT command. */
static int UNMOUNT(int argc, char **argv)
{
	char *bDev = 0;
	BDevPtr bDevPtr;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* <DISK> is required. */
	if (i >= argc)
		goto usage;
	bDev = argv[i++];
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Unmount this bDev. */
	if ((bDevPtr = BDevMount(bDev, 0, 0)) == 0) {
		printf("?NODISK [%s]\n", bDev);
		goto error;
	}
	if (BDevStatus(bDevPtr, 0) != kBDevStatusClosed)
		BDevUnmount(bDev);

	/* Confirm that this bDev has been unmounted. */
	ShowBDevMount(bDev, 0);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* UNLOAD() implements the UNLOAD command. */
static int UNLOAD(int argc, char **argv)
{
	Byte (*rdByte)(Word address) = RdByte;
	char objectFileBuffer[kMaxFileName];
	const char *objectFile = 0;
	int cCode = 0;
	int stripZeros = 0;
	int codeFile = 0;
	int i;
	int pageCount;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		case 'C':
			if (codeFile)
				goto usage;
			codeFile++;
			break;
		case 'R':
			if (rdByte == RdRom)
				goto usage;
			rdByte = RdRom;
			break;
		case 'S':
			if (stripZeros)
				goto usage;
			stripZeros++;
			break;
		default:
			goto usage;
		}
	}
	/* <HEXFILE> is required. */
	if (i >= argc)
		goto usage;
	objectFile = argv[i++];
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* The default code file extension is ".H". */
	/* The default object file extension is ".HEX". */
	if (strlen(FileExtension(objectFile)) != 0)
		sprintf(objectFileBuffer,
		        "%s.%s",
		        FileName(objectFile),
		        FileExtension(objectFile));
	else if (codeFile)
		sprintf(objectFileBuffer, "%s.H", FileName(objectFile));
	else
		sprintf(objectFileBuffer, "%s.HEX", FileName(objectFile));
	objectFile = objectFileBuffer;

	/* Do the unload. */
	pageCount = Assemble(0,
	                     rdByte,
	                     0,
	                     0,
	                     0,
	                     codeFile ? 0 : objectFile,
	                     codeFile ? objectFile : 0,
	                     0,
	                     0,
	                     0,
	                     stripZeros,
	                     0);
	if (pageCount == 0)
		goto error;
	printf("%d PAGES\n", pageCount);

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/* UNTRACE() implements the UNTRACE command. */
static int UNTRACE(int argc, char **argv)
{
	char *traceFileName = 0;
	char *countStr = 0;
	long count;
	int disable = 0;
	int i;

	/* Parse the command line switches. */
	for (i = 1; (i < argc) && (*(argv[i]) == '-'); i++) {
		char *token = argv[i];
		switch (token[1]) {
		default:
			goto usage;
		}
	}
	/* <COUNT> is optonal. */
	if (i < argc) {
		countStr = argv[i++];
		if (!strcmp(countStr, ".")) {
			countStr = 0;
			disable = 1;
		}
		else if (!StringToLong(countStr, &count)) {
			countStr = 0;
			i--;
		}
	}
	/* <OUTPUT FILE> is optional. */
	if (!disable && (i < argc)) {
		traceFileName = argv[i++];
	}
	/* No more arguments are allowed. */
	if (i < argc)
		goto usage;

	/* Disable? */
	if (disable) {
		CloseTraceFile();
		gTraceDisplay = 0;
	}
	/* Enable? */
	else {
		/* Enable tracing. */
		gTraceCount = (countStr == 0) ? 1 : count;
		gTraceDisplay = 0;
		/* Open Trace File? */
		if (traceFileName != 0)
			if (!OpenTraceFile(traceFileName))
				gTraceCount = 0;
	}

	/* All done, no error, return zero exit status. */
	return 0;

	/* Command syntax error. */
usage:
	MonitorHelp(argv[0]);
	goto error;

	/* Return non-zero exit status if there was an error. */
error:
	return 1;

}

/**********************************************************************/
#pragma mark *** MONITOR ***

#pragma mark gMonitorCommands[]
struct {
	char *name;
	int (*function)(int argc, char **argv);
	char *shortDocumentation;
	char *longDocumentation;
} gMonitorCommands[] = {

{ "ABOUT", ABOUT, "Display the Copyright.", 0 },
 
{ "ASM", ASSEMBLE, "Assemble " CPU " code.", ";See: ASSEMBLE." },

{ "ASSEMBLE", ASSEMBLE, "Assemble " CPU " code.",
  "ASSEMBLE [-L] [-S] [-K] [-B<BIAS>] <CODEFILE> [ <HEXFILE> ]\n"
  ";Note: Use -L to produce a list file.\n"
  ";      Use -S to supress zero records.\n"
  ";      Use -K to supress the <HEXFILE> output.\n"
  ";      Use -B to specify an address bias.\n"
  ";Note: The <CODEFILE> can be in SOURCE, HEX or COM format.\n"
  ";Note: The default 8080 source extension is \".ASM\".\n"
#ifdef Z80
  ";      The default Z80 source extension is \".Z80\".\n"
#endif
  ";      The default HEXFILE extension is \".HEX\".\n"
  ";      The default LISTFILE extension is \".LST\".\n"
  "Assembler Directives:\n"
  "    .8080    .DB       .DEFB     .DEFS     .DEFW     .DS\n"
  "    .DW      .ELSE     .ELSEIF   .END      .ENDIF    .EQU\n"
  "    .IF      .OPCODES  .ORG      .SET      .SOURCE   .TITLE\n"
  "    .Z80     DB        DEFB      DEFS      DEFW      DS\n"
  "    DW       ELSE      ELSEIF    END       ENDIF     EQU\n"
  "    IF       MACLIB    ORG       SET       TITLE\n"
  "Note: Use the .OPCODES directive to generate a list of opcodes."
},

{ "ATTACH", ATTACH, "Attach a character device.",
  "ATTACH <PHYSDEV> <FILE>   ; attaches <PHYSDEV> to the <FILE>,\n"
  "ATTACH <LOGDEV> <PHYSDEV> ; attaches <LOGDEV> to the <PHYSDEV>,\n"
  "ATTACH                    ; lists the current device table\n"
  "ATTACH <LOGDEV>           ; lists the device table entry\n"
  "ATTACH <PHYSDEV>          ; lists the device table entry\n"
  ";Note: Use the <FILE> \"CONSOLE\" for console input/output.\n"
  ";Note: <PHYSDEV> is one of:\n"
  ";        TTY:, CRT:, UC1:, UC2:\n"
  ";        LPT:, UL1:, UL2:, UL3:\n"
  ";        PTR:, UR1:, UR2:, UR3:\n"
  ";        PTP:, UP1:, UP2:, UP3:\n"
  ";Note: <LOGDEV> is one of: CON:, LST:, RDR:, PUN:\n"
  ";        CON: <PHYSDEV> are TTY:, CRT:, UC1:, BAT:\n"
  ";        LST: <PHYSDEV> are TTY:, CRT:, LPT:, UL1:\n"
  ";        RDR: <PHYSDEV> are TTY:, PTR:, UR1:, UR2:\n"
  ";        PTP: <PHYSDEV> are TTY:, PTP:, UP1:, UP2:"
},

{ "BOOT", BOOT, "Boot the " CPU " " kProgram ".",
  "BOOT        ; run monitor commands from the " kBOOTBAT " file"
},

{ "BREAK", BREAK, "Set or remove breakpoints.",
  "BREAK                   ; displays current breakpoints\n"
  "BREAK .                 ; removes all breakpoints\n"
  "BREAK <BREAKPOINT>...   ; sets and removes breakpoints\n"
  ";    [+]<ADDRESS>         sets a breakpoint\n"
  ";    -<ADDRESS>           removes a breakpoint"
},

{ "CLOCK", CLOCK, "Acess the time of day device.",
  "CLOCK [ <NEW VALUE> ]   ; shows or sets the time of day"
},

{ "COPY", COPY, "Copy a disk.",
  "COPY [ -R ] <DISK> <FILE>  ; copy the <DISK> to the <FILE>\n"
  "Note: Use -R to create a read-only ASCII file."
},

{ "DDT", DDTHELP, "CP/M DDT-style command syntax.",
  "As     -- LOAD s              L      -- LIST\n"
  "D      -- DISPLAY             Ls     -- LIST s\n"
  "Ds     -- DISPLAY s           Ls,f   -- LIST s f\n"
  "Ds,f   -- DISPLAY s f         Ms,f,d -- MOVE s f d\n"
  "Fs,f,c -- FILL s f c          Ss     -- SET s\n"
  "G      -- GO                  T      -- TRACE\n"
  "Gs     -- GO s                Tn     -- TRACE n\n"
  "Gs,b   -- GO -Bb s            U      -- UNTRACE\n"
  "Gs,b,c -- GO -Bb -Bc s        Un     -- UNTRACE n\n"
  "G,b    -- GO -Bb              X      -- EXAMINE\n"
  "G,b,c  -- GO -Bb -Bc          Xr     -- EXAMINE r"
},

{ "DETACH", DETACH, "Detach a character device.",
  "DETACH <PHYSDEV>    ; detaches the file associated with <PHYSDEV>\n"
  ";Note: <PHYSDEV> is one of:\n"
  ";        TTY:, CRT:, UC1:, UC2:\n"
  ";        LPT:, UL1:, UL2:, UL3:\n"
  ";        PTR:, UR1:, UR2:, UR3:\n"
  ";        PTP:, UP1:, UP2:, UP3:"
},

{ "DISPLAY", DISPLAY, "Display memory.",
  "DISPLAY [-R] [<ADDRESS>]            ; display 256 bytes of memory\n"
  "DISPLAY [-R] <ADDRESS> <ENDADDRESS> ; display memory\n"
  ";Note: Use -R to access the ROM directly."
},

{ "EXAMINE", EXAMINE, "Examine the CPU State.",
  "EXAMINE               ; Display the CPU State.\n"
  "EXAMINE <REGISTER> ]  ; Set the CPU State.\n"
#ifdef Z80
  ";  C, CC    CARRY flag (alternate)\n"
  ";  Z, ZZ    ZERO flag (alternate)\n"
  ";  M, MM    SIGN flag (alternate)\n"
  ";  E, EE    PARITY flag (alternate)\n"
  ";  I, II    HALFCARRY flag (alternate)\n"
  ";  N, NN    SUBTRACT flag (alternate)\n"
  ";  A, AA    A register (alternate)\n"
  ";  B, BB    BC register (alternate)\n"
  ";  D, DD    DE register (alternate)\n"
  ";  H, HH    HL register (alternate)\n"
  ";  S        SP register\n"
  ";  P        PC register\n"
  ";  X        IX register\n"
  ";  Y        IY register"
#else
  ";  C       CARRY flag\n"
  ";  Z       ZERO flag\n"
  ";  M       SIGN flag\n"
  ";  E       PARITY flag\n"
  ";  I       HALFCARRY flag\n"
  ";  N       SUBTRACT flag\n"
  ";  A       A register\n"
  ";  B       BC register\n"
  ";  D       DE register\n"
  ";  H       HL register\n"
  ";  S       SP register\n"
  ";  P       PC register"
#endif
},

{ "FILL", FILL, "Fill memory.",
  "FILL [-R] <ADDRESS> <ENDADDRESS> <BYTE> ; fill memory with bytes.\n"
  ";Note: Use -R to access the ROM directly."
},

{ "FORMAT", FORMAT, "Format a disk.",
  "FORMAT [ <FLAGS> ] <FILE> [ <SIZE> ] ; creates a CP/M disk file\n"
  ";                        default <SIZE> is 256256\n"
  "; -B <BLOCK SIZE>        default 1024 if <SIZE> < 256K, else 2048\n"
  "; -D <# DIR ENTRIES - 1> default 63 if <SIZE> < 256K, else 1023\n"
  "; -S <SECTORS PER TRACK> default 26 if <SIZE> < 256K, else 64\n"
  "; -O <TRACK OFFSET>      default 0\n"
  "; -X <SKEW FACTOR>       default 1\n"
  "FORMAT <FILE> 256256   ; creates a standard floppy disk"
},

{ "GO", GO, "Continue " CPU " execution.",
  "GO [-B<BREAKPOINT>] [<ADDRESS>] ; continues " CPU " execution\n"
  ";Note: Use -B to set a temporary breakpoint."
},

{ "HELP", HELP, "Display information about commands.",
  "HELP                   ; list all commands\n"
  "HELP <COMMAND>         ; displays information about <COMMAND>"
},

{ "IMPORT", IMPORT, "Import programs and files.",
  "IMPORT <COMFILE> [ <CODEFILE> ] ; import a program\n"
  "IMPORT <TEXTFILE>               ; import a text file\n"
  "IMPORT -T <TEXTFILE>            ; import a text file\n"
  "IMPORT -B <BINARYFILE>          ; import a binary file\n"
  ";Note: The CP/M CPP must be running.\n"
  ";      The -T option requires PIP to be installed.\n"
  ";      The -B option requires GETFILE to be installed.\n"
  ";Note: The <CODEFILE> can be in SOURCE, HEX or COM format.\n"
  ";      The default <CODEFILE> is <COMFILE> with \".HEX\""
  ";      substituted for \".COM\".\n"
  ";Note: The default <COMFILE> extension is \".COM\"\n"
  ";Note: <TEXTFILE> extensions are ASM, Z80, HEX, LIB and TXT." 
},

{ "KEY", KEY, "Show key codes.", 0 },

{ "LIST", LIST, "List instructions.",
  "LIST [-R] <ADDRESS> ; list next 16 instructions from memory\n"
  "LIST [-R] <ADDRESS> <OUTPUTFILE> ; static disassembly from memory\n"
  ";Note: Use -R to disassemble directly from the ROM.\n"
  ";Note: Static disassembly does not follow dynamic references."
},

{ "LOAD", LOAD, "Load " CPU " code into memory.",
  "LOAD [-B<BIAS>] [-R] [-S] [-Z] [ <CODEFILE> ... ]\n"
  "LOAD [-B<BIAS>] [-R] [-S] [-Z] <ADDRESS> ; interactive assembly\n"
  ";Note: Use -B to specify an address bias.\n"
  ";      Use -R to load code directly into the ROM.\n"
  ";      Use -S to supress zero records.\n"
  ";      Use -Z to zero memory before loading.\n"
  ";Note: The page count can be used for the CP/M \"SAVE\" command.\n"
  ";Note: The <CODEFILE> can be in SOURCE, HEX or COM format.\n"
  ";Note: Load the default ROM if no <CODEFILE> is specified."
},

{ "MAP", MAP, "Access the memory mapping device.",
  "MAP [-P] <BANK> <VALUE>    ; set the memory bank\n"
  "MAP [-P] <BANK>            ; show the memory bank\n"
  "MAP [-P]                   ; show the entire memory map\n"
  ";Note: Use -P to do physical to logical mapping."
},

{ "MOUNT", MOUNT, "Mount a disk.",
  "MOUNT [-R] <DISK> <FILE>   ; mounts <FILE> as CP/M disk <DISK>\n"
  "MOUNT                      ; lists the current mount table\n"
  "MOUNT [-V] <DISK>          ; lists the mount table entry\n"
  "MOUNT -Z <DISK>            ; clears the system tracks\n"
  "MOUNT -D <DISK> [ <NAME> ] ; shows the disk directory\n"
  "MOUNT -F [ -N <FCB NUMBER> ] <DISK> [ <NAME> ] ; shows the FCB\n"
  "MOUNT -A <DISK>            ; shows the disk allocation vector\n"
  ";    <DISK> is one of:\n"
  ";        A: B: C: D: E: F: G: H: I: J: K: L: M: N: O: P:\n"
  ";Note: Use -R to mount a disk read-only."
},

{ "MOVE", MOVE, "Move memory.",
  "MOVE [-R] <ADDRESS> <ENDADDRESS> <DESTADDRESS> ; move bytes\n"
  ";Note: Use -R to access the ROM directly."
},

{ "PRINT", PRINT, "Evaluate an expression.",
  "PRINT <EXPRESSION>     ; prints the expression result\n"
  ";Note: This uses the assembler expression syntax."
},

{ "QUIT", QUIT, "Terminate the " CPU " " kProgram ".",
  "QUIT                   ; terminates the " CPU " " kProgram ""
},

{ "RESET", RESET, "Reset the " CPU " " kProgram ".",
  "RESET     ; zeros all " CPU " registers\n"
  ";           zeros RAM\n"
  ";           resets memory MAP to ROM\n"
  ";           unmounts all CP/M disks\n"
  ";           detaches all CP/M character devices\n"
  ";           attaches TTY to CONSOLE"
},

{ "SET", SET, "Set memory values.",
  "SET [-R] <ADDRESS> [<HEXVALUE>] ; sets memory values\n"
  ";Note: Use -R to access the ROM directly.\n"
  ";Note: If <HEXVALUE> is two or less digits, a byte is written.\n"
  ";      If <HEXVALUE> is three or more digits, a word is written."
},

{ "SYSID", SYSID, "Access the system ID device.",
  "SYSID [ <NEW VALUE> ]  ; shows or sets the System ID"
},

{ "TRACE", TRACE, "Trace instruction execution.",
  "TRACE [<COUNT>]                 ; trace instructions\n"
  "TRACE .                         ; disables tracing\n"
  "TRACE [<COUNT>] [<OUTPUT FILE>] ; write trace output to a file"
},

{ "UNMOUNT", UNMOUNT, "Unmount a disk.",
  "UNMOUNT <DISK>                  ; unmounts the file associated with <DISK>"
},

{ "UNLOAD", UNLOAD, "Unload " CPU " code to a file.",
  "UNLOAD [-S] [-R] [-C] <HEXFILE>  ; unloads 64K bytes of memory\n"
  ";Note: Use -C to change the output format to C language.\n"
  ";      Use -R to access the ROM directly.\n"
  ";      Use -S to supress zero records."
},

{ "UNTRACE", UNTRACE, "Untrace instruction execution.",
  "UNTRACE [<COUNT>]                 ; untrace instructions\n"
  "UNTRACE .                         ; disables tracing\n"
  "UNTRACE [<COUNT>] [<OUTPUT FILE>] ; write trace output to a file"
},

{ 0, 0, 0, 0 }

};

/* MonitorHelp() displays command documentation. */
static void MonitorHelp(char *command)
{
	int i;

	/* List available commands. */
	if (command == 0) {
		for (i = 0; gMonitorCommands[i].name != 0; i++) {
			printf("%10s", gMonitorCommands[i].name);
			if (gMonitorCommands[i].shortDocumentation != 0)
				printf(" -- %s",
				       gMonitorCommands[i].shortDocumentation);
			printf("\n");
			if (((i + 1) % (kConsoleMaxY - 1)) == 0) {
				printf("-more-");
				if (getchar() != ' ')
					break;
				printf("\r      \r");
			}
		}
	}
	/* Display documentation on a specific command. */
	else {
		for (i = 0; gMonitorCommands[i].name != 0; i++)
			if (!strcmp(gMonitorCommands[i].name, command))
				break;
		if (gMonitorCommands[i].name == 0)
			printf("?%s\n", command);
		else {
			if (gMonitorCommands[i].longDocumentation != 0) {
				if (gMonitorCommands[i].shortDocumentation != 0)
					puts(gMonitorCommands[i].shortDocumentation);
				puts("Usage:");
				puts(gMonitorCommands[i].longDocumentation);
			}
			else if (gMonitorCommands[i].shortDocumentation != 0) {
				puts("Usage:");
				printf("%s  ; %s\n",
				       command,
				       gMonitorCommands[i].shortDocumentation);
			}
			else {
				puts("Usage:");
				puts(gMonitorCommands[i].name);
			}
		}
	}

}

/* MonitorCommand() processes monitor commands. */
int MonitorCommand(const char *format, ...)
{
	va_list ap;
	char buffer[256];
	char *delimiters = " \t";
	char *argv[10];
	int argc;
	int i;
	int n;
	char *b;

	/* The system monitor is now active. */
	SetSystemFlags(kSystemMonitor, 0);
	gMonitorActive = 1;

	/* Get the next command. */
	printf(kConsoleCleanLine kConsoleColorSystem CPU ">");
	if (format != 0) {
		va_start(ap, format);
		vsnprintf(buffer, sizeof(buffer), format, ap);
		va_end(ap);
		printf("%s\n", buffer);
	}
	else {
		if (gCommandFile != 0) {
			if (fgets(buffer, sizeof(buffer), gCommandFile) == 0)
				CloseCommandFile();
			else {
				if ((b = strchr(buffer, '\r')) != 0)
					*b = 0;
				else if ((b = strchr(buffer, '\n')) != 0)
					*b = 0;
				printf("%s\n", buffer);
			}
		}
		if (gCommandFile == 0) {
			if (gets(buffer) == 0)
				strcpy(buffer, "QUIT");
		}
	}

	/* Convert command to uppercase. */
	strcasecpy(buffer, buffer);

	/* Remove any comments from the command buffer. */
	if ((b = strchr(buffer, ';')) != 0)
		*b = 0;

	/* Check for DDT commands. */
	(void)DDT(buffer);

	/* Prepare argc and argv. */
	argc = 0;
	if ((argv[argc] = strtok(buffer, delimiters)) != 0)
		while (argc < 10)
			if ((argv[++argc] = strtok(0, delimiters)) == 0)
				break;

	/* Search for and execute the command. */
	n = 0;
	if (argc > 0) {
		for (i = 0; gMonitorCommands[i].name != 0; i++)
			if (!strcmp(gMonitorCommands[i].name, argv[0]))
				break;
		if (gMonitorCommands[i].name == 0) {
			if (!OpenCommandFile(argv[0])) {
				printf("?%s\n", argv[0]);
				n = -1;
			}
		}
		else if ((n = gMonitorCommands[i].function(argc, argv)) != 0) {
			printf("EXIT(%d)\n", n);
			CloseCommandFile();
		}
	}

	/* The system monitor is now inactive. */
	printf(kConsoleColorReset);
	gMonitorActive = 0;

	/* Clear the kSystemBreak flag. */
	SetSystemFlags(0, kSystemBreak);

	/* Set the kSystemBreak flag if history is enabled. */
	if (gHistoryEnable > 0)
		SetSystemFlags(kSystemBreak, 0);

	/* Set the kSystemBreak flag if tracing is enabled. */
	if ((gTraceCount > 0) || (gTraceFile != 0))
		SetSystemFlags(kSystemBreak, 0);

	/* Set the kSystemBreak flag if breakpoints are enabled. */
	if ((gMaxBreak > 0) || (gMaxTBreak > 0))
		SetSystemFlags(kSystemBreak, 0);

	/* Exit the monitor if the trace count is non-zero. */
	if (gTraceCount > 0)
		SetSystemFlags(0, kSystemMonitor);

	/* All done, no error, return non-zero. */
	/* Return zero if there was an error. */
	return n == 0;

}

/* MonitorClose() terminates system monitor activity. */
void MonitorClose(void)
{

	if (gTraceFile != 0) {
		fclose(gTraceFile);
		gTraceFile = 0;
	}

}

/* MonitorFlags() performs system monitor activity. */
Byte MonitorFlags(CpuStatePtr s)
{
	int showCpuState = 0;

	/* Note the CPU State and the current PC. */
	gCpuStatePtr = s;
	gAddress = s->pc.word;

	/* Reset? */
	if (GetSystemFlags() & kSystemReset)
		MonitorCommand("RESET");

	/* Breakpoint? */
	if (GetSystemFlags() & kSystemBreak) {
		int i;
		unsigned hit = 0;
		/* Search for a Temporary BreakPoint match. */
		for (i = 0; i < gMaxTBreak; i++)
			if (gAddress == gTBreakAddress[i])
				hit++;
		/* Search for a BreakPoint match. */
		for (i = 0; i < gMaxBreak; i++)
			if (gAddress == gBreakAddress[i])
				hit++;
		/* Break if this is the last instruction to trace. */
		if ((gTraceCount > 0) && (--gTraceCount == 0))
			hit++;
		/* Enter the Monitor if a BreakPoint is Hit. */
		if (hit) {
			SystemMessage("*%04X\n", gAddress & 0xFFFF);
			SetSystemFlags(kSystemMonitor, 0);
			gMaxTBreak = 0;
		}
	}

	/* Monitor? */
    while (GetSystemFlags() & kSystemMonitor)
    	MonitorCommand(0);

	/* Trace? */
	if (GetSystemFlags() & kSystemBreak) {
		char buffer[256];
		int i;
		/* Collect state history if enabled... */
		if (gHistoryEnable > 0) {
			for (i = kMaxHistory - 1; i > 0; i--)
				gHistory[i] = gHistory[i - 1];
			gHistory[0] = *gCpuStatePtr;
			if (gHistoryEnable < kMaxHistory)
				gHistoryEnable++;
		}
		/* Display trace if enable... */
		if (gTraceDisplay) {
			DissasembleCpuState(buffer, gCpuStatePtr);
			SystemMessage("%s\n", buffer);
		}
		/* Output trace to a file if enabled... */
		if (gTraceFile != 0) {
			DissasembleCpuState(buffer, gCpuStatePtr);
			fprintf(gTraceFile, "%s\n", buffer);
		}
	}

	/* All done, return non-zero if halting. */
	return GetSystemFlags() & kSystemHalt;

}
