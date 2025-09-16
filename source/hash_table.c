/*
 * File: hash_table.c
 * ------------------
 * Hash table.
 *
 * By: Marcus 2018
 */

#include "hash_table.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef HT_REF_COUNT
static int sTables = 0;
#endif


/* For custom memory manager. */
static void *(*Malloc)(size_t) = malloc;
static void (*Free)(void *ptr) = free;
static char *(*Strdup)(const char *src) = strdup;


/*
 * HT_SetCustomMalloc
 * ------------------
 */
void HT_SetCustomMalloc(void *(*f)(size_t)) {
    Malloc = f;
}

/*
 * HT_SetCustomFree
 * ----------------
 */
void HT_SetCustomFree(void (*f)(void *)) {
    Free = f;
}

/*
 * HT_SetCustomStrdup
 * ------------------
 */
void HT_SetCustomStrdup(char *(*f)(const char *src)) {
    Strdup = f;
}

/*
 * HT_Hash
 * -------
 */
unsigned int HT_Hash(const char *skey, int ikey) {
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
}

/*
 * Function: HT_Create
 * -------------------
 */
HashTable *HT_Create(unsigned int capacity) {
    HashTable *ht;
    unsigned int i;

    ht = (HashTable *)Malloc(sizeof(HashTable));
    if (!capacity) capacity = HT_DEF_CAPACITY;
    
    /* n7 lock flag. */
    ht->lock = 0;
  
    ht->list = (HashEntry **)Malloc(sizeof(HashEntry *)*capacity);
    for (i = 0; i < capacity; i++) ht->list[i] = 0;
  
    ht->capacity = capacity;
    ht->minCapacity = capacity;
    ht->entries = 0;

#ifdef HT_REF_COUNT
    sTables++;
#endif

    return ht;
}

/*
 * Function: HT_Clear
 * ------------------
 */
void HT_Clear(HashTable *ht, void (*delFunc)(void *)) {
    int i;

    if (ht->entries == 0) return;

    for (i = 0; i < ht->capacity; i++) {
        if (ht->list[i]) {
            HashEntry *e = ht->list[i];
            while (e) {
                HashEntry *next = e->next;
                if (e->data && delFunc) delFunc(e->data);
                if (e->skey) Free(e->skey);
                Free(e);
                e = next;
                ht->entries--;
            }
            ht->list[i] = 0;
        }
    }
    assert(ht->entries == 0);
    
    if (ht->capacity > ht->minCapacity) HT_Resize(ht, ht->minCapacity);
}

/*
 * Function: HT_EntryCount
 * -----------------------
 */
int HT_EntryCount(HashTable *ht) {
    return ht->entries;
}

/*
 * Function: HT_CollisionCount
 * ---------------------------
 */
int HT_CollisionCount(HashTable *ht, int *maxCollisions) {
    int collisions = 0;
    int m = 0;
    for (int i = 0; i < ht->capacity; i++) {
        if (ht->list[i]) {
            int c = 0;
            HashEntry *e = ht->list[i];
            while (e->next) {
                c++;
                e = e->next;
            }
            collisions += c;
            if (c > m) m = c;
        }
    }
    if (maxCollisions) *maxCollisions = m;

    return collisions;
}


/*
 * Function: HT_Free
 * -----------------
 */
void HT_Free(HashTable *ht, void (*delFunc)(void *)) {
    if (ht) {
        if (ht->list) {
            if (ht->entries > 0) {
                for (int i = 0; i < ht->capacity; i++) {
                    if (ht->list[i]) {
                        HashEntry *e = ht->list[i];
                        while (e) {
                            HashEntry *next = e->next;
                            if (e->data && delFunc) delFunc(e->data);
                            if (e->skey) Free(e->skey);
                            Free(e);
                            e = next;
                            ht->entries--;
                        }
                        ht->list[i] = 0;
                    }
                }
            }
            /* Clear shrinks the table, we don't want that when deallocating. */
            /*HT_Clear(ht, delFunc);*/
            Free(ht->list);
        }
        Free(ht);
#ifdef HT_REF_COUNT
        sTables--;
#endif
    }
}

/*
 * Function: ApplyDataFunction
 * ---------------------------
 */
