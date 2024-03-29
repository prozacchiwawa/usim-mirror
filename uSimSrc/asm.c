/* uSim asm.c
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

/*
 * Code assembly covers several input and output options.
 * 1) Input directly from memory
 * 2) Input from a code array
 * 3) Input from a binary file
 * 4) Input from a HEX file
 * 5) Input from a source file
 * 6) Output directly to memory
 * 7) Output to a code file
 * 8) Output to a HEX file
 * 9) Output to a listing file
 *
 * Direct memory access is done via function arguments.
 *
 * The code array is similar in form to a HEX file.  It is used to
 * include assembled code in C-language source.
 *
 * Binary input files are files that have non-text bytes.  They are
 * assumed to be in CP/M COM format based at address 0100H.
 *
 * The HEX output and input use the CP/M standard format.
 *
 * The listing output format is similar that of the CP/M ASM.
 *
 * The assembler source language is similar that of the CP/M ASM.
 * A source file stack is maintained to allow the processing of
 * multiple source input files.
 *
 * The assembly instruction set is calculated from the same operation
 * definitions used to build Cpu() and Disassemble().
 *
 * Assembly source macro's are not supported.
 *
 * The expression evaluator can be directly accessed via Calculate().
 *
 * The most important functions to grok are:
 *   CalculateInstructionSet()
 *   ParseOperand()
 *   SearchCompareCode()
 *   GenerateCode()
 *   ParseTerm()
 *
 * It works like this...
 * The operations are defined as a list of name/code pairs.  The name
 * actually is composed of the opcode and the operand.  An example is:
 *   "LXI B,%2" "01NNNN"
 * The opcode is "LXI".  The operand is "B,%2".  The code is "01NNNN".
 * The "%2" indicates that the operand contains a word value "NNNN".
 * CalculateInstructionSet() will transform these into:
 *   "LXI" "B,:" "01%2"
 * If the source line is:
 *   "LXI B,1234H"
 * ParseOperand() will produce:
 *   "B,11064Q"
 * SearchCompareCode() equates this to:
 *   "B,:"
 * GenerateCode() combines:
 *   "01%2" "B,11064Q"
 * To emit:
 *   "013412"
 *
 * On a different note, the expression evaluator, ParseTerm(), is
 * pretty cool.  Here, operation priority is the name of the game.
 * ParseTerm() recurses on increasingly higher priorities until
 * something gets evaluated.  The lowest priority is for "=", and the
 * higest is '~'.  Note how a single recursive function implements
 * the complete evaluation algorithm.
 *
 */

/**********************************************************************/

#undef GENERATE_TABLES
#undef CALCULATE_TABLES
#define kTablesFile "ASMTAB.H"

/* To generate the tables file:
 * 1) #define GENERATE_TABLES
 * 2) Execute Assemble().
 * 3) Move the tables file to the source directory.
 * 4) #undef GENERATE_TABLES
 */

#ifdef GENERATE_TABLES
#define CALCULATE_TABLES
#endif

#include "ustdio.h"
#include "digits.h"
#include "string.h"
#include "file.h"
#include "sort.h"
#include "stack.h"
#include "search.h"
#include <ctype.h>
#include <stdlib.h>

#include "memory.h"
#include "system.h"
#include "cpu.h"
#include "asm.h"

#ifdef GENERATE_TABLES
#define Z80
#endif

#pragma mark String
typedef char String[65];
typedef char *StringPtr;

#pragma mark Name
typedef char Name[17];
typedef char *NamePtr;

/**********************************************************************/
#pragma mark *** OPERATION TABLE ***

#pragma mark Opcode
typedef struct Opcode Opcode;
typedef const Opcode *OpcodePtr;
struct Opcode {
	const char *name;
	const short code;
	const short maxCode;
};

#pragma mark Code
typedef struct Code Code;
typedef const Code *CodePtr;
struct Code {
	const short opcode;
	const short operand;
	const char *format;
};

#pragma mark Operand
typedef struct Operand Operand;
typedef const Operand *OperandPtr;
struct Operand {
	const char *format;
};

#pragma mark Register
typedef struct Register Register;
typedef const Register *RegisterPtr;
struct Register {
	const char *name;
};

#ifdef CALCULATE_TABLES

typedef struct Operation Operation;
typedef Operation *OperationPtr;
struct Operation {
	const char *code;
	const char *name8080;
#ifdef Z80
	const char *nameZ80;
#endif
};

