/* uSim READ.ME
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

What is uSim?  uSim is a system simulator that runs CP/M.  It emulates
either an i8080 or Z80 cpu with 64K RAM/ROM, a Console, various disk
drives and character I/O devices.  The "C" source is suitable for a
teaching tool.  Everything from the assembler to the console window is
available to the programming student.  The system is quite fast and
robust as a CP/M workstation.  Other i8080 or Z80 system software can
be developed as well using the builtin assembler/disassembler/debugger.
CP/M can be run directly from ROM, or from the system tracks on a disk.
Complete BOOT and BIOS sources are provided for a 63.5K CP/M.  A CP/M
distribution has been serialized for uSim.

/**********************************************************************/

The 1.0 release builds with CodeWarrior 4, IDE 3.2 with PPC and 68K
Macintosh targets.

The BOOT.BAT will boot from the CPM.DSK read-only distribution disk.

Use <ESCAPE><ESCAPE> or <COMMAND>-M to enter the monitor.
Use the monitor HELP command for more information.


/**********************************************************************/

Macintosh Code-Warrior Build Instructions

SETTING:
User Access Path: {project}:uSimSrc:
Heap Size = 1024
Stack Size = 128
ANSI Strict C
Prefix File = macPPC.prefix
Fast/Small Optimizations

SOURCE FILES:
dir.c
pstring.c
string.c
digits.c
stack.c
search.c
file.c
sort.c
ring.c
console.c
sioux.c
gets.c
printf.c
stdio.c
cpu.c
memory.c
bdev.c
cdev.c
clock.c
hostfile.c
system.c
asm.c
dasm.c
ddt.c
monitor.c
main.c

LINK FILES:
MSL RuntimePPC.lib
InterfaceLib
MathLib
MSL C.PPC.Lib
