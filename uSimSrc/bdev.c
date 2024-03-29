/* uSim bdev.c
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
#include "string.h"
#include "file.h"
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>

#include "memory.h"
#include "system.h"
#include "cpu.h"
#include "bdev.h"
#include "monitor.h"
#include "dir.h"
#include "hostfile.h"

/**********************************************************************/
#pragma mark *** BLOCK DEVICE ***

#define kEmptyByte 0xE5

/* Parameters for a "standard" 256K floppy disk. */
#define SSSIZ 256256
#define SSTPD 77
#define SSOFF 2
#define SSSKF 6
#define SSSPT 26
#define SSBLS 1024
#define SSDRM 63

/* Parameters for a "standard" 5MB hard disk. */
#define HDSIZ 5242880
#define HDTPD 640
#define HDOFF 0
#define HDSKF 1
#define HDSPT 64
#define HDBLS 2048
#define HDDRM 1023

/* CP/M Disk/Directory parameters. */
#pragma mark struct BDevParameterBlock
struct BDevParameterBlock {
	unsigned long siz;
	unsigned long spd;
	WordBytes tpd;
	WordBytes spt;
	WordBytes dsm;
	WordBytes drm;
	WordBytes dsz;
	WordBytes bls;
	WordBytes bsh;
	WordBytes blm;
	WordBytes exm;
	WordBytes dbl;
	WordBytes alb;
	WordBytes cks;
	WordBytes off;
	WordBytes alv;
	WordBytes skf;
	Byte xlt[kMaxSPT];
};

typedef int (*BDevWriteFunction)(BDevPtr, unsigned long, Byte *);
typedef int (*BDevReadFunction)(BDevPtr, unsigned long, Byte *);
typedef void (*BDevCloseFunction)(BDevPtr);

/* Block Device State Structure. */
#pragma mark struct BDev
struct BDev {
	const char bDev[3];
	const Byte index;
	char name[kMaxFileName];
	int isOpen;
	int isReadOnly;
	Word dpAddress;
	WordBytes xltAddress;
	WordBytes alvAddress;
	WordBytes csvAddress;
	WordBytes pbAddress;
	BDevParameterBlock pb;
	Byte alv[kMaxALV];
	Byte csv[kMaxCKS];
	BDevWriteFunction bDevWrite;
	BDevReadFunction bDevRead;
	BDevCloseFunction bDevClose;
	void *cookie;
};

/* Instantiation of all 16 Block Devices. */
#pragma mark gBDev[]
static BDev gBDev[kMaxBDev] = {
	{ "A:", 0x0 },
	{ "B:", 0x1 },
	{ "C:", 0x2 },
	{ "D:", 0x3 },
	{ "E:", 0x4 },
	{ "F:", 0x5 },
	{ "G:", 0x6 },
	{ "H:", 0x7 },
	{ "I:", 0x8 },
	{ "J:", 0x9 },
	{ "K:", 0xA },
	{ "L:", 0xB },
	{ "M:", 0xC },
	{ "N:", 0xD },
	{ "O:", 0xE },
	{ "P:", 0xF }
};