#ifdef Z80
#define OPERATION(INDEX, CODE, JMP, N8080, NZ80, F8080, FZ80) \
	{ #CODE, N8080, NZ80 },
#else
#define OPERATION(INDEX, CODE, JMP, N8080, NZ80, F8080, FZ80) \
	{ #CODE, N8080 },
#endif

/* gOperations[] contains the raw operation table. */
static Operation gOperations[] = {
#include "op.h"
#ifdef Z80
#include "opcb.h"
#include "opdd.h"
#include "opddcb.h"
#include "oped.h"
#include "opfd.h"
#include "opfdcb.h"
	{ 0, 0, 0 }
#else
	{ 0, 0 }
#endif
};

/* GetOpcode() extracts the opcode from an operation name. */
static char *GetOpcode(const char *operationName)
{
	static char buffer[16];
	char *b = buffer;

	while (isalpha(*operationName))
		*b++ = *operationName++;

	*b = 0;

	return buffer;

}

/* GetOperand() extracts the operand from an operation name. */
static char *GetOperand(const char *operationName)
{

	while (isalpha(*operationName))
		operationName++;

	while (isspace(*operationName))
		operationName++;

	return (char *)operationName;

}

/* GetRegister() extracts the Nth register name from an operand. */
static char *GetRegister(const char *operand, int n)
{
	static char buffer[16];
	char *b;

	while (n-- >= 0) {
		while ((*operand != 0) && !isalpha(*operand))
			operand++;
		b = buffer;
		while (isalpha(*operand))
			*b++ = *operand++;
		*b = 0;
	}

	if (strlen(buffer) == 0)
		goto error;

	return buffer;

error:
	return 0;

}

/* CodeFormat() converts operation formats to code form. */
static int CodeFormat(char *codeFormat, const char *format)
{

	/* Interpret '%n' to adjust the format string. */
	while (*format != 0) {
		if (*format++ == '%') {
			switch (*format++) {
			case '1': /* CCNN => CC%1 (BYTE) */
				codeFormat[2] = '%';
				codeFormat[3] = '1';
				codeFormat[4] = 0;
				break;
			case '2': /* CCNNNN => CC%2 (WORD) */
				codeFormat[2] = '%';
				codeFormat[3] = '2';
				codeFormat[4] = 0;
				break;
			case '3': /* CCNN => CC%3 (PC OFFSET) */
				codeFormat[2] = '%';
				codeFormat[3] = '3';
				codeFormat[4] = 0;
				break;
			case '4': /* CCCCNNNN => CCCC%4 (WORD) */
				codeFormat[4] = '%';
				codeFormat[5] = '4';
				codeFormat[6] = 0;
				break;
			case '5': /* CCCCNNNN => CCCC%5NN (BYTE) */
				codeFormat[4] = '%';
				codeFormat[5] = '5';
				break;
			case '6': /* CCCCNN => CCCC%6 (BYTE) */
				codeFormat[4] = '%';
				codeFormat[5] = '6';
				codeFormat[6] = 0;
				break;
			case '7': /* CCCCNNNN => CCCCNN%7 (BYTE) */
				codeFormat[6] = '%';
				codeFormat[7] = '7';
				codeFormat[8] = 0;
				break;
			default:
				goto error;
			}
		}
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* OperandFormat() adjusts an operand format string. */
static void OperandFormat(char *operandFormat, const char *format)
{

	/* Replace '%n' with ':' to adjust the format string. */
	while (*format != 0) {
		if (*format == '%') {
			format++;
			*operandFormat++ = ':';
			format++;
		}
		else
			*operandFormat++ = *format++;
	}
	*operandFormat = 0;

}

/* SortCompareOpcode() compares two opcode name/format strings. */
static int SortCompareOpcode(const char *nameA, const char *nameB)
{
	char aa[16];
	char bb[16];

	/* Replace '%n' with ':'. */
	OperandFormat(aa, nameA);
	OperandFormat(bb, nameB);

	/* Compare the strings. */
	return strcmp(aa, bb);

}

/* OperationName8080() extracts the 8080 name from gOperations[]. */
static char *OperationName8080(const int i)
{
	static char buffer[32];

	strcpy(buffer, gOperations[i].name8080);

	return buffer;

}

/* SortCompareOpcode8080() compares 8080 opcodes for sorting. */
static int SortCompareOpcode8080(const short *a, const short *b)
{

	return SortCompareOpcode(gOperations[*a].name8080,
	                         gOperations[*b].name8080);

}

/* SortCompareOperand8080() compares 8080 operands for sorting. */
static int SortCompareOperand8080(const short *a, const short *b)
{

	return SortCompareOpcode(GetOperand(gOperations[*a].name8080),
	                         GetOperand(gOperations[*b].name8080));

}

#ifdef Z80

/* OperationNameZ80() extracts the Z80 name from gOperations[]. */
static const char *OperationNameZ80(const int i)
{
	static char buffer[32];

	strcpy(buffer, gOperations[i].nameZ80);

	return buffer;

}

/* SortCompareOpcodeZ80() compares Z80 opcodes for sorting. */
static int SortCompareOpcodeZ80(const short *a, const short *b)
{

	return SortCompareOpcode(gOperations[*a].nameZ80,
	                         gOperations[*b].nameZ80);

}

/* SortCompareOperandZ80() compares Z80 operands for sorting. */
static int SortCompareOperandZ80(const short *a, const short *b)
{

	return SortCompareOpcode(GetOperand(gOperations[*a].nameZ80),
	                         GetOperand(gOperations[*b].nameZ80));

}

#endif

/* SortCompareRegister() compares register names for sorting. */
static int SortCompareRegister(const RegisterPtr a, const RegisterPtr b)
{

	return strcmp(a->name, b->name);

}

/* SortCompareCode() compares code formats for sorting. */
static int SortCompareCode(const short *a, const short *b)
{

	return strcmp(gOperations[*a].code, gOperations[*b].code);

}

#pragma mark InstructionSet
typedef struct InstructionSet InstructionSet;
typedef InstructionSet *InstructionSetPtr;
struct InstructionSet {
	const char *name;
	const char *(*OperationName)(const int);
	SortCompareFunction SortCompareOpcode;
	SortCompareFunction SortCompareOperand;
	SortCompareFunction SortCompareCode;
	SortCompareFunction SortCompareRegister;
	int maxOperation;
	short *operationByOpcode;
	short *operationByOperand;
	int maxOpcode;
	Opcode *opcodes;
	int maxCode;
	Code *codes;
	int maxOperand;
	Operand *operands;
	int maxRegister;
	Register *registers;
};

static InstructionSetPtr g8080;
#ifdef Z80
static InstructionSetPtr gZ80;
#endif

/* FreeInstructionSet() deallocates an instruction set. */
static void FreeInstructionSet(InstructionSetPtr g)
{
	int i;

	if (g != 0) {
		if (g->operationByOpcode != 0)
			free(g->operationByOpcode);
		g->operationByOpcode = 0;
		if (g->operationByOperand != 0)
			free(g->operationByOperand);
		g->operationByOperand = 0;
		if (g->opcodes != 0) {
			for (i = 0; i < g->maxOpcode; i++)
				if (g->opcodes[i].name != 0)
					free((void *)g->opcodes[i].name);
			free(g->opcodes);
		}
		g->opcodes = 0;
		if (g->operands != 0) {
			for (i = 0; i < g->maxOperand; i++)
				if (g->operands[i].format != 0)
					free((void *)g->operands[i].format);
			free(g->operands);
		}
		g->operands = 0;
		if (g->codes != 0) {
			for (i = 0; i < g->maxCode; i++)
				if (g->codes[i].format != 0)
					free((void *)g->codes[i].format);
			free(g->codes);
		}
		g->codes = 0;
		if (g->registers != 0) {
			for (i = 0; i < g->maxRegister; i++)
				if (g->registers[i].name != 0)
					free((void *)g->registers[i].name);
			free(g->registers);
		}
		g->registers = 0;
		free(g);
	}

}

/* FreeInstructionSetTables() deallocates all instruction sets. */
static void FreeInstructionSetTables(void)
{

	FreeInstructionSet(g8080);
	g8080 = 0;
#ifdef Z80
	FreeInstructionSet(gZ80);
	gZ80 = 0;
#endif

}

#ifdef GENERATE_TABLES

/* GenerateInstructionSetTable() generates one instruction set. */
static int GenerateInstructionSetTable(FILE *f, InstructionSetPtr g)
{
	int i;

	/* Generate gOpcodes[]. */
	printf("%d %s OPCODES\n", g->maxOpcode, g->name);
	fprintf(f, "static const int gMaxOpcode%s = %d;\n",
	        g->name,
	        g->maxOpcode);
	fprintf(f, "static const Opcode gOpcodes%s[] = {\n", g->name);
	for (i = 0; i < g->maxOpcode; i++) {
		OpcodePtr opcode = &g->opcodes[i];
		fprintf(f, "	/* %03d */ { \"%s\", %d, %d },\n",
		        i,
		        opcode->name,
		        opcode->code,
		        opcode->maxCode);
	}
	fprintf(f, "	{ 0, 0 }\n");
	fprintf(f, "};\n\n");

	/* Generate gCodes[]. */
	printf("%d %s INSTRUCTIONS\n", g->maxCode, g->name);
	fprintf(f, "static const int gMaxCode%s = %d;\n",
	        g->name,
	        g->maxCode);
	fprintf(f, "static Code gCodes%s[] = {\n", g->name);
	for (i = 0; i < g->maxCode; i++) {
		CodePtr code = &g->codes[i];
		fprintf(f, "	/* %03d */ { %d, %d, \"%s\" },\n",
		        i,
		        code->opcode,
		        code->operand,
		        code->format);
	}
	fprintf(f, "	{ 0, 0, 0 }\n");
	fprintf(f, "};\n\n");

	/* Generate gOperands[]. */
	printf("%d %s OPERANDS\n", g->maxOperand, g->name);
	fprintf(f, "static const int gMaxOperand%s = %d;\n",
	        g->name,
	        g->maxOperand);
	fprintf(f, "static const Operand gOperands%s[] = {\n", g->name);
	for (i = 0; i < g->maxOperand; i++) {
		fprintf(f, "	/* %03d */ { \"%s\" },\n",
		        i,
		        g->operands[i].format);
	}
	fprintf(f, "	{ 0 }\n");
	fprintf(f, "};\n\n");

	/* Generate gRegisters[]. */
	printf("%d %s REGISTERS\n", g->maxRegister, g->name);
	fprintf(f, "static const int gMaxRegister%s = %d;\n",
	        g->name,
	        g->maxRegister);
	fprintf(f, "static const Register gRegisters%s[] = {\n", g->name);
	for (i = 0; i < g->maxRegister; i++) {
		fprintf(f, "	/* %03d */  { \"%s\" },\n",
		        i,
		        g->registers[i].name);
	}
	fprintf(f, "	0\n");
	fprintf(f, "};\n\n");

	/* All done, no error, return non-zero. */
	return 1;

}

/* GenerateInstructionSetTables() generates the tables file. */
static int GenerateInstructionSetTables(const char *file)
{
	FILE *f;

	printf("GENERATING %s\n", file);

	if ((f = fopen(file, "w")) == 0)
		goto error;

	fprintf(f, "/* %s GENERATED BY " kProgram " asm.c */\n\n", file);

	/* Generate the 8080 instruction set. */
	if (!GenerateInstructionSetTable(f, g8080))
		goto error;

#ifdef Z80

	/* Generate the Z80 instruction set. */
	fprintf(f, "#ifdef Z80\n\n");
	if (!GenerateInstructionSetTable(f, gZ80))
		goto error;
	fprintf(f, "#endif\n\n");

#endif

	fclose(f);

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	if (f != 0)
		fclose(f);
	return 0;

}

#endif

/* CalculateInstructionSet() calculates an instruction set. */
static int CalculateInstructionSet(InstructionSetPtr g)
{
	int i;
	int j;
	int k;
	char buffer[16];
	char *x;
	char *y;

	/* Calculate maxOperation. */
	g->maxOperation = 0;
	for (i = 0; gOperations[i].code != 0; i++) {
		if (strlen(g->OperationName(i)) > 0)
			g->maxOperation++;
	}

	/* Allocate operationByOpcode[], operationByOperand[]. */
	g->operationByOpcode = malloc(sizeof(short) * g->maxOperation);
	if (g->operationByOpcode == 0)
		goto error;
	g->operationByOperand = malloc(sizeof(short) * g->maxOperation);
	if (g->operationByOperand == 0)
		goto error;

	/* Init operationByOpcode[], operationByOperand[]. */
	g->maxOperation = 0;
	for (i = 0; gOperations[i].code != 0; i++) {
		if (strlen(g->OperationName(i)) > 0) {
			g->operationByOpcode[g->maxOperation] = i;
			g->operationByOperand[g->maxOperation++] = i;
		}
	}

	/* Sort operationByOpcode, operationByOperand[]. */
	Sort(g->operationByOpcode,
	     g->maxOperation,
	     sizeof(short),
	     g->SortCompareOpcode);
	Sort(g->operationByOperand,
	     g->maxOperation,
	     sizeof(short),
	     g->SortCompareOperand);

	/* Allocate registers[], partially unused. */
	g->registers = malloc(sizeof(Register) * g->maxOperation);
	if (g->registers == 0)
		goto error;

	/* Init registers[]. */
	for (i = 0; i < g->maxOperation; i++)
		g->registers[i].name = 0;

	/* Calculate registers[], calculate maxRegiser. */
	g->maxRegister = 0;
	for (i = 0; i < g->maxOperation; i++) {
		x = GetOperand(g->OperationName(g->operationByOpcode[i]));
		for (j = 0; (y = GetRegister(x, j)) != 0; j++) {
			for (k = 0; k < g->maxRegister; k++) {
				if (!strcmp(y, g->registers[k].name))
					break;
			}
			if (k == g->maxRegister) {
				g->registers[k].name = malloc(strlen(y) + 1);
				if (g->registers[k].name == 0)
					goto error;
				strcpy((char *)(g->registers[g->maxRegister++].name), y);
			}
		}
	}

	/* Sort registers[]. */
	Sort(g->registers,
	     g->maxRegister,
	     sizeof(char *),
	     g->SortCompareRegister);

	/* Calculate maxOperand. */
	g->maxOperand = 0;
	x = buffer;
	strcpy(x, "$");
	for (i = 0; i < g->maxOperation; i++) {
		y = GetOperand(g->OperationName(g->operationByOperand[i]));
		if (strcmp(x, y)) {
			g->maxOperand++;
			strcpy(x, y);
		}
	}

	/* Allocate operands[]. */
	g->operands = malloc(sizeof(Operand) * g->maxOperand);
	if (g->operands == 0)
		goto error;

	/* Init operands[]. */
	for (i = 0; i < g->maxOperand; i++)
		g->operands[i].format = 0;

	/* Calculate operands[]. */
	g->maxOperand = 0;
	x = "$";
	for (i = 0; i < g->maxOperation; i++) {
		y = GetOperand(g->OperationName(g->operationByOperand[i]));
		if (strcmp(x, y)) {
			if ((x = malloc(strlen(y) + 1)) == 0)
				goto error;
			strcpy(x, y);
			g->operands[g->maxOperand++].format = x;
		}
	}

	/* Calculate maxOpcode. */
	g->maxOpcode = 0;
	x = buffer;
	strcpy(x, "$");
	for (i = 0; i < g->maxOperation; i++) {
		y = GetOpcode(g->OperationName(g->operationByOpcode[i]));
		if (strcmp(x, y)) {
			g->maxOpcode++;
			strcpy(x, y);
		}
	}

	/* Allocate opcodes[]. */
	g->opcodes = malloc(sizeof(Opcode) * g->maxOpcode);
	if (g->opcodes == 0)
		goto error;

	/* Init opcodes[]. */
	for (i = 0; i < g->maxOpcode; i++) {
		g->opcodes[i].name = 0;
		*(short *)&(g->opcodes[i].code) = g->maxOperation;
		*(short *)&(g->opcodes[i].maxCode) = 0;
	}

	/* Calculate opcodes[]. */
	g->maxOpcode = 0;
	x = "$";
	for (i = 0; i < g->maxOperation; i++) {
		y = GetOpcode(g->OperationName(g->operationByOpcode[i]));
		if (strcmp(x, y)) {
			if ((x = malloc(strlen(y) + 1)) == 0)
				goto error;
			strcpy(x, y);
			g->opcodes[g->maxOpcode++].name = x;
		}
	}

	/* Calculate maxCode. */
	g->maxCode = g->maxOperation;

	/* Allocate codes[]. */
	if ((g->codes = malloc(sizeof(Code) * g->maxCode)) == 0)
		goto error;

	/* Init codes[]. */
	for (i = 0; i < g->maxCode; i++) {
		*(short *)&(g->codes[i].opcode) = 0;
		*(short *)&(g->codes[i].operand) = 0;
		g->codes[i].format = 0;
	}

	/* Calculate codes[]. */
	for (i = 0; i < g->maxOperation; i++) {
		OperationPtr operation = &gOperations[g->operationByOpcode[i]];
		char *opcode =
			GetOpcode(g->OperationName(g->operationByOpcode[i]));
		char *operand =
			GetOperand(g->OperationName(g->operationByOpcode[i]));
		Code *code = (Code *)&g->codes[i];
		if ((code->format = malloc(strlen(operation->code) + 1)) == 0)
			goto error;
		strcpy((char *)(code->format), operation->code);
		for (j = 0; j < g->maxOpcode; j++)
			if (!strcmp(g->opcodes[j].name, opcode)) {
				*(short *)&(code->opcode) = j;
				break;
			}
		if (j >= g->maxOpcode)
			goto error;
		if (g->opcodes[j].code == g->maxOperation) {
			*(short *)&(g->opcodes[j].code) = i;
			*(short *)&(g->opcodes[j].maxCode) = i;
		}
		(*(short *)&(g->opcodes[j].maxCode))++;
		for (j = 0; j < g->maxOperand; j++)
			if (!strcmp(g->operands[j].format, operand)) {
				*(short *)&(code->operand) = j;
				break;
			}
		if (j >= g->maxOperand)
			goto error;
	}

	/* Adjust code formats. */
	for (i = 0; i < g->maxCode; i++) {
		x = (char *)g->operands[g->codes[i].operand].format;
		y = (char *)g->codes[i].format;
		if (!CodeFormat(y, x))
			goto error;
	}

	/* Adjust operand formats. */
	for (i = 0; i < g->maxOperand; i++) {
		x = (char *)g->operands[i].format;
		y = x;
		OperandFormat(y, x);
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* The 8080 instruction set. */
static const int gMaxOpcode8080;
static const Opcode *gOpcodes8080;
static const int gMaxCode8080;
static const Code *gCodes8080;
static const int gMaxOperand8080;
static const Operand *gOperands8080;
static const int gMaxRegister8080;
static const Register *gRegisters8080;

#ifdef Z80
/* The Z80 instruction set. */
static const int gMaxOpcodeZ80;
static const Opcode *gOpcodesZ80;
static const int gMaxCodeZ80;
static const Code *gCodesZ80;
static const int gMaxOperandZ80;
static const Operand *gOperandsZ80;
static const int gMaxRegisterZ80;
static const Register *gRegistersZ80;
#endif

/* CalculateInstructionSetTables() calculates the instruction tables. */
static int CalculateInstructionSetTables(void)
{

	/* Init the 8080 instruction set. */
	if ((g8080 = malloc(sizeof(InstructionSet))) == 0)
		goto error;
	g8080->name = "8080";
	g8080->OperationName = OperationName8080;
	g8080->SortCompareOpcode =
		(SortCompareFunction)SortCompareOpcode8080;
	g8080->SortCompareOperand =
		(SortCompareFunction)SortCompareOperand8080;
	g8080->SortCompareCode =
		(SortCompareFunction)SortCompareCode;
	g8080->SortCompareRegister =
		(SortCompareFunction)SortCompareRegister;
	g8080->maxOperation = 0;
	g8080->operationByOpcode = 0;
	g8080->operationByOperand = 0;
	g8080->maxOpcode = 0;
	g8080->opcodes = 0;
	g8080->maxCode = 0;
	g8080->codes = 0;
	g8080->maxOperand = 0;
	g8080->operands = 0;
	g8080->maxRegister = 0;
	g8080->registers = 0;

	/* Calculate the 8080 instruction set. */
	if (!CalculateInstructionSet(g8080))
		goto error;

	/* Export the 8080 instruction set. */
	*(int *)&gMaxOpcode8080 = g8080->maxOpcode;
	gOpcodes8080 = g8080->opcodes;
	*(int *)&gMaxCode8080 = g8080->maxCode;
	gCodes8080 = g8080->codes;
	*(int *)&gMaxOperand8080 = g8080->maxOperand;
	gOperands8080 = g8080->operands;
	*(int *)&gMaxRegister8080 = g8080->maxRegister;
	gRegisters8080 = g8080->registers;

#ifdef Z80

	/* Init the Z80 instruction set. */
	if ((gZ80 = malloc(sizeof(InstructionSet))) == 0)
		goto error;
	gZ80->name = "Z80";
	gZ80->OperationName = OperationNameZ80;
	gZ80->SortCompareOpcode =
		(SortCompareFunction)SortCompareOpcodeZ80;
	gZ80->SortCompareOperand =
		(SortCompareFunction)SortCompareOperandZ80;
	gZ80->SortCompareCode =
		(SortCompareFunction)SortCompareCode;
	gZ80->SortCompareRegister =
		(SortCompareFunction)SortCompareRegister;
	gZ80->maxOperation = 0;
	gZ80->operationByOpcode = 0;
	gZ80->operationByOperand = 0;
	gZ80->maxOpcode = 0;
	gZ80->opcodes = 0;
	gZ80->maxCode = 0;
	gZ80->codes = 0;
	gZ80->maxOperand = 0;
	gZ80->operands = 0;
	gZ80->maxRegister = 0;
	gZ80->registers = 0;
	
	/* Calculate the Z80 instruction set. */
	if (!CalculateInstructionSet(gZ80))
		goto error;

	/* Export the Z80 instruction set. */
	*(int *)&gMaxOpcodeZ80 = gZ80->maxOpcode;
	gOpcodesZ80 = gZ80->opcodes;
	*(int *)&gMaxCodeZ80 = gZ80->maxCode;
	gCodesZ80 = gZ80->codes;
	*(int *)&gMaxOperandZ80 = gZ80->maxOperand;
	gOperandsZ80 = gZ80->operands;
	*(int *)&gMaxRegisterZ80 = gZ80->maxRegister;
	gRegistersZ80 = gZ80->registers;

#endif

#ifdef GENERATE_TABLES
	/* Generate the instruction set tables file. */
	if (!GenerateInstructionSetTables(kTablesFile))
		goto error;
#endif

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

#else

/* Include the instruction set tables file. */
#include kTablesFile

#endif

/* The current instruction set. */
#pragma mark gOpcodes[]
static int gMaxOpcode;
static const Opcode *gOpcodes;
#pragma mark gCodes[]
static int gMaxCode;
static const Code *gCodes;
#pragma mark gOperands[]
static int gMaxOperand;
static const Operand *gOperands;
#pragma mark gRegisters[]
static int gMaxRegister;
static const Register *gRegisters;

/* SearchCompareCode() compares code operands for lookup. */
static int SearchCompareCode(const void *a, const void *b)
{
	int result;
	const char *aa = ((OperandPtr)a)->format;
	const char *bb = gOperands[((CodePtr)b)->operand].format;
	int valueA;
	int valueB;

	/* ':' matches any number */
	/* numbers match values */
	/* otherwise match strings */
	do {
		if (isdigit(*aa)) {
			if (*bb == ':') {
				aa += StringToInt(aa, &valueA);
				bb++;
				result = 0;
			}
			else if (isdigit(*bb)) {
				aa += StringToInt(aa, &valueA);
				bb += StringToInt(bb, &valueB);
				result = valueA - valueB;
			}
			else
				result = (*aa++ - *bb++);
		}
		else {
			result = (*aa - *bb++);
			if (*aa++ == 0)
				break;
		}
	} while (result == 0);

	/* All done, return the comparison result. */
	return result;

}

/* LookupCode() searches gCodes[] for an operand match. */
static CodePtr LookupCode(OpcodePtr opcodePtr, OperandPtr operandPtr)
{

	return BinarySearch(operandPtr,
	                    &gCodes[opcodePtr->code],
	                    opcodePtr->maxCode - opcodePtr->code,
	                    sizeof(Code),
	                    SearchCompareCode);

}

/* SearchCompareRegister() compares register names for lookup. */
static int SearchCompareRegister(const void *a, const void *b)
{

	return strcmp((const char *)a, ((RegisterPtr)b)->name);

}

/* LookupRegister() searches gRegisters[] for a name match. */
static RegisterPtr LookupRegister(char *name)
{

	return BinarySearch(name,
	                    gRegisters,
	                    gMaxRegister,
	                    sizeof(Register),
	                    SearchCompareRegister);

}

/* SearchCompareOpcode() compares opcode names for lookup. */
static int SearchCompareOpcode(char *a, OpcodePtr b)
{

	return strcmp(a, b->name);

}

/* LookupOpcode() searches gOpcodes[] for a name match. */
static OpcodePtr LookupOpcode(char *name)
{

	return BinarySearch(name,
	                    gOpcodes,
	                    gMaxOpcode,
	                    sizeof(Opcode),
	                    (SearchCompareFunction)SearchCompareOpcode);

}

/**********************************************************************/
#pragma mark *** SOURCE FILE STACK ***

typedef struct SourceFile SourceFile;
typedef SourceFile *SourceFilePtr;
struct SourceFile {
	FILE *file;
	char name[kMaxFileName];
	int lineNumber;
};

#define kMaxSourceFileStack 5
static Stack gSourceFileStack;
static SourceFile gSourceFile;

/* CloseSourceFile() closes the current source file. */
/* The previous source file is popped from the stack. */
static int CloseSourceFile(void)
{

	/* Close the current source file if it is open. */
	if (gSourceFile.file != 0) {
		fclose(gSourceFile.file);
		gSourceFile.file = 0;
		strcpy(gSourceFile.name, "");
		gSourceFile.lineNumber = 0;
	}

	/* Pop the previous source file from the source file stack. */
	if (!StackPop(&gSourceFileStack, &gSourceFile))
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* OpenSourceFile() opens a source input file. */
/* The current source file is pushed onto the stack. */
static int OpenSourceFile(const char *file)
{
	static SourceFile *sourceFileStackData = 0;

  if (!sourceFileStackData) {
    sourceFileStackData = calloc(sizeof(SourceFile), kMaxSourceFileStack);
  }

	/* Reset the source file stack if no source file is open. */
	if (gSourceFile.file == 0) {
		StackReset(&gSourceFileStack,
		           sourceFileStackData,
		           kMaxSourceFileStack,
		           sizeof(*sourceFileStackData));
	}
	/* Otherwise, push the current source file onto the stack. */
	else if (!StackPush(&gSourceFileStack, &gSourceFile))
		goto error;

	/* Open the source input file. */
	if ((gSourceFile.file = FOpenPath(file, "r")) == 0)
		/* Generate the kSYSTEMEQU file if it does not exist. */
		if (!strcasecmp(file, kSYSTEMEQU))
			if (GenerateSystemEqu(file))
				gSourceFile.file = fopen(file, "r");
	if (gSourceFile.file == 0)
		goto error;

	/* Note the source file name. */
	strncpy(gSourceFile.name, file, sizeof(gSourceFile.name));

	/* Ready for ReadSourceLine(). */
	gSourceFile.lineNumber = 0;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	if (gSourceFile.file == 0)
		CloseSourceFile();
	return 0;

}

static int gSourceEnd;
static char gSourceLine[256];
static char *gNextChar;

/* ReadSourceLine() reads the next line from the source input file. */
static int ReadSourceLine(void)
{
	char *nl;
	char *cr;

	/* Error if gSourceEnd is set. See DirectiveEND(). */
	if (gSourceEnd)
		goto error;

	/* Read the next line, pop source files if EOF. */
	while (fgets(gSourceLine, sizeof(gSourceLine), gSourceFile.file) == 0);

  if (!CloseSourceFile())
    goto error;

	/* Count this source line. */
	gSourceFile.lineNumber++;

	/* Remove any carriage return or linefeed from the line. */
	if ((cr = strchr(gSourceLine, '\r')) != 0)
		*cr = 0;
	else if ((nl = strchr(gSourceLine, '\n')) != 0)
		*nl = 0;

	/* The source line is ready to be parsed. */
	gNextChar = gSourceLine;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** ERROR REPORING ***

static int gFinal;
static char gErrorMessage[7];
static int gErrorCount;

enum ErrorCode {
	kDataError = 'D',
	kExpressionError = 'E',
	kLabelError = 'L',
	kNotImplemented = 'N',
	kOverflow = 'O',
	kPhaseError = 'P',
	kRegisterError = 'R',
	kSyntaxError = 'S',
	kValueError = 'V',
	kUnknownError = 'X'
};

/* DisplayError() displays the source line and error position. */
static void DisplayError(char errorCode)
{
	int n;

	/* Display the source line. */
	printf("%s\n", gSourceLine);

	/* Point to the bad spot in the source line. */
	n = gNextChar - gSourceLine;
	while (n-- > 0)
		printf(" ");
	printf("^\n");

	/* Describe the source line location and its error. */
	printf("?FILE \"%s\" LINE %d: ",
	       gSourceFile.name,
	       gSourceFile.lineNumber);
	switch (errorCode) {
	case kDataError:
		printf("DATA ERROR");
		break;
	case kExpressionError:
		printf("EXPRESSION ERROR");
		break;
	case kLabelError:
		printf("LABEL ERROR");
		break;
	case kNotImplemented:
		printf("NOT IMPLEMENTED");
		break;
	case kOverflow:
		printf("OVERFLOW");
		break;
	case kPhaseError:
		printf("PHASE ERROR");
		break;
	case kRegisterError:
		printf("REGISTER ERROR");
		break;
	case kSyntaxError:
		printf("SYNTAX ERROR");
		break;
	case kValueError:
		printf("VALUE ERROR");
		break;
	case kUnknownError:
		printf("UNKNOWN ERROR");
		break;
	}
	printf("\n");

}

/* Error() reports errors to the gErrorMessage[] buffer. */
static void Error(char errorCode)
{
	int n;

	if (gFinal) {

		/* Display the first 5 errors. */
		if (gErrorCount++ < 5)
			DisplayError(errorCode);

		/* Handle gErrorMessage[] overflow. */
		n = strlen(gErrorMessage);
		if (n == (sizeof(gErrorMessage) - 1)) {
			errorCode = '*';
			n--;
		}

		gErrorMessage[n++] = errorCode;
		gErrorMessage[n] = 0;

	}

}

/**********************************************************************/
#pragma mark *** LISTING GENERATOR ***

#define kListLinesPerPage 55

enum ListMode {
	kListNone,
	kListSource,
	kListSourceAddress,
	kListSourceValue,
	kListSourceAddressCode
};

static FILE *gListFile;
static String gListTitle;
static int gListMode;
static int gListAddress;
static long gListLineNumber;
static int gListPageNumber;
static int gListTopOfPage;
static Byte gListCodeBuffer[256];
static int gListCodeIndex;

/* CloseListFile() closes the list output file. */
static void CloseListFile(void)
{

	if (gListFile != 0) {

		fclose(gListFile);

		gListFile = 0;

	}

}

/* OpenListFile() opens the list output file. */
static int OpenListFile(const char *file)
{

	CloseListFile();

	if ((gListFile = fopen(file, "w")) == 0)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* ListTopOfPage() starts a new listing page. */
static void ListTopOfPage(int formFeed)
 {

	if (!formFeed) {
		gListPageNumber = 1;
		gListLineNumber = 1;
	}

	gListTopOfPage = kListLinesPerPage;

	fprintf(gListFile,
	        "%s\n"
	        " %-40s USIM ASSEMBLER      PAGE %3d\n"
	        "\n"
	        "\n",
	        formFeed ? "\f" : "",
	        gListTitle,
	        gListPageNumber++);

}

/* ListSourceLine() lists the current source line. */
static void ListSourceLine(void)
{

	/* Do nothing if list mode is <NONE> */
	if (gListMode != kListNone) {

		/* Top of page? */
		if (gListTopOfPage == 0)
			ListTopOfPage(0);
		else if (--gListTopOfPage == 0)
			ListTopOfPage(1);

		/* Start the line if listing source only. */
		if (gListMode == kListSource)
			fprintf(gListFile, "%-6s       ",
			        gErrorMessage);
		/* Start the line if listing source and value. */
		else if (gListMode == kListSourceValue)
			fprintf(gListFile, "%-6s %04X= ",
			        gErrorMessage,
			        gListAddress & 0xFFFF);
		/* Start the line if listing source and address. */
		else
			fprintf(gListFile, "%-6s %04X  ",
			        gErrorMessage,
			        gListAddress & 0xFFFF);

		/* Finish the line if not listing code */
		if (gListMode != kListSourceAddressCode)
			fprintf(gListFile, "%9s%4d  %s\n",
			        "",
			        gListLineNumber,
			        gSourceLine);
		/* Finish the line if listing address and code. */
		else {
			/* Get four bytes of code. */
			int base = 0;
			int b3 = gListCodeBuffer[base + 3] & 0xFF;
			int b2 = gListCodeBuffer[base + 2] & 0xFF;
			int b1 = gListCodeBuffer[base + 1] & 0xFF;
			int b0 = gListCodeBuffer[base + 0] & 0xFF;
			/* List up to four bytes of code. */
			switch(gListCodeIndex) {
			case 0:
				fprintf(gListFile, "%9s", 
				        ""); 
				break;
			case 1:
				fprintf(gListFile, "%02X%7s",
				        b0, ""); 
				break;
			case 2:
				fprintf(gListFile, "%02X%02X%5s",
				        b0, b1, ""); 
				break;
			case 3:
				fprintf(gListFile, "%02X%02X%02X%3s",
				        b0, b1, b2, ""); 
				break;
			default:
				fprintf(gListFile, "%02X%02X%02X%02X%1s",
				        b0, b1, b2, b3, "");
			}
			/* Finish with the line number and source text. */
			fprintf(gListFile, "%4d  %s\n",
			        gListLineNumber,
			        gSourceLine);
			/* List more lines of code if needed... */
			while ((gListCodeIndex -= 4) > 0) {
				base += 4;
				/* Top of page? */
				if (--gListTopOfPage == 0)
					ListTopOfPage(1);
				/* Start the line listing address and code. */
				fprintf(gListFile, "%6s %04X  ",
				        "",
				        gListAddress + base);
				/* Get up to four bytes of code. */
				switch(gListCodeIndex) {
				default:
					b3 = gListCodeBuffer[base + 3] & 0xFF;
				case 3:
					b2 = gListCodeBuffer[base + 2] & 0xFF;
				case 2:
					b1 = gListCodeBuffer[base + 1] & 0xFF;
				case 1:
					b0 = gListCodeBuffer[base + 0] & 0xFF;
				case 0:
					break;
				}
				/* List up to four bytes of code. */
				switch(gListCodeIndex) {
				case 0:	
					break;
				case 1:
					fprintf(gListFile, "%02X\n",
					        b0);
					break;
				case 2:
					fprintf(gListFile, "%02X%02X\n",
					        b0, b1);
					break;
				case 3:
					fprintf(gListFile, "%02X%02X%02X\n",
					        b0, b1, b2);
					break;
				default:
					fprintf(gListFile, "%02X%02X%02X%02X\n",
					        b0, b1, b2, b3);
				}
			}
		}

	}

	/* Clear the error message buffer. */
	strcpy(gErrorMessage, "");

}

/**********************************************************************/
#pragma mark *** CODE GENERATOR ***

static void (*gWrByte)(Word address, Byte value);

/* WriteCodeBytes() emits code directly to memory. */
static void WriteCodeBytes(Word address, Byte *buffer, int count)
{
	int i;

	for (i = 0; i < count; i ++)
		gWrByte(address + i, buffer[i]);

}

static FILE *gCodeFile;

static int gCodeIndex;

/* WriteCodeFile() emits code to the code output file. */
static void WriteCodeFile(Word address, Byte *buffer, int count)
{
	int i;

	fprintf(gCodeFile, (gCodeIndex++ % 16) ? " " : "");
	fprintf(gCodeFile, "0x%02X,", count & 0xFF);
	fprintf(gCodeFile, (gCodeIndex % 16) ? "" : "\n");

	fprintf(gCodeFile, (gCodeIndex++ % 16) ? " " : "");
	fprintf(gCodeFile, "0x%02X,", (address >> 8) & 0xFF);
	fprintf(gCodeFile, (gCodeIndex % 16) ? "" : "\n");

	fprintf(gCodeFile, (gCodeIndex++ % 16) ? " " : "");
	fprintf(gCodeFile, "0x%02X,", (address >> 0) & 0xFF);
	fprintf(gCodeFile, (gCodeIndex % 16) ? "" : "\n");

	for (i = 0; i < count; ++i) {
		fprintf(gCodeFile, (gCodeIndex++ % 16) ? " " : "");
		fprintf(gCodeFile, "0x%02X,", buffer[i] & 0xFF);
		fprintf(gCodeFile, (gCodeIndex % 16) ? "" : "\n");
	}

}

/* CloseCodeFile() closes the code output file. */
static void CloseCodeFile(void)
{

	if (gCodeFile != 0) {
		fprintf(gCodeFile, (gCodeIndex++ % 16) ? " " : "");
		fprintf(gCodeFile, "0x%02X\n", 0);
		fclose(gCodeFile);
	}

	gCodeFile = 0;
}

/* OpenCodeFile() opens the code output file. */
static int OpenCodeFile(const char *file)
{

	printf("GENERATING %s\n", file);

	if ((gCodeFile = fopen(file, "w")) == 0)
		goto error;

	fprintf(gCodeFile, "/* %s GENERATED BY " kProgram " asm.c */\n\n",
	        file);

	gCodeIndex = 0;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

static FILE *gObjectFile;

/* WriteObjectFile() emits code to the object output file. */
static void WriteObjectFile(Word address, Byte *buffer, int count)
{
	int i;
	int cksum;

	/* Write one HEX record. See AssembleHexRecord(). */

	cksum = 0;

	fprintf(gObjectFile, ":%02X%04X00",
	        count,
	        address);

	cksum += count;
	cksum += ((address >> 8) & 0xFF);
	cksum += (address & 0xFF);

	for (i = 0; i < count; ++i) {
		fprintf(gObjectFile, "%02X", buffer[i]);
		cksum += buffer[i];
	}

	fprintf(gObjectFile, "%02X\n" , (-cksum) & 0xFF);

}

/* CloseObjectFile() closes the object output file. */
static void CloseObjectFile(void)
{

	if (gObjectFile != 0) {
		fprintf(gObjectFile, ":00000000\n");
		fclose(gObjectFile);
	}

	gObjectFile = 0;

}

/* OpenObjectFile() opens the object output file. */
static int OpenObjectFile(const char *file)
{

	if ((gObjectFile = fopen(file, "w")) == 0)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

#define kMaxEmitCodeBuffer 16
static int gEmitCodeIndex;
static Byte gEmitCodeBuffer[kMaxEmitCodeBuffer];
static Word gEmitCodeAddress;
static int gMaxAddress;
static int gNextEmitAddress;
static int gCurrentAddress;
static int gStripZeros;
static Word gEmitCodeBias;

/* EmitCodeBuffer() flushes the emit code buffer. */
static void EmitCodeBuffer(void)
{
	int writeCodeBuffer;
	int i;

	if (gEmitCodeIndex > 0) {

		/* Disable writing zero code buffers if gStripZeros is set. */
		writeCodeBuffer = 1;
		if (gStripZeros) {
			writeCodeBuffer = 0;
			for (i = 0; i < gEmitCodeIndex; i++) {
				if (gEmitCodeBuffer[i] != 0) {
					writeCodeBuffer = 1;
					break;
				}
			}
		}

		/* Write the emit code buffer. */
		if (writeCodeBuffer) {
			/* Write to memory... */
			if (gWrByte != 0)
				WriteCodeBytes(gEmitCodeAddress + gEmitCodeBias,
				               gEmitCodeBuffer,
				               gEmitCodeIndex);
			/* Write to the code output file... */
			if (gCodeFile != 0)
				WriteCodeFile(gEmitCodeAddress + gEmitCodeBias,
				              gEmitCodeBuffer,
				              gEmitCodeIndex);
			/* Write to the object output file... */
			if (gObjectFile != 0)
				WriteObjectFile(gEmitCodeAddress + gEmitCodeBias,
				                gEmitCodeBuffer,
				                gEmitCodeIndex);
		}


	}

	/* Reset the emit code buffer. */
	gEmitCodeIndex = 0;
	gNextEmitAddress = gCurrentAddress;

}

/* EmitByte() emits one byte. */
static void EmitByte(int byte)
{

	/* Emit code only if this is the final assembly pass. */
	if (gFinal) {

		/* Note byte in the list code buffer. */
		if (gListCodeIndex < sizeof(gListCodeBuffer))
			gListCodeBuffer[gListCodeIndex++] = byte;

		/* Flush the emit code buffer if it is full. */
		if (gEmitCodeIndex >= kMaxEmitCodeBuffer)
			EmitCodeBuffer();
		/* Flush the emit code buffer if the address changed. */
		else if (gCurrentAddress != gNextEmitAddress)
			EmitCodeBuffer();

		/* Note the address of the first byte. */
		if (gEmitCodeIndex == 0)
			gEmitCodeAddress = gCurrentAddress;
		gEmitCodeBuffer[gEmitCodeIndex++] = byte;

		/* Note the maximum address seen. */
		if (gCurrentAddress > gMaxAddress)
			gMaxAddress = gCurrentAddress;

	}

	/* Increment the gCurrentAddress. */
	gCurrentAddress++;
	if (gCurrentAddress > 0xFFFF)
		gCurrentAddress = 0;

	gNextEmitAddress = gCurrentAddress;

}

/* EmitWord() emits a two byte word. */
static void EmitWord(int word)
{
	WordBytes x;

	x.word = word;
	EmitByte(x.byte.low);
	EmitByte(x.byte.high);

}

/* GenerateCode() generates instruction set code */
static int GenerateCode(CodePtr codePtr, OperandPtr operandPtr)
{
	OpcodePtr opcodePtr = &gOpcodes[codePtr->opcode];
	int valueA;
	int isValueA;
	int valueB;
	int isValueB;
	const char *f;
	char buffer[3];
	char *b;
	int code;

	/* Extract any numeric arguments from the operand format. */
	f = operandPtr->format;
	while ((*f != 0) && !isdigit(*f))
		f++;
	f += (isValueA = StringToInt(f, &valueA));
	while ((*f != 0) && !isdigit(*f))
		f++;
	f += (isValueB = StringToInt(f, &valueB));

	/* Evaluate the code format, generate code bytes. */
	f = codePtr->format;
	while (*f != 0) {
		if (*f == '%') {
			f++;
			switch (*f++) {
			case '1': /* CCNN => CC%1 (BYTE) */
			case '5': /* CCCCNNNN => CCCC%5NN (BYTE) */
			case '6': /* CCCCNN => CCCC%6 (BYTE) */
				if (!isValueA)
					goto unknownError;
				EmitByte(valueA);
				if (!IsChar(valueA))
					goto overflow;
				break;
			case '2': /* CCNNNN => CC%2 (WORD) */
			case '4': /* CCCCNNNN => CCCC%4 (WORD) */
				if (!isValueA)
					goto unknownError;
				EmitWord(valueA);
				if (!IsShort(valueA))
					goto overflow;
				break;
			case '3': /* CCNN => CC%3 (PC OFFSET) */
				if (!isValueA)
					goto unknownError;
				valueA = valueA - gCurrentAddress;
				EmitByte(valueA);
				if (!IsChar(valueA))
					goto overflow;
				break;
			case '7': /* CCCCNNNN => CCCCNN%7 (BYTE) */
				if (!isValueB)
					goto unknownError;
				EmitByte(valueA);
				if (!IsChar(valueB))
					goto overflow;
				break;
			default:
				goto unknownError;
			}
		}
		else {
			b = buffer;
			*b++ = *f++;
			*b++ = *f++;
			*b = 0;
			if (!DigitsToInt(buffer, &code, 16))
				goto unknownError;
			EmitByte(code);
			if (!IsChar(code))
				goto unknownError;
		}
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Unknown Error */
unknownError:
	Error(kUnknownError);
	goto error;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** SYMBOL TABLE ***

enum SymbolState {
	kSymbolUNDEFINED = 0,
	kSymbolLABEL = 1,
	kSymbolEQU = 2,
	kSymbolSET = 3
};

#pragma mark Symbol
typedef struct Symbol Symbol;
typedef Symbol *SymbolPtr;
struct Symbol {
	Name name;
	int state;
	long value;
};

#pragma mark HashEntry
typedef struct HashEntry HashEntry;
typedef HashEntry *HashEntryPtr;
struct HashEntry {
	Symbol symbol;
	HashEntryPtr left;
	HashEntryPtr right;
};

#pragma mark gHashTable[]
#define kMaxHashTable 27
static HashEntryPtr gHashTable[kMaxHashTable]; 

/* FreeSymbolTable() deallocates the symbol table. */
static void FreeSymbolTable(void)
{
	int i;

	for (i = 0; i < kMaxHashTable; i++) {
		HashEntryPtr h = gHashTable[i];
		while (h != 0) {
			HashEntryPtr right = h->right;
			free(h);
			h = right;
		}
		gHashTable[i] = 0;
	}

}

/* LookupSymbol() searches the symbol table for a name match. */
/* A new symbol will be allocated if an existing one is not found. */
static SymbolPtr LookupSymbol(char *name)
{
	HashEntryPtr h;
	HashEntryPtr left;
	int hashval;

	if ((*name >= 'A') && (*name <= 'Z'))
		hashval = *name - 'A';
	else
		hashval = 27;

	for (h = gHashTable[hashval]; h != 0; h = h->right)
		if (!strcmp(name, h->symbol.name))
			goto found;

	if ((h = (HashEntryPtr)malloc(sizeof(HashEntry))) == 0)
		goto error;

	strcpy(h->symbol.name, name);
	h->symbol.state = kSymbolUNDEFINED;
	h->symbol.value = 0;

	h->right = 0;
	h->left  = 0;

	if (gHashTable[hashval] == 0)
		gHashTable[hashval] = h;
	else if (strcmp(name, gHashTable[hashval]->symbol.name) > 0) {
		gHashTable[hashval]->left = h;
		h->right = gHashTable[hashval];
		gHashTable[hashval] = h;
	} 
	else {
		for (left = gHashTable[hashval];
		     left != 0;
		     left = left->right) {
			if (left->right == 0) {
				left->right = h;
				h->left  = left;
				break;	
			}
			else if (strcmp(name, left->right->symbol.name) < 0) {
				h->right = left->right;
				left->right->left = h;
				left->right = h;
				h->left  = left;
				break;
			}
		}
	}

found:
	return &h->symbol;

error:
	return 0;

}

static void ListSymbolTable(void)
{
	HashEntryPtr h ;
	int line;
	int i;
	int count;
	int pageNumber;

	pageNumber = 1;
	count = 1;
	line = kListLinesPerPage;
	for (i = 0; i < kMaxHashTable; i++) {
		for (h = gHashTable[i]; h; h = h->right) {
			if (line >= kListLinesPerPage) {
				fprintf(gListFile,
				        "\f\n"
				        " %-40s SYMBOL TABLE        PAGE %3d\n"
				        "\n"
				        "\n",
				        gListTitle,
				        pageNumber++);
				line = 0;
			}
			fprintf(gListFile, "%16s = %04X  ",
			        h->symbol.name,
			        h->symbol.value);
			if (count >= 4) {
				line++;
				count = 0;
				fprintf(gListFile, "\n");
			}
			count++;
		}
	}

}

/**********************************************************************/
#pragma mark *** PARSER UTILITIES ***

static int ParseSpace(void)
{
	char *putBack = gNextChar;

	while ((*gNextChar != 0) && isspace(*gNextChar))
		*gNextChar++;

	if (*gNextChar == 0)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	gNextChar = putBack;
	return 0;

}

static int ParseChar(char c)
{
	char *putBack = gNextChar;

	if (*gNextChar++ != c)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	gNextChar = putBack;
	return 0;

}

static NamePtr ParseName(void)
{
	static Name name;
	char *putBack = gNextChar;
	char *n = name;
	int i;

	if (!ParseSpace())
		goto error;

	if (!(isalpha(*gNextChar) || (*gNextChar == '.')))
		goto error;

	*n++ = toupper(*gNextChar++);
	i = 1;
	while (isalnum(*gNextChar) || (*gNextChar == '.') || (*gNextChar == '$')) {
		if (*gNextChar != '$') {
			if (i++ < (sizeof(Name) - 1))
				*n++ = toupper(*gNextChar);
		}
		gNextChar++;
	}
	*n = 0;

	return name;

error:
	gNextChar = putBack;
	return 0;

}

static int ParseByte(Byte *byte)
{
	char *putBack = gNextChar;
	char b[3];

	if ((b[0] = *gNextChar++) == 0)
		goto error;
	if ((b[1] = *gNextChar++) == 0)
		goto error;
	b[2] = 0;

	if (DigitsToChar(b, (char *)byte, 16) != 2)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	gNextChar = putBack;
	return 0;

}

static int ParseNumber(long *value)
{
	char *putBack = gNextChar;
	int n;

	if (!ParseSpace())
		goto error;

	gNextChar += (n = StringToLong(gNextChar, value));

	if (n == 0)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	gNextChar = putBack;
	return 0;

}

static StringPtr ParseString(void)
{
	static String string;
	char *putBack = gNextChar;
	char delimiter;
	int i;

	if (!ParseSpace())
		goto error;

	delimiter = *gNextChar++;
	if ((delimiter != '\"') && (delimiter != '\''))
		goto error;
	i = 0;
	while (*gNextChar != delimiter) {
		if (*gNextChar == 0)
			goto error;
		string[i++] = *gNextChar++;
		if (i == sizeof(string))
			goto error;
	}
	string[i] = 0;
	gNextChar++;

	return string;

error:
	gNextChar = putBack;
	return 0;

}

/**********************************************************************/
#pragma mark *** EXPRESSION EVALUATOR ***

static int gCalculate;

enum Operator {
	kBinaryEQ,
	kBinaryNE,
	kBinaryGT,
	kBinaryGE,
	kBinaryLE,
	kBinaryLT,
	kBinaryLogicalOR,
	kBinaryLogicalAND,
	kBinaryOR,
	kBinaryXOR,
	kBinaryAND,
	kBinarySHR,
	kBinarySHL,
	kBinaryPLUS,
	kBinaryMINUS,
	kBinaryMODULO,
	kBinaryDIVIDE,
	kBinaryMULTIPLY,
	kUnaryHIGH,
	kUnaryLOW,
	kUnaryPLUS,
	kUnaryMINUS,
	kUnaryNOT,
	kBinaryPriority = kBinaryMULTIPLY,
	kUnaryPriority = kUnaryNOT,
	kFirstUnary = kUnaryHIGH,
	kFirstBinary = kBinaryEQ
};

/* ParseOperator() parses an expression operator. */
static int ParseOperator(int priority, int *function)
{
	char *putBack = gNextChar;
	NamePtr namePtr;

	if (!ParseSpace() || (strchr(";!", *gNextChar)))
		goto error;

	switch (*gNextChar++) {

	case '+':
		if (priority <= kBinaryPriority)
			*function = kBinaryPLUS;
		else
			*function = kUnaryPLUS;
		break;

	case '-':
		if (priority <= kBinaryPriority)
			*function = kBinaryMINUS;
		else
			*function = kUnaryMINUS;
		break;

	case '~':
		*function = kUnaryNOT;
		break;

	case '*':
		*function = kBinaryMULTIPLY;
		break;

	case '/':
		*function = kBinaryDIVIDE;
		break;

	case '%':
		*function = kBinaryMODULO;
		break;

	case '^':
		*function = kBinaryXOR;
		break;

	case '&':
		if (*gNextChar == '&') {
			gNextChar++;
			*function = kBinaryLogicalAND;
		}
		else
			*function = kBinaryAND;
		break;

	case '|':
		if (*gNextChar == '|') {
			gNextChar++;
			*function = kBinaryLogicalOR;
		}
		else
			*function = kBinaryOR;
		break;

	case '=':
		if (*gNextChar == '=')
			gNextChar++;
		*function = kBinaryEQ;
		break;

	case '!':
		if (*gNextChar != '=') 
			goto error;
		*function = kBinaryNE;
		gNextChar++;
		break;

	case '>':
		if (*gNextChar == '=') {
			*function = kBinaryGE;
			gNextChar++;
		}
		else if (*gNextChar == '>') {
			*function = kBinarySHR;
			gNextChar++;
		}
		else
			*function = kBinaryGT;
		break;

	case '<':
		if (*gNextChar == '=') {
			*function = kBinaryLE;
			gNextChar++;
		}
		else if (*gNextChar == '<') {
			*function = kBinarySHL;
			gNextChar++;
		}
		else
			*function = kBinaryLT;
		break;

	default:
		gNextChar--;
		if ((namePtr = ParseName()) == 0)
			goto error;
		else if (!strcmp(namePtr, "SHL"))
			*function = kBinarySHL;
		else if (!strcmp(namePtr, "SHR"))
			*function = kBinarySHR;
		else if (!strcmp(namePtr, "AND"))
			*function = kBinaryAND;
		else if (!strcmp(namePtr, "OR"))
			*function = kBinaryOR;
		else if (!strcmp(namePtr, "XOR"))
			*function = kBinaryXOR;
		else if (!strcmp(namePtr, "MOD"))
			*function = kBinaryMODULO;
		else if (!strcmp(namePtr, "GT"))
			*function = kBinaryGT;
		else if (!strcmp(namePtr, "GE"))
			*function = kBinaryGE;
		else if (!strcmp(namePtr, "LE"))
			*function = kBinaryLE;
		else if (!strcmp(namePtr, "LT"))
			*function = kBinaryLT;
		else if (!strcmp(namePtr, "EQ"))
			*function = kBinaryEQ;
		else if (!strcmp(namePtr, "NE"))
			*function = kBinaryNE;
		else if (!strcmp(namePtr, "NOT"))
			*function = kUnaryNOT;
		else if (!strcmp(namePtr, "HIGH"))
			*function = kUnaryHIGH;
		else if (!strcmp(namePtr, "LOW"))
			*function = kUnaryLOW;
		else
			goto error;
	}

	if (*function != priority)
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	gNextChar = putBack;
	return 0;

}

/* ParseTerm() parses and evaluates an expression term. */
static int ParseTerm(int priority, long *result)
{
	char *putBack = gNextChar;
	int function;
	long value2;
	NamePtr namePtr;
	SymbolPtr symbolPtr;
	StringPtr stringPtr;

	if (priority <= kBinaryPriority) {
		if (!ParseTerm(priority + 1, result))
			goto error;
		while (ParseOperator(priority, &function)) {
			if (!ParseTerm(priority + 1, &value2))
				goto error;
			switch (function) {
			case kBinaryPLUS:
				*result = (*result + value2);
				break;
			case kBinaryMINUS:
				*result = (*result - value2);
				break;
			case kBinaryMULTIPLY:
				*result = (*result * value2);
				break;
			case kBinaryDIVIDE:
				if (value2 == 0)
					goto overflow;
				*result = (*result / value2);
				break;
			case kBinaryMODULO:
				if (value2 == 0)
					goto overflow;
				*result = (*result % value2);
				break;
			case kBinarySHL:
				*result = (*result << value2);
				break;
			case kBinarySHR:
				*result = (*result >> value2);
				break;
			case kBinaryOR:
				*result = (*result | value2);
				break;
			case kBinaryAND:
				*result = (*result & value2);
				break;
			case kBinaryXOR:
				*result = (*result ^ value2);
				break;
			case kBinaryLogicalOR:
				*result = (*result || value2);
				break;
			case kBinaryLogicalAND:
				*result = (*result && value2);
				break;
			case kBinaryEQ:
				*result = (*result == value2);
				break;
			case kBinaryNE:
				*result = (*result != value2);
				break;
			case kBinaryGT:
				*result = (*result > value2);
				break;
			case kBinaryGE:
				*result = (*result >= value2);
				break;
			case kBinaryLE:
				*result = (*result <= value2);
				break;
			case kBinaryLT:
				*result = (*result < value2);
				break;
			}
		}
	}
	else if (priority <= kUnaryPriority) {
		if (ParseOperator(priority, &function)) {
			if (!ParseTerm(kFirstUnary, result))
				goto error;
			switch (function) {
			case kUnaryHIGH:
				*result = (*result >> 8) & 0xFF;
				break;
			case kUnaryLOW:
				*result = (*result >> 0) & 0xFF;
				break;
			case kUnaryPLUS:
				*result = + *result;
				break;
			case kUnaryMINUS:
				*result = - *result;
				break;
			case kUnaryNOT:
				*result = ~ *result;
				break;
			}
		}
		else if (!ParseTerm(priority + 1, result))
			goto error;
	}
	else {
		if (!ParseSpace())
			goto expressionError;
		else if (ParseChar('(')) {
			if (!ParseTerm(kFirstBinary, result))
				goto error;
			if (!ParseSpace())
				goto expressionError;
			if (!ParseChar(')'))
				goto expressionError;
		}
		else if (ParseChar('$')) {
			if (gCalculate)
				goto expressionError;
			*result = gCurrentAddress;
		}
		else if ((namePtr = ParseName()) != 0) {
			if (gCalculate)
				goto expressionError;
			if ((symbolPtr = LookupSymbol(namePtr)) == 0)
				goto expressionError;
			if ((symbolPtr->state == kSymbolUNDEFINED) && gFinal)
				goto expressionError;
			*result = symbolPtr->value;
		}
		else if ((stringPtr = ParseString()) != 0) {
			switch (strlen(stringPtr)) {
			case 1:
				*result = stringPtr[0];
				break;
			case 2:
				*result = (stringPtr[0] << 8) | (stringPtr[1] << 0);
				break;
			default:
				goto expressionError;
			}
		}
		else if (!ParseNumber(result))
			goto expressionError;
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Expression Error */
expressionError:
	Error(kExpressionError);
	goto error;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	gNextChar = putBack;
	return 0;

}

/* ParseExpression() parses and evaluates an expression */
static int ParseExpression(long *result)
{

	/* Start the evauation at the first binary priority. */
	return ParseTerm(kFirstBinary, result);

}

/* Calculate() provides direct access to the expression evaluator. */
int CalculateExpression(char *buffer, long *result)
{

	/* Prepare the "source line". */
	strcpy(gSourceLine, buffer);
	gNextChar = gSourceLine;

	/* Prepare to Calculate... */
	gFinal = 0;
	gCalculate = 1;

	/* Parse and evaluate the expression. */
	if (!ParseExpression(result))
		goto error;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** ASSEMBLER DIRECTIVES ***

static int gIfDepth;
static int gIfFalse;

static int gEntryAddress;

enum DirectiveCode {
	kDirective8080,
	kDirectiveDB,
	kDirectiveDS,
	kDirectiveDW,
	kDirectiveELSE,
	kDirectiveELSEIF,
	kDirectiveEND,
	kDirectiveENDIF,
	kDirectiveEQU,
	kDirectiveIF,
	kDirectiveOPCODES,
	kDirectiveORG,
	kDirectiveSET,
	kDirectiveSOURCE,
	kDirectiveTITLE,
	kDirectiveZ80
};

typedef struct Directive Directive;
typedef Directive *DirectivePtr;
struct Directive {
	char *name;
	int code;
};

#pragma mark gDirectives[]
static Directive gDirectives[] = {
	/* 000 */ { ".8080",    kDirective8080    },
	/* 001 */ { ".DB",      kDirectiveDB      },
	/* 002 */ { ".DEFB",    kDirectiveDB      },
	/* 003 */ { ".DEFS",    kDirectiveDS      },
	/* 004 */ { ".DEFW",    kDirectiveDW      },
	/* 005 */ { ".DS",      kDirectiveDS      },
	/* 006 */ { ".DW",      kDirectiveDW      },
	/* 007 */ { ".ELSE",    kDirectiveELSE    },
	/* 008 */ { ".ELSEIF",  kDirectiveELSEIF  },
	/* 009 */ { ".END",     kDirectiveEND     },
	/* 010 */ { ".ENDIF",   kDirectiveENDIF   },
	/* 011 */ { ".EQU",     kDirectiveEQU     },
	/* 012 */ { ".IF",      kDirectiveIF      },
	/* 013 */ { ".OPCODES", kDirectiveOPCODES },
	/* 014 */ { ".ORG",     kDirectiveORG     },
	/* 015 */ { ".SET",     kDirectiveSET     },
	/* 016 */ { ".SOURCE",  kDirectiveSOURCE  },
	/* 017 */ { ".TITLE",   kDirectiveTITLE   },
	/* 018 */ { ".Z80",     kDirectiveZ80     },
	/* 019 */ { "DB",       kDirectiveDB      },
	/* 020 */ { "DEFB",     kDirectiveDB      },
	/* 021 */ { "DEFS",     kDirectiveDS      },
	/* 022 */ { "DEFW",     kDirectiveDW      },
	/* 023 */ { "DS",       kDirectiveDS      },
	/* 024 */ { "DW",       kDirectiveDW      },
	/* 025 */ { "ELSE",     kDirectiveELSE    },
	/* 026 */ { "ELSEIF",   kDirectiveELSEIF  },
	/* 027 */ { "END",      kDirectiveEND     },
	/* 028 */ { "ENDIF",    kDirectiveENDIF   },
	/* 029 */ { "EQU",      kDirectiveEQU     },
	/* 030 */ { "IF",       kDirectiveIF      },
	/* 031 */ { "MACLIB",   kDirectiveSOURCE  },
	/* 032 */ { "ORG",      kDirectiveORG     },
	/* 033 */ { "SET",      kDirectiveSET     },
	/* 034 */ { "TITLE",    kDirectiveTITLE   }
};
#define kMaxDirective (sizeof(gDirectives) / sizeof(Directive))

/* SearchCompareDirective() compares directive names for lookup. */
static int SearchCompareDirective(char *a, DirectivePtr b)
{

	return strcmp(a, b->name);

}

/* LookupDirective() searches gDirectives[] for a name match. */
static DirectivePtr LookupDirective(char *name)
{

	return BinarySearch(name,
	                    gDirectives,
	                    kMaxDirective,
	                    sizeof(Directive),
	                    (SearchCompareFunction)SearchCompareDirective);

}

/* Directive8080() enables the 8080 instruction set. */
static inline int Directive8080(void)
{

	/* Select the 8080 instruction set tables. */
  gMaxOpcode = gMaxOpcode8080;
	gOpcodes = gOpcodes8080;
	gMaxCode = gMaxCode8080;
	gCodes = gCodes8080;
	gMaxOperand = gMaxOperand8080;
	gOperands = gOperands8080;
	gMaxRegister = gMaxRegister8080;
	gRegisters = gRegisters8080;

	/* All done, no error, return non-zero. */
	return 1;

}

/* DirectiveZ80() enables the Z80 instruction set. */
static inline int DirectiveZ80(void)
{

#ifdef Z80

	/* Select the Z80 instruction set tables. */
	*(int *)&gMaxOpcode = gMaxOpcodeZ80;
	gOpcodes = gOpcodesZ80;
	*(int *)&gMaxCode = gMaxCodeZ80;
	gCodes = gCodesZ80;
	*(int *)&gMaxOperand = gMaxOperandZ80;
	gOperands = gOperandsZ80;
	*(int *)&gMaxRegister = gMaxRegisterZ80;
	gRegisters = gRegistersZ80;

#else

	goto notImplemented;

#endif

	/* All done, no error, return non-zero. */
	return 1;

	/* Not Implemented */
notImplemented:
	Error(kNotImplemented);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveDS() allocates uninitialized storage. */
static inline int DirectiveDS(void)
{
	long value;

	/* Error if bad <EXPRESSION>. */
	if (!ParseExpression(&value))
		goto error;
	/* Error if not <WORD> value. */
	if (!IsShort(value))
		goto overflow;
	/* Error if negative value. */
	if (value < 0)
		goto overflow;

	/* Allocate the ininitialized storage. */
	gCurrentAddress += value;

	/* Wrap address. */
	if (gCurrentAddress > 0xFFFF)
		gCurrentAddress -= 0xFFFF;

	/* All done, no error, return non-zero. */
	return 1;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveORG() sets the gCurrentAddress. */
static inline int DirectiveORG(void)
{
	long value;

	/* Error if bad <EXPRESSION>. */
	if (!ParseExpression(&value))
		goto error;
	/* Error if not <WORD> value. */
	if (!IsShort(value))
		goto overflow;

	/* Set gCurrentAddress. */
	gCurrentAddress = value & 0xFFFF;

	/* All done, no error, return non-zero. */
	return 1;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveIF() handles conditional assembly. */
static inline int DirectiveIF(void)
{
	long value;

	/* Error if bad <EXPRESSION>. */
	if (!ParseExpression(&value))
		goto error;

	/* Increase the depth. */
	gIfDepth++;

	/* If assembly is enabled... */
	if (!gIfFalse) {
		/* Disable assembly if <EXPRESSION> is zero. */
		if (value == 0)
			gIfFalse = gIfDepth;

	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveELSE() handles conditional assembly. */
static inline int DirectiveELSE(void)
{

	/* Error if no <IF>. */
	if (gIfDepth == 0)
		goto syntaxError;

	/* If assembly is enabled... */
	if (!gIfFalse)
		/* then disable assembly. */
		gIfFalse = gIfDepth;
	/* If assembly is not enabled at this depth... */
	else if (gIfFalse == gIfDepth)
		/* then enable assembly. */
		gIfFalse = 0;

	/* All done, no error, return non-zero. */
	return 1;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveELSEIF() handles conditional assembly. */
static inline int DirectiveELSEIF(void)
{
	long value;

	/* Error if bad <EXPRESSION>. */
	if (!ParseExpression(&value))
		goto error;

	/* If assembly is enabled... */
	if (!gIfFalse)
		/* then disable assembly. */
		gIfFalse = gIfDepth;
	/* If is assembly is not enabled at this depth... */
	else if (gIfFalse == gIfDepth) {
		/* then enable assembly if <EXPRESSION> is not zero. */
		if (value != 0)
			gIfFalse = 0;
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveENDIF() handles conditional assembly. */
static inline int DirectiveENDIF(void)
{

	/* Error if no <IF>. */
	if (gIfDepth == 0)
		goto syntaxError;

	/* If assembly is not enabled at this depth... */
	if (gIfFalse == gIfDepth)
		/* then enable assembly. */
		gIfFalse = 0;

	/* Decrease depth. */
	gIfDepth--;

	/* All done, no error, return non-zero. */
	return 1;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveSET() sets a symbol value. */
static inline int DirectiveSET(SymbolPtr symbolPtr)
{
	long value;

	/* Error if no <SYMBOL> is specified. */
	if (symbolPtr == 0)
		goto syntaxError;
	/* Error if bad <EXPRESSION>. */
	if (!ParseExpression(&value))
		goto error;
	/* Error if not <WORD> value. */
	if (!IsShort(value))
		goto overflow;

	/* Set value if symbol is undefined. */
	if (symbolPtr->state == kSymbolUNDEFINED) {
		symbolPtr->state = kSymbolSET;
		symbolPtr->value = value;
	}

	/* Error if defined symbol is not <SET>. */
	if (symbolPtr->state != kSymbolSET)
		goto unknownError;

	/* Allow <SET> symbol value to be changed. */
	symbolPtr->value = value;

	/* All done, no error, return non-zero. */
	return 1;

	/* Unknown Error */
unknownError:
	Error(kUnknownError);
	goto error;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveEQU() equates a symbol value. */
static inline int DirectiveEQU(SymbolPtr symbolPtr)
{
	long value;

	/* Error if no <SYMBOL> is specified. */
	if (symbolPtr == 0)
		goto syntaxError;
	/* Error if bad <EXPRESSION>. */
	if (!ParseExpression(&value))
		goto error;
	/* Error if not <WORD> value. */
	if (!IsShort(value))
		goto overflow;

	/* Equate value if symbol is undefined. */
	if (symbolPtr->state == kSymbolUNDEFINED) {
		symbolPtr->state = kSymbolEQU;
		symbolPtr->value = value;
	}

	/* Error if defined symbol is not <EQU>. */
	if (symbolPtr->state != kSymbolEQU)
		goto unknownError;

	/* Error if attempt to change <EQU> symbol value. */
	if (symbolPtr->value != value)
		goto phaseError;

	/* All done, no error, return non-zero. */
	return 1;

	/* Unknown Error */
unknownError:
	Error(kUnknownError);
	goto error;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Phase Error */
phaseError:
	Error(kPhaseError);
	goto error;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveOPCODES() writes the instruction set to a file. */
static inline int DirectiveOPCODES(void)
{
	FILE *f = 0;
	StringPtr file;
	const char *opcode;
	char operand[16];
	char code[16];
	char *b;
	int i;
	int j;

	/* Parse the output file name. */
	if ((file = ParseString()) == 0)
		if ((file = ParseName()) == 0)
			goto syntaxError;

	if (gFinal) {

		printf("GENERATING %s\n", file);

		/* Open the output file. */
		if ((f = fopen(file, "w")) == 0)
			goto unknownError;

		/* Note the instruction set selection. */
		if (gOpcodes == gOpcodes8080)
			fprintf(f, "        .8080\n");
#ifdef Z80
		if (gOpcodes == gOpcodesZ80)
			fprintf(f, "        .Z80\n");
#endif

		/* Output the instruction set. */
		for (i = 0; i < gMaxOpcode; i++) {
			opcode = gOpcodes[i].name;
			for (j = gOpcodes[i].code; j < gOpcodes[i].maxCode; j++) {
				/* Replace ':' with '1' in operand format string. */
				strcpy(operand, gOperands[gCodes[j].operand].format);
				for (b = operand; *b != 0; b++)
					if (*b == ':')
						*b = '1';
				/* Replace '%n' with 'NN' in code format string. */
				strcpy(code, gCodes[j].format);
				for (b = code; *b != 0; b++) {
					if (*b == '%') {
						*b++ = 'N';
						switch (*b) {
						case '1': /* CCNN => CC%1 (BYTE) */
						case '3': /* CCNN => CC%3 (PC OFFSET) */
						case '5': /* CCCCNNNN => CCCC%5NN (BYTE) */
						case '6': /* CCCCNN => CCCC%6 (BYTE) */
						case '7': /* CCCCNNNN => CCCCNN%7 (BYTE) */
							*b = 'N';
							break;
						case '2': /* CCNNNN => CC%2 (WORD) */
						case '4': /* CCCCNNNN => CCCC%4 (WORD) */
							*b++ = 'N';
							*b++ = 'N';
							*b++ = 'N';
							*b-- = 0;
							break;
						default:
							goto unknownError;
						}
					}
				}
				fprintf(f, "%8s%-8s%-8s; %s\n", "", opcode, operand, code);
			}
		}

		fprintf(f, "%8sEND\n", "");

		fclose(f);

	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Unknown Error */
unknownError:
	Error(kUnknownError);
	goto error;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Return 0 if there was an error. */
error:
	if (f != 0)
		fclose(f);
	return 0;

}

/* DirectiveTITLE() sets the listing title. */
static inline int DirectiveTITLE(void)
{
	StringPtr stringPtr;

	/* Parse the title text */
	if ((stringPtr = ParseString()) == 0)
		if ((stringPtr = ParseName()) == 0)
			goto syntaxError;

	/* Set the gListTitle. */
	strcpy(gListTitle, stringPtr);

	/* All done, no error, return non-zero. */
	return 1;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveSOURCE() includes source lines from another file. */
static inline int DirectiveSOURCE(void)
{
	StringPtr stringPtr;

	/* Parse the source file name. */
	if ((stringPtr = ParseString()) == 0)
		if ((stringPtr = ParseName()) == 0)
			goto syntaxError;

	/* Open the source file. */
	if (!OpenSourceFile(stringPtr))
		goto unknownError;

	/* All done, no error, return non-zero. */
	return 1;

	/* Unknown Error */
unknownError:
	Error(kUnknownError);
	goto error;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveEND() terminates assembly. */
static inline int DirectiveEND(void)
{
	char *putBack = gNextChar;
	long value;

	/* Set end of assembly source. */
	gSourceEnd = 1;

	/* Parse optional <ENTRY> address. */
	gEntryAddress = 0;
	if (ParseSpace() && (*gNextChar != ';') && (*gNextChar != '!')) {
		if (!ParseExpression(&value))
			goto error;
		if (!IsShort(value))
			goto overflow;
		gEntryAddress = value & 0xFFFF;
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	gNextChar = putBack;
	return 0;

}

/* DirectiveDB() allocates initialized byte storage. */
static inline int DirectiveDB(void)
{
	StringPtr stringPtr;
	long value;

	/* Generate initialized data bytes. */
	do {
		/* A string... */
		if ((stringPtr = ParseString()) != 0) {
			while (*stringPtr != 0)
				EmitByte(*stringPtr++);
		}
		/* A number... */
		else {
			if (!ParseExpression(&value))
				goto error;
			if (!IsChar(value))
				goto overflow;
			EmitByte(value);
		}
	} while (ParseSpace() && ParseChar(','));

	/* All done, no error, return non-zero. */
	return 1;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* DirectiveDB() allocates initialized word storage. */
static inline int DirectiveDW(void)
{
	long value;

	/* Generate initialized data words. */
	do {
		if (!ParseExpression(&value))
			goto error;
		if (!IsShort(value))
			goto overflow;
		EmitWord(value);
	} while (ParseSpace() && ParseChar(','));

	/* All done, no error, return non-zero. */
	return 1;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** ASSEMBLE SOURCE LINE ***

static int gDebug;

/* SetLabel() sets the value of a label symbol. */
static int SetLabel(SymbolPtr symbolPtr)
{

	/* If the symbol is undefined, define it to be a LABEL. */
	if (symbolPtr->state == kSymbolUNDEFINED) {
		symbolPtr->value = gCurrentAddress;
		symbolPtr->state = kSymbolLABEL;
	}

	/* Error if the symbol is not a LABEL. */
	if (symbolPtr->state != kSymbolLABEL)
		goto syntaxError;

	/* Error if the symbol value is not gCurrentAddress. */
	if (symbolPtr->value != gCurrentAddress)
		goto phaseError;

	/* All done, no error, return non-zero. */
	return 1;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Phase Error */
phaseError:
	Error(kPhaseError);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* ParseSourceLine() parses the initial line tokens. */
static inline int
	ParseSourceLine(SymbolPtr *symbolPtr,
	                DirectivePtr *directivePtr,
	                OpcodePtr *opcodePtr)
{
	NamePtr name;

	*opcodePtr = 0;
	*symbolPtr = 0;
	*directivePtr = 0;

	/* Error if there is nothing there. */
	if ((name = ParseName()) == 0)
		goto syntaxError;

	if ((*opcodePtr = LookupOpcode(name)) != 0)
		/* <OPCODE> */ ;
	else if ((*directivePtr = LookupDirective(name)) != 0)
		/* <DIRECTIVE> */;
	else if (LookupRegister(name) != 0)
		goto syntaxError;
	else if ((*symbolPtr = LookupSymbol(name)) == 0)
		goto unknownError;
	else if (*name == '.')
		goto syntaxError;
	else {
		if (!ParseChar(':'))
			/* <SYMBOL> */;
		else if (SetLabel(*symbolPtr))
			/* <LABEL>: */;
		else
			goto error;
		if (ParseSpace()) {
			if ((name = ParseName()) == 0)
				;
			else if ((*opcodePtr = LookupOpcode(name)) != 0)
				/* <OPCODE> */;
			else if ((*directivePtr = LookupDirective(name)) != 0)
				/* <DIRECTIVE> */;
			else
				goto syntaxError;
		}
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Unknown Error */
unknownError:
	Error(kUnknownError);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* ParseRegister() parses a named register. */
static RegisterPtr ParseRegister(void)
{
	char *putBack = gNextChar;
	RegisterPtr registerPtr;
	StringPtr name;

	/* Parse the register name. */
	if ((name = ParseName()) == 0)
		goto error;

	/* Lookup the register by name. */
	if ((registerPtr = LookupRegister(name)) == 0)
		goto error;

	return registerPtr;

error:
	gNextChar = putBack;
	return 0;

}

/* ParseOperand() parses the operand. */
static OperandPtr ParseOperand(void)
{
	static Operand operand;
	static char format[64];
	char buffer[16];
	RegisterPtr registerPtr;
	long number;

	operand.format = format;
	strcpy(format, "");
	while (ParseSpace()) {
		char *restart = gNextChar;
		if (strchr(";!", *gNextChar) != 0)
			break;
		if ((registerPtr = ParseRegister()) != 0) {
			/* <REGISTER> */
			sprintf(buffer, "%s", registerPtr->name);
		}
		else if (!ParseChar('(')) {
			if (!ParseExpression(&number))
				goto error;
			/* <WORD> */
			sprintf(buffer, "%oQ", number);
		}
		else if ((registerPtr = ParseRegister()) != 0) {
			if (!ParseSpace())
				goto error;
			if (strchr("+-", *gNextChar) != 0) {
				if (!ParseExpression(&number))
					goto error;
				if (!ParseChar(')'))
					goto syntaxError;
				/* ( <REGISTER> + <NUMBER> ) */
				sprintf(buffer, "(%s+%oQ)", registerPtr->name, number);
			}
			else {
				if (!ParseChar(')'))
					goto syntaxError;
				/* ( <REGISTER> ) */
				sprintf(buffer, "(%s)", registerPtr->name);
			}
		}
		else {
			if (!ParseExpression(&number))
				goto error;
			if (!ParseChar(')'))
				goto syntaxError;
			if (ParseSpace() && (strchr(",;!", *gNextChar) == 0)) {
				gNextChar = restart;
				if (!ParseExpression(&number))
					goto error;
				/* <NUMBER> */
				sprintf(buffer, "%oQ", number);
			}
			else {
				/* ( <NUMBER> ) */
				sprintf(buffer, "(%oQ)", number);
			}
		}
		/* Compose operand format string.*/
		strcat(format, buffer);
		if (ParseSpace() && ParseChar(',')) {
			/* <OPERAND> , <OPERAND> */
			strcat(format, ",");
			continue;
		}
		break;
	}

	/* All done, return a pointer to the operand. */
	return &operand;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Overflow */
overflow:
	Error(kOverflow);
	goto error;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* AssembleSourceLine() assembles one source line. */
static int AssembleSourceLine(void)
{
	OperandPtr operandPtr;
	CodePtr codePtr;
	OpcodePtr opcodePtr;
	SymbolPtr symbolPtr;
	DirectivePtr directivePtr;

	/* Parse an optional line number. */
	if (ParseNumber(&gListLineNumber))
		;

	/* Lines beginning with '*' are comments. */
	if (ParseSpace() && ParseChar('*'))
		;

	/* Assemble one source line. */
	else while (ParseSpace()) {

		/* Continue assembly on this line if '!'. */
		if (ParseChar('!'))
			continue;

		/* The resit of the line is a comment if ';'. */
		if (ParseChar(';'))
			break;

		/* Parse any <LABEL>, <DIRECTIVE> or <OPCODE>. */
		if (!ParseSourceLine(&symbolPtr, &directivePtr, &opcodePtr))
			goto error;

		if (gFinal && (gListFile != 0) && gDebug) {
			if (symbolPtr != 0)
				fprintf(gListFile,
				        "SYMBOL: %s\n",
				        symbolPtr->name);
			if (directivePtr != 0)
				fprintf(gListFile,
				        "DIRECTIVE: %s\n",
				        directivePtr->name);
			if (opcodePtr != 0)
				fprintf(gListFile,
				        "OPCODE: %s\n",
				        opcodePtr->name);
		}

		/* Handle IF, ELSE, ELSEIF and ENDIF directives. */
		if (directivePtr != 0) {
			switch (directivePtr->code) {
			case kDirectiveIF:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveIF())
					goto error;
				continue;
			case kDirectiveELSE:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveELSE())
					goto error;
				continue;
			case kDirectiveELSEIF:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveELSEIF())
					goto error;
				continue;
			case kDirectiveENDIF:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveENDIF())
					goto error;
				continue;
			}
		}

		/* Ignore source code if disabled by IF/ELSE directive. */
		if (gIfFalse) {
			gListMode = kListSource;
			while ((*gNextChar != 0) &&
			       (*gNextChar != ';') &&
			       (*gNextChar != '!'))
				gNextChar++;
			continue;
		}

		/* Handle EQU and SET directives. */
		/* Handle ORG directive. */
		/* Handle 8080 and Z80 directives. */
		/* Handle OPCODES, TITLE and SOURCE directives. */
		if (directivePtr != 0) {
			switch (directivePtr->code) {
			case kDirectiveSET:
				if (!DirectiveSET(symbolPtr))
					goto error;
				/* List source and value. */
				gListMode = kListSourceValue;
				gListAddress = symbolPtr->value;
				continue;
			case kDirectiveEQU:
				if (!DirectiveEQU(symbolPtr))
					goto error;
				/* List source and value. */
				gListMode = kListSourceValue;
				gListAddress = symbolPtr->value;
				continue;
			case kDirectiveORG:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveORG())
					goto error;
				/* List source and address. */
				gListMode = kListSourceAddress;
				gListAddress = gCurrentAddress;
				continue;
			case kDirective8080:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!Directive8080())
					goto error;
				continue;
			case kDirectiveZ80:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveZ80())
					goto error;
				continue;
			case kDirectiveOPCODES:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveOPCODES())
					goto error;
				continue;
			case kDirectiveTITLE:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveTITLE())
					goto error;
				continue;
			case kDirectiveSOURCE:
				if (symbolPtr != 0)
					goto syntaxError;
				if (!DirectiveSOURCE())
					goto error;
				continue;
			}
		}

		/* If there is a <LABEL>... */
		if (symbolPtr != 0) {
			/* then set it to the current address. */
			/* This handles a <LABEL> without the ':'. */
			if (!SetLabel(symbolPtr))
				goto error;
			if (gFinal && (gListFile != 0) && gDebug)
				fprintf(gListFile,
				        "LABEL: %s\n",
				        symbolPtr->name);
			symbolPtr = 0;
		}

		/* List source and address. */
		gListMode = kListSourceAddress;

		/* Handle END directive. */
		/* Handle DS, DB and DW directives. */
		if (directivePtr != 0) {
			switch (directivePtr->code) {
			case kDirectiveEND:
				if (!DirectiveEND())
					goto error;
				continue;
			case kDirectiveDS:
				if (!DirectiveDS())
					goto error;
				continue;
			case kDirectiveDB:
				if (!DirectiveDB())
					goto error;
				/* List source, address and code. */
				gListMode = kListSourceAddressCode;
				continue;
			case kDirectiveDW:
				if (!DirectiveDW())
					goto error;
				/* List source, address and code. */
				gListMode = kListSourceAddressCode;
				continue;
			}
		}

		/* Assemble an instruction if an <OPCODE> is specified. */
		if (opcodePtr != 0) {
			/* Parse the <OPERAND>. */
			if ((operandPtr = ParseOperand()) == 0)
				goto error;
			if (gFinal && (gListFile != 0) && gDebug)
				fprintf(gListFile,
				        "OPERAND: %s\n",
				        operandPtr->format);
			/* Lookup the <CODE> for this <OPCODE><OPERAND>. */
			if ((codePtr = LookupCode(opcodePtr, operandPtr)) == 0)
				goto syntaxError;
			if (gFinal && (gListFile != 0) && gDebug)
				fprintf(gListFile,
				        "CODE: %s\n",
				        codePtr->format);
			/* Generate the <CODE>. */
			if (!GenerateCode(codePtr, operandPtr))
				goto error;
			/* List source, address and code. */
			gListMode = kListSourceAddressCode;
		}

	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** ASSEMBLE HEX RECORD ***

/* AssembleHexRecord() assembles one line of HEX source. */
static inline int AssembleHexRecord(void)
{
	WordBytes address;
	Byte byte;
	Byte cksum;
	Byte count;
	Byte code;
	Byte old_cksum;
	unsigned i;

	/* HEX records begin with a ':'. */
	if (!ParseChar(':'))
		goto syntaxError;

	/* Compose a checksum of all bytes in the HEX record. */
	cksum = 0;

	/* Parse the count <BYTE> of the data bytes. */
	if (!ParseByte(&count)) 
		goto syntaxError;
	cksum += count;

	/* Parse the address <WORD>. */
	if (!ParseByte(&address.byte.high))
		goto syntaxError;
	cksum += address.byte.high;
	if (!ParseByte(&address.byte.low))
		goto syntaxError;
	cksum += address.byte.low;

	/* Parse and ignore the code <BYTE>. */
	if (!ParseByte(&code))
		goto syntaxError;
	cksum += code;

	/* If there are data bytes in the record... */
	if (count > 0) {
		/* Set the gCurrentAddress. */
		gCurrentAddress = address.word;
		gListAddress = gCurrentAddress;
		/* Parse and emit each data <BYTE>. */
		for (i = 0; i < count; i++) {
			if (!ParseByte(&byte))
				goto syntaxError;
			cksum += byte;
			EmitByte(byte);
		}
		/* Parse the checksum <BYTE>. */
		if (!ParseByte(&old_cksum))
			goto syntaxError;
		cksum += old_cksum;
		/* Error if the checksum failed. */
		if (cksum != 0)
			goto valueError;
		/* List source, address and code. */
		gListMode = kListSourceAddressCode;
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Syntax Error */
syntaxError:
	Error(kSyntaxError);
	goto error;

	/* Value Error */
valueError:
	Error(kValueError);
	goto error;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** ASSEMBLE FROM SOURCE LINE ***

static int AssembleFromSourceLine(const char *sourceLine)
{
	int currentAddress;

	/* Make two passes through the source line. */
	currentAddress = gCurrentAddress;
	for (gFinal = 0; gFinal < 2; gFinal++) {

		/* Select the default instrucion set. */
#ifdef Z80
		DirectiveZ80();
#else
		Directive8080();
#endif

		/* Prepare for this pass through the source line. */
		gListTopOfPage = 0;
		gIfDepth = 0;
		gIfFalse = 0;
		gSourceEnd = 0;
		gCurrentAddress = currentAddress;

		strcpy(gSourceLine, sourceLine);
		gSourceFile.lineNumber++;
		gNextChar = gSourceLine;

		/* Prepare to list this source line. */
		strcpy(gErrorMessage, "");
		gListCodeIndex = 0;
		gListAddress = gCurrentAddress;
		gListLineNumber = gSourceFile.lineNumber;
		/* List source only. */
		gListMode = kListSource;

		/* If the line begins with a ':' it is a HEX record. */
		if (*gNextChar == ':')
			AssembleHexRecord();
		/* Otherwise it is a normal source line. */
		else
			AssembleSourceLine();

	}

	/* Flush the emit code buffer. */
	EmitCodeBuffer();

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;


}

/**********************************************************************/
#pragma mark *** ASSEMBLE FROM SOURCE FILE ***

/* AssembleFromSourceFile() assembles lines from a source file. */
static int
	AssembleFromSourceFile(const char *sourceFile,
	                       const char *listFile)
{
	int currentAddress;

	/* Open the list file if one is specified. */
	if ((listFile != 0) && !OpenListFile(listFile)) {
		printf("?LIST FILE \"%s\"\n", listFile);
		goto error;
	}

	/* Make two passes through the source file. */
	currentAddress = gCurrentAddress;
	for (gFinal = 0; gFinal < 2; gFinal++) {

		/* Open the source input file. */
		if (!OpenSourceFile(sourceFile)) {
			printf("?SOURCE FILE \"%s\"\n", sourceFile);
			goto error;
		}

		/* Select the default instrucion set. */
#ifdef Z80
		if (!strcmp(FileExtension(sourceFile), "ASM"))
			Directive8080();
		else
			DirectiveZ80();
#else
		Directive8080();
#endif
	
		/* Prepare for this pass through the source file. */
		gListTopOfPage = 0;
		gIfDepth = 0;
		gIfFalse = 0;
		gSourceEnd = 0;
		gCurrentAddress = currentAddress;

		/* For each line in the source file... */
		while (ReadSourceLine()) {

			/* Prepare to list this source line. */
			strcpy(gErrorMessage, "");
			gListCodeIndex = 0;
			gListAddress = gCurrentAddress;
			gListLineNumber = gSourceFile.lineNumber;
			/* List source only. */
			gListMode = kListSource;

			/* If the line begins with a ':' it is a HEX record. */
			if (*gNextChar == ':')
				AssembleHexRecord();
			/* Otherwise it is a normal source line. */
			else
				AssembleSourceLine();

			/* List the source line if pass 2. */
			if (gFinal && (gListFile != 0))
				ListSourceLine();

		}

		/* All done with the source input file. */
		CloseSourceFile();

	}

	/* Flush the emit code buffer. */
	EmitCodeBuffer();

	/* List the symbol table. */
	if (gListFile != 0)
		ListSymbolTable();

	/* Close the list output file. */
	CloseListFile();

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** ASSEMBLE FROM SOURCE CODE ***

/* AssembleFromSourceCode() assembles from a source code array. */
static void AssembleFromSourceCode(const Byte *sourceCode)
{
	Byte count;

	/* Prepare to emit code. */
	gListCodeIndex = 0;
	gFinal = 1;
	EmitCodeBuffer();

	/* While there is code to emit... */
	while ((count = *sourceCode++) > 0) {
		/* Set the gCurrentAddress. */
		gCurrentAddress = *sourceCode++ << 8;
		gCurrentAddress |= *sourceCode++;
		/* Emit the code. */
		while (count-- > 0)
			EmitByte(*sourceCode++);
	}
	/* Flush the emit code buffer. */
	EmitCodeBuffer();

}

/**********************************************************************/
#pragma mark *** ASSEMBLE FROM COM FILE ***

/* IsComFile() returns non-zero if the file is binary. */
static int IsComFile(const char *comFile)
{
	Byte buffer[128];
	FILE *f;
	int isBinary;
	int n;
	int i;

	/* Open the input file. */
	if ((f = fopen(comFile, "rb")) == 0)
		goto error;
 
 	/* Read the entire file... */
 	/* The file is binary if any byte is not text. */
	isBinary = 0;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
		for (i = 0; i < n; i++) {
			if (buffer[i] & 0x80) {
				isBinary++;
				break;
			}
		}
		if (isBinary)
			break;
	}

	/* Close the input file. */
	fclose(f);

	/* Return non-zero if the file is binary. */
	return isBinary;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/* AssembleFromComFile() assembles from a binary file. */
static int AssembleFromComFile(const char *comFile)
{
	Byte buffer[128];
	FILE *f;
	int n;
	int i;

	/* Open the binary input file. */
	if ((f = fopen(comFile, "rb")) == 0)
		goto error;

	/* Assume a load point of 100H */
	gCurrentAddress = 0x0100;

	/* Prepare to emit code. */
	gListCodeIndex = 0;
	gFinal = 1;
	EmitCodeBuffer();

	/* Read the entire file, emit each byte. */
	while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0)
		for (i = 0; i < n; i++)
			EmitByte(buffer[i]);

	/* Flush the emit code buffer. */
	EmitCodeBuffer();

	/* Close the binary input file. */
	fclose(f);

	/* All done, no error, return non-zero. */
	return 1;

	/* Return 0 if there was an error. */
error:
	return 0;

}

/**********************************************************************/
#pragma mark *** ASSEMBLE FROM MEMORY ***

/* AssembleFromMemory() assembles code directly from memory. */
static void AssembleFromMemory(Byte (*rdByte)(Word address))
{

	/* Prepare to emit code. */
	gListCodeIndex = 0;
	gFinal = 1;
	EmitCodeBuffer();

	/* Emit every byte in memory. */
	gCurrentAddress = 0;
	do
		EmitByte(rdByte(gCurrentAddress));
	while (gCurrentAddress != 0);

	/* Flush the emit code buffer. */
	EmitCodeBuffer();

}

/**********************************************************************/
#pragma mark *** ASSEMBLE MAIN ***

/* Assemble() assembles code. */
int
	Assemble(const char *sourceLine,
	         Byte (*rdByte)(Word address),
	         const Byte *sourceCode,
	         const char *sourceFile,
	         const char *listFile,
	         const char *objectFile,
	         const char *codeFile,
	         void (*wrByte)(Word address, Byte value),
	         Word *addressPtr,
	         Word addressBias,
	         int stripZeros,
	         int debug)
{
	int pageCount = 0;
	gCalculate = 0;
	gStripZeros = stripZeros;
	gNextEmitAddress = 0;
	gEmitCodeIndex = 0;
	gSourceFile.file = 0;
	gListFile = 0;
	gObjectFile = 0;
	gCodeFile = 0;
	gErrorCount = 0;
	gDebug = debug;
	gEmitCodeBias = addressBias;

#ifdef CALCULATE_TABLES
	if (!CalculateInstructionSetTables())
		goto error;
#endif

	/* Set the current address. */
	gCurrentAddress = (addressPtr != 0) ? *addressPtr : 0;
	gMaxAddress = gCurrentAddress;

	/* Open the object output file if one is specified. */
	if ((objectFile != 0) && !OpenObjectFile(objectFile)) {
		printf("?OBJECT FILE \"%s\"\n", objectFile);
		goto error;
	}

	/* Open the code output file if one is specified. */
	if ((codeFile != 0) && !OpenCodeFile(codeFile)) {
		printf("?CODE FILE \"%s\"\n", codeFile);
		goto error;
	}

	/* Set the memory write function if one is specified. */
	gWrByte = wrByte;

	/* Assemble from memory if a memory read function is specified. */
	if (rdByte != 0)
		AssembleFromMemory(rdByte);

	/* Assemble from a source code array if one is specified. */
	if (sourceCode != 0)
		AssembleFromSourceCode(sourceCode);

	/* Assemble source line if one is specified. */
	if (sourceLine != 0)
		AssembleFromSourceLine(sourceLine);

	/* If a source input file is specified... */
	if (sourceFile != 0) {
		/* Assemble from a binary input file... */
		if (IsComFile(sourceFile)) {
			if (!AssembleFromComFile(sourceFile))
				goto error;
		}
		/* Assemble from a text input file... */
		else {
			if (!AssembleFromSourceFile(sourceFile, listFile))
				goto error;
		}
	}

	/* Error if errors. */
	if (gErrorCount) {
		printf("%d ERRORS\n", gErrorCount);
		goto error;
	}

	/* Return the current address. */
	if (addressPtr != 0)
		*addressPtr = gCurrentAddress;

	/* Determine the output page count. */
	if ((pageCount = (gMaxAddress >> 8) & 0xFF) < 1)
		pageCount = 1;

error:

	/* Close files and free tables. */
	CloseObjectFile();
	CloseCodeFile();
	CloseListFile();
	while (CloseSourceFile())
		;
	FreeSymbolTable();
#ifdef CALCULATE_TABLES
	FreeInstructionSetTables();
#endif

	/* All done, if no errors, return pageCount. */
	/* return zero if there were any errors. */
	return pageCount;

}