void HT_ApplyDataFunction(HashTable *ht, void (*func)(void *, void *), void *userData) {
    for (int i = 0; i < ht->capacity; i++) {
        if (ht->list[i]) {
            HashEntry *e = ht->list[i];
            while (e) {
                if (e->data) func(e->data, userData);
                e = e->next;
            }
        }
    }
}

void HT_ApplyKeyFunction(HashTable *ht, void (*func)(char *, int, void *, void *), void *userData) {
    int i;

    for (i = 0; i < ht->capacity; i++) {
        if (ht->list[i]) {
            HashEntry *e = ht->list[i];
            while (e) {
                func(e->skey, e->ikey, e->data, userData);
                e = e->next;
            }
        }
    }
}

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

/*
 * Function: HT_Add
 * ----------------
 */
int HT_Add(HashTable *ht, const char *skey, int ikey, void *data) {
    unsigned int hash;
    HashEntry *e;
  
    hash = HT_Hash(skey, ikey)%ht->capacity;

    if (ht->list[hash] == 0) {
        e = (HashEntry *)Malloc(sizeof(HashEntry));
        ht->list[hash] = e;
    }
    else {
        e = ht->list[hash];
        if (skey) {
            while (e) {
                if (e->ikey == ikey && e->skey && strcmp(skey, e->skey) == 0) {
                    return 0;
                }
                if (!e->next) {
                    e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                    e = e->next;
                    break;
                }
                e = e->next;
            }
        }
        else {
            while (e) {
                if (e->ikey == ikey && !e->skey) {
                    return 0;
                }
                if (!e->next) {
                    e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                    e = e->next;
                    break;
                }
                e = e->next;
            }
        }
    }
    e->skey = skey ? Strdup(skey) : 0;
    e->ikey = ikey;
    e->next = 0;
    e->data = data;
    ht->entries++;
    
    if ((double)ht->entries/(double)ht->capacity > HT_GROW_AT) {
        HT_Resize(ht, ht->capacity*2);
    }

    return 1;
}

int HT_AddPH(HashTable *ht, unsigned int hash, const char *skey, int ikey, void *data) {
    HashEntry *e;

    hash %= ht->capacity;

    if (ht->list[hash] == 0) {
        e = (HashEntry *)Malloc(sizeof(HashEntry));
        ht->list[hash] = e;
    }
    else {
        e = ht->list[hash];
        if (skey) {
            while (e) {
                if (e->ikey == ikey && e->skey && strcmp(skey, e->skey) == 0) {
                    return 0;
                }
                if (!e->next) {
                    e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                    e = e->next;
                    break;
                }
                e = e->next;
            }
        }
        else {
            while (e) {
                if (e->ikey == ikey && !e->skey) {
                    return 0;
                }
                if (!e->next) {
                    e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                    e = e->next;
                    break;
                }
                e = e->next;
            }
        }
    }
    e->skey = skey ? Strdup(skey): 0;
    e->ikey = ikey;
    e->next = 0;
    e->data = data;
    ht->entries++;

    if ((double)ht->entries/(double)ht->capacity > HT_GROW_AT) {
        HT_Resize(ht, ht->capacity*2);
    }
    
    return 1;
}


void HT_UnsafeAdd(HashTable *ht, const char *skey, int ikey, void *data) {
    unsigned int hash;
    HashEntry *e;
  
    e = (HashEntry *)Malloc(sizeof(HashEntry));
    e->skey = skey ? Strdup(skey) : 0;
    e->ikey = ikey;
    e->data = data;

    hash = HT_Hash(skey, ikey)%ht->capacity;

    if (ht->list[hash] == 0) {
        e->next = 0;
        ht->list[hash] = e;
    }
    else {
        e->next = ht->list[hash];
        ht->list[hash] = e;
    }

    ht->entries++;
    
    if ((double)ht->entries/(double)ht->capacity > HT_GROW_AT) {
        HT_Resize(ht, ht->capacity*2);
    }
}

void HT_UnsafeAddPH(HashTable *ht, unsigned int hash, const char *skey, int ikey, void *data) {
    HashEntry *e;

    e = (HashEntry *)Malloc(sizeof(HashEntry));
    e->skey = skey ? Strdup(skey) : 0;
    e->ikey = ikey;
    e->data = data;

    hash %= ht->capacity;

    if (ht->list[hash] == 0) {
        e->next = 0;
        ht->list[hash] = e;
    }
    else {
        e->next = ht->list[hash];
        ht->list[hash] = e;
    }

    ht->entries++;

    if ((double)ht->entries/(double)ht->capacity > HT_GROW_AT) {
        HT_Resize(ht, ht->capacity*2);
    }
}

