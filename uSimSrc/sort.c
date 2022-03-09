/* sort.c * Copyright (C) 2000, Tsurishaddai Williamson, tsuri@earthlink.net *  * This program is free software; you can redistribute it and/or * modify it under the terms of the GNU General Public License * as published by the Free Software Foundation; either version 2 * of the License, or (at your option) any later version. *  * This program is distributed in the hope that it will be useful, * but WITHOUT ANY WARRANTY; without even the implied warranty of * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the * GNU General Public License for more details. *  * You should have received a copy of the GNU General Public License * along with this program; if not, write to the Free Software * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. *//**********************************************************************/#include <stdlib.h>#include "sort.h"#define BUBBLESORT/* SwapSortElements() swaps two elements for sort. */static void SwapSortElements(void *a, void *b, size_t size){	char *aa = (char *)a;	char *bb = (char *)b;	/* Swap the elements one byte at a time. */	while (size--) {		char x = *aa;		*aa++ = *bb;		*bb++ = x;	}}/* UnSort() randomizes an array. */void	UnSort(void *elements,	       unsigned maxElement,	       unsigned elementSize){	if (elements == 0)		;	else if (maxElement == 0)		;	else if (elementSize == 0)		;	else {		char *e = elements;		unsigned i;		for (i = 0; i < maxElement; i++) {			void *a = &e[i * elementSize];			void *b = &e[(rand() % maxElement) * elementSize];			SwapSortElements(a, b, elementSize);		}	}}/* Sort() sorts an array. */void	Sort(void *elements,	           unsigned maxElement,	           unsigned elementSize,	           SortCompareFunction compare){	if (elements == 0)		;	else if (maxElement == 0)		;	else if (elementSize == 0)		;	else if (compare == 0)		;	else if (maxElement > 1) {#ifdef BUBBLESORT		char *e = elements;		unsigned i = maxElement - 1;		unsigned j;		do {			for (j = 0; j < i; j++) {				void *a = &e[(j + 0) * elementSize];				void *b = &e[(j + 1) * elementSize];				if (compare(a, b) > 0)					SwapSortElements(a, b, elementSize);			}		} while (i-- > 0);#endif	}}