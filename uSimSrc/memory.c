/* uSim memory.c
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

#include "memory.h"

/**********************************************************************/
#pragma mark *** MEMORY ***

#pragma mark struct Memory
static struct Memory {

	unsigned long maxRom;
	unsigned minRomBank;
	unsigned maxRomBank;
	Byte *rom;

	unsigned long maxRam;
	unsigned minRamBank;
	unsigned maxRamBank;
	Byte *ram;

	Byte *bitBucket;
	unsigned index[kMaxBank];

} gMemory;

Byte *gRdBank[kMaxBank];
Byte *gWrBank[kMaxBank];

/* RomSize() returns the size of the physical ROM. */
unsigned long RomSize(void)
{

	return gMemory.maxRom;

}

/* RdRom() reads directly from the physical ROM. */
Byte RdRom(Word address)
{

	return (address >= gMemory.maxRom) ? 0 : gMemory.rom[address];

}

/* WrRom() writes directly to the physical RAM. */
void WrRom(Word address, Byte value)
{

	if (address < gMemory.maxRom)
		gMemory.rom[address] = value;

}

/* ZeroRom() zeros the physical ROM. */
void ZeroRom(void)
{

	MemoryZero(gMemory.rom, gMemory.maxRom);

}

/* RamSize() returns the size of the physical RAM. */
unsigned long RamSize(void)
{

	return gMemory.maxRam;

}

/* RdRam() reads directly from the physical RAM. */
Byte RdRam(Word address)
{

	return (address >= gMemory.maxRam) ? 0 : gMemory.ram[address];

}

/* WrRam() writes directly to the physical RAM. */
void WrRam(Word address, Byte value)
{

	if (address < gMemory.maxRam)
		gMemory.ram[address] = value;

}

/* ZeroRam() zeros the physical RAM. */
void ZeroRam(void)
{

	MemoryZero(gMemory.ram, gMemory.maxRam);

}

/* MinRomBank() returns the minimum ROM bank index. */
/* MinRomBank() <= romBank < MaxRomBank() */
unsigned MinRomBank(void)
{

	return gMemory.minRomBank;

}

/* MaxRomBank() returns the maximum ROM bank index. */
/* MinRomBank() <= romBank < MaxRomBank() */
unsigned MaxRomBank(void)
{

	return gMemory.maxRomBank;

}

/* MinRamBank() returns the minimum RAM bank index. */
/* MinRamBank() <= ramBank < MaxRamBank() */
unsigned MinRamBank(void)
{

	return gMemory.minRamBank;

}

/* MaxRamBank() returns the maximum RAM bank index. */
/* MinRamBank() <= ramBank < MaxRamBank() */
unsigned MaxRamBank(void)
{

	return gMemory.maxRamBank;

}

/* RdBank() returns the physical bank index. */
/* 0 <= logicalBank < kMaxBank */
unsigned RdBank(unsigned logical)
{

	/* 0 <= logicalBank < kMaxBank */
	if (logical < 0)
		goto error;
	if (logical >= kMaxBank)
		goto error;

	/* All done, no error, return the physical bank index. */
	return gMemory.index[logical];

	/* Return an invalid physical index if there was an error. */
error:
	return gMemory.maxRamBank;

}

/* WrBank() sets the physical bank index. */
/* 0 <= logicalBank < kMaxBank */
unsigned WrBank(unsigned logical, unsigned physical)
{

	/* 0 <= logicalBank < kMaxBank */
	if (logical < 0)
		goto error;
	if (logical >= kMaxBank)
		goto error;

	/* MinRomBank() <= physicalBank < MaxRamBank() */
	if (physical < 0)
		goto error;
	if (physical >= gMemory.maxRamBank)
		goto error;

	/* Set the physical bank index. */
	gMemory.index[logical] = physical;

	/* If the physical bank is ROM... */
	if (physical < gMemory.minRamBank) {
		/* then set the read bank to ROM... */
		gRdBank[logical] =
			&gMemory.rom[(physical - gMemory.minRomBank) * kBankSize];
		/* and set the write bank to the bit bucket. */
		gWrBank[logical] = gMemory.bitBucket;
	}
	/* If the physical bank is RAM... */
	else {
		/* then set the read bank to RAM... */
		gRdBank[logical] = 
			&gMemory.ram[(physical - gMemory.minRamBank) * kBankSize];
		/* and set the write bank to RAM. */
		gWrBank[logical] = gRdBank[logical];
	}

	/* All done, no error, return the physical bank index. */
	return physical;

	/* Return an invalid physical index if there was an error. */
error:
	return gMemory.maxRamBank;

}

#pragma mark RdByte
/* RdByte() reads a byte of memory. */
static inline Byte RdByte(Word address);

#pragma mark WrByte
/* WrByte() writes a byte of memory. */
static inline void WrByte(Word address, Byte value);

#pragma mark RwByte
/* RwByte() returns a pointer to read and write a byte of memory. */
static inline Byte *RwByte(Word address);

/* RdBytes() reads many bytes of memory. */
void RdBytes(Byte *destination, Word sourceAddress, Word size)
{

	while (size--)
		*destination++ = RdByte(sourceAddress++);

}