/*
 * Function: HT_AddEntry
 * ---------------------
 * Used when resizing.
 */
void HT_AddEntry(HashTable *ht, HashEntry *e) {
    unsigned int hash;

    hash = HT_Hash(e->skey, e->ikey)%ht->capacity;

    if (ht->list[hash] == 0) {
        e->next = 0;
        ht->list[hash] = e;
    }
    else {
        e->next = ht->list[hash];
        ht->list[hash] = e;
    }

    ht->entries++;

}


/*
 * Function: HT_Delete
 * -------------------
 */
int HT_Delete(HashTable *ht, const char *skey, int ikey, void (*delFunc)(void *)) {
    unsigned int hash;
    int result = 0;

    hash = HT_Hash(skey, ikey)%ht->capacity;

    if (!ht->list[hash]) return 0;

    HashEntry *e = ht->list[hash];
    
    if (skey) {        
        if (e->ikey == ikey && e->skey && strcmp(e->skey, skey) == 0) {
            HashEntry *next = e->next;
            if (delFunc) delFunc(e->data);
            Free(e->skey);
            Free(e);
            ht->list[hash] = next;
            ht->entries--;
            result = 1;
        }
        else {
            for (; e->next; e = e->next) {
                if (e->next->ikey == ikey && e->next->skey && strcmp(e->next->skey, skey) == 0) {
                    HashEntry *next = e->next->next;
                    if (delFunc) delFunc(e->next->data);
                    Free(e->next->skey);
                    Free(e->next);
                    e->next = next;
                    ht->entries--;
                    result = 1;
                    break;
                }
            }
        }
    }
    else {
        if (e->ikey == ikey && !e->skey) {
            HashEntry *next = e->next;
            if (delFunc) delFunc(e->data);
            Free(e);
            ht->list[hash] = next;
            ht->entries--;
            result = 1;
        }
        else {
            for (; e->next; e = e->next) {
                if (e->next->ikey == ikey && !e->next->skey) {
                    HashEntry *next = e->next->next;
                    if (delFunc) delFunc(e->next->data);
                    Free(e->next);
                    e->next = next;
                    ht->entries--;
                    result = 1;
                    break;
                }
            }
        }
    }
    
    if (result) {
        if (ht->capacity/2 >= ht->minCapacity &&
            (double)ht->entries/(double)ht->capacity < HT_SHRINK_AT) {
            HT_Resize(ht, ht->capacity/2);
        }
    }
    
    return result;
}

/*
 * HT_RemoveEntry
 * --------------
 * Remove entry without deleting, for re-insertion.
 */
int HT_RemoveEntry(HashTable *ht, HashEntry *entry) {
    unsigned int hash;
    int result = 0;

    hash = HT_Hash(entry->skey, entry->ikey)%ht->capacity;

    if (!ht->list[hash]) return 0;

    HashEntry *e = ht->list[hash];
    
    if (e == entry) {
        HashEntry *next = e->next;
        ht->list[hash] = next;
        ht->entries--;
        result = 1;
    }
    else {
        for (; e->next; e = e->next) {
            if (e->next == entry) {
                HashEntry *next = e->next->next;
                e->next = next;
                ht->entries--;
                result = 1;
                break;
            }
        }
    }
    
    return result;
}


/*
 * Function: HT_Function
 * ---------------------
 */
int HT_Function(HashTable *ht, const char *skey, int ikey, void (*func)(void *)) {
    unsigned int hash;

    hash = HT_Hash(skey, ikey)%ht->capacity;

    if (!ht->list[hash]) return 0;

    HashEntry *e = ht->list[hash];
    if (skey) {
        if (e->ikey == ikey && e->skey && strcmp(e->skey, skey) == 0) {
            func(e->data);
            return 1;
        }
        else {
            for (; e->next; e = e->next) {
                if (e->next->ikey == ikey && e->next->skey && strcmp(e->next->skey, skey) == 0) {
                    func(e->next->data);
                    return 1;
                }
            }
        }
    }
    else {
        if (e->ikey == ikey && !e->skey) {
            func(e->data);
            return 1;
        }
        else {
            for (; e->next; e = e->next) {
                if (e->next->ikey == ikey && !e->next->skey) {
                    func(e->next->data);
                    return 1;
                }
            }
        }
    }
    
    return 0;
}

