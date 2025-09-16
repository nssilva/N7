/*
 * n7mm.c
 * ------
 *
 * By: Marcus 2022
 */


#include "n7mm.h"
#include "stdlib.h"
#include "stdio.h"
#include "time.h"

#define MAX_BUCKETS 5


static int sDebugOutput = 0;

static size_t sBucketSize = 0;
static MemBucket sBuckets[MAX_BUCKETS];
static MemHeader *sFirst;
static MemHeader *sCurrent;
static int sBucketCount = 0;
static int sCurrentBucket = 0;
static void (*sMarkAndSweep)() = 0;
static void (*sDestructors[8])(void*);
static void (*sError)(const char *msg) = 0;
static int sGC = 0;

static unsigned long TimeMS() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000 + ts.tv_nsec/1000000;
}


/*
 * MM_SetDebugOutput
 * -----------------
 */
void MM_SetDebugOutput(int value) {
    sDebugOutput = value;
}

/*
 * MM_SetErrorFunction
 * -------------------
 */
void MM_SetErrorFunction(void (*f)(const char *msg)) {
    sError = f;
}

/*
 * MM_SetMarkAndSweepFunction
 * --------------------------
 * All collectable objects are marked as dead when the set function is called. The function can
 * use MM_Alive to check if a pointer is alive and MM_MarkAlive to mark it alive.
 */
void MM_SetMarkAndSweepFunction(void (*f)()) {
    sMarkAndSweep = f;
}

/*
 * MM_SetDestructorFunction
 * ------------------------
 * Set destructor function for a collectable type, called for dead pointers after mark and sweep.
 */
void MM_SetDestructorFunction(unsigned int type, void (*f)(void *ptr)) {
    sDestructors[type - 1] = f;
}

/*
 * MM_AddBucket
 * ------------
 */