/* WrBytes() writes many bytes of memory. */
void WrBytes(Word destinationAddress, Byte *source, Word size)
{

	while (size--)
		WrByte(destinationAddress++, *source++);

}

/* MemoryZero() zeros physical memory. */
void MemoryZero(void *memory, unsigned long size)
{
	char *byte = (char *)memory;

	while (size--)
		*byte++ = 0;

}

/* MemoryReset() resets the memory system to default values. */
int MemoryReset(void)
{
	unsigned n;

	/* Error if not open. */
	if (gMemory.rom == 0)
		goto error;
	if (gMemory.bitBucket == 0)
		goto error;
	if (gMemory.ram == 0)
		goto error;

	/* Do not zero the ROM. */

	/* Zero the Bit Bucket. */
	MemoryZero(gMemory.bitBucket, kBankSize);

	/* Zero the RAM. */
	MemoryZero(gMemory.ram, gMemory.maxRam);

	/* Map in the ROM. */
	for (n = 0; n < kMaxBank; n++)
		WrBank(n, 0);

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* MemoryClose() terminates the memory system. */
void MemoryClose(void)
{

	/* Disconnect the ROM. */
	gMemory.maxRom = 0;
	gMemory.minRomBank = 0;
	gMemory.maxRomBank = 0;
	gMemory.rom = 0;

	/* Disconnect the Bit Bucket. */
	gMemory.bitBucket = 0;

	/* Disconnect the RAM. */
	gMemory.maxRam = 0;
	gMemory.minRamBank = 0;
	gMemory.maxRamBank = 0;
	gMemory.ram = 0;

}

/* MemoryOpen() prepares the memory system for normal operation. */
int
	MemoryOpen(Byte *rom,
	           unsigned long maxRom,
	           Byte *ram,
	           unsigned long maxRam,
	           Byte *bitBucket)
{
	WordBytes test;

	/* Insure that BIG_ENDIAN is defined properly. */
	test.word = 0x1234;
	if ((test.byte.low != 0x34) || (test.byte.high != 0x12))
		goto error;

	/* Prepare the physical ROM. */
	if (rom == 0)
		goto error;
	gMemory.rom = rom;
	if ((maxRom % kBankSize) != 0)
		goto error;
	gMemory.maxRom = maxRom;
	gMemory.minRomBank = 0;
	gMemory.maxRomBank =
		gMemory.minRomBank + (gMemory.maxRom / kBankSize);

	/* Prepare the physical RAM. */
	if (ram == 0)
		goto error;
	gMemory.ram = ram;
	if ((maxRam % kBankSize) != 0)
		goto error;
	gMemory.maxRam = maxRam;
	gMemory.minRamBank = gMemory.maxRomBank;
	gMemory.maxRamBank =
		gMemory.minRamBank + (gMemory.maxRam / kBankSize);

	/* Prepare the bit bucket. */
	if (bitBucket == 0)
		goto error;
	gMemory.bitBucket = bitBucket;

	/* Reset the memory system. */
	if (!MemoryReset())
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	MemoryClose();
	return 0;

}

/* ShowPhysicalMemoryMap() displays the physical memory map. */
void ShowPhysicalMemoryMap(Byte physicalBank)
{
	int isRom = physicalBank < MaxRomBank();
	unsigned long firstPhysicalAddress =
		(physicalBank - (isRom ? 0 : MaxRomBank())) * kBankSize;
	unsigned long lastPhysicalAddress =
		firstPhysicalAddress + kBankSize - 1;
	Byte logicalBank;

	printf("16K %s %d: %08X - %08X",
	       isRom ? "ROM" : "RAM",
	       physicalBank,
	       firstPhysicalAddress,
	       lastPhysicalAddress);

	for (logicalBank = 0; logicalBank < kMaxBank; logicalBank++) {
		if (RdBank(logicalBank) == physicalBank) {
			printf(" => BANK");
			for (logicalBank = 0; logicalBank < kMaxBank; logicalBank++)
				if (RdBank(logicalBank) == physicalBank)
					printf(" %d", logicalBank);
		}
	}

	printf("\n");

}

/* ShowLogicalMemoryMap() displays the logical memory map. */
void ShowLogicalMemoryMap(Byte logicalBank)
{
	Word firstLogicalAddress = logicalBank * kBankSize;
	Word lastLogicalAddress = firstLogicalAddress + kBankSize - 1;
	Byte physicalBank = RdBank(logicalBank);
	int isRom = physicalBank < MaxRomBank();
	unsigned long firstPhysicalAddress =
		(physicalBank - (isRom ? 0 : MaxRomBank())) * kBankSize;
	unsigned long lastPhysicalAddress =
		firstPhysicalAddress + kBankSize - 1;

	printf("16K BANK %d: %04X - %04X => 16K %s %d: %08X - %08X\n",
	       logicalBank,
	       firstLogicalAddress,
	       lastLogicalAddress,
	       isRom ? "ROM" : "RAM",
	       physicalBank,
	       firstPhysicalAddress,
	       lastPhysicalAddress);

}

/**********************************************************************/
#pragma mark *** DMA ***

WordBytes gDMA;