/*
 * Function: HT_Get
 * ----------------
 */
void *HT_Get(HashTable *ht, const char *skey, int ikey) {
    unsigned int hash;
    HashEntry *e;

    hash = HT_Hash(skey, ikey)%ht->capacity;
    if (skey) {
        for (e = ht->list[hash]; e  != 0; e = e->next)
            if (e->ikey == ikey && e->skey && strcmp(e->skey, skey) == 0) return e->data;
    }
    else {
        for (e = ht->list[hash]; e  != 0; e = e->next)
            if (e->ikey == ikey && !e->skey) return e->data;
    }

    return 0;
}

void *HT_GetPH(HashTable *ht, unsigned int hash, const char *skey, int ikey) {
    HashEntry *e;

    hash %= ht->capacity;
    if (skey) {
        for (e = ht->list[hash]; e  != 0; e = e->next)
            if (e->ikey == ikey && e->skey && strcmp(e->skey, skey) == 0) return e->data;
    }
    else {
        for (e = ht->list[hash]; e  != 0; e = e->next)
            if (e->ikey == ikey && !e->skey) return e->data;
    }

    return 0;    
}

/*
 * Function: HT_GetEntry
 * ---------------------
 */
HashEntry *HT_GetEntry(HashTable *ht, const char *skey, int ikey) {
    unsigned int hash;
    HashEntry *e;

    hash = HT_Hash(skey, ikey)%ht->capacity;
    if (skey) {
        for (e = ht->list[hash]; e  != 0; e = e->next)
            if (e->ikey == ikey && e->skey && strcmp(e->skey, skey) == 0) return e;
    }
    else {
        for (e = ht->list[hash]; e  != 0; e = e->next)
            if (e->ikey == ikey && !e->skey) return e;
    }
    return 0;
}

HashEntry *HT_GetOrCreateEntry(HashTable *ht, const char *skey, int ikey) {
    unsigned int hash;
    HashEntry *e;

    hash = HT_Hash(skey, ikey)%ht->capacity;

    if (ht->list[hash] == 0) {
        e = (HashEntry *)Malloc(sizeof(HashEntry));
        ht->list[hash] = e;
    }
    else {
        e = ht->list[hash];
        if (skey) {
            while (e) {
                if (e->ikey == ikey && e->skey && strcmp(skey, e->skey) == 0) {
                    return e;
                }
                if (!e->next) {
                    e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                    e = e->next;
                    break;
                }
                e = e->next;
            }
        }
        else {
            while (e) {
                if (e->ikey == ikey && !e->skey) {
                    return e;
                }
                if (!e->next) {
                    e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                    e = e->next;
                    break;
                }
                e = e->next;
            }
        }
    }
    e->skey = skey ? Strdup(skey) : 0;
    e->ikey = ikey;
    e->next = 0;
    e->data = 0;
    ht->entries++;

    if ((double)ht->entries/(double)ht->capacity > HT_GROW_AT) {
        HT_Resize(ht, ht->capacity*2);
    }
    
    return e;
}

HashEntry *HT_GetOrCreateEntryPH(HashTable *ht, unsigned int hash, const char *skey, int ikey) {
    HashEntry *e;

    hash %= ht->capacity;

    if (ht->list[hash] == 0) {
        e = (HashEntry *)Malloc(sizeof(HashEntry));
        ht->list[hash] = e;
    }
    else {
        e = ht->list[hash];
        if (skey) {
            while (e) {
                if (e->ikey == ikey && e->skey && strcmp(skey, e->skey) == 0) {
                    return e;
                }
                if (!e->next) {
                    e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                    e = e->next;
                    break;
                }
                e = e->next;
            }
        }
        else {
            while (e) {
                if (e->ikey == ikey && !e->skey) {
                    return e;
                }
                if (!e->next) {
                    e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                    e = e->next;
                    break;
                }
                e = e->next;
            }
        }
    }
    e->skey = skey ? Strdup(skey) : 0;
    e->ikey = ikey;
    e->next = 0;
    e->data = 0;
    ht->entries++;

    if ((double)ht->entries/(double)ht->capacity > HT_GROW_AT) {
        HT_Resize(ht, ht->capacity*2);
    }
    
    return e;
}

