/*
 * File: hash_table.h
 * ------------------
 * Hash table.
 *
 * By: Marcus 2018
 */

#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#include "stddef.h"

/*#define HT_REF_COUNT*/

#define HT_DEF_CAPACITY 1
#define HT_GROW_AT 0.6
#define HT_SHRINK_AT 0.25


/*
 * Struct: HashEntry
 * -----------------
 */
typedef struct _HashEntry HashEntry;
struct _HashEntry {
    void *data;
    char *skey;
    int ikey;
    HashEntry *next;
};

/*
 * Struct: HashTable
 * -----------------
 */
typedef struct {
    char lock;  /* Also for n7 ... should've made an aggregate instead. */
    int capacity;
    int minCapacity;
    int entries;
    HashEntry **list;
} HashTable;


/*
 * HT_SetCustomMalloc
 * ------------------
 */
void HT_SetCustomMalloc(void *(*f)(size_t size));

/*
 * HT_SetCustomFree
 * ----------------
 */
void HT_SetCustomFree(void (*f)(void *ptr));

/*
 * HT_SetCustomStrdup
 * ------------------
 */
void HT_SetCustomStrdup(char *(*f)(const char *str));

/*
 * Function: HT_Create
 * -------------------
 */
HashTable *HT_Create(unsigned int capacity);

/*
 * Function: HT_EntryCount
 * -----------------------
 */
int HT_EntryCount(HashTable *ht);

/*
 * Function: HT_CollisionCount
 * ---------------------------
 */
int HT_CollisionCount(HashTable *ht, int *maxCollisions);

/*
 * Function: HT_Clear
 * ------------------
 */
void HT_Clear(HashTable *ht, void (*delFunc)(void *));

/*
 * Function: HT_Free
 * -----------------
 */
void HT_Free(HashTable *ht, void (*delFunc)(void *));

/*
 * Function: ApplyDataFunction
 * ---------------------------
 */
void HT_ApplyDataFunction(HashTable *ht, void (*func)(void *, void *), void *userData);

/*
 * Function: HT_ApplyKeyFunction
 * -----------------------------
 */
void HT_ApplyKeyFunction(HashTable *ht, void (*func)(char *, int, void *, void *), void *userData);

/*
 * Function: HT_Hash
 * -----------------
 */
/*unsigned int inline HT_Hash(const char *skey, int ikey) {
    if (skey) {
        unsigned int hash = 5381;
        int c;

        while ((c = *skey++)) hash = ((hash << 5) + hash) + c;
        hash ^= ikey*2654435761;

        return hash;
    }
    else {
        return ikey*2654435761;
    }
}*/

unsigned int HT_Hash(const char *skey, int ikey);

/*
 * Function: HT_Add
 * ----------------
 */
int HT_Add(HashTable *ht, const char *skey, int ikey, void *data);
int HT_AddPH(HashTable *ht, unsigned int hash, const char *skey, int ikey, void *data);

/*
 * Function: HT_UnsafeAdd
 * ----------------------
 * No checking if key already exists.
 */
void HT_UnsafeAdd(HashTable *ht, const char *skey, int ikey, void *data);
void HT_UnsafeAddPH(HashTable *ht, unsigned int hash, const char *skey, int ikey, void *data);


HashEntry *HT_NewUnsafeEntry(HashTable *ht, const char *skey, int ikey);

/*
 * Function: HT_AddEntry
 * ---------------------
 * Used when resizing.
 */
void HT_AddEntry(HashTable *ht, HashEntry *e);

/*
 * Function: HT_Delete
 * -------------------
 */
int HT_Delete(HashTable *ht, const char *skey, int ikey, void (*delFunc)(void *));

/*
 * Function: HT_Function
 * ---------------------
 */
int HT_Function(HashTable *ht, const char *skey, int ikey, void (*func)(void *));

/*
 * Function: HT_Get
 * ----------------
 */
void *HT_Get(HashTable *ht, const char *skey, int ikey);
void *HT_GetPH(HashTable *ht, unsigned int h, const char *skey, int ikey);

/*
 * Function: HT_GetEntry
 * ---------------------
 */
HashEntry *HT_GetEntry(HashTable *ht, const char *skey, int ikey);
HashEntry *HT_GetOrCreateEntry(HashTable *ht, const char *skey, int ikey);
HashEntry *HT_GetOrCreateEntryPH(HashTable *ht, unsigned int hash, const char *skey, int ikey);

/*
 * Function: FindEntry
 * -------------------
 */ 
HashEntry *HT_FindEntry(HashTable *ht, int (*func)(void *, void *), void *userData);

/*
 * Function: HT_GetEntriesArray
 * ----------------------------
 */
HashEntry **HT_GetEntriesArray(HashTable *ht);

/*
 * Function: HT_Exists
 * -------------------
 */
int HT_Exists(HashTable *ht, const char *skey, int ikey);

/*
 * Function: HT_Resize
 * -----------------------
 */
void HT_Resize(HashTable *ht, unsigned int capacity);

/*
 * HT_ReIndex
 * ----------
 */
void HT_ReIndex(HashTable *ht, int minIndex, int maxIndex);


int HT_TableCount();

#endif
