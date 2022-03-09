/* uSim memory.h * Copyright (C) 2000, Tsurishaddai Williamson, tsuri@earthlink.net *  * This program is free software; you can redistribute it and/or * modify it under the terms of the GNU General Public License * as published by the Free Software Foundation; either version 2 * of the License, or (at your option) any later version. *  * This program is distributed in the hope that it will be useful, * but WITHOUT ANY WARRANTY; without even the implied warranty of * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the * GNU General Public License for more details. *  * You should have received a copy of the GNU General Public License * along with this program; if not, write to the Free Software * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. *//**********************************************************************/typedef unsigned char Byte;typedef unsigned short Word;typedef union {	struct {#ifdef BIG_ENDIAN		Byte high, low;#else		Byte low, high;#endif	} byte;	Word word;} WordBytes;#define	kMaxBank 4#define	kBankSize 16384extern Byte *gRdBank[kMaxBank];extern Byte *gWrBank[kMaxBank];extern unsigned long RomSize(void);extern Byte RdRom(Word address);extern void WrRom(Word address, Byte value);extern void ZeroRom(void);extern unsigned long RamSize(void);extern Byte RdRam(Word address);extern void WrRam(Word address, Byte value);extern void ZeroRam(void);extern unsigned MinRomBank(void);extern unsigned MaxRomBank(void);extern unsigned MinRamBank(void);extern unsigned MaxRamBank(void);extern unsigned RdBank(unsigned logical);extern unsigned WrBank(unsigned logical, unsigned physical);static inline Byte RdByte(Word address){	Word index = (Word)(address >> 14);	Word offset = (Word)(address & 0x3FFF);	return gRdBank[index][offset];}static inline void WrByte(Word address, Byte value){	Word index = (Word)(address >> 14);	Word offset = (Word)(address & 0x3FFF);	gWrBank[index][offset] = value;}static inline Byte *RwByte(Word address){	Word index = (Word)(address >> 14);	Word offset = (Word)(address & 0x3FFF);	Byte *valuePtr;	valuePtr = &gWrBank[index][offset];	*valuePtr = gRdBank[index][offset];	return valuePtr;}extern void	RdBytes(Byte *destination, Word sourceAddress, Word size);extern void	WrBytes(Word destinationAddress, Byte *source, Word size);extern void MemoryZero(void *memory, unsigned long size);extern int MemoryReset(void);extern void MemoryClose(void);extern int	MemoryOpen(Byte *rom,	           unsigned long maxRom,	           Byte *ram,	           unsigned long maxRam,	           Byte *bitBucket);extern void ShowPhysicalMemoryMap(Byte physicalBank);extern void ShowLogicalMemoryMap(Byte logicalBank);extern WordBytes gDMA;