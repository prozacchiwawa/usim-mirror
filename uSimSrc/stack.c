/* stack.c
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

#include <string.h>
#include "stack.h"

/* StackReset() resets a Sing to initial values. */
StackPtr
	StackReset(StackPtr stackPtr,
	           void *elements,
	           unsigned maxElement,
	           unsigned elementSize)
{

	/* Check the parameters... */
	if (stackPtr == 0)
		goto error;
	if (elements == 0)
		goto error;
	if (maxElement == 0)
		goto error;
	if (elementSize == 0)
		goto error;

	/* Reset the stack. */
	stackPtr->count = 0;
	stackPtr->insert = elements;
	stackPtr->maxElement = maxElement;
	stackPtr->elementSize = elementSize;

	/* All done, no error, return a pointer to the Stack. */
	return stackPtr;

	/* Return zero if there was an error. */
error:
	return 0;

}

static inline char *get_ptr(StackPtr stackPtr) {
  return stackPtr->insert + (stackPtr->elementSize * stackPtr->count);
}

/* RingCount() counts the Elements in a Stack. */
unsigned StackCount(StackPtr stackPtr)
{

	return stackPtr->count;

}

/* StackPush() pushes an Element into a Stack. */
int StackPush(StackPtr stackPtr, void *element)
{

	/* Error if the Stack has not been Reset. */
	if (stackPtr->maxElement == 0)
		goto error;

	/* Error if the Stack is full. */
	if (stackPtr->count == stackPtr->maxElement)
		goto error;

	/* Insert the Element. */
	memcpy(get_ptr(stackPtr), element, stackPtr->elementSize);

	/* Increase the Stack Count. */
	stackPtr->count++;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* StackPop() pops an Element from a Stack. */
int StackPop(StackPtr stackPtr, void *element)
{

	/* Error if the Stack is empty. */
	if (stackPtr->count == 0)
		goto error;

	/* Reduce the Stack Count. */
	stackPtr->count--;

	/* Remove the Element. */
	memcpy(element, get_ptr(stackPtr), stackPtr->elementSize);

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}