/*
 * Function: FindEntry
 * -------------------
 * Find any (first possible) entry where user function returns 1.
 */ 
HashEntry *HT_FindEntry(HashTable *ht, int (*func)(void *, void *), void *userData) {
    if (ht->entries == 0) return 0;
    
    for (int i = 0; i < ht->capacity; i++) {
        if (ht->list[i]) {
            HashEntry *e = ht->list[i];
            while (e) {
                if (e->data && func(e->data, userData)) return e;
                e = e->next;
            }
        }
    }
    return 0;    
}

/*
 * Function: HT_GetEntries
 * -----------------------
 * Get a list of all entries in table.
 */
HashEntry *HT_GetEntries(HashTable *ht) {
    HashEntry *list = 0;
    HashEntry *e = 0;
    int i;
    
    for (i = 0; i < ht->capacity; i++) {
        if (ht->list[i]) {
            if (e) {
                e->next = (HashEntry *)Malloc(sizeof(HashEntry));
                e->next->data = ht->list[i]->data;
                e->next->skey = ht->list[i]->skey;
                e->next->ikey = ht->list[i]->ikey;
                e->next->next = 0;
                e = e->next;
            }
            else {
                list = (HashEntry *)Malloc(sizeof(HashEntry));
                list->data = ht->list[i]->data;
                list->skey = ht->list[i]->skey;
                list->ikey = ht->list[i]->ikey;
                list->next = 0;
                e = list;
            }
        }
    }
    return list;
}

/*
 * Function: HT_GetEntriesArray
 * ----------------------------
 * Get all hash entries as a null terminated array.
 */
HashEntry **HT_GetEntriesArray(HashTable *ht) {
    HashEntry **result = (HashEntry **)Malloc(sizeof(HashEntry *)*(ht->entries + 1));
    int index = 0;
        
    for (int i = 0; i < ht->capacity; i++) {
        HashEntry *e = ht->list[i];
        while (e) {
            result[index++] = e;
            e = e->next;
        }
    }
    result[index] = 0;
        
    return result;
}

/*
 * Function: HT_Exists
 * -------------------
 */
int HT_Exists(HashTable *ht, const char *skey, int ikey) {
    /* return HT_Get(ht, key) != 0; */
    unsigned int hash;
    HashEntry *e;

    hash = HT_Hash(skey, ikey)%ht->capacity;
    if (skey) {
        for (e = ht->list[hash]; e  != 0; e = e->next)
            if (e->ikey == ikey && e->skey && strcmp(e->skey, skey) == 0) return 1;
    }
    else {
        for (e = ht->list[hash]; e  != 0; e = e->next)
            if (e->ikey == ikey && !e->skey) return 1;
    }

    return 0;

}

/*
 * Function: HT_Resize
 * -----------------------
 */
void HT_Resize(HashTable *ht, unsigned int capacity) {
    HashEntry **oldList;
    unsigned int oldCapacity;
    unsigned int i;

    if (capacity < 1) capacity = 1;
    if (ht->minCapacity > capacity) ht->minCapacity = capacity;
    if (ht->capacity == capacity) return;
    
    oldList = ht->list;
    oldCapacity = ht->capacity;

    ht->list = (HashEntry **)Malloc(sizeof(HashEntry *)*capacity);
    ht->capacity = capacity;
    for (i = 0; i < capacity; i++) ht->list[i] = 0;
    
    for (i = 0; i < oldCapacity; i++) {
        HashEntry *e = oldList[i];
        while (e) {
            /* Keep old entry. */
            HashEntry *next = e->next;
            HT_AddEntry(ht, e);
            ht->entries--;
            e = next;
        }
    }
    Free(oldList);
}

/*
 * HT_ReIndex
 * ----------
 */
void HT_ReIndex(HashTable *ht, int minIndex, int maxIndex) {
    int currentIndex = minIndex;    
    for (int i = minIndex; i <= maxIndex; i++) {
        HashEntry *e = HT_GetEntry(ht, 0, i);        
        if (e) {
            if (e->ikey != currentIndex) {
                HT_RemoveEntry(ht, e);
                e->ikey = currentIndex;
                HT_AddEntry(ht, e);
            }
            currentIndex++;
        }
    }
}


int HT_TableCount() {
#ifdef HT_REF_COUNT
    return sTables;
#else
    return 0;
#endif
}