/* SolicitBDevName() solicits a block device name. */
static int SolicitBDevName(BDevPtr bDevPtr, const char *name)
{
	char defaultName[10];

	/* Prepare the default name.  Excample: "DISKA.DSK". */
	strcpy(defaultName, "DISK");
	strcpy(strchr(defaultName, 0), bDevPtr->bDev);
	strcpy(strchr(defaultName, ':'), ".DSK");

	/* Get the specified name. */
	strcpy(bDevPtr->name, name);

	/* Solicit a name if none was specified. */
	if (strlen(bDevPtr->name) == 0) {

		if (!gMonitorActive)
			printf(kConsoleCleanLine kConsoleColorSystem);

		printf(CPU ">MOUNT %s [%s]>", bDevPtr->bDev, defaultName);

		if (gets(bDevPtr->name) == 0)
			strcpy(bDevPtr->name, ".");

		if (!gMonitorActive)
			printf(kConsoleColorReset);

	}

	/* Use the default name if none is specified. */
	if (strlen(bDevPtr->name) == 0)
		strcpy(bDevPtr->name, defaultName);

	/* Error if name is ".". */
	if (!strcmp(bDevPtr->name, "."))
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* IsDir() checks if a file is really a directory. */
static int IsDir(const char *file)
{
	DirPtr dirPtr;

	/* Try to access a directory... */
	if ((dirPtr = DirOpen(file)) == 0)
		goto error;

	DirClose(dirPtr);

	/* All done, the file is a directory, return non-zero. */
	return 1;

	/* Return zero if the file is not a directory. */
error:
	return 0;

}

/* EmptySector() sets empty values to a sector. */
static void EmptySector(Byte *sector)
{
	unsigned i;

	i = kBDevSectorSize;
	while (i--)
		*sector++ = kEmptyByte;

}

/* gcd() computes the greates common divisor. */
static long gcd(long m, long n)
{

	if (n <= 0)
		return 1;

	for (;;) {
		int r = m % n;
		if (r == 0)
			break;
		m = n;
		n = r;
	}

	return n;

}

/* ComputeBDevParameters() computes CP/M block device parameters. */
static void ComputeBDevParameters(BDevParameterBlockPtr pb)
{
	long tpd;
	long spt;
	long spd;
	long bls;
	long drm;
	long off;
	long skf;
	long siz;
	long dsm;
	long bsh;
	long blm;
	long exm;
	long dsz;
	long dbl;
	long alb;
	long cks;
	long alv;
	long nxtsec;
	long nxtbas;
	long neltst;
	long nelts;
	unsigned i;

	/* Input parameters. */
	tpd = pb->tpd.word;
	spt = pb->spt.word;
	bls = pb->bls.word;
	drm = pb->drm.word;
	off = pb->off.word;
	skf = pb->skf.word;

	/* Compute the total number of sectors in the disk. */
	spd = tpd * spt;

	/* Compute the total number of bytes in the disk. */
	siz = spd * 128;

	/* Compute the total number of blocks in the disk. */
	dsm = ((siz - (off * spt * 128)) / bls) - 1;

	/* Compute the data allocation block shift. */
	bsh = bls - 1;
	bsh = ((bsh >> 1) & 0x5555) + (bsh & 0x5555);
	bsh = ((bsh >> 2) & 0x3333) + (bsh & 0x3333);
	bsh = ((bsh >> 4) & 0x0F0F) + (bsh & 0x0F0F);
	bsh = (bsh & 0xFF) + (bsh >> 8);
	bsh = bsh - 7;

	/* Compute the data allocation block mask. */
	blm = (bls / 128) - 1;

	/* Compute the extent mask. */
	exm = (dsm < 256) ? ((bls / 1024) - 1) : ((bls / 2048) - 1);

	/* Compute the number of bytes used for the directory. */
	dsz = (drm + 1) * 32;

	/* Compute the number of blocks used for the directory. */
	dbl = (dsz + bls - 1) / bls;

	/* Compute the directory allocation block mask. */
	alb = ~((1 << (16 - dbl)) - 1);

	/* Compute the size of the directory check vector. */
	cks = ((drm + 1) / 4);

	/* Compute the size of the data allocation vector. */
	alv = ((dsm / 8) + 1);

	/* Save the output parameters. */
	pb->siz = siz;
	pb->spd = spd;
	pb->dsm.word = dsm;
	pb->bsh.word = bsh;
	pb->blm.word = blm;
	pb->exm.word = exm;
	pb->dsz.word = dsz;
	pb->dbl.word = dbl;
	pb->alb.word = alb;
	pb->cks.word = cks;
	pb->alv.word = alv;

	/* Compute the sector translation table */
	nxtsec = 0;
	nxtbas = 0;
	neltst = spt / gcd(spt, skf);
	nelts = neltst;
	for (i = 0; i < spt; i++) {
		pb->xlt[i] = nxtsec + 1;
		nxtsec += skf;
		if (nxtsec >= spt)
			nxtsec -= spt;
		if (--nelts == 0) {
			nelts = neltst;
			nxtsec = ++nxtbas;
		}
	}

}

/**********************************************************************/
#pragma mark *** RAM DISK ***

/* Ram Disk State Structure. */
#pragma mark struct RamDisk
typedef struct RamDisk RamDisk;
typedef RamDisk *RamDiskPtr;
struct RamDisk {
	Byte ram[1];
};

/* RamDiskWrite() writes a sector to a Ram Disk. */
static int
	RamDiskWrite(BDevPtr bDevPtr, unsigned long sectorIndex, Byte *sector)
{
	RamDiskPtr ramDiskPtr = bDevPtr->cookie;
	unsigned long bytePosition;

	/* Compute the Byte Position. */
	bytePosition = sectorIndex * kBDevSectorSize;

	/* Write to the ram disk. */
	memcpy(&ramDiskPtr->ram[bytePosition], sector, kBDevSectorSize);

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* RamDiskRead() reads a sector from a Ram Disk. */
static int
	RamDiskRead(BDevPtr bDevPtr, unsigned long sectorIndex, Byte *sector)
{
	RamDiskPtr ramDiskPtr = bDevPtr->cookie;
	unsigned long bytePosition;

	/* Compute the Byte Position. */
	bytePosition = sectorIndex * kBDevSectorSize;

	/* Read from the ram disk. */
	memcpy(sector, &ramDiskPtr->ram[bytePosition], kBDevSectorSize);

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* RamDiskClose() terminates access to a Ram Disk. */
static void RamDiskClose(BDevPtr bDevPtr)
{
	RamDiskPtr ramDiskPtr = bDevPtr->cookie;

	/* Deallocate the Ram Disk State Structure. */
	if (ramDiskPtr != 0) {
		free(ramDiskPtr);
	}

	bDevPtr->cookie = 0;

}

/* RamDiskOpen() prepares access to a Ram Disk. */
static int RamDiskOpen(BDevPtr bDevPtr)
{
	RamDiskPtr ramDiskPtr;
	unsigned i;

	/* A Ram Disk can not be read-only. */
	if (bDevPtr->isReadOnly) {
		SystemMessage("?RDONLY [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}

	/* Allocate the Ram Disk State Structure. */
	bDevPtr->cookie = ramDiskPtr =
		malloc(sizeof(RamDisk) - 1 + SSSIZ);
	if (ramDiskPtr == 0) {
		SystemMessage("?MALLOC [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}

	/* Format the ramdisk. */
	for (i = 0; i < SSSIZ; i++)
		ramDiskPtr->ram[i] = kEmptyByte;

	/* Prepare the standard parameters. */
	bDevPtr->pb.tpd.word = SSTPD;
	bDevPtr->pb.spt.word = SSSPT;
	bDevPtr->pb.bls.word = SSBLS;
	bDevPtr->pb.drm.word = SSDRM;
	bDevPtr->pb.off.word = SSOFF;
	bDevPtr->pb.skf.word = SSSKF;
	ComputeBDevParameters(&bDevPtr->pb);

	/* Set the write, read and close functions. */
	bDevPtr->bDevWrite = RamDiskWrite;
	bDevPtr->bDevRead = RamDiskRead;
	bDevPtr->bDevClose = RamDiskClose;

	/* The block device is open. */
	bDevPtr->isOpen = 1;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	RamDiskClose(bDevPtr);
	return 0;

}

/**********************************************************************/
#pragma mark *** DIRECTORY DISK ***

#define kMaxDirectoryCache 3

#pragma mark struct DirectoryCache
typedef struct DirectoryCache DirectoryCache;
typedef DirectoryCache *DirectoryCachePtr;
struct DirectoryCache {
	FILE *file;
	unsigned long firstSector;
	unsigned long lastSector;
	int reference;
};

/* Directory Disk State Structure. */
#pragma mark struct DirectoryDisk
typedef struct DirectoryDisk DirectoryDisk;
typedef DirectoryDisk *DirectoryDiskPtr;
struct DirectoryDisk {
	char pathPrefix[kSizeofFilePath];
	DirectoryCache directoryDiskCache[kMaxDirectoryCache];
	BDevParameterBlockPtr pb;
	Byte fcb[1];
};

/* DirectoryCacheLoad() loads a Directory Cache Entry. */
static DirectoryCachePtr
	DirectoryCacheLoad(DirectoryDiskPtr directoryDiskPtr,
	                   unsigned long sectorIndex)
{
	char fileName[kSizeofFileName];
	char path[kSizeofFilePath];
	Byte *fcb;
	int sectorsPerDisk = directoryDiskPtr->pb->spd;
	int sectorsPerBlock = directoryDiskPtr->pb->bls.word / 128;
	int maxFCB = directoryDiskPtr->pb->drm.word + 1;
	int i;
	int j;
	unsigned long blockNumber;
	unsigned long firstSector;
	unsigned long lastSector;
	DirectoryCachePtr directoryCachePtr;

	/* Pick the least used Directory Cache Entry. */
	for (i = 0; i < kMaxDirectoryCache; i++) {
		directoryCachePtr =
			&directoryDiskPtr->directoryDiskCache[i];
		if (directoryCachePtr->reference == 0)
			break;
	}

	/* Close any existing file access. */
	if (directoryCachePtr->file != 0)
		fclose(directoryCachePtr->file);

	/* Convert the sector index to a block number. */
	blockNumber = sectorIndex / sectorsPerBlock;

	/* Determine the file name. */
	for (i = 0; i < maxFCB; i++) {
		fcb = &directoryDiskPtr->fcb[i * 32];
		for (j = 16; j < 32; j++)
			if (fcb[j] == blockNumber)
				break;
		if (j < 32)
			break;
	}
	strcpy(fileName, FileNameFromFCB((FileFCB *)fcb));

	/* Determine the first and last sector numbers. */
	directoryCachePtr->firstSector = sectorsPerDisk;
	directoryCachePtr->lastSector = 0;
	for (i = 0; i < maxFCB; i++) {
		if (strcmp(fileName, FileNameFromFCB((FileFCB *)fcb)))
			continue;
		for (j = 16; j < 32; j++) {
			blockNumber = fcb[j];
			if (blockNumber == 0)
				break;
			firstSector = blockNumber * sectorsPerBlock;
			lastSector = firstSector + sectorsPerBlock - 1;
			if (firstSector < directoryCachePtr->firstSector)
				directoryCachePtr->firstSector = firstSector;
			if (lastSector > directoryCachePtr->lastSector)
				directoryCachePtr->lastSector = lastSector;
		}
	}

	/* Build the file path. */
	strcpy(path, directoryDiskPtr->pathPrefix);
	strcat(path, fileName);

	/* Open the file. */
	directoryCachePtr->file = fopen(path, "rb");
	if (directoryCachePtr->file == 0)
		goto error;

	/* All done, no error, return the Directory Cache Pointer. */
	return directoryCachePtr;

	/* Return zero if there was an error. */
error:
	return 0;


}

/* DirectoryCacheReference() references a Directory Cache Entry. */
static DirectoryCachePtr
	DirectoryCacheReference(DirectoryDiskPtr directoryDiskPtr,
	                        unsigned long sectorIndex)
{
	DirectoryCachePtr directoryCachePtr;
	int i;

	/* Search for the Directory Cache Entry. */
	for (i = 0; i < kMaxDirectoryCache; i++) {
		directoryCachePtr = &directoryDiskPtr->directoryDiskCache[i];
		if (sectorIndex < directoryCachePtr->firstSector)
			continue;
		else if (sectorIndex > directoryCachePtr->lastSector)
			continue;
		else if (directoryCachePtr->file == 0)
			continue;
		else
			break;
	}

	/* Open a new Directory Cache Entry if not found. */
	if (i >= kMaxDirectoryCache) {
		directoryCachePtr =
			DirectoryCacheLoad(directoryDiskPtr, sectorIndex);
		if (directoryCachePtr == 0)
			goto error;
	}

	/* Update The Directory Cache Reference. */
	for (i = 0; i < kMaxDirectoryCache; i++) {
		DirectoryCachePtr x =
			&directoryDiskPtr->directoryDiskCache[i];
		/* Reduce any refernce more recent than this one. */
		if (x->reference > directoryCachePtr->reference)
			x->reference--;
	}
	directoryCachePtr->reference = kMaxDirectoryCache - 1;

	/* All done, no error, return the Directory Cache Pointer. */
	return directoryCachePtr;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* DirectoryCacheRead() reads from a Directory Cache Entry. */
static int
	DirectoryCacheRead(DirectoryCachePtr directoryCachePtr,
	                   unsigned long sectorIndex,
	                   Byte *sector)
{
	unsigned long bytePosition;
	int i;
	int n;

	/* Compute the Byte Position. */
	bytePosition = directoryCachePtr->firstSector * kBDevSectorSize;
	bytePosition += sectorIndex * kBDevSectorSize;

	/* Seek to the specified position. */
	if (fseek(directoryCachePtr->file, bytePosition, 0) != 0)
		goto error;

	/* Read from the file. */
	n = fread(sector, 1, kBDevSectorSize, directoryCachePtr->file);

	/* Fill the unused portion of the sector with Control-Z's. */
	for (i = n; i < kBDevSectorSize; i++)
		sector[i] = 'Z' - '@';

	/* Error if the read failed. */
	if (n == 0)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* DirectoryDiskRead() reads a sector from a Directory Disk. */
static int
	DirectoryDiskRead(BDevPtr bDevPtr,
	                  unsigned long sectorIndex,
	                  Byte *sector)
{
	DirectoryDiskPtr directoryDiskPtr = bDevPtr->cookie;
	unsigned long bytePosition;
	DirectoryCachePtr directoryCachePtr;

	/* Compute the Byte Position. */
	bytePosition = sectorIndex * kBDevSectorSize;

	/* Read from the FCB array. */
	if ((bytePosition + kBDevSectorSize - 1) < bDevPtr->pb.dsz.word)
		memcpy(sector,
		       &directoryDiskPtr->fcb[bytePosition],
		       kBDevSectorSize);
	/* Otherwise read from a file. */
	else {
		/* Refeerenc the Directory Cache Entry. */
		directoryCachePtr =
			DirectoryCacheReference(directoryDiskPtr, sectorIndex);
		if (directoryCachePtr == 0)
			goto error;
		/* Read the sector from the Directory Cache. */
		if (!DirectoryCacheRead(directoryCachePtr, sectorIndex, sector))
			goto error;
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* DirectoryDiskClose() terminates access to a Directory Disk. */
static void DirectoryDiskClose(BDevPtr bDevPtr)
{
	DirectoryDiskPtr directoryDiskPtr = bDevPtr->cookie;
	DirectoryCachePtr directoryCachePtr;
	int i;

	/* Deallocate the Directory Disk State Structure. */
	if (directoryDiskPtr != 0) {
		for (i = 0; i < kMaxDirectoryCache; i++) {
			directoryCachePtr =
				&directoryDiskPtr->directoryDiskCache[i];
			if (directoryCachePtr->file != 0)
				fclose(directoryCachePtr->file);
		}
		free(directoryDiskPtr);
	}

	bDevPtr->cookie = 0;

}

/* DirectoryDiskOpen() prepares access to a Directory Disk. */
static int DirectoryDiskOpen(BDevPtr bDevPtr)
{
	DirPtr dirPtr = 0;
	DirEntryPtr dirEntryPtr;
	unsigned index;
	unsigned i;
	const char *name;
	const char *extension;
	DirectoryDiskPtr directoryDiskPtr;
	Byte *fcb;

	/* Directory disks are read-only. */
	bDevPtr->isReadOnly = 1;

	/* Prepare 5MB parameters. */
	bDevPtr->pb.tpd.word = HDTPD;
	bDevPtr->pb.off.word = HDOFF;
	bDevPtr->pb.skf.word = HDSKF;
	bDevPtr->pb.spt.word = HDSPT;
	bDevPtr->pb.bls.word = HDBLS;
	bDevPtr->pb.drm.word = HDDRM;
	ComputeBDevParameters(&bDevPtr->pb);

	/* Allocate the Directory Disk State Structure. */
	bDevPtr->cookie = directoryDiskPtr =
		malloc(sizeof(DirectoryDisk) - 1 + bDevPtr->pb.dsz.word);
	if (directoryDiskPtr == 0) {
		SystemMessage("?MALLOC [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}

	/* Set the parameter block pointer. */
	directoryDiskPtr->pb = &bDevPtr->pb;

	/* Clear the Directory Cache. */
	for (i = 0; i < kMaxDirectoryCache; i++) {
		DirectoryCachePtr directoryCachePtr =
			&directoryDiskPtr->directoryDiskCache[i];
		directoryCachePtr->file = 0;
		directoryCachePtr->firstSector = 0;
		directoryCachePtr->lastSector = 0;
	}

	/* Clear the FCB Array. */
	for (index = 0; index <= bDevPtr->pb.drm.word; index++) {
		fcb = &directoryDiskPtr->fcb[index * 32];
		for (i = 0; i < 32; i++)
			fcb[i] = kEmptyByte;
	}

	/* Build the FCB Array. */
	if ((dirPtr = DirOpenPath(bDevPtr->name)) == 0) {
		SystemMessage("?OPEN [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}
	index = 0;
	while ((dirEntryPtr = DirRead(dirPtr)) != 0) {
		strcpy(directoryDiskPtr->pathPrefix, dirEntryPtr->pathPrefix);
		if (!dirEntryPtr->isFile)
			continue;
		name = FileName(dirEntryPtr->name);
		for (i = 0; name[i] != 0; i++)
			if (name[i] <= ' ')
				break;
		if ((i >= 8) || (name[i] != 0))
			continue;
		extension = FileExtension(dirEntryPtr->name);
		for (i = 0; extension[i] != 0; i++)
			if (extension[i] <= ' ')
				break;
		if ((i >= 8) || (extension[i] != 0))
			continue;
		if (index > bDevPtr->pb.drm.word)
			goto error;
/* XXX Build the Directory Entry for this File. */
		fcb = &directoryDiskPtr->fcb[index++ * 32];
		fcb[0] = 0;
		for (i = 1; i < 9; i++)
			fcb[i] = (*name != 0) ? *name++ : ' ';
		for (i = 9; i < 12; i++)
			fcb[i] = (*extension != 0) ? *extension++ : ' ';
		fcb[12] = 0;
		fcb[13] = 0;
		fcb[14] = 0;
		fcb[15] = 0;
		for (i = 16; i < 32; i++)
			fcb[i] = 0;
	}
	DirClose(dirPtr);

	/* Set the write, read and close functions. */
	bDevPtr->bDevWrite = 0;
	bDevPtr->bDevRead = DirectoryDiskRead;
	bDevPtr->bDevClose = DirectoryDiskClose;

	/* The block device is open. */
	bDevPtr->isOpen = 1;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	DirectoryDiskClose(bDevPtr);
	return 0;

}

/**********************************************************************/
#pragma mark *** ASCII DISK ***

static char gAsciiDiskMagic[] =
	"This is a uSim CP/M read-only ASCII disk image.";

/* Ascii Disk State Structure. */
#pragma mark struct AsciiDisk
typedef struct AsciiDisk AsciiDisk;
typedef AsciiDisk *AsciiDiskPtr;
struct AsciiDisk {
	FILE *file;
	fpos_t *fpos;
};

/* AsciiDiskRead() reads a sector from a Ascii Disk. */
static int
	AsciiDiskRead(BDevPtr bDevPtr,
	             unsigned long sectorIndex,
	             Byte *sector)
{
	AsciiDiskPtr fileDiskPtr = bDevPtr->cookie;
	char buffer[256];
	char *b;
	char high;
	char low;
	Byte cksum;
	Byte *byte;
	int i;
	int j;

	/* Seek to the position in the Ascii Disk. */
	if (fsetpos(fileDiskPtr->file, &fileDiskPtr->fpos[sectorIndex]) != 0)
		goto error;

	/* Read from the Ascii Disk. */
	byte = sector;
	cksum = 0;
	for (i = 0; i < 4; i++) {
		if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
			goto error;
		b = buffer;
		for (j = 0; j < 32; j++) {
			if ((*b >= '0') && (*b <= '9'))
				high = *b - '0';
			else if ((*b >= 'A') && (*b <= 'F'))
				high = 10 + *b - 'A';
			else
				goto formatError;
			b++;
			if ((*b >= '0') && (*b <= '9'))
				low = *b - '0';
			else if ((*b >= 'A') && (*b <= 'F'))
				low = 10 + *b - 'A';
			else
				goto formatError;
			b++;
			*byte = (high << 4) | low;
			cksum += *byte++;
		}
	}
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		goto formatError;
	b = buffer;
	if ((*b >= '0') && (*b <= '9'))
		high = *b - '0';
	else if ((*b >= 'A') && (*b <= 'F'))
		high = 10 + *b - 'A';
	else
		goto formatError;
	b++;
	if ((*b >= '0') && (*b <= '9'))
		low = *b - '0';
	else if ((*b >= 'A') && (*b <= 'F'))
		low = 10 + *b - 'A';
	else
		goto formatError;
	b++;
	cksum += (high << 4) | low;
	if (cksum != 0)
		goto cksumError;

	/* All done, no error, return non-zero. */
	return 1;

formatError:
	SystemMessage("?FORMAT\n");
	goto error;

cksumError:
	SystemMessage("?CKSUM\n");
	goto error;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* AsciiDiskClose() terminates access to a Ascii Disk. */
static void AsciiDiskClose(BDevPtr bDevPtr)
{
	AsciiDiskPtr fileDiskPtr = bDevPtr->cookie;

	/* Deallocate the Ascii Disk State Structure. */
	if (fileDiskPtr != 0) {
		if (fileDiskPtr->file != 0)
			fclose(fileDiskPtr->file);
		if (fileDiskPtr->fpos != 0)
			free(fileDiskPtr->fpos);
		free(fileDiskPtr);
	}

	bDevPtr->cookie = 0;

}

/* AsciiDiskOpen() prepares access to a Ascii Disk. */
static int AsciiDiskOpen(BDevPtr bDevPtr)
{
	AsciiDiskPtr fileDiskPtr = bDevPtr->cookie;
	char buffer[256];
	int tpd;
	int spt;
	int bls;
	int drm;
	int off;
	int skf;
	int track;
	int sector;
	int n;

	/* Allocate the Ascii Disk State Structure. */
	bDevPtr->cookie = fileDiskPtr =
		malloc(sizeof(AsciiDisk));
	if (fileDiskPtr == 0) {
		SystemMessage("?MALLOC [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}
	fileDiskPtr->fpos = 0;

	/* Try to open the file read-only. */
	fileDiskPtr->file = FOpenPath(bDevPtr->name, "r");

	/* Error if the fopen() failed. */
	if (fileDiskPtr->file == 0) {
		SystemMessage("?OPEN [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}

	/* Check the magic. */
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		goto error;
	if (strncmp(buffer, gAsciiDiskMagic, strlen(gAsciiDiskMagic)))
		goto error;
	/* Read the header, prepare the parameters. */
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		n = 0;
	else
		n = sscanf(buffer, "TPD=%d SPT=%d BLS=%d DRM=%d OFF=%d SKF=%d",
		           &tpd,
		           &spt,
		           &bls,
		           &drm,
		           &off,
		           &skf);
	if (n != 6) {
		SystemMessage("?PARAMETERS [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}
	bDevPtr->pb.tpd.word = tpd;
	bDevPtr->pb.spt.word = spt;
	bDevPtr->pb.bls.word = bls;
	bDevPtr->pb.drm.word = drm;
	bDevPtr->pb.off.word = off;
	bDevPtr->pb.skf.word = skf;
	ComputeBDevParameters(&bDevPtr->pb);

	/* Allocate the fpos array. */
	fileDiskPtr->fpos = malloc(sizeof(fpos_t) * bDevPtr->pb.spd);
	if (fileDiskPtr->fpos == 0) {
		SystemMessage("?MALLOC [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}

	/* Load the fpos array. */
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		goto formatError;
	if (strncmp(buffer, "EMPTY SECTOR", 12))
		goto formatError;
	if (fgetpos(fileDiskPtr->file, &fileDiskPtr->fpos[0]) != 0)
		goto formatError;
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		goto formatError;
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		goto formatError;
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		goto formatError;
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		goto formatError;
	if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
		goto formatError;
	for (n = 0; n < bDevPtr->pb.spd; n++)
		fileDiskPtr->fpos[n] = fileDiskPtr->fpos[0];
	while (fgets(buffer, sizeof(buffer), fileDiskPtr->file) != 0) {
		if (sscanf(buffer, "TRACK %d, SECTOR %d", &track, &sector) != 2)
			goto formatError;
		n = (track * bDevPtr->pb.spt.word) + sector;
		if (n >= bDevPtr->pb.spd)
			goto formatError;
		if (fgetpos(fileDiskPtr->file, &fileDiskPtr->fpos[n]) != 0)
			goto formatError;
		if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
			goto formatError;
		if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
			goto formatError;
		if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
			goto formatError;
		if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
			goto formatError;
		if (fgets(buffer, sizeof(buffer), fileDiskPtr->file) == 0)
			goto formatError;
	}

	/* Set the write, read and close functions. */
	bDevPtr->bDevWrite = 0;
	bDevPtr->bDevRead = AsciiDiskRead;
	bDevPtr->bDevClose = AsciiDiskClose;

	/* The block device is open, read-only. */
	bDevPtr->isReadOnly = 1;
	bDevPtr->isOpen = 1;

	/* All done, no error, return non-zero. */
	return 1;

formatError:
	SystemMessage("?FORMAT [%s => %s]\n",
	              bDevPtr->bDev,
	              bDevPtr->name);
	goto error;

	/* Return zero if there was an error. */
error:
	AsciiDiskClose(bDevPtr);
	return 0;

}

/* AsciiDiskCopy() copies a Ascii DIsk. */
int AsciiDiskCopy(BDevPtr bDevPtr, const char *fileName)
{
	Byte sector[kBDevSectorSize];
	Byte emptySector[kBDevSectorSize];
	FILE *f;
	Word trackNumber;
	Word sectorNumber;
	int i;
	Byte cksum;

	/* Error if the block device is not open. */
	if (!bDevPtr->isOpen)
		goto error;

	/* Does the file already exist? */
	printf("COPYING %s [%s] (R/O) %dK...\n",
	       bDevPtr->bDev,
	       fileName,
	       bDevPtr->pb.siz / 1024);
	if ((f = fopen(fileName, "r")) != 0) {
		printf("?EXISTS: [%s]\n", fileName);
		fclose(f);
	}

	/* Solicit OK to continue... */
	printf("CONTINUE (Y/N)?");
	switch (ConsoleInput(10)) {
	case 'Y':
	case 'y':
		printf(" Y\n");
		break;
	default:
		printf(" N\n");
		goto error;
	}

	/* Open the output file. */
	if ((f = fopen(fileName, "w")) == 0) {
		printf("?ERROR: [%s]\n", fileName);
		goto error;
	}

	/* Write the header. */
	fprintf(f, "%s\n", gAsciiDiskMagic);
	fprintf(f, "TPD=%ld SPT=%ld BLS=%ld DRM=%ld OFF=%ld SKF=%ld\n",
		        bDevPtr->pb.tpd.word,
		        bDevPtr->pb.spt.word,
		        bDevPtr->pb.bls.word,
		        bDevPtr->pb.drm.word,
		        bDevPtr->pb.off.word,
		        bDevPtr->pb.skf.word);
	/* Write an empty sector. */
	EmptySector(emptySector);
	fprintf(f, "EMPTY SECTOR\n");
	cksum = 0;
	for (i = 0; i < kBDevSectorSize; i++) {
		fprintf(f, "%02X", emptySector[i] & 0xFF);
		cksum += emptySector[i];
		if (((i + 1) % 32) == 0)
			fprintf(f, "\n");
	}
	fprintf(f, "%02X\n", (-cksum) & 0xFF);

	/* Write all the sectors on the disk. */
	for (trackNumber = 0;
	     trackNumber < bDevPtr->pb.tpd.word;
	     trackNumber++) {
		for (sectorNumber = 0;
		     sectorNumber < bDevPtr->pb.spt.word;
		     sectorNumber++) {
			if (!BDevRead(bDevPtr, trackNumber, sectorNumber, sector))
				goto error;
			if (memcmp(sector, emptySector, kBDevSectorSize) != 0) {
				fprintf(f, "TRACK %d, SECTOR %d\n",
				        trackNumber,
				        sectorNumber);
				cksum = 0;
				for (i = 0; i < kBDevSectorSize; i++) {
					fprintf(f, "%02X", sector[i] & 0xFF);
					cksum += sector[i];
					if (((i + 1) % 32) == 0)
						fprintf(f, "\n");
				}
				fprintf(f, "%02X\n", (-cksum) & 0xFF);
			}
		}
	}

	/* Close the output file. */
	if (fclose(f) != 0) {
		printf("?CLOSE\n");
		f = 0;
		goto error;
	}

	/* The format is complete. */
	printf("COPY COMPLETE.\n");

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	if (f != 0)
		fclose(f);
	return 0;

}

/**********************************************************************/
#pragma mark *** FILE DISK ***

static char gFileDiskMagic[] =
	"This is a uSim CP/M read/write BINARY disk image."
	"\015\012\012";

/* A File Disk begins with a header. */
#pragma mark struct FileDiskHeader
typedef struct FileDiskHeader FileDiskHeader;
typedef FileDiskHeader *FileDiskHeaderPtr;
struct FileDiskHeader {
	char magic[sizeof(gFileDiskMagic)];
	char info[kBDevSectorSize
	          - (sizeof(gFileDiskMagic)
	             + 2
	             + 2
	             + 2
	             + 2
	             + 2
	             + 2)];
	Byte tpd[2];
	Byte spt[2];
	Byte bls[2];
	Byte drm[2];
	Byte off[2];
	Byte skf[2];
};

/* File Disk State Structure. */
#pragma mark struct FileDisk
typedef struct FileDisk FileDisk;
typedef FileDisk *FileDiskPtr;
struct FileDisk {
	FILE *file;
	long headerOffset;
};

/* FileDiskWrite() writes a sector to a File Disk. */
static int
	FileDiskWrite(BDevPtr bDevPtr,
	              unsigned long sectorIndex,
	              Byte *sector)
{
	FileDiskPtr fileDiskPtr = bDevPtr->cookie;
	unsigned long bytePosition;

	/* Compute the Byte Position. */
	bytePosition = fileDiskPtr->headerOffset;
	bytePosition += sectorIndex * kBDevSectorSize;

	/* Seek to the position in the File Disk. */
	if (fseek(fileDiskPtr->file, bytePosition, 0) != 0)
		goto error;

	/* Write to the File Disk. */
	if (fwrite(sector, kBDevSectorSize, 1, fileDiskPtr->file) != 1)
		goto error;

	/* Flush the File Disk. */
	fflush(fileDiskPtr->file);

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* FileDiskRead() reads a sector from a File Disk. */
static int
	FileDiskRead(BDevPtr bDevPtr,
	             unsigned long sectorIndex,
	             Byte *sector)
{
	FileDiskPtr fileDiskPtr = bDevPtr->cookie;
	unsigned long bytePosition;

	/* Compute the Byte Position. */
	bytePosition = fileDiskPtr->headerOffset;
	bytePosition += sectorIndex * kBDevSectorSize;

	/* Seek to the position in the File Disk. */
	if (fseek(fileDiskPtr->file, bytePosition, 0) != 0)
		goto error;

	/* Read from the File Disk. */
	if (fread(sector, kBDevSectorSize, 1, fileDiskPtr->file) != 1)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* FileDiskClose() terminates access to a File Disk. */
static void FileDiskClose(BDevPtr bDevPtr)
{
	FileDiskPtr fileDiskPtr = bDevPtr->cookie;

	/* Deallocate the File Disk State Structure. */
	if (fileDiskPtr != 0) {
		if (fileDiskPtr->file != 0)
			fclose(fileDiskPtr->file);
		free(fileDiskPtr);
	}

	bDevPtr->cookie = 0;

}

/* FileDiskOpen() prepares access to a File Disk. */
static int FileDiskOpen(BDevPtr bDevPtr)
{
	FileDiskHeader header;
	FileDiskPtr fileDiskPtr = bDevPtr->cookie;

	/* Insure that the header size is correct. */
	if (sizeof(FileDiskHeader) != kBDevSectorSize) {
		SystemMessage("?HEADER\n");
		goto error;
	}

	/* Allocate the File Disk State Structure. */
	bDevPtr->cookie = fileDiskPtr =
		malloc(sizeof(FileDisk));
	if (fileDiskPtr == 0) {
		SystemMessage("?MALLOC [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}

	/* Try to open the file read/write. */
	if (!bDevPtr->isReadOnly) {
		fileDiskPtr->file = FOpenPath(bDevPtr->name, "r+b");
		if (fileDiskPtr->file == 0)
			bDevPtr->isReadOnly = 1;
	}

	/* Try to open the file read-only. */
	if (bDevPtr->isReadOnly)
		fileDiskPtr->file = FOpenPath(bDevPtr->name, "rb");

	/* Error if the fopen() failed. */
	if (fileDiskPtr->file == 0) {
		SystemMessage("?OPEN [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
	}

	/* Prepare standard parameters in case there is no header. */
	bDevPtr->pb.tpd.word = SSTPD;
	bDevPtr->pb.spt.word = SSSPT;
	bDevPtr->pb.bls.word = SSBLS;
	bDevPtr->pb.drm.word = SSDRM;
	bDevPtr->pb.off.word = SSOFF;
	bDevPtr->pb.skf.word = SSSKF;
	ComputeBDevParameters(&bDevPtr->pb);

	/* Set the write, read and close functions. */
	bDevPtr->bDevWrite = FileDiskWrite;
	bDevPtr->bDevRead = FileDiskRead;
	bDevPtr->bDevClose = FileDiskClose;

	/* The block device is open. */
	bDevPtr->isOpen = 1;

	/* Try to read a parameter header... */
	fileDiskPtr->headerOffset = 0;
	BDevRead(bDevPtr, 0, 0, (Byte *)&header);
	if (!strcmp(header.magic, gFileDiskMagic)) {
		bDevPtr->pb.tpd.byte.low = header.tpd[0];
		bDevPtr->pb.tpd.byte.high = header.tpd[1];
		bDevPtr->pb.spt.byte.low = header.spt[0];
		bDevPtr->pb.spt.byte.high = header.spt[1];
		bDevPtr->pb.bls.byte.low = header.bls[0];
		bDevPtr->pb.bls.byte.high = header.bls[1];
		bDevPtr->pb.drm.byte.low = header.drm[0];
		bDevPtr->pb.drm.byte.high = header.drm[1];
		bDevPtr->pb.off.byte.low = header.off[0];
		bDevPtr->pb.off.byte.high = header.off[1];
		bDevPtr->pb.skf.byte.low = header.skf[0];
		bDevPtr->pb.skf.byte.high = header.skf[1];
		fileDiskPtr->headerOffset = sizeof(header);
		ComputeBDevParameters(&bDevPtr->pb);
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	FileDiskClose(bDevPtr);
	return 0;

}

/* FileDiskCopy() copies a File DIsk. */
int FileDiskCopy(BDevPtr bDevPtr, const char *fileName)
{
	Byte sector[kBDevSectorSize];
	FileDiskHeader header;
	FILE *f;
	Word trackNumber;
	Word sectorNumber;

	/* Error if the block device is not open. */
	if (!bDevPtr->isOpen)
		goto error;

	/* Does the file already exist? */
	printf("COPYING %s [%s] (R/W) %dK...\n",
	       bDevPtr->bDev,
	       fileName,
	       bDevPtr->pb.siz / 1024);
	if ((f = fopen(fileName, "r")) != 0) {
		printf("?EXISTS: [%s]\n", fileName);
		fclose(f);
	}

	/* Solicit OK to continue... */
	printf("CONTINUE (Y/N)?");
	switch (ConsoleInput(10)) {
	case 'Y':
	case 'y':
		printf(" Y\n");
		break;
	default:
		printf(" N\n");
		goto error;
	}

	/* Open the output file. */
	if ((f = fopen(fileName, "wb")) == 0) {
		printf("?ERROR: [%s]\n", fileName);
		goto error;
	}

	/* Write the header. */
	MemoryZero(&header, kBDevSectorSize);
	strcpy(header.magic, gFileDiskMagic);
	sprintf(header.info,
	        "TPD=%ld SPT=%ld BLS=%ld DRM=%ld OFF=%ld SKF=%ld"
	        "\015\012\012",
	        bDevPtr->pb.tpd.word,
	        bDevPtr->pb.spt.word,
	        bDevPtr->pb.bls.word,
	        bDevPtr->pb.drm.word,
	        bDevPtr->pb.off.word,
	        bDevPtr->pb.skf.word);
	header.tpd[0] = bDevPtr->pb.tpd.byte.low;
	header.tpd[1] = bDevPtr->pb.tpd.byte.high;
	header.spt[0] = bDevPtr->pb.spt.byte.low;
	header.spt[1] = bDevPtr->pb.spt.byte.high;
	header.bls[0] = bDevPtr->pb.bls.byte.low;
	header.bls[1] = bDevPtr->pb.bls.byte.high;
	header.drm[0] = bDevPtr->pb.drm.byte.low;
	header.drm[1] = bDevPtr->pb.drm.byte.high;
	header.off[0] = bDevPtr->pb.off.byte.low;
	header.off[1] = bDevPtr->pb.off.byte.high;
	header.skf[0] = bDevPtr->pb.skf.byte.low;
	header.skf[1] = bDevPtr->pb.skf.byte.high;
	if (fwrite(&header, sizeof(header), 1, f) != 1) {
		printf("?WRITE\n");
		goto error;
	}

	/* Write all the sectors on the disk. */
	for (trackNumber = 0;
	     trackNumber < bDevPtr->pb.tpd.word;
	     trackNumber++) {
		for (sectorNumber = 0;
		     sectorNumber < bDevPtr->pb.spt.word;
		     sectorNumber++) {
			if (!BDevRead(bDevPtr, trackNumber, sectorNumber, sector))
				goto error;
			if (fwrite(sector, kBDevSectorSize, 1, f) == 0) {
				printf("?WRITE\n");
				goto error;
			}
		}
	}

	/* Close the output file. */
	if (fclose(f) != 0) {
		printf("?CLOSE\n");
		f = 0;
		goto error;
	}

	/* The format is complete. */
	printf("COPY COMPLETE.\n");

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	if (f != 0)
		fclose(f);
	return 0;

}

/* FileDiskFormat() formats a File Disk. */
int
	FileDiskFormat(char *fileName,
	               long siz,
	               long spt,
	               long bls,
	               long drm,
	               long off,
	               long skf)
{
	Byte sector[kBDevSectorSize];
	BDevParameterBlock pb;
	FileDiskHeader header;
	FILE *f = 0;
	int standardFloppy;
	long minBLS;
	long n;

	/* Insure that the header size is correct. */
	if (sizeof(FileDiskHeader) != kBDevSectorSize) {
		printf("?HEADER\n");
		goto error;
	}

	/* It is a "standard floppy" if the parameters match. */
	standardFloppy = ((siz == -1) || (siz == SSSIZ)) &&
	                 ((spt == -1) || (spt == SSSPT)) &&
	                 ((bls == -1) || (bls == SSBLS)) &&
	                 ((drm == -1) || (drm == SSDRM)) &&
	                 ((off == -1) || (off == SSOFF)) &&
	                 ((skf == -1) || (skf == SSSKF));

	/* Set any defalt values. */
	if (siz < (256 * 1024)) {
		if (bls == -1)
			bls = SSBLS;
		if (drm == -1)
			drm = SSDRM;
		if (spt == -1)
			spt = SSSPT;
		if (off == -1)
			off = 0;
		if (skf == -1)
			skf = 1;
	}
	else {
		if (bls == -1)
			bls = HDBLS;
		if (drm == -1)
			drm = HDDRM;
		if (spt == -1)
			spt = HDSPT;
		if (off == -1)
			off = 0;
		if (skf == -1)
			skf = 1;
	}

	/* Compute the BDev Parameters. */
	pb.siz = standardFloppy ? SSSIZ : siz;
	pb.spt.word = standardFloppy ? SSSPT : spt;
	pb.bls.word = standardFloppy ? SSBLS : bls;
	pb.drm.word = standardFloppy ? SSDRM : drm;
	pb.off.word = standardFloppy ? SSOFF : off;
	pb.skf.word = standardFloppy ? SSSKF : skf;
	pb.tpd.word = pb.siz / (pb.spt.word * kBDevSectorSize);
	ComputeBDevParameters(&pb);

	/* Check for bad BDev Parameter values. */
	siz = pb.siz;
	minBLS = ((pb.dsm.word < 256) ? 1024 : 2048);
	if (siz > kMaxSIZ) {
		printf("?ERROR: %ld > %ld\n", siz, kMaxSIZ);
		goto error;
	}
	else if (spt > kMaxSPT) {
		printf("?ERROR: -S %ld > %ld\n", spt, kMaxSPT);
		goto error;
	}
	else if (drm > kMaxDRM) {
		printf("?ERROR: -D %ld > \n", drm, kMaxDRM);
		goto error;
	}
	else if (bls > kMaxBLS) {
		printf("?ERROR: -B %ld > \n", bls, kMaxBLS);
		goto error;
	}
	else if ((bls & (bls - 1)) != 0) {
		printf("?ERROR: -B %d\n", bls);
		goto error;
	}
	else if (bls < minBLS) {
		printf("?ERROR: -B %ld < %ld\n", bls, minBLS);
		goto error;
	}
	else if (pb.dbl.word > 16) {
		printf("?ERROR: -F %ld -B %ld\n", drm, bls);
		goto error;
	}
	else if (pb.dbl.word >= pb.dsm.word) {
		printf("?ERROR: -F %ld -B %ld\n", drm, bls);
		goto error;
	}

	/* Ready to format... */
	printf("FORMATTING [%s] %dK...\n", fileName, siz / 1024);
	ShowBDevParameterBlock(&pb, 0);

	/* Does the file already exist? */
	if ((f = fopen(fileName, "r")) != 0) {
		printf("?EXISTS: [%s]\n", fileName);
		fclose(f);
	}

	/* Solicit OK to continue... */
	printf("CONTINUE (Y/N)?");
	switch (ConsoleInput(10)) {
	case 'Y':
	case 'y':
		printf(" Y\n");
		break;
	default:
		printf(" N\n");
		goto error;
	}

	/* Open the output file. */
	if ((f = fopen(fileName, "wb")) == 0) {
		printf("?ERROR: [%s]\n", fileName);
		goto error;
	}

	/* Write a header if not a "standard floppy". */
	if (!standardFloppy) {
		MemoryZero(&header, kBDevSectorSize);
		strcpy(header.magic, gFileDiskMagic);
		sprintf(header.info,
		        "TPD=%ld SPT=%ld BLS=%ld DRM=%ld OFF=%ld SKF=%ld"
		        "\015\012\012",
		        pb.tpd.word,
		        pb.spt.word,
		        pb.bls.word,
		        pb.drm.word,
		        pb.off.word,
		        pb.skf.word);
		header.tpd[0] = pb.tpd.byte.low;
		header.tpd[1] = pb.tpd.byte.high;
		header.spt[0] = pb.spt.byte.low;
		header.spt[1] = pb.spt.byte.high;
		header.bls[0] = pb.bls.byte.low;
		header.bls[1] = pb.bls.byte.high;
		header.drm[0] = pb.drm.byte.low;
		header.drm[1] = pb.drm.byte.high;
		header.off[0] = pb.off.byte.low;
		header.off[1] = pb.off.byte.high;
		header.skf[0] = pb.skf.byte.low;
		header.skf[1] = pb.skf.byte.high;
		if (fwrite(&header, sizeof(header), 1, f) != 1) {
			printf("?WRITE\n");
			goto error;
		}
	}

	/* Erase all sectors on the disk. */
	EmptySector(sector);
	n = pb.tpd.word * pb.spt.word;
	while (n-- > 0) {
		if (fwrite(sector, kBDevSectorSize, 1, f) == 0) {
			printf("?WRITE\n");
			goto error;
		}
	}

	/* Close the output file. */
	if (fclose(f) != 0) {
		printf("?CLOSE\n");
		f = 0;
		goto error;
	}

	/* The format is complete. */
	printf("FORMAT COMPLETE.");

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	if (f != 0)
		fclose(f);
	return 0;

}

/**********************************************************************/
#pragma mark *** BLOCK DEVICE ***

/* BDevIndexToPtr() returns a pointer to a block device. */
BDevPtr BDevIndexToPtr(unsigned n)
{

	/* Return zero if the index is out of bounds. */
	return (n < kMaxBDev) ? &gBDev[n] : 0;

}

/* BDevStatus() returns the status of a block device. */
int BDevStatus(BDevPtr bDevPtr, char *name)
{

	/* Closed if the block device is not open. */
	if (!bDevPtr->isOpen)
		goto closed;

	/* Return the physical device name if requested. */
	if (name != 0)
		strcpy(name, bDevPtr->name);

	/* All done, return the block device status. */
	return bDevPtr->isReadOnly ? kBDevStatusReadOnly :
	                          kBDevStatusReadWrite;

	/* Return kBDevStatusClosed if closed. */
closed:
	if (name != 0)
		strcpy(name, "");
	return kBDevStatusClosed;

}

/* BDevInstallParameters() copies the parameters into memory. */
int BDevInstallParameters(BDevPtr bDevPtr, Word dpAddress)
{
	static BDevPtr previousBDev;
	unsigned i;

	/* If the bDev is not open, use standard parameters. */
	if (!bDevPtr->isOpen) {
		bDevPtr->pb.tpd.word = SSTPD;
		bDevPtr->pb.spt.word = SSSPT;
		bDevPtr->pb.bls.word = SSBLS;
		bDevPtr->pb.drm.word = SSDRM;
		bDevPtr->pb.off.word = SSOFF;
		bDevPtr->pb.skf.word = SSSKF;
		ComputeBDevParameters(&bDevPtr->pb);
		for (i = 0; i < kMaxALV; i++)
			bDevPtr->alv[i] = 0;
		for (i = 0; i < kMaxCKS; i++)
			bDevPtr->csv[i] = 0;
	}

	/* Note the disk parameters address. */
	bDevPtr->dpAddress = dpAddress;

	/* Compute the address of the sector translation table. */
	bDevPtr->xltAddress.byte.low = RdByte(dpAddress + 0);
	bDevPtr->xltAddress.byte.high = RdByte(dpAddress + 1);

	/* Compute the address of the disk parameter block. */
	bDevPtr->pbAddress.byte.low = RdByte(dpAddress + 10);
	bDevPtr->pbAddress.byte.high = RdByte(dpAddress + 11);

	/* Compute the address of the directory check vector. */
	bDevPtr->csvAddress.byte.low = RdByte(dpAddress + 12);
	bDevPtr->csvAddress.byte.low = RdByte(dpAddress + 13);

	/* Compute the address of the data allocation vector. */
	bDevPtr->alvAddress.byte.low = RdByte(dpAddress + 14);
	bDevPtr->alvAddress.byte.low = RdByte(dpAddress + 15);

	/* Install the disk parameter block. */
	i = bDevPtr->pbAddress.word;
	WrByte(i++, bDevPtr->pb.spt.byte.low);
	WrByte(i++, bDevPtr->pb.spt.byte.high);
	WrByte(i++, bDevPtr->pb.bsh.byte.low);
	WrByte(i++, bDevPtr->pb.blm.byte.low);
	WrByte(i++, bDevPtr->pb.exm.byte.low);
	WrByte(i++, bDevPtr->pb.dsm.byte.low);
	WrByte(i++, bDevPtr->pb.dsm.byte.high);
	WrByte(i++, bDevPtr->pb.drm.byte.low);
	WrByte(i++, bDevPtr->pb.drm.byte.high);
	WrByte(i++, bDevPtr->pb.alb.byte.high);
	WrByte(i++, bDevPtr->pb.alb.byte.low);
	WrByte(i++, bDevPtr->pb.cks.byte.low);
	WrByte(i++, bDevPtr->pb.cks.byte.high);
	WrByte(i++, bDevPtr->pb.off.byte.low);
	WrByte(i++, bDevPtr->pb.off.byte.high);

	/* Install the sector translation table. */
	for (i = 0; i < bDevPtr->pb.spt.word; i++)
		WrByte(bDevPtr->xltAddress.word + i, bDevPtr->pb.xlt[i]);

	/* If a previous bDev was active... */
	if (previousBDev != 0) {
		/* then save the data allocation vector... */
		for (i = 0; i < kMaxALV; i++)
			previousBDev->alv[i] =
				RdByte(previousBDev->alvAddress.word + i);
		/* and save the directory check vector. */
		for (i = 0; i < kMaxCKS; i++)
			previousBDev->csv[i] =
				RdByte(previousBDev->csvAddress.word + i);
	}
	previousBDev = bDevPtr;

	/* Install the data allocation vector. */
	for (i = 0; i < kMaxALV; i++)
		WrByte(bDevPtr->alvAddress.word + i, bDevPtr->alv[i]);

	/* Install the directory check vector. */
	for (i = 0; i < kMaxCKS; i++)
		WrByte(bDevPtr->csvAddress.word + i, bDevPtr->csv[i]);

	/* Error if the bDev is not open. */
	if (!bDevPtr->isOpen)
		goto error;

	/* All done, no error, return the block device status. */
	return BDevStatus(bDevPtr, 0);

	/* Return kBDevStatusError if there was an error. */
error:
	return kBDevStatusError;

}

/* BDevWrite() writes a sector to a block device. */
int
	BDevWrite(BDevPtr bDevPtr,
	          Word trackNumber,
	          Word sectorNumber,
	          Byte *sector)
{
	unsigned long sectorIndex;

	/* Error if the block device is not open. */
	if (!bDevPtr->isOpen)
		goto error;

	/* Error if the block device is read-only. */
	if (bDevPtr->isReadOnly)
		goto error;

	/* Ignore the upper byte of the Sector Number for small disks. */
	if (bDevPtr->pb.spt.word < 256)
		sectorNumber &= 0x00FF;

	/* Ignore the upper byte of the Track Number for small disks. */
	if (bDevPtr->pb.tpd.word < 256)
		trackNumber &= 0x00FF;

	/* Convert the sector/track to a sector index. */
	sectorIndex = (trackNumber * bDevPtr->pb.spt.word) + sectorNumber;
	if (sectorIndex >= bDevPtr->pb.spd)
		goto error;

	/* Write a sector. */
	if (!bDevPtr->bDevWrite(bDevPtr, sectorIndex, sector))
		goto error;

	/* All done, no error, return the block device status. */
	return BDevStatus(bDevPtr, 0);

	/* Return kBDevStatusError if there was an error. */
error:
	SystemMessage("BDevWrite: %s TRACK %d, SECTOR %d ERROR\n",
	              bDevPtr->bDev,
	              trackNumber,
	              sectorNumber);
	return kBDevStatusError;

}

/* BDevRead() reads a sector from a block device. */
int
	BDevRead(BDevPtr bDevPtr,
	         Word trackNumber,
	         Word sectorNumber,
	         Byte *sector)
{
	unsigned long sectorIndex;

	/* Error if the block device is not open. */
	if (!bDevPtr->isOpen)
		goto error;

	/* Ignore the upper byte of the Sector Number for small disks. */
	if (bDevPtr->pb.spt.word < 256)
		sectorNumber &= 0x00FF;

	/* Ignore the upper byte of the Track Number for small disks. */
	if (bDevPtr->pb.tpd.word < 256)
		trackNumber &= 0x00FF;

	/* Convert the sector/track to a sector index. */
	sectorIndex = (trackNumber * bDevPtr->pb.spt.word) + sectorNumber;
	if (sectorIndex >= bDevPtr->pb.spd)
		goto error;

	/* Read a sector. */
	if (!bDevPtr->bDevRead(bDevPtr, sectorIndex, sector))
		goto error;

	/* All done, no error, return the block device status. */
	return BDevStatus(bDevPtr, 0);

	/* Return kBDevStatusError if there was an error. */
error:
	SystemMessage("BDevRead: %s TRACK %d, SECTOR %d ERROR\n",
	              bDevPtr->bDev,
	              trackNumber,
	              sectorNumber);
	EmptySector(sector);
	return kBDevStatusError;

}

/* BDevReadFCB() reads a file control block. */
static int BDevReadFCB(BDevPtr bDevPtr, Word index, Byte *fcb)
{
	Byte buffer[kBDevSectorSize];
	Word sector;
	int status;

	sector = index / 4;

	status =
		BDevRead(bDevPtr,
		         bDevPtr->pb.off.word + (sector / bDevPtr->pb.spt.word),
		         bDevPtr->pb.xlt[sector % bDevPtr->pb.spt.word] - 1,
		         buffer);

	memcpy(fcb, &buffer[(index % 4) * 32], 32);

	/* All done, no error, return non-zero. */
	/* Return zero if there was an error. */
	return status != kBDevStatusError;
}

/* BDevReadALV() computes the data allocation vector. */
static int BDevReadALV(BDevPtr bDevPtr, Byte *alv)
{
	Byte fcb[32];
	int i;
	int j;

	/* Clear the data allocation vector. */
	for (i = 0; i < (kMaxALV * 8); i++)
		alv[i] = 0;

	/* Note single references to the directory blocks. */
	for (i = 0; i < bDevPtr->pb.dbl.word; i++)
		alv[i] = 1;

	/* Note data block references for all files. */
	for (i = 0; i <= bDevPtr->pb.drm.word; i++) {
		if (!BDevReadFCB(bDevPtr, i, fcb))
			goto error;
		if (fcb[0] == kEmptyByte)
			continue;
		for (j = 16; j < 32; j++) {
			if (fcb[j] == 0)
				break;
			alv[fcb[j]]++;
		}
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* BDevClose() terminates access to a block device. */
void BDevClose(BDevPtr bDevPtr)
{

	/* Close the block device if it is open. */
	if (bDevPtr->isOpen != 0)
		bDevPtr->bDevClose(bDevPtr);

	bDevPtr->isOpen = 0;

}

/* BDevOpen() prepares access to a block device. */
int BDevOpen(BDevPtr bDevPtr, const char *name, int readOnly)
{
	unsigned i;

	/* Error if the block device is already open. */
	if (bDevPtr->isOpen)
		goto error;

	/* Clear the block device structure. */
	bDevPtr->cookie = 0;
	bDevPtr->dpAddress = 0;
	bDevPtr->pbAddress.word = 0;
	bDevPtr->xltAddress.word = 0;
	for (i = 0; i < kMaxALV; i++)
		bDevPtr->alv[i] = 0;
	for (i = 0; i < kMaxCKS; i++)
		bDevPtr->csv[i] = 0;

	/* Will this block device be read-only? */
	bDevPtr->isReadOnly = readOnly;

	/* Solicit a Block Device Name. */
	if (!SolicitBDevName(bDevPtr, name))
		goto error;

	/* Error if another BDev is already using this name. */
	for (i = 0; i < kMaxBDev; i++) {
		if (i == bDevPtr->index)
			continue;
		if (!strcmp(bDevPtr->name, gBDev[i].name))
			goto error;
	}

	/* Select an open function. */
	if (!strcasecmp(bDevPtr->name, "RAMDISK")) {
		if (!RamDiskOpen(bDevPtr))
			goto error;
	}
	/* Open a Directory Disk if it is a directory. */
	else if (IsDir(bDevPtr->name)) {
#if 0
		if (!DirectoryDiskOpen(bDevPtr))
			goto error;
#else
		SystemMessage("?FILE [%s => %s]\n",
		              bDevPtr->bDev,
		              bDevPtr->name);
		goto error;
#endif
	}
	/* Open an ASCII or BINARY File Disk. */
	else if (!AsciiDiskOpen(bDevPtr))
		if (!FileDiskOpen(bDevPtr))
			goto error;

	/* All done, no error, return the bDev status. */
	return BDevStatus(bDevPtr, 0);

	/* Return bDev error status if there was an error. */
error:
	return kBDevStatusError;

}

/* BDevMount() mounts a file as a block device. */
BDevPtr BDevMount(const char *bDev, const char *file, int readOnly)
{
	unsigned bDevNumber;

	for (bDevNumber = 0; bDevNumber < kMaxBDev; bDevNumber++)
		if (!strcmp(bDev, gBDev[bDevNumber].bDev))
			break;
	if (bDevNumber >= kMaxBDev)
		goto error;

	if (file != 0) {

		if (gBDev[bDevNumber].isOpen)
			goto error;

		BDevOpen(&gBDev[bDevNumber], file, readOnly);

		if (!gBDev[bDevNumber].isOpen)
			goto error;

	}

	/* All done, no error, return a pointer to the bDev. */
	return &gBDev[bDevNumber];

	/* Return zero if there was an error. */
error:
	return 0;
}

/* BDevUnmount() unmounts a block device. */
void BDevUnmount(const char *bDev)
{
	unsigned bDevNumber;

	/* Search for the bDev to close, all if bDev is 0. */
	for (bDevNumber = 0; bDevNumber < kMaxBDev; bDevNumber++)
		if ((bDev == 0) || !strcmp(bDev, gBDev[bDevNumber].bDev))
			if (gBDev[bDevNumber].isOpen)
				BDevClose(&gBDev[bDevNumber]);

}

/* ShowBDevParameterBlock() displays a bDev parameter block. */
void ShowBDevParameterBlock(BDevParameterBlockPtr pb, int showXLT)
{
	int i;

	printf("SIZ = %8lu	; TOTAL NUMBER OF BYTES IN DISK\n",
	       pb->siz);

	printf("TPD = %8u	; NUMBER OF TRACKS IN DISK\n",
	       pb->tpd.word);

	printf("SPT = %8u	; NUMBER OF SECTORS PER TRACK\n",
	       pb->spt.word);

	printf("DSM = %8u	; TOTAL NUMBER OF BLOCKS IN DISK\n",
	       pb->dsm.word);

	printf("DRM = %8u 	; TOTAL NUMBER OF DIRECTORY ENTRIES\n",
	       pb->drm.word);

	printf("BLS = %8u	; DATA ALLOCATION BLOCK SIZE\n",
	       pb->bls.word);

	printf("BSH = %8u	; DATA ALLOCATION BLOCK SHIFT\n",
	       pb->bsh.word);

	printf("BLM =    0%04XH	; DATA ALLOCATION BLOCK MASK\n",
	       pb->blm.word);

	printf("EXM = %8u	; EXTENT MASK\n",
	       pb->exm.word);

	printf("ALB =    0%04XH	; DIRECTORY ALLOCATION BLOCK MASK\n",
	       pb->alb.word);

	printf("CKS = %8u	; SIZE OF DIRECTORY CHECK VECTOR\n",
	       pb->cks.word);

	printf("OFF = %8u	; NUMBER OF RESERVED TRACKS\n",
	       pb->off.word);

	printf("ALV = %8u	; SIZE OF DATA ALLOCATION VECTOR\n",
	       pb->alv.word);

	printf("SKF = %8u	; SECTOR SKEW FACTOR",
	       pb->skf.word);

	if (showXLT) {
		for (i = 0; i < pb->spt.word; i++) {
			if ((i % 16) == 0)
				printf("\n");
			printf(" %2d", pb->xlt[i]);
		}
	}

	printf("\n");

}

/* EraseSystemTracks() erases the system tracks on a disk. */
int EraseSystemTracks(BDevPtr bDevPtr)
{
	Byte buffer[kBDevSectorSize];
	Word track;
	Word sector;
	unsigned n;

	/* Ready to erase system tracks... */
	printf("ABOUT TO ERASE %d SYSTEM TRACKS ON %s => %s\n",
	       bDevPtr->pb.off.word,
	       bDevPtr->bDev,
	       bDevPtr->name);

	/* Solicit OK to continue... */
	printf("CONTINUE (Y/N)?");
	switch (ConsoleInput(10)) {
	case 'Y':
	case 'y':
		printf(" Y\n");
		break;
	default:
		printf(" N\n");
		goto error;
	}

	/* Erase all sectors on the system tracks. */
	EmptySector(buffer);
	track = 0;
	sector = 0;
	n = bDevPtr->pb.off.word * bDevPtr->pb.spt.word;
	while (n-- > 0) {
		if (!BDevWrite(bDevPtr, track, sector++, buffer))
			goto error;
		if (sector == bDevPtr->pb.spt.word) {
			track++;
			sector = 0;
		}
	}

	/* The system tracks have been erased. */
	printf("SYSTEM TRACKS ERASED.\n");

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* ShowBDevALV() displays the data allocation vector. */
int ShowBDevALV(const char *bDev, unsigned n, char *name)
{
#pragma unused(n)
#pragma unused(name)
	BDevPtr bDevPtr;
	Byte alv[kMaxALV * 8];
	int i;

	/* Error if the block device is not mounted. */
	bDevPtr = BDevMount(bDev, 0, 0);
	if (bDevPtr == 0) {
		printf("?NODISK [%s]\n", bDev);
		goto error;
	}
	else if (!bDevPtr->isOpen) {
		printf("?NOMOUNT [%s]\n", bDev);
		goto error;
	}

	/* Compute the data allocation vector. */
	if (!BDevReadALV(bDevPtr, alv))
		goto error;

	/* Display the data allocation vector. */
	for (i = 0; i < bDevPtr->pb.alv.word * 8; i++) {
		if ((i % 16) != 0)
			printf(" ");
		printf("%2d", alv[i] & 0xFF);
		if (((i + 1) % 16) == 0)
			printf("\n");
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* ShowBDevFCB() displays a file control block. */
int ShowBDevFCB(const char *bDev, unsigned n, char *name)
{
#pragma unused(name)
	BDevPtr bDevPtr;
	Byte fcb[32];
	int i;
	int j;

	/* Error if the block device is not mounted. */
	bDevPtr = BDevMount(bDev, 0, 0);
	if (bDevPtr == 0) {
		printf("?NODISK [%s]\n", bDev);
		goto error;
	}
	else if (!bDevPtr->isOpen) {
		printf("?NOMOUNT [%s]\n", bDev);
		goto error;
	}

	/* Display the file control block. */
	for (i = 0; i <= bDevPtr->pb.drm.word; i++) {
		if ((n != -1) && (n != i))
			continue;
		if (!BDevReadFCB(bDevPtr, i, fcb))
			goto error;
		if (n == -1) {
			if (fcb[0] == kEmptyByte)
				continue;
		}
		printf("%02d ", i);
		printf("%02X %c%c%c%c%c%c%c%c %c%c%c %02X %02X%02X%02X",
		       fcb[0],
		       ((fcb[1] < ' ') || (fcb[1] >= 0x7F)) ? '.' : fcb[1],
		       ((fcb[2] < ' ') || (fcb[2] >= 0x7F)) ? '.' : fcb[2],
		       ((fcb[3] < ' ') || (fcb[3] >= 0x7F)) ? '.' : fcb[3],
		       ((fcb[4] < ' ') || (fcb[4] >= 0x7F)) ? '.' : fcb[4],
		       ((fcb[5] < ' ') || (fcb[5] >= 0x7F)) ? '.' : fcb[5],
		       ((fcb[6] < ' ') || (fcb[6] >= 0x7F)) ? '.' : fcb[6],
		       ((fcb[7] < ' ') || (fcb[7] >= 0x7F)) ? '.' : fcb[7],
		       ((fcb[8] < ' ') || (fcb[8] >= 0x7F)) ? '.' : fcb[8],
		       ((fcb[9] < ' ') || (fcb[9] >= 0x7F)) ? '.' : fcb[9],
		       ((fcb[10] < ' ') || (fcb[10] >= 0x7F)) ? '.' : fcb[10],
		       ((fcb[11] < ' ') || (fcb[11] >= 0x7F)) ? '.' : fcb[11],
		       fcb[12],
		       fcb[13],
		       fcb[14],
		       fcb[15]);
		for (j = 16; j < 32; j++) {
			if (fcb[j] == 0)
				break;
			printf(" %02X", fcb[j]);
		}
		printf("\n");
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* ShowBDevDIR() displays a directory entry. */
int ShowBDevDIR(const char *bDev, unsigned n, char *name)
{
#pragma unused(n)
#pragma unused(name)
	BDevPtr bDevPtr;
	Byte fcb[32];
	int i;
	int j;

	/* Error if the block device is not mounted. */
	bDevPtr = BDevMount(bDev, 0, 0);
	if (bDevPtr == 0) {
		printf("?NODISK [%s]\n", bDev);
		goto error;
	}
	else if (!bDevPtr->isOpen) {
		printf("?NOMOUNT [%s]\n", bDev);
		goto error;
	}

	/* Display the directory entry. */
	j = 0;
	for (i = 0; i <= bDevPtr->pb.drm.word; i++) {
		if (!BDevReadFCB(bDevPtr, i, fcb))
			goto error;
		if (fcb[0] == kEmptyByte)
			continue;
		if ((n % 4) != 0)
			printf(" : ");
		printf("%3u %c%c%c%c%c%c%c%c %c%c%c",
		       fcb[0],
		       (fcb[1] < ' ') ? '.' : fcb[1],
		       (fcb[2] < ' ') ? '.' : fcb[2],
		       (fcb[3] < ' ') ? '.' : fcb[3],
		       (fcb[4] < ' ') ? '.' : fcb[4],
		       (fcb[5] < ' ') ? '.' : fcb[5],
		       (fcb[6] < ' ') ? '.' : fcb[6],
		       (fcb[7] < ' ') ? '.' : fcb[7],
		       (fcb[8] < ' ') ? '.' : fcb[8],
		       (fcb[9] < ' ') ? '.' : fcb[9],
		       (fcb[10] < ' ') ? '.' : fcb[10],
		       (fcb[11] < ' ') ? '.' : fcb[11]);
		if (((j + 1) % 4) == 0)
			printf("\n");
		j++;
	}
	if ((n % 4) != 0)
		printf("\n");

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* ShowBDevMount() displays mount information for a bDev. */
void ShowBDevMount(char *bDev, int verbose)
{

	if (bDev == 0) {
		ShowBDevMount("A:", 0);
		ShowBDevMount("B:", 0);
		ShowBDevMount("C:", 0);
		ShowBDevMount("D:", 0);
		ShowBDevMount("E:", 0);
		ShowBDevMount("F:", 0);
		ShowBDevMount("G:", 0);
		ShowBDevMount("H:", 0);
		ShowBDevMount("I:", 0);
		ShowBDevMount("J:", 0);
		ShowBDevMount("K:", 0);
		ShowBDevMount("L:", 0);
		ShowBDevMount("M:", 0);
		ShowBDevMount("N:", 0);
		ShowBDevMount("O:", 0);
		ShowBDevMount("P:", 0);
	}
	else {
		BDevPtr bDevPtr = BDevMount(bDev, 0, 0);
		if (bDevPtr == 0)
			printf("?NODISK [%s]\n", bDev);
		else if (!bDevPtr->isOpen)
			printf("%s =>\n", bDev);
		else {
			printf("%s => %s", bDev, bDevPtr->name);
			printf(" %s", bDevPtr->isReadOnly ? "(R/O)" : "(R/W)");
			printf(" %lu\n", bDevPtr->pb.siz);
			if (verbose) {
				printf("DP  =    0%04XH"
				       "	; ADDRESS OF CP/M DISK PARAMETER HEADER\n",
				       bDevPtr->dpAddress);
				printf("PB  =    0%04XH"
				       "	; ADDRESS OF CP/M DISK PARAMETER BLOCK\n",
				       bDevPtr->pbAddress.word);
				printf("XLT =    0%04XH"
				       "	; ADDRESS OF CP/M TRANSLATION VECTOR\n",
				       bDevPtr->xltAddress.word);
				ShowBDevParameterBlock(&bDevPtr->pb, 1);
			}
		}
	}

}
