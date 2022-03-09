/* uSim hostfile.c * Copyright (C) 2000, Tsurishaddai Williamson, tsuri@earthlink.net *  * This program is free software; you can redistribute it and/or * modify it under the terms of the GNU General Public License * as published by the Free Software Foundation; either version 2 * of the License, or (at your option) any later version. *  * This program is distributed in the hope that it will be useful, * but WITHOUT ANY WARRANTY; without even the implied warranty of * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the * GNU General Public License for more details. *  * You should have received a copy of the GNU General Public License * along with this program; if not, write to the Free Software * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. *//**********************************************************************/#include "ustdio.h"#include "file.h"#include "memory.h"#include "system.h"#include "cpu.h"#include "monitor.h"#include "hostfile.h"/**********************************************************************/#pragma mark *** HOST FILE ***#ifdef BSD#define remove unlink#endif/* FileNameFromFCB() extracts the file name from the FCB. */const char *FileNameFromFCB(FileFCB *fcb){	static char fileName[20];	char *f = fileName;	unsigned i;	char *c;	/* Build the FILE.EXT from the FCB. */	c = fcb->fileName;	for (i = 1; i < 9; i++) {		if (*c == ' ')			break;		*f++ = *c++;	}	c = fcb->fileExtension;	if (*c != ' ') {		*f++ = '.';		for (i = 9; i < 12; i++) {			if (*c == ' ')				break;			*f++ = *c++;		}	}	*f = 0;	/* All done, return a pointer to the static buffer. */	return fileName;}/* HostFileWrite() writes to a host file. */int HostFileWrite(FileFCB *fcb, char *buffer){	unsigned long n;	/* Error if not open. */	if (fcb->file == 0) {		SystemMessage("?NOFILE [%s]\n", FileNameFromFCB(fcb));		goto error;	}	/* Seek to the specified position. */	if (fseek(fcb->file, fcb->position * fcb->count, 0) != 0) {		SystemMessage("?SEEK [%s]\n", FileNameFromFCB(fcb));		goto error;	}	/* Write to the Host File. */	n = fwrite(buffer, 1, fcb->count, fcb->file);	/* Error if the write failed. */	if (n != fcb->count) {		SystemMessage("?WRITE [%s]\n", FileNameFromFCB(fcb));		goto error;	}	/* Advance the buffer position. */	fcb->position += 1;	/* All done, no error, return non-zero. */	return 1;	/* Return 0 if there was an error. */error:	return 0;}/* HostFileRead() reads from a host file. */int HostFileRead(FileFCB *fcb, char *buffer){	unsigned i;	unsigned long n;	/* Error if not open. */	if (fcb->file == 0) {		SystemMessage("?NOFILE [%s]\n", FileNameFromFCB(fcb));		goto error;	}	/* Seek to the specified position. */	if (fseek(fcb->file, fcb->position * fcb->count, 0) != 0) {		SystemMessage("?SEEK [%s]\n", FileNameFromFCB(fcb));		goto error;	}	/* Read from the Host FIle. */	n = fread(buffer, 1, fcb->count, fcb->file);	/* Fill the unused portion of the buffer with Control-Z's. */	for (i = n; i < fcb->count; i++)		buffer[i] = 'Z' - '@';	/* Note the number of bytes actually read. */	fcb->count = n;	/* Error if the read failed. */	if (fcb->count == 0) {		if (fcb->eof++)			SystemMessage("?READ [%s]\n", FileNameFromFCB(fcb));		goto error;	}	/* Advance the buffer position. */	fcb->position += 1;	/* All done, no error, return non-zero. */	return 1;	/* Return 0 if there was an error. */error:	return 0;}/* HostFileMake() creates a host file. */int HostFileMake(FileFCB *fcb){	const char *fileName;	/* Extract the file name from the FCB. */	fileName = FileNameFromFCB(fcb);	/* Error if the Host File already exists. */	if ((fcb->file = FOpenPath(fileName, "r")) != 0) {		fclose(fcb->file);		fcb->file = 0;		SystemMessage("?EXISTS [%s]\n", fileName);		goto error;	}	/* Create the Host File. */	if ((fcb->file = FOpenPath(fileName, "wb+")) == 0) {		SystemMessage("?OPEN [%s]\n", fileName);		goto error;	}	/* Reset the Host File. */	fcb->position = 0;	fcb->eof = 0;	/* All done, no error, return non-zero. */	return 1;	/* Return 0 if there was an error. */error:	return 0;}/* HostFileDelete() deletes a host file. */int HostFileDelete(FileFCB *fcb){	const char *fileName;	FILE *f;	/* Extract the file name from the FCB. */	fileName = FileNameFromFCB(fcb);	/* Remove the Host File if it exists. */	if ((f = FOpenPath(fileName, "r")) != 0) {		fclose(f);		if (remove(fileName) != 0) {			SystemMessage("?REMOVE [%s]\n", fileName);			goto error;		}	}	/* All done, no error, return non-zero. */	return 1;	/* Return 0 if there was an error. */error:	return 0;}/* HostFileClose() closes a host file. */int HostFileClose(FileFCB *fcb){	if (fcb->file != 0) {		if (fclose(fcb->file) != 0) {			SystemMessage("?CLOSE [%s]\n", FileNameFromFCB(fcb));			goto error;		}	}	fcb->file = 0;	/* All done, no error, return non-zero. */	return 1;	/* Return 0 if there was an error. */error:	return 0;}/* HostFileOpen() opens a host file. */int HostFileOpen(FileFCB *fcb){	const char *fileName;	/* Extract the file name from the FCB. */	fileName = FileNameFromFCB(fcb);	/* Open the Host File. */	if ((fcb->file = FOpenPath(fileName, "rb+")) == 0) {		if ((fcb->file = FOpenPath(fileName, "rb")) == 0) {			SystemMessage("?OPEN [%s]\n", fileName);			goto error;		}	}	/* Reset the Host File. */	fcb->position = 0;	fcb->eof = 0;	/* All done, no error, return non-zero. */	return 1;	/* Return 0 if there was an error. */error:	return 0;}