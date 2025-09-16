/*
 * n7mm.c
 * ------
 * Memory manager with support for garbage collected pointers.
 *
 * By: Marcus 2022
 */

#ifndef __N7_MM_H__
#define __N7_MM_H__

#include "stddef.h"

/* Memory block header. */
typedef struct sMemHeader MemHeader;

struct sMemHeader {
	size_t size;
	MemHeader *next;
	struct {
        unsigned char bucket : 4;
        unsigned char type: 2;
        unsigned char marked: 1;
        unsigned char used: 1;
    } status;
};

/* Memory bucket. */
typedef struct {
    size_t size;
    void *memory;
    MemHeader *firstHeader;
} MemBucket;

/* Memory information, returned by MM_Available. */
typedef struct {
    size_t available;
    unsigned int blocks;
    unsigned int freeBlocks;
} MemInfo;

/* Macros for mark and sweep implementation. */
#define MM_Alive(ptr) ((MemHeader *)ptr - 1)->status.marked
#define MM_MarkAlive(ptr) ((MemHeader *)ptr - 1)->status.marked = 1
#define MM_FastFree(ptr) {((MemHeader *)ptr - 1)->status.used = 0; ((MemHeader *)ptr - 1)->status.type = 0;}

/*
 * MM_SetDebugOutput
 * -----------------
 */
void MM_SetDebugOutput(int value);

/*
 * MM_SetErrorFunction
 * -------------------
 */
void MM_SetErrorFunction(void (*f)(const char *msg));

/*
 * MM_SetMarkAndSweepFunction
 * --------------------------
 * All collectable objects are marked as dead when the set function is called. The function can
 * use MM_Alive to check if a pointer is alive and MM_MarkAlive to mark it alive.
 */
void MM_SetMarkAndSweepFunction(void (*f)());

/*
 * MM_SetDestructorFunction
 * ------------------------
 * Set destructor function for a collectable type, called for dead pointers after mark and sweep.
 */
void MM_SetDestructorFunction(unsigned int type, void (*f)(void *ptr));

/*
 * MM_Init
 * -------
 * Init memory manager with the specified bucket size.
 */
int MM_Init(size_t size);

/*
 * MM_Terminate
 * ------------
 * Terminate memory manager.
 */
void MM_Terminate();

/*
 * MM_Malloc
 * ---------
 * malloc replacement.
 */
void *MM_Malloc(size_t size);

/*
 * MM_SetType
 * ----------
 * Set collectable type for allocated pointer. The default value is 0 means that the pointer is not1
 * subject to garbage collecting.
 */
void MM_SetType(void *ptr, unsigned char type);

/*
 * MM_Realloc
 * ----------
 * realloc replacement.
 */
void *MM_Realloc(void *ptr, size_t size);

/*
 * MM_Free
 * -------
 * Free replacement.
 */
void MM_Free(void *ptr);

/*
 * MM_GarbageCollect
 * -----------------
 * Perform garbage collecting. MM_Malloc/Realloc/Strdup automatically call it when out of memory.
 */
int MM_GarbageCollect();

/*
 * MM_Strdup
 * ---------
 * strdup replacement.
 */
char *MM_Strdup(const char *src);

/*
 * MM_Available
 * ------------
 * Returns available memory in bytes and other information.
 */
MemInfo MM_Available();

/*
 * MM_PrintMemoryInfo
 * ------------------
 * Print some information about the memory to stdout.
 */
void MM_PrintMemoryInfo();


#endif