int MM_AddBucket(size_t size) {
    if (sBucketCount >= MAX_BUCKETS) return 0;
    
    sBuckets[sBucketCount].size = sizeof(MemHeader) + size;
    sBuckets[sBucketCount].memory = malloc(sBuckets[sBucketCount].size);
    if (sBuckets[sBucketCount].memory) {
        if (sDebugOutput) printf("mm: Created bucket %d (%llu bytes)\n", sBucketCount, size);
		sBuckets[sBucketCount].firstHeader = (MemHeader *)sBuckets[sBucketCount].memory;
		sBuckets[sBucketCount].firstHeader->size = size;
        sBuckets[sBucketCount].firstHeader->status.bucket = 0;
        sBuckets[sBucketCount].firstHeader->status.type = 0;
		sBuckets[sBucketCount].firstHeader->status.used = 0;
		sBuckets[sBucketCount].firstHeader->next = 0;
        sBucketCount++;
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * MM_Init
 * -------
 * Init memory manager with the specified bucket size.
 */
int MM_Init(size_t size) {
    int success;
    
    sBucketSize = size;
    
    success = MM_AddBucket(sBucketSize);
    if (success) {
        sCurrentBucket = 0;
        sFirst = sBuckets[sCurrentBucket].firstHeader;
        sCurrent = 0;
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * MM_Terminate
 * ------------
 * Terminate memory manager.
 */
void MM_Terminate() {
	//sAllocatedSize = 0;
	//free(sMemory);
    for (int i = 0; i < sBucketCount; i++) {
        free(sBuckets[i].memory);
    }
}

/*
 * MM_FindFree
 * -----------
 */
MemHeader *MM_FindFree(MemHeader *mh, size_t size) {
    do {
		while (mh && mh->status.used) mh = mh->next;
		if (mh) {
			MemHeader* next = mh->next;
			/* 240318. */
			/*while (next && !next->status.used) {*/
			while (mh->size < size && next && !next->status.used) {
				mh->size += sizeof(MemHeader) + next->size;
				next = next->next;
			}
			mh->next = next;
			if (mh->size < size) mh = mh->next;
		}
	} while (mh && (mh->status.used || mh->size < size));
    return mh;
}

/*
 * MM_Malloc
 * ---------
 * malloc replacement:
 *   Search current bucket, from sCurrent and from sFirst
 *   Unless found, search rest of buckets
 *   Unless found, garbage collect and search through buckets again
 *   Unless found, create new bucket if possible, else error
 */
void *MM_Malloc(size_t size) {
    MemHeader *mh = 0;
    
    /* Search for free spot. */
    if (!(sCurrent && (mh = MM_FindFree(sCurrent, size)))) {
        if (!(mh = MM_FindFree(sFirst, size))) {
            if (sBucketCount > 1) {
                /* 240318. */
                /*for (int i = 0; i < sBucketCount; i++) {
                    if (i != sCurrentBucket) {
                        if ((mh = MM_FindFree(sBuckets[i].firstHeader, size))) {
                            sCurrentBucket = i;
                            sFirst = sBuckets[i].firstHeader;
                            sCurrent = 0;
                            break;
                        }
                    }
                }*/
                for (int i = 0; i < sBucketCount; i++) {
                    sCurrentBucket = (sCurrentBucket + 1)%sBucketCount;
                    if ((mh = MM_FindFree(sBuckets[sCurrentBucket].firstHeader, size))) {
                        sFirst = sBuckets[i].firstHeader;
                        sCurrent = 0;
                        break;
                    }
                }
            }
        }
    }
    
    
    /* Garbage collect. */
    if (!mh && sMarkAndSweep) {
        MM_GarbageCollect(); /* Should return bucket with most available memory. */
        /* Search buckets again. */
        for (int i = 0; i < sBucketCount; i++) {
            if ((mh = MM_FindFree(sBuckets[i].firstHeader, size))) {
                sCurrentBucket = i;
                sFirst = sBuckets[i].firstHeader;
                sCurrent = 0;
                break;
            }
        }
    }
    
    /* Create new bucket. */
    if (!mh) {
        if (MM_AddBucket(sBucketSize)) {
            sCurrentBucket = sBucketCount - 1;
            sFirst = sBuckets[sCurrentBucket].firstHeader;
            sCurrent = 0;
            mh = sFirst;
        }
    }

    /* Got one? */
    if (mh) {
		if (mh->size > size) {
			if (mh->size - size > sizeof(MemHeader)) {
				MemHeader* next = mh->next;
				mh->next = (MemHeader*)((char*)mh + sizeof(MemHeader) + size);
				mh->next->size = mh->size - size - sizeof(MemHeader);
				mh->next->next = next;
                mh->next->status.bucket = 0;
                mh->next->status.type = 0;
				mh->next->status.used = 0;
				mh->size = size;
			}
		}
        mh->status.bucket = 0;
        mh->status.type = 0;
		mh->status.used = 1;

		sCurrent = mh->next;
		while (sCurrent && sCurrent->status.used) sCurrent = sCurrent->next;

        return (void *)(++mh);
    }
    else {
        if (sError) {
            sError("Out of memory in Malloc");
        }
        else {
            fprintf(stderr, "mm: Out of memory in Malloc\n");
            exit(0);
        }
        return 0;
    }
}

/*
 * MM_SetType
 * ----------
 * Set collectable type for allocated pointer. The default value is 0 means that the pointer is not1
 * subject to garbage collecting.
 */
void MM_SetType(void *ptr, unsigned char type) {
    MemHeader *mh = (MemHeader *)ptr;
    (--mh)->status.type = type;
}

/*
 * MM_Realloc
 * ----------
 * realloc replacement.
 */
void *MM_Realloc(void *ptr, size_t size) {
	void *newPtr = 0;

	if (!ptr) return MM_Malloc(size);

	if (size == 0) {
		MM_Free(ptr);
		return 0;
	}
   
    if ((newPtr = MM_Malloc(size))) {
        char *ocp = (char *)ptr;
        char *ncp = (char *)newPtr;
        MemHeader *mh = (MemHeader *)ptr;
        mh--;
        if (mh->size < size) size = mh->size;
        for (unsigned int i = 0; i < size; i++) ncp[i] = ocp[i];
        // SHOULD COPY TYPE FROM OLD MH!!!
        MM_Free(ptr);
        return newPtr;
    }
    else {
        if (sError) {
            sError("Out of memory in Realloc");
        }
        else {
            fprintf(stderr, "mm: Out of memory in Realloc\n");
            exit(0);
        }
        return 0;
    }
}

/*
 * MM_Free
 * -------
 * Free replacement.
 */
void MM_Free(void *ptr) {
	MemHeader *mh;
    
    if (!ptr) return;
    
    mh = (MemHeader*)ptr;
    mh--;
	if (mh->status.used) {
        mh->status.type = 0;
		mh->status.used = 0;
	}
}


int MM_GarbageCollect() {
    MemHeader *mh;
    unsigned long long st;
    unsigned int unmarkTime, markAndSweepTime, deleteTime, mergeTime;
    int count = 0;
    sGC = 1;
    
    /* Mark everything dead. */
    if (sDebugOutput) printf("mm: Marking dead\n");
    st = TimeMS();
    for (int i = 0; i < sBucketCount; i++) {
        mh = sBuckets[i].firstHeader;
        while (mh) {
            mh->status.marked = 0;
            mh = mh->next;
        }
    }
    unmarkTime = (unsigned int)(TimeMS() - st);
    
    /* Mark live. */
    if (sDebugOutput) printf("mm: Marking live\n");
    st = TimeMS();
    sMarkAndSweep();
    markAndSweepTime = (unsigned int)(TimeMS() - st);
    
    /* Free. */
    if (sDebugOutput) printf("mm: Releasing memory\n");
    st = TimeMS();
    for (int i = 0; i < sBucketCount; i++) {
        /* Uhm, could probably merge here instead of later ... */
        mh = sBuckets[i].firstHeader;
        while (mh) {
            if (mh->status.type && !mh->status.marked) {
                sDestructors[mh->status.type - 1]((void *)(mh + 1));
                mh->status.type = 0;
                count++;
            }
            mh = mh->next;
        }
    }
    deleteTime = (unsigned int)(TimeMS() - st);
    
    /* Merge. */
    if (sDebugOutput) printf("mm: Merging\n");
    st = TimeMS();
    for (int i = 0; i < sBucketCount; i++) {
        MemHeader *next;
        mh = sBuckets[i].firstHeader;
        do {
            while (mh && mh->status.used) mh = mh->next;
            if (mh) {
                next = mh->next;
                while (next && !next->status.used) {
                    mh->size += sizeof(MemHeader) + next->size;
                    next = next->next;
                }
                mh->next = next;
                mh = next;
            }
        } while (mh);
    }
    mergeTime = (unsigned int)(TimeMS() - st);

    if (sDebugOutput) {
        printf("mm: Garbage collected %d objects, %d/%d/%d/%d\n", count, unmarkTime, markAndSweepTime, deleteTime, mergeTime);
    }

    sCurrentBucket = 0;
    sFirst = sBuckets[sCurrentBucket].firstHeader;
    sCurrent = 0;
    
    sGC = 0;
    
    return 0;
}

/*
 * MM_Strdup
 * ---------
 * strdup replacement.
 */
char *MM_Strdup(const char *src) {
    size_t len;
    char *dst;
    const char *c;
    
    if (!src) return 0;

    len = 0;
    c = src;
    while (*c != '\0') {
        len++;
        c++;
    }
    
    dst = (char *)MM_Malloc(len + 1);
    if (dst) {
        for (size_t i = 0; i < len; i++) dst[i] = src[i];
        dst[len] = '\0';
    }
    
    return dst;
}

/*
 * MM_Available
 * ------------
 * Returns available memory in bytes.
 */
MemInfo MM_Available(int bucket) {
    MemHeader *mh = sBuckets[bucket].firstHeader;
    MemInfo info;
 
    info.available = 0;
    info.blocks = 0;
    info.freeBlocks = 0;
    while (mh) {
        if (!mh->status.used) {
            info.available += sizeof(MemHeader) + mh->size;
            info.freeBlocks++;
        }
        info.blocks++;
        mh = mh->next;
    }
    return info;
}


/*
 * MM_PrintMemoryInfo
 * ------------------
 * Print some information about the memory to stdout.
 */
void MM_PrintMemoryInfo() {
    if (!sDebugOutput) return;

    for (int i = 0; i < sBucketCount; i++) {
        MemHeader *mh = sBuckets[i].firstHeader;
        size_t sum = 0;
        int totalCount = 0;
        int typedCount = 0;
        
        printf("mm: Bucket %d\n", i);
        while (mh) {
            if (mh->status.used) {
                totalCount++;
                typedCount += mh->status.type != 0;
            }
            sum += sizeof(MemHeader) + mh->size;
            mh = mh->next;
        }
        printf("    %d allocations, %d collectable\n", totalCount, typedCount);
        if (sum == sBuckets[i].size) printf("  No corruption detected\n");
        else printf("    Corruption detected, block sum %llu is not equal to allocated size %llu\n", sum, sBuckets[i].size);

        mh = sFirst;
        while (mh->next) mh = mh->next;
        if ((char *)mh + mh->size + sizeof(MemHeader) - (char *)sFirst != sBuckets[i].size) printf("    Range is invalid");
    }
}

