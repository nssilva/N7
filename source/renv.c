/*
 * renv.c
 * ------
 * N7 runtime.
 *
 * Coercion rules:
 *   =              Comparison if both sides are of the same type, else strings
 *                  are converted to numbers for numeric comparison if the other
 *                  side is numeric. Comparison of label/table with any other
 *                  type is false.
 *   <>             The opposite of =.
 *   <, <=, >, >=   For two strings, string comparison is used. For any mix of
 *                  number and string, numeric comparison is used. All other
 *                  types of comparisons are false.
 *   +              String concatenation with possible conversion if any side is
 *                  string, else numeric operation with possible conversion.
 *   -, *, /, %     Numeric operations, possible conversion of both sides
 *
 * NOTES:
 *   The gc needs to grow if it's called "too often". And it may possibly, just
 *   maybe, need to be able to shrink. The problem is that its only used for
 *   keeping track of table references. It doesn't care about the size of the
 *   tables. So with a large alloc table, huge tables may stay alive for very
 *   long, unless the garbage collector is called periodically ... hm.
 * By: Marcus 2021-2022
 */

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "time.h"
#include "unistd.h"
#include "pthread.h"
#include "setjmp.h"

#include "renv.h"
#include "n7mm.h"
#include "syscmd.h"
#include "bytecodes.h"

/* included extensions. */
#include "s3d.h"

/* Needed for runtime error message box */
#include "windowing.h"

//#define RELEASE_MODE

#define DEFAULT_HEAP_SIZE   16777216
#define STACK_SIZE          65536
#define MEMORY_STACK_SIZE   65536
#define CALL_STACK_SIZE     16384
#define ITERATOR_STACK_SIZE 16384

/* Should be increased to something much higher, this is just for testing. */
/*#define DEFAULT_ALLOC_TABLE_SIZE 65536 */
/*#define DEFAULT_ALLOC_TABLE_SIZE 16384 */
#define DEFAULT_ALLOC_TABLE_SIZE 512
#define GC_PERFORMANCE_CYCLES 16
#define GC_PERFORMANCE_MS_LOW 1000
#define GC_PERFORMANCE_MS_HIGH 5000

/* String constants. */
static char **sStrings;
static unsigned int *sStringHashes;
static int sStringCount;

/* Instructions. */
static Instruction *sInstructions;
static Instruction *sInstruction;
static int sInstructionCount;

/* Line number metadata. */
static LineNumberMetadata *sLineNumbers;
static int sLineNumberCount;

/* Filename metadata. */
static FilenameMetadata *sFilenames;
static int sFilenameCount;

/* Registers. */
static Variable sRegisters[10];

/* Stack. */
static Variable sStack[STACK_SIZE];
static int sStackIndex;

/* Memory stack. */
static Variable *sMemoryStack[MEMORY_STACK_SIZE];
static int sMemoryStackIndex;

/* Program memory. */
static Variable *sProgramMemory;
static Variable *sMemory;
static Variable sMemoryParent;

/* Call stack. */
typedef struct {
    int instructionIndex;
    Variable local;
} CallEnv;
static CallEnv sCallStack[CALL_STACK_SIZE];
static int sCallStackIndex;

/* Iterators for tables. */
typedef struct {
    HashTable *table;
    HashEntry **list;
    HashEntry **current;
    char isWrapper;
} TableIterator;
static TableIterator *sIteratorStack[ITERATOR_STACK_SIZE];
static int sIteratorStackIndex;

/* System command functions. */
static N7CFunction *sSystemCommandFunctions;

typedef struct {
    N7CFunction function;
    char *name;
} ExternalFunction;

/* External C functions. */
static ExternalFunction *sExternalFunctions = 0;
static int sExternalFunctionCount = 0;
static int sExternalFunctionsSize = 0;

/* Error. */
static jmp_buf sErrorJmpBuf; /* For worst cases. */
static char sError[1024];
static int sRunning;

/* Debug flag. */
static int sDebugOutput = 0;

/* Win32 or not. */
static int sWin32 = 0;

/* For garbage collecting. */
static void MarkAndSweep();

static void DeleteTable(void *ptr) {
    HT_Free((HashTable *)ptr, DeleteVariable);
}

static int RENV_Run(int argc, char **argv);
static void ThrowError(const char *msg);
static void FunctionCallError(const char *name, int expectedArguments, int actualArguments);
static void DumpMemory(Variable *memory);
static void DumpRegisters();
static void DumpStack();
/*static Variable CopyVariable(Variable *v);*/
static void CopyVariable(Variable *dst, Variable *v);
static char *StringConcat(const char *a, const char *b);
static unsigned long TimeMS();

static double Mod(double x, double y);

static int ValueTrue(Variable *v);

/*
 * RENV_RunFile
 * ------------
 */
int RENV_RunFile(FILE *file, int argc, char **argv, int win32) {
    sWin32 = win32;
     
    if (file) {
        int result;
        int heapSize;
       
        fread(&sDebugOutput, sizeof(char), 1, file);
        fread(&heapSize, sizeof(unsigned int), 1, file);
       
        /* Load line number metadata. */
        fread(&sLineNumberCount, sizeof(int), 1, file);
        if (sLineNumberCount > 0) {
            sLineNumbers = (LineNumberMetadata *)malloc(sizeof(LineNumberMetadata)*sLineNumberCount);
            fread(sLineNumbers, sizeof(LineNumberMetadata), sLineNumberCount, file);
        }
        else sLineNumbers = 0;
        
        fread(&sFilenameCount, sizeof(int), 1, file);
        if (sFilenameCount > 0) {
            sFilenames = (FilenameMetadata *)malloc(sizeof(FilenameMetadata)*sFilenameCount);
            for (int i = 0; i < sFilenameCount; i++) {
                int len;
                fread(&sFilenames[i].instructionIndex, sizeof(int), 1, file);
                fread(&len, sizeof(int), 1, file);
                sFilenames[i].filename = (char *)malloc(sizeof(char)*(len + 1));
                fread(sFilenames[i].filename, sizeof(char), len, file);
                sFilenames[i].filename[len] = '\0';
            }
        }
        else sFilenames = 0;
        
        /* Load strings. */
        fread(&sStringCount, sizeof(int), 1, file);
        if (sStringCount > 0) {
            sStrings = (char **)malloc(sizeof(char *)*sStringCount);
            sStringHashes = (unsigned int *)malloc(sizeof(unsigned int)*sStringCount);
            for (int i = 0; i < sStringCount; i++) {
                int len;
                fread(&len, sizeof(int), 1, file);
                sStrings[i] = (char *)malloc(sizeof(char)*(len + 1));
                fread(sStrings[i], sizeof(char), len, file);
                sStrings[i][len] = '\0';
                sStringHashes[i] = HT_Hash(sStrings[i], 0);
            }
        }
        else sStrings = 0;
        
        /* Load instructions. */
        fread(&sInstructionCount, sizeof(int), 1, file);
        sInstructions = (Instruction *)malloc(sizeof(Instruction)*sInstructionCount);
        fread(sInstructions, sizeof(Instruction), sInstructionCount, file);
        
        fclose(file);

        MM_SetDebugOutput(sDebugOutput);        
        MM_Init(heapSize > 0 ? heapSize : DEFAULT_HEAP_SIZE);
        MM_SetMarkAndSweepFunction(MarkAndSweep);
        MM_SetDestructorFunction(1, DeleteTable);
        MM_SetErrorFunction(ThrowError);
        HT_SetCustomMalloc(MM_Malloc);
        HT_SetCustomFree(MM_Free);
        HT_SetCustomStrdup(MM_Strdup);

        /* Set up system commands, functions called via BC_SYS. */
        sSystemCommandFunctions = SYS_Init();
        
        /* Let included extensions register their functions. */
        S3D_Init();                
        
#ifdef HT_REF_COUNT
        if (sDebugOutput) printf("renv: Tables: %d\n", HT_TableCount());
#endif
        
        result = RENV_Run(argc, argv);

        /* Terminate extensions. */
        S3D_Terminate();
        
        for (int i = 0; i < sStringCount; i++) free(sStrings[i]);
        if (sExternalFunctions) {
            for (int i = 0; i < sExternalFunctionCount; i++) free(sExternalFunctions[i].name);
            free(sExternalFunctions);
        }
        if (sStrings) free(sStrings);
        if (sStringHashes) free(sStringHashes);
        if (sInstructions) free(sInstructions);
        if (sLineNumbers) free(sLineNumbers);
        if (sFilenames) {
            for (int i = 0; i < sFilenameCount; i++) free(sFilenames[i].filename);
            free(sFilenames);
        }
        
#ifdef HT_REF_COUNT
        /* Should be same as before RENV_Run. */
        if (sDebugOutput) printf("renv: Tables: %d\n", HT_TableCount());
#endif       

        MM_PrintMemoryInfo();
        MM_Terminate();
       
        return result;
    }
    else {
        sprintf(sError, "Could not read file");
       
        return RENV_FAILURE;
    }
}

/*
 * RegisterN7CFunction
 * -------------------
 * Register a function with a name, so that a running n7 program can access it
 * through LOAD_FUNCTION.
 */
void RegisterN7CFunction(const char *name, N7CFunction function) {
    if (sExternalFunctionCount == sExternalFunctionsSize) {
        sExternalFunctionsSize += 64;
        /* Dude, if it fails, you lose the list and can't release the names, FIX! */
        sExternalFunctions = (ExternalFunction *)realloc(sExternalFunctions, sizeof(ExternalFunction)*sExternalFunctionsSize);
        if (!sExternalFunctions) {
            RuntimeError("Could not allocate memory for extension functions");
            return;
        }
    }
    sExternalFunctions[sExternalFunctionCount].name = strdup(name);
    sExternalFunctions[sExternalFunctionCount].function = function;
    sExternalFunctionCount++;
}

int GetN7CFunctionIndex(const char *name) {
    int i;
    for (i = 0; i < sExternalFunctionCount; i++) if (!strcmp(name, sExternalFunctions[i].name)) break;
    return i == sExternalFunctionCount ? -1 : i;
}

N7CFunction GetN7CFunction(int index) {
    if (index >= 0 && index < sExternalFunctionCount) return sExternalFunctions[index].function;
    else return 0;
}

/* HasConsole
 * ----------
 * Return 1 if not a win32 program.
 */
int HasConsole() {
  return !sWin32;
}

int RENV_Run(int argc, char **argv) {
    Variable swap;
    Variable *swapRef;
    Variable *var;
    Variable *args;
    HashEntry *he;
    N7CFunction n7c;
    char *str;
    char numKey[16];
    char eval = 0;
    int result = 0;
    int thrownError = 0;

    strcpy(sError, "");
    
    /* Init gc. */
    /*InitGC();*/
    
    /* Init memory. */
    sProgramMemory = (Variable *)MM_Malloc(sizeof(Variable));
    sProgramMemory->value.t = NewHashTable(1);
    sProgramMemory->type = VAR_TBL;
    sMemory = sProgramMemory;
    sMemoryParent.type = VAR_UNSET;
    
    /* Add command line arguments to args variable. */
    args = (Variable *)MM_Malloc(sizeof(Variable));
    args->value.t = NewHashTable(argc ? argc : 1);
    args->type = VAR_TBL;
    for (int i = 0; i < argc; i++) {
        Variable *arg = (Variable *)MM_Malloc(sizeof(Variable));
        arg->type = VAR_STR;
        arg->value.s = strdup(argv[i]);
        HT_Add(args->value.t, 0, i, arg);
    }
    HT_Add(sProgramMemory->value.t, "args", 0, args);

    /* Init registers. */
    for (int i = 0; i < 10; i++) sRegisters[i].type = VAR_UNSET;
    
    /* Init stack. */
    for (int i = 0; i < STACK_SIZE; i++) sStack[i].type = VAR_UNSET;
    sStackIndex = 0;
    
    /* Init memory stack. */
    sMemoryStackIndex = 0;
    
    /* Init call stack. */
    sCallStackIndex = 0;
    
    /* Init iterator stack. */
    sIteratorStackIndex = 0;
    for (int i = 0; i < ITERATOR_STACK_SIZE; i++) sIteratorStack[i] = 0;
    
    unsigned long startTime = TimeMS();
    
    sInstruction = sInstructions;

    /* Only mm can throw this. */
    thrownError = setjmp(sErrorJmpBuf);
    sRunning = !thrownError;

    while (sRunning) {
        switch (sInstruction->cmd) {
            case BC_NOP:
                break;
            case BC_END:
                sRunning = 0;
                break;
            case BC_MDUMP:
                DumpMemory(sMemory);
                break;
            case BC_RDUMP:
                DumpRegisters();
                break;
            case BC_SDUMP:
                DumpStack();
                break;
            case BC_MADD_S:
#ifndef RELEASE_MODE
                if (sMemory->type != VAR_TBL) {
                    RuntimeErrorS("Can't add identifier '%s', parent is not a table (BC_MADD_S)", sStrings[sInstruction->lparam.i]);
                }
                else {
#endif
                    he = HT_GetOrCreateEntryPH(sMemory->value.t, sStringHashes[sInstruction->lparam.i], sStrings[sInstruction->lparam.i], 0);
                    if (!he->data) {
                        var = (Variable *)MM_Malloc(sizeof(Variable));
                        var->type = VAR_UNSET;
                        he->data = var;
                    }
#ifndef RELEASE_MODE                    
                }
#endif             
                break;
            case BC_MADD_N:
#ifndef RELEASE_MODE
                if (sMemory->type != VAR_TBL) {
                    itoa(sInstruction->lparam.i, numKey, 10);
                    RuntimeErrorS("Can't add index %d, parent is not a table (BC_MADD_N)", numKey);
                }
                else {
#endif
                    he = HT_GetOrCreateEntry(sMemory->value.t, 0, sInstruction->lparam.i);
                    if (!he->data) {
                        var = (Variable *)MM_Malloc(sizeof(Variable));
                        var->type = VAR_UNSET;
                        he->data = var;
                    }
#ifndef RELEASE_MODE
                }
#endif
                break;
            case BC_MADD_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) {
#ifndef RELEASE_MODE
                    if (sMemory->type == VAR_TBL) {
#endif       
                        he = HT_GetOrCreateEntry(sMemory->value.t, sRegisters[sInstruction->lparam.i].value.s, 0);
                        if (!he->data) {
                            var = (Variable *)MM_Malloc(sizeof(Variable));
                            var->type = VAR_UNSET;
                            he->data = var;
                        }
#ifndef RELEASE_MODE
                    }
                    else {
                        RuntimeErrorS("Can't add identifier '%s', parent is not a table (BC_MADD_R)", sRegisters[sInstruction->lparam.i].value.s);
                    }
#endif
                }
                else if (sRegisters[sInstruction->lparam.i].type == VAR_NUM) {
#ifndef RELEASE_MODE
                    if (sMemory->type == VAR_TBL) {
#endif
                        he = HT_GetOrCreateEntry(sMemory->value.t, 0, (int)sRegisters[sInstruction->lparam.i].value.n);
                        if (!he->data) {
                            var = (Variable *)MM_Malloc(sizeof(Variable));
                            var->type = VAR_UNSET;
                            he->data = var;
                        }
#ifndef RELEASE_MODE
                    }
                    else {
                        itoa((int)sRegisters[sInstruction->lparam.i].value.n, numKey, 10);
                        RuntimeErrorS("Can't add index %s, parent is not a table (BC_MADD_R)", numKey);
                    }
#endif
                }
                else {
                    RuntimeError("Register contains no identifier or index (BC_MADD_R)");
                }
                break;
            case BC_OPT_MALS_S:
#ifndef RELEASE_MODE
                if (sMemory->type != VAR_TBL) {
                    RuntimeErrorS("Can't add identifier '%s', parent is not a table (BC_OPT_MALS_S)", sStrings[sInstruction->lparam.i]);
                }
                else {
#endif
                    he = HT_GetOrCreateEntryPH(sMemory->value.t, sStringHashes[sInstruction->lparam.i], sStrings[sInstruction->lparam.i], 0);
                    if (!he->data) {
                        var = (Variable *)MM_Malloc(sizeof(Variable));
                        var->type = VAR_UNSET;
                        he->data = var;
                    }
#ifndef RELEASE_MODE
                    if (sMemoryStackIndex <= 0) {
                        RuntimeError("Memory stack is empty (BC_OPT_MALS_S)");
                        break;
                    }
#endif
                    sMemoryParent = *(Variable *)he->data;
                    sMemory = sMemoryStack[sMemoryStackIndex - 1];
                    sMemoryStack[sMemoryStackIndex - 1] = (Variable *)he->data;
#ifndef RELEASE_MODE
                }
#endif             
                break;
            case BC_OPT_MALS_N:
#ifndef RELEASE_MODE
                if (sMemory->type != VAR_TBL) {
                    itoa(sInstruction->lparam.i, numKey, 10);
                    RuntimeErrorS("Can't add index %d, parent is not a table (BC_OPT_MALS_N)", numKey);
                }
                else {
#endif
                    he = HT_GetOrCreateEntry(sMemory->value.t, 0, sInstruction->lparam.i);
                    if (!he->data) {
                        var = (Variable *)MM_Malloc(sizeof(Variable));
                        var->type = VAR_UNSET;
                        he->data = var;
                    }
#ifndef RELEASE_MODE
                    if (sMemoryStackIndex <= 0) {
                        RuntimeError("Memory stack is empty (BC_OPT_MALS_N)");
                        break;
                    }
#endif
                    sMemoryParent = *(Variable *)he->data;
                    sMemory = sMemoryStack[sMemoryStackIndex - 1];
                    sMemoryStack[sMemoryStackIndex - 1] = (Variable *)he->data;
#ifndef RELEASE_MODE
                }
#endif
                break;
            case BC_OPT_MALS_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) {
#ifndef RELEASE_MODE
                    if (sMemory->type == VAR_TBL) {
#endif
                        he = HT_GetOrCreateEntry(sMemory->value.t, sRegisters[sInstruction->lparam.i].value.s, 0);
                        if (!he->data) {
                            var = (Variable *)MM_Malloc(sizeof(Variable));
                            var->type = VAR_UNSET;
                            he->data = var;
                        }
#ifndef RELEASE_MODE
                        if (sMemoryStackIndex <= 0) {
                            RuntimeError("Memory stack is empty (BC_OPT_MALS_R)");
                            break;
                        }
#endif
                        sMemoryParent = *(Variable *)he->data;
                        sMemory = sMemoryStack[sMemoryStackIndex - 1];
                        sMemoryStack[sMemoryStackIndex - 1] = (Variable *)he->data;
#ifndef RELEASE_MODE
                    }
                    else {
                        RuntimeErrorS("Can't add identifier '%s', parent is not a table (BC_OPT_MALS_R)", sRegisters[sInstruction->lparam.i].value.s);
                    }
#endif
                }
                else if (sRegisters[sInstruction->lparam.i].type == VAR_NUM) {
#ifndef RELEASE_MODE
                    if (sMemory->type == VAR_TBL) {
#endif
                        he = HT_GetOrCreateEntry(sMemory->value.t, 0, (int)sRegisters[sInstruction->lparam.i].value.n);
                        if (!he->data) {
                            var = (Variable *)MM_Malloc(sizeof(Variable));
                            var->type = VAR_UNSET;
                            he->data = var;
                        }
#ifndef RELEASE_MODE
                        if (sMemoryStackIndex <= 0) {
                            RuntimeError("Memory stack is empty (BC_OPT_MALS_R)");
                            break;
                        }
#endif
                        sMemoryParent = *(Variable *)he->data;
                        sMemory = sMemoryStack[sMemoryStackIndex - 1];
                        sMemoryStack[sMemoryStackIndex - 1] = (Variable *)he->data;
#ifndef RELEASE_MODE
                    }
                    else {
                        itoa((int)sRegisters[sInstruction->lparam.i].value.n, numKey, 10);
                        RuntimeErrorS("Can't add index %s, parent is not a table (BC_OPT_MALS_R)", numKey);
                    }
#endif
                }
                else {
                    RuntimeError("Register contains no identifier or index (BC_OPT_MALS_R)");
                }
                break;
            /////////////////////////////////
 

            case BC_MLOAD:
                sMemoryParent = *sMemory;
                sMemory = sProgramMemory;
                break;
            case BC_MLOAD_S:
#ifndef RELEASE_MODE
                if (sMemory->type != VAR_TBL) {
                    RuntimeErrorS("Can't load identifier '%s', parent is not a table (BC_MLOAD_S)", sStrings[sInstruction->lparam.i]);
                }
                else {
                    sMemoryParent = *sMemory;
                    if (!(sMemory = (Variable *)HT_GetPH(sMemory->value.t, sStringHashes[sInstruction->lparam.i], sStrings[sInstruction->lparam.i], 0))) {
                        RuntimeErrorS("Identifier '%s' not found (BC_MLOAD_S)", sStrings[sInstruction->lparam.i]);
                    }
                }
#else
                sMemoryParent = *sMemory;
                sMemory = (Variable *)HT_GetPH(sMemory->value.t, sStringHashes[sInstruction->lparam.i], sStrings[sInstruction->lparam.i], 0);
#endif
                break;
            case BC_MLOAD_N:
#ifndef RELEASE_MODE
                if (sMemory->type != VAR_TBL) {
                    itoa(sInstruction->lparam.i, numKey, 10);
                    RuntimeErrorS("Can't load index %s, parent is not a table (BC_MLOAD_N)", numKey);
                }
                else {
                    sMemoryParent = *sMemory;
                    if (!(sMemory = (Variable *)HT_Get(sMemory->value.t, 0, sInstruction->lparam.i))) {
                        itoa(sInstruction->lparam.i, numKey, 10);
                        RuntimeErrorS("Index %s not found (BC_MLOAD_N)", numKey);
                    }
                }
#else
                sMemoryParent = *sMemory;
                sMemory = (Variable *)HT_Get(sMemory->value.t, 0, sInstruction->lparam.i);
#endif
                break;
            case BC_MLOAD_R:
                sMemoryParent = *sMemory;
#ifndef RELEASE_MODE
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) {
                    if (sMemory->type == VAR_TBL) {
                        if (!(sMemory = (Variable *)HT_Get(sMemory->value.t, sRegisters[sInstruction->lparam.i].value.s, 0))) {
                            RuntimeErrorS("Identifier %s not found (BC_MLOAD_R)", sRegisters[sInstruction->lparam.i].value.s);
                        }
                    }
                    else {
                        RuntimeErrorS("Can't load identifier '%s', parent is not a table (BC_MLOAD_R)", sRegisters[sInstruction->lparam.i].value.s);
                    }
                }
                else if (sRegisters[sInstruction->lparam.i].type == VAR_NUM) {
                    if (sMemory->type == VAR_TBL) {
                        if (!(sMemory = (Variable *)HT_Get(sMemory->value.t, 0, (int)sRegisters[sInstruction->lparam.i].value.n))) {
                            itoa((int)sRegisters[sInstruction->lparam.i].value.n, numKey, 10);
                            RuntimeErrorS("Index %s not found (BC_MLOAD_R)", numKey);
                        }
                    }
                    else {
                        itoa((int)sRegisters[sInstruction->lparam.i].value.n, numKey, 10);
                        RuntimeErrorS("Can't load index %s, parent is not a table (BC_MLOAD_R)", numKey);
                    }
                }
                /* Dammet, could this cause problems? */
                else if (sRegisters[sInstruction->lparam.i].type == VAR_TBL) {
                    sMemory = &sRegisters[sInstruction->lparam.i];
                }
                else {
                    RuntimeError("Register contains no identifier or index (BC_MLOAD_R)");
                }
#else
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) {
                    sMemory = (Variable *)HT_Get(sMemory->value.t, sRegisters[sInstruction->lparam.i].value.s, 0);
                }
                else if (sRegisters[sInstruction->lparam.i].type == VAR_NUM) {
                    sMemory = (Variable *)HT_Get(sMemory->value.t, 0, (int)sRegisters[sInstruction->lparam.i].value.n);
                }
                /* Dammet, could this cause problems? */
                else if (sRegisters[sInstruction->lparam.i].type == VAR_TBL) {
                    sMemory = &sRegisters[sInstruction->lparam.i];
                }
                else {
                    RuntimeError("Register contains no identifier or index (BC_MLOAD_R)");
                }
#endif
                break;
            case BC_MLOADS:
                sMemoryParent = *sMemory;
                sMemory = &sStack[sStackIndex - 1];
                break;
                
            case BC_MSET_S:
                if (sMemory->type == VAR_STR) free(sMemory->value.s);
                sMemory->type = VAR_STR;
                sMemory->value.s = strdup(sStrings[sInstruction->lparam.i]);
                break;
            case BC_MSET_N:
                if (sMemory->type == VAR_STR) free(sMemory->value.s);
                sMemory->type = VAR_NUM;
                sMemory->value.n = sInstruction->lparam.d;
                break;
            case BC_MSET_L:
                if (sMemory->type == VAR_STR) free(sMemory->value.s);
                sMemory->type = VAR_LBL;
                sMemory->value.l = sInstruction->lparam.i;
                break;
            case BC_MSET_R:
                if (sMemory->type == VAR_STR) free(sMemory->value.s);
                *sMemory = sRegisters[sInstruction->lparam.i];
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) sMemory->value.s = strdup(sRegisters[sInstruction->lparam.i].value.s);
                break;
            /* Assignment optimization: MSWAP + MSET + MPOP */
            case BC_OPT_MSSP_R:
#ifndef RELEASE_MODE
                if (sMemoryStackIndex <= 0) {
                    RuntimeError("Memory stack is empty (BC_OPT_MSSP_R");
                    break;
                }
#endif
                /* Set memory on stack and then dec the index. */
                swapRef = sMemoryStack[sMemoryStackIndex - 1];
                if (swapRef->type == VAR_STR) free(swapRef->value.s);
                *swapRef = sRegisters[sInstruction->lparam.i];
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) swapRef->value.s = strdup(sRegisters[sInstruction->lparam.i].value.s);
                sMemoryParent = *swapRef;
                sMemoryStackIndex--;
                break;
                
            case BC_LPTBL_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                if (sMemoryParent.type == VAR_TBL && !(sMemoryParent.value.t == sProgramMemory->value.t || (sCallStackIndex > 0 && sMemoryParent.value.t == sCallStack[sCallStackIndex - 1].local.value.t ))) {
                    sRegisters[sInstruction->lparam.i] = sMemoryParent;
                }
                else {
                    sRegisters[sInstruction->lparam.i].type = VAR_UNSET;
                }
                break;
            case BC_MCLR:
                if (sMemory->type == VAR_STR) free(sMemory->value.s);
                sMemory->type = VAR_UNSET;
                break;
                
            case BC_MGET_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i] = *sMemory;
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) sRegisters[sInstruction->lparam.i].value.s = strdup(sMemory->value.s);
                break;

            case BC_MPUSH:
#ifndef RELEASE_MODE
                if (sMemoryStackIndex >= MEMORY_STACK_SIZE) {
                    RuntimeError("Memory stack limit reached (BC_MPUSH)");
                    break;
                }
#endif            
                sMemoryStack[sMemoryStackIndex++] = sMemory;
                break;            
            case BC_MPOP:
#ifndef RELEASE_MODE
                if (sMemoryStackIndex <= 0) {
                    RuntimeError("Memory stack is empty (BC_MPOP)");
                    break;
                }
#endif
                sMemoryParent = *sMemory;
                sMemory = sMemoryStack[--sMemoryStackIndex];
                break;
            case BC_MSWAP:
#ifndef RELEASE_MODE
                if (sMemoryStackIndex <= 0) {
                    RuntimeError("Memory stack is empty (BC_MSWAP)");
                    break;
                }
#endif
                sMemoryParent = *sMemory;
                swapRef = sMemory;
                sMemory = sMemoryStack[sMemoryStackIndex - 1];
                sMemoryStack[sMemoryStackIndex - 1] = swapRef;
                break;
                
            case BC_CLR_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i].type = VAR_UNSET;
                break;

            case BC_MOVE_R_S:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i].type = VAR_STR;
                sRegisters[sInstruction->lparam.i].value.s = strdup(sStrings[sInstruction->rparam.i]);
                break;
            case BC_MOVE_R_N:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                sRegisters[sInstruction->lparam.i].value.n = sInstruction->rparam.d;
                break;
            case BC_MOVE_R_L:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i].type = VAR_LBL;
                sRegisters[sInstruction->lparam.i].value.l = sInstruction->rparam.i;
                break;
            case BC_MOVE_R_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i] = sRegisters[sInstruction->rparam.i];
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) sRegisters[sInstruction->lparam.i].value.s = strdup(sRegisters[sInstruction->lparam.i].value.s);
                break;
            
            case BC_JMP_L:
                sInstruction = &sInstructions[sInstruction->lparam.i - 1];
                break;
            case BC_EVAL_R:
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_UNSET: eval = 0; break;
                    case VAR_NUM: eval = sRegisters[sInstruction->lparam.i].value.n != 0.0; break;
                    case VAR_STR: eval = sRegisters[sInstruction->lparam.i].value.s[0] != '\0'; break;
                    case VAR_LBL: eval = sRegisters[sInstruction->lparam.i].value.l >= 0; break;
                    case VAR_TBL: eval = HT_EntryCount(sRegisters[sInstruction->lparam.i].value.t) > 0; break;
                }
                break;
            case BC_ECMP_R_R:
                if (sRegisters[sInstruction->lparam.i].type == sRegisters[sInstruction->rparam.i].type) {
                    switch (sRegisters[sInstruction->rparam.i].type) {
                        case VAR_UNSET:
                            eval = 0;
                            break;
                        case VAR_NUM:
                            eval = sRegisters[sInstruction->lparam.i].value.n == sRegisters[sInstruction->rparam.i].value.n;
                            break;
                        case VAR_STR:
                            eval = strcmp(sRegisters[sInstruction->lparam.i].value.s, sRegisters[sInstruction->rparam.i].value.s) == 0;
                            break;
                        case VAR_LBL:                        
                            eval = sRegisters[sInstruction->lparam.i].value.l == sRegisters[sInstruction->rparam.i].value.l; 
                            break;
                        case VAR_TBL:
                            eval = sRegisters[sInstruction->lparam.i].value.t == sRegisters[sInstruction->rparam.i].value.t; 
                            break;
                    }
                }
                else {
                    eval = 0;
                }
                break;
            case BC_JMPT_L:
                if (eval) sInstruction = &sInstructions[sInstruction->lparam.i - 1];
                break;
            case BC_JMPF_L:
                if (!eval) sInstruction = &sInstructions[sInstruction->lparam.i - 1];
                break;
                
            case BC_JMPET_R_L: /* Any reason for actually setting eval? YES, THE DAMNED ITERATOR HACK!!! */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_UNSET:
                        eval = 0;
                        break;
                    case VAR_NUM:
                        if ((eval = sRegisters[sInstruction->lparam.i].value.n != 0.0)) sInstruction = &sInstructions[sInstruction->rparam.i - 1]; 
                        break;
                    case VAR_STR:
                        if ((eval = sRegisters[sInstruction->lparam.i].value.s[0] != '\0')) sInstruction = &sInstructions[sInstruction->rparam.i - 1];
                        break;
                    case VAR_LBL:
                        if ((eval = sRegisters[sInstruction->lparam.i].value.l >= 0)) sInstruction = &sInstructions[sInstruction->rparam.i - 1];
                        break;
                    case VAR_TBL:
                        if ((eval = HT_EntryCount(sRegisters[sInstruction->lparam.i].value.t) > 0))  sInstruction = &sInstructions[sInstruction->rparam.i - 1];
                        break;
                }
                break;
            case BC_JMPEF_R_L: /* Any reason for actually setting eval? YES!!! */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_UNSET:
                        eval = 0;
                        sInstruction = &sInstructions[sInstruction->rparam.i - 1];
                        break;
                    case VAR_NUM:
                        if (!(eval = sRegisters[sInstruction->lparam.i].value.n != 0.0)) sInstruction = &sInstructions[sInstruction->rparam.i - 1]; 
                        break;
                    case VAR_STR:
                        if (!(eval = sRegisters[sInstruction->lparam.i].value.s[0] != '\0')) sInstruction = &sInstructions[sInstruction->rparam.i - 1];
                        break;
                    case VAR_LBL:
                        if (!(eval = sRegisters[sInstruction->lparam.i].value.l >= 0)) sInstruction = &sInstructions[sInstruction->rparam.i - 1];
                        break;
                    case VAR_TBL:
                        if (!(eval = HT_EntryCount(sRegisters[sInstruction->lparam.i].value.t) > 0))  sInstruction = &sInstructions[sInstruction->rparam.i - 1];
                        break;
                }
                break;
                
            case BC_PUSH_R:
#ifndef RELEASE_MODE
                if (sStackIndex >= STACK_SIZE) {
                    RuntimeError("Stack limit reached (BC_PUSH_R)");
                    break;
                }
#endif
                sStack[sStackIndex] = sRegisters[sInstruction->lparam.i];
                if (sStack[sStackIndex].type == VAR_STR) sStack[sStackIndex].value.s = strdup(sStack[sStackIndex].value.s);
                sStackIndex++;
                break;
            case BC_PUSH_N:
#ifndef RELEASE_MODE
                if (sStackIndex >= STACK_SIZE) {
                    RuntimeError("Stack limit reached (BC_PUSH_R)");
                    break;
                }
#endif
                sStack[sStackIndex].type = VAR_NUM;
                sStack[sStackIndex].value.n = sInstruction->lparam.d;
                sStackIndex++;
                break;
            case BC_PUSH_S:
#ifndef RELEASE_MODE
                if (sStackIndex >= STACK_SIZE) {
                    RuntimeError("Stack limit reached (BC_PUSH_R)");
                    break;
                }
#endif
                sStack[sStackIndex].type = VAR_STR;
                sStack[sStackIndex].value.s = strdup(sStrings[sInstruction->lparam.i]);
                sStackIndex++;
                break;
            case BC_PUSH_L:
#ifndef RELEASE_MODE
                if (sStackIndex >= STACK_SIZE) {
                    RuntimeError("Stack limit reached (BC_PUSH_R)");
                    break;
                }
#endif
                sStack[sStackIndex].type = VAR_LBL;
                sStack[sStackIndex].value.l = sInstruction->lparam.i;
                sStackIndex++;
                break;
                
            case BC_POP_R:
#ifndef RELEASE_MODE
                if (sStackIndex <= 0) {
                    RuntimeError("Stack is empty (BC_POP_R)");
                    break;
                }
#endif
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i] = sStack[--sStackIndex];
                break;
            case BC_SWAP_R:
#ifndef RELEASE_MODE
                if (sStackIndex <= 0) {
                    RuntimeError("Stack is empty (BC_SWAP_R)");
                    break;
                }
#endif            
                swap = sRegisters[sInstruction->lparam.i];
                sRegisters[sInstruction->lparam.i] = sStack[sStackIndex - 1];
                sStack[sStackIndex - 1] = swap;
                break;
            /* swap registers and pop to left */
            case BC_SPOP_R_R: /* Things will fuck up if lparam = rparam. */
                if (sRegisters[sInstruction->rparam.i].type == VAR_STR) free(sRegisters[sInstruction->rparam.i].value.s);
                sRegisters[sInstruction->rparam.i] = sRegisters[sInstruction->lparam.i];
                sRegisters[sInstruction->lparam.i] = sStack[--sStackIndex];
                break;
                
            case BC_OR_R_R: /* Lparam or rparam, result (1 or 0) in lparam. */
                str = sRegisters[sInstruction->lparam.i].type == VAR_STR ? sRegisters[sInstruction->lparam.i].value.s : 0;
                sRegisters[sInstruction->lparam.i].value.n = ValueTrue(&sRegisters[sInstruction->lparam.i]) || ValueTrue(&sRegisters[sInstruction->rparam.i]);
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                if (str) free(str);
                break;
            case BC_AND_R_R: /* Lparam and rparam, result (1 or 0) in lparam. */
                str = sRegisters[sInstruction->lparam.i].type == VAR_STR ? sRegisters[sInstruction->lparam.i].value.s : 0;
                sRegisters[sInstruction->lparam.i].value.n = ValueTrue(&sRegisters[sInstruction->lparam.i]) && ValueTrue(&sRegisters[sInstruction->rparam.i]);
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                if (str) free(str);
                break;

            case BC_POR:
                sStackIndex--;
                result = ValueTrue(&sStack[sStackIndex]) || ValueTrue(&sRegisters[0]);
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0].value.n = result;
                sRegisters[0].type = VAR_NUM;
                if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                sStack[sStackIndex].type = VAR_UNSET;
                break;
            case BC_PAND:
                sStackIndex--;
                result = ValueTrue(&sStack[sStackIndex]) && ValueTrue(&sRegisters[0]);
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0].value.n = result;
                sRegisters[0].type = VAR_NUM;
                if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                sStack[sStackIndex].type = VAR_UNSET;
                break;

            case BC_EQL_R_R: /* Lparam = rparam, result (1 or 0) in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n == sRegisters[sInstruction->rparam.i].value.n; 
                                break;
                            case VAR_STR:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n == swap.value.n; 
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                swap = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = swap.value.n == sRegisters[sInstruction->rparam.i].value.n; 
                                break;
                            case VAR_STR:
                                sRegisters[sInstruction->lparam.i].value.n = strcmp(sRegisters[sInstruction->lparam.i].value.s, sRegisters[sInstruction->rparam.i].value.s) == 0;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        free(str);
                        break;
                    case VAR_LBL:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_LBL:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.l == sRegisters[sInstruction->rparam.i].value.l;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        break;
                    case VAR_TBL:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_TBL:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.t == sRegisters[sInstruction->rparam.i].value.t;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        break;
                    default:
                        /* 210806, added unset constant. */
                        /*sRegisters[sInstruction->lparam.i].value.n = 0;*/
                        sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->rparam.i].type == VAR_UNSET;
                        break;
                }
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                break;
            case BC_LESS_R_R: /* Lparam < rparam, result (1 or 0) in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n < sRegisters[sInstruction->rparam.i].value.n; 
                                break;
                            case VAR_STR:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n < swap.value.n;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                swap = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = swap.value.n < sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            case VAR_STR:
                                sRegisters[sInstruction->lparam.i].value.n = strcmp(sRegisters[sInstruction->lparam.i].value.s, sRegisters[sInstruction->rparam.i].value.s) < 0;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        free(str);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i].value.n = 0;
                        break;
                }
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                break;
            case BC_GRE_R_R: /* Lparam > rparam, result (1 or 0) in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n > sRegisters[sInstruction->rparam.i].value.n; 
                                break;
                            case VAR_STR:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n > swap.value.n;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                swap = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = swap.value.n > sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            case VAR_STR:
                                sRegisters[sInstruction->lparam.i].value.n = strcmp(sRegisters[sInstruction->lparam.i].value.s, sRegisters[sInstruction->rparam.i].value.s) > 0;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        free(str);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i].value.n = 0;
                        break;
                }
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                break;
            case BC_LEQL_R_R: /* Lparam <= rparam, result (1 or 0) in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n <= sRegisters[sInstruction->rparam.i].value.n; 
                                break;
                            case VAR_STR:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n <= swap.value.n;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                swap = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = swap.value.n <= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            case VAR_STR:
                                sRegisters[sInstruction->lparam.i].value.n = strcmp(sRegisters[sInstruction->lparam.i].value.s, sRegisters[sInstruction->rparam.i].value.s) <= 0;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        free(str);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i].value.n = 0;
                        break;
                }
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                break;
            case BC_GEQL_R_R: /* Lparam >= rparam, result (1 or 0) in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n >= sRegisters[sInstruction->rparam.i].value.n; 
                                break;
                            case VAR_STR:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n >= swap.value.n;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                swap = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = swap.value.n >= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            case VAR_STR:
                                sRegisters[sInstruction->lparam.i].value.n = strcmp(sRegisters[sInstruction->lparam.i].value.s, sRegisters[sInstruction->rparam.i].value.s) >= 0;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 0;
                                break;
                        }
                        free(str);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i].value.n = 0;
                        break;
                }
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                break;
            case BC_NEQL_R_R: /* Lparam <> rparam, result (1 or 0) in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n != sRegisters[sInstruction->rparam.i].value.n; 
                                break;
                            case VAR_STR:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n != swap.value.n; 
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 1;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                swap = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = swap.value.n != sRegisters[sInstruction->rparam.i].value.n; 
                                break;
                            case VAR_STR:
                                sRegisters[sInstruction->lparam.i].value.n = strcmp(sRegisters[sInstruction->lparam.i].value.s, sRegisters[sInstruction->rparam.i].value.s) != 0;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 1;
                                break;
                        }
                        free(str);
                        break;
                    case VAR_LBL:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_LBL:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.l != sRegisters[sInstruction->rparam.i].value.l;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 1;
                                break;
                        }
                        break;
                    case VAR_TBL:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_TBL:
                                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.t != sRegisters[sInstruction->rparam.i].value.t;
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i].value.n = 1;
                                break;
                        }
                        break;
                    default:
                        /* 210806. */
                        /*sRegisters[sInstruction->lparam.i].value.n = 1;*/
                        sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->rparam.i].type != VAR_UNSET;
                        break;
                }
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                break;

            case BC_SPEQL:
                sStackIndex--;
                switch (sStack[sStackIndex].type) {
                    case VAR_UNSET:
                        result = sRegisters[0].type == VAR_UNSET;
                        break;
                    case VAR_NUM:
                        if (sRegisters[0].type == VAR_NUM) result = sStack[sStackIndex].value.n == sRegisters[0].value.n;
                        else if (sRegisters[0].type == VAR_STR) result = sStack[sStackIndex].value.n == ToNumber(&sRegisters[0]);
                        else result = 0;
                        break;
                    case VAR_STR:
                        if (sRegisters[0].type == VAR_STR) result = strcmp(sStack[sStackIndex].value.s, sRegisters[0].value.s) == 0;
                        else if (sRegisters[0].type == VAR_NUM) result = ToNumber(&sStack[sStackIndex]) == sRegisters[0].value.n;
                        else result = 0;
                        break;
                    case VAR_LBL:
                        result = sRegisters[0].type == VAR_LBL && sStack[sStackIndex].value.l == sRegisters[0].value.l;
                        break;
                    case VAR_TBL:
                        result = sRegisters[0].type == VAR_TBL && sStack[sStackIndex].value.t == sRegisters[0].value.t;
                        break;
                }
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0].type = VAR_NUM;
                sRegisters[0].value.n = result;
                if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                sStack[sStackIndex].type = VAR_UNSET;
                break;
            case BC_SPLESS:
                sStackIndex--;
                switch (sStack[sStackIndex].type) {
                    case VAR_UNSET:
                        result = 0;
                        break;
                    case VAR_NUM:
                        if (sRegisters[0].type == VAR_NUM) result = sStack[sStackIndex].value.n < sRegisters[0].value.n;
                        else if (sRegisters[0].type == VAR_STR) result = sStack[sStackIndex].value.n < ToNumber(&sRegisters[0]);
                        else result = 0;
                        break;
                    case VAR_STR:
                        if (sRegisters[0].type == VAR_STR) result = strcmp(sStack[sStackIndex].value.s, sRegisters[0].value.s) < 0;
                        else if (sRegisters[0].type == VAR_NUM) result = ToNumber(&sStack[sStackIndex]) < sRegisters[0].value.n;
                        else result = 0;
                        break;
                    case VAR_LBL:
                        result = 0;
                        break;
                    case VAR_TBL:
                        result = 0;
                        break;
                }
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0].type = VAR_NUM;
                sRegisters[0].value.n = result;
                if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                sStack[sStackIndex].type = VAR_UNSET;
                break;
            case BC_SPGRE:
                sStackIndex--;
                switch (sStack[sStackIndex].type) {
                    case VAR_UNSET:
                        result = 0;
                        break;
                    case VAR_NUM:
                        if (sRegisters[0].type == VAR_NUM) result = sStack[sStackIndex].value.n > sRegisters[0].value.n;
                        else if (sRegisters[0].type == VAR_STR) result = sStack[sStackIndex].value.n > ToNumber(&sRegisters[0]);
                        else result = 0;
                        break;
                    case VAR_STR:
                        if (sRegisters[0].type == VAR_STR) result = strcmp(sStack[sStackIndex].value.s, sRegisters[0].value.s) > 0;
                        else if (sRegisters[0].type == VAR_NUM) result = ToNumber(&sStack[sStackIndex]) > sRegisters[0].value.n;
                        else result = 0;
                        break;
                    case VAR_LBL:
                        result = 0;
                        break;
                    case VAR_TBL:
                        result = 0;
                        break;
                }
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0].type = VAR_NUM;
                sRegisters[0].value.n = result;
                if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                sStack[sStackIndex].type = VAR_UNSET;
                break;
            case BC_SPLEQL:
                sStackIndex--;
                switch (sStack[sStackIndex].type) {
                    case VAR_UNSET:
                        result = 0;
                        break;
                    case VAR_NUM:
                        if (sRegisters[0].type == VAR_NUM) result = sStack[sStackIndex].value.n <= sRegisters[0].value.n;
                        else if (sRegisters[0].type == VAR_STR) result = sStack[sStackIndex].value.n <= ToNumber(&sRegisters[0]);
                        else result = 0;
                        break;
                    case VAR_STR:
                        if (sRegisters[0].type == VAR_STR) result = strcmp(sStack[sStackIndex].value.s, sRegisters[0].value.s) <= 0;
                        else if (sRegisters[0].type == VAR_NUM) result = ToNumber(&sStack[sStackIndex]) <= sRegisters[0].value.n;
                        else result = 0;
                        break;
                    case VAR_LBL:
                        result = 0;
                        break;
                    case VAR_TBL:
                        result = 0;
                        break;
                }
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0].type = VAR_NUM;
                sRegisters[0].value.n = result;
                if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                sStack[sStackIndex].type = VAR_UNSET;
                break;
            case BC_SPGEQL:
                sStackIndex--;
                switch (sStack[sStackIndex].type) {
                    case VAR_UNSET:
                        result = 0;
                        break;
                    case VAR_NUM:
                        if (sRegisters[0].type == VAR_NUM) result = sStack[sStackIndex].value.n >= sRegisters[0].value.n;
                        else if (sRegisters[0].type == VAR_STR) result = sStack[sStackIndex].value.n >= ToNumber(&sRegisters[0]);
                        else result = 0;
                        break;
                    case VAR_STR:
                        if (sRegisters[0].type == VAR_STR) result = strcmp(sStack[sStackIndex].value.s, sRegisters[0].value.s) >= 0;
                        else if (sRegisters[0].type == VAR_NUM) result = ToNumber(&sStack[sStackIndex]) >= sRegisters[0].value.n;
                        else result = 0;
                        break;
                    case VAR_LBL:
                        result = 0;
                        break;
                    case VAR_TBL:
                        result = 0;
                        break;
                }
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0].type = VAR_NUM;
                sRegisters[0].value.n = result;
                if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                sStack[sStackIndex].type = VAR_UNSET;
                break;
            case BC_SPNEQL:
                sStackIndex--;
                switch (sStack[sStackIndex].type) {
                    case VAR_UNSET:
                        result = sRegisters[0].type != VAR_UNSET;
                        break;
                    case VAR_NUM:
                        if (sRegisters[0].type == VAR_NUM) result = sStack[sStackIndex].value.n != sRegisters[0].value.n;
                        else if (sRegisters[0].type == VAR_STR) result = sStack[sStackIndex].value.n != ToNumber(&sRegisters[0]);
                        else result = 1;
                        break;
                    case VAR_STR:
                        if (sRegisters[0].type == VAR_STR) result = strcmp(sStack[sStackIndex].value.s, sRegisters[0].value.s) != 0;
                        else if (sRegisters[0].type == VAR_NUM) result = ToNumber(&sStack[sStackIndex]) != sRegisters[0].value.n;
                        else result = 1;
                        break;
                    case VAR_LBL:
                        result = !(sRegisters[0].type == VAR_LBL && sStack[sStackIndex].value.l == sRegisters[0].value.l);
                        break;
                    case VAR_TBL:
                        result = !(sRegisters[0].type == VAR_TBL && sStack[sStackIndex].value.t == sRegisters[0].value.t);
                        break;
                }
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0].type = VAR_NUM;
                sRegisters[0].value.n = result;
                if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                sStack[sStackIndex].type = VAR_UNSET;
                break;

            case BC_ADD_R_R: /* Lparam + rparam, result in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n += sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            case VAR_STR:
                                swap = ToNewString(&sRegisters[sInstruction->lparam.i], 8);
                                sRegisters[sInstruction->lparam.i].value.s = StringConcat(swap.value.s, sRegisters[sInstruction->rparam.i].value.s);
                                sRegisters[sInstruction->lparam.i].type = VAR_STR;
                                free(swap.value.s);
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n += swap.value.n;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_STR:
                                sRegisters[sInstruction->lparam.i].value.s = StringConcat(str, sRegisters[sInstruction->rparam.i].value.s);
                                break;
                            default:
                                swap = ToNewString(&sRegisters[sInstruction->rparam.i], 8);
                                sRegisters[sInstruction->lparam.i].value.s = StringConcat(str, swap.value.s);
                                free(swap.value.s);
                                break;
                        }
                        free(str);
                        break;
                    default:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n += sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            case VAR_STR:
                                swap = ToNewString(&sRegisters[sInstruction->lparam.i], 8);
                                sRegisters[sInstruction->lparam.i].value.s = StringConcat(swap.value.s, sRegisters[sInstruction->rparam.i].value.s);
                                sRegisters[sInstruction->lparam.i].type = VAR_STR;
                                free(swap.value.s);
                                break;
                            default:
                                sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n += swap.value.n;
                                break;
                        }
                        break;
                }
                break;
            case BC_SUB_R_R: /* Lparam - rparam, result in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n -= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n -= swap.value.n;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n -= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n -= swap.value.n;
                                break;
                        }
                        free(str);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n -= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n -= swap.value.n;
                                break;
                        }
                        break;
                }
                break;
            case BC_MUL_R_R: /* Lparam * rparam, result in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n *= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n *= swap.value.n;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n *= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n *= swap.value.n;
                                break;
                        }
                        free(str);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n *= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n *= swap.value.n;
                                break;
                        }
                        break;
                }
                break;
            case BC_DIV_R_R: /* Lparam / rparam, result in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n /= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n /= swap.value.n;
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n /= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n /= swap.value.n;
                                break;
                        }
                        free(str);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n /= sRegisters[sInstruction->rparam.i].value.n;
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n /= swap.value.n;
                                break;
                        }
                        break;
                }
                break;
            case BC_MOD_R_R: /* Lparam % rparam, result in lparam. */
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = Mod(sRegisters[sInstruction->lparam.i].value.n, sRegisters[sInstruction->rparam.i].value.n);
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = Mod(sRegisters[sInstruction->lparam.i].value.n, swap.value.n);
                                break;
                        }
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = Mod(sRegisters[sInstruction->lparam.i].value.n, sRegisters[sInstruction->rparam.i].value.n);
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = Mod(sRegisters[sInstruction->lparam.i].value.n, swap.value.n);
                                break;
                        }
                        free(str);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        switch (sRegisters[sInstruction->rparam.i].type) {
                            case VAR_NUM:
                                sRegisters[sInstruction->lparam.i].value.n = Mod(sRegisters[sInstruction->lparam.i].value.n, sRegisters[sInstruction->rparam.i].value.n);
                                break;
                            default:
                                swap = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                                sRegisters[sInstruction->lparam.i].value.n = Mod(sRegisters[sInstruction->lparam.i].value.n, swap.value.n);
                                break;
                        }
                        break;
                }
                break;
                
            case BC_SPADD:
                /* reg 0 = stack + reg0. */
                sStackIndex--;
                if (sRegisters[0].type == VAR_STR || sStack[sStackIndex].type == VAR_STR) {
                    ToString(&sRegisters[0], 8);
                    ToString(&sStack[sStackIndex], 8);
                    str = sRegisters[0].value.s;
                    sRegisters[0].value.s = StringConcat(sStack[sStackIndex].value.s, str);
                    free(str);
                    free(sStack[sStackIndex].value.s);
                    sStack[sStackIndex].type = VAR_UNSET;
                }
                else {
                    if (sRegisters[0].type != VAR_NUM) ToNumber(&sRegisters[0]);
                    if (sStack[sStackIndex].type != VAR_NUM) ToNumber(&sStack[sStackIndex]);
                    sRegisters[0].value.n = sStack[sStackIndex].value.n + sRegisters[0].value.n;
                }
                break;
            case BC_SPSUB:
                sStackIndex--;
                if (sRegisters[0].type != VAR_NUM) ToNumber(&sRegisters[0]);
                if (sStack[sStackIndex].type != VAR_NUM) ToNumber(&sStack[sStackIndex]);
                sRegisters[0].value.n = sStack[sStackIndex].value.n - sRegisters[0].value.n;
                break;
            case BC_SPMUL:
                sStackIndex--;
                if (sRegisters[0].type != VAR_NUM) ToNumber(&sRegisters[0]);
                if (sStack[sStackIndex].type != VAR_NUM) ToNumber(&sStack[sStackIndex]);
                sRegisters[0].value.n = sStack[sStackIndex].value.n*sRegisters[0].value.n;
                break;
            case BC_SPDIV:
                sStackIndex--;
                if (sRegisters[0].type != VAR_NUM) ToNumber(&sRegisters[0]);
                if (sStack[sStackIndex].type != VAR_NUM) ToNumber(&sStack[sStackIndex]);
                sRegisters[0].value.n = sStack[sStackIndex].value.n/sRegisters[0].value.n;
                break;
            case BC_SPMOD:
                sStackIndex--;
                if (sRegisters[0].type != VAR_NUM) ToNumber(&sRegisters[0]);
                if (sStack[sStackIndex].type != VAR_NUM) ToNumber(&sStack[sStackIndex]);
                sRegisters[0].value.n = Mod(sStack[sStackIndex].value.n, sRegisters[0].value.n);
                break;

            case BC_NEG_R: /* Lparam = -lparam. */
                sRegisters[sInstruction->lparam.i].value.n = -ToNumber(&sRegisters[sInstruction->lparam.i]);
                break;
            case BC_CTBL_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i].value.t = NewHashTable(1);
                sRegisters[sInstruction->lparam.i].type = VAR_TBL;
                break;
                
            case BC_STR_R_R:
                if (sInstruction->lparam.i == sInstruction->rparam.i) {
                    if (sRegisters[sInstruction->lparam.i].type != VAR_STR) {
                        sRegisters[sInstruction->lparam.i] = ToNewString(&sRegisters[sInstruction->lparam.i], 8);
                    }
                }
                else {
                    if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                    sRegisters[sInstruction->lparam.i] = ToNewString(&sRegisters[sInstruction->rparam.i], 8);
                }
                break;
            case BC_STR_R:
                ToString(&sRegisters[sInstruction->lparam.i], 8);
                break;
            case BC_NUM_R_R:
                if (sInstruction->lparam.i == sInstruction->rparam.i) {
                    if (sRegisters[sInstruction->lparam.i].type == VAR_STR) {
                        char *str = sRegisters[sInstruction->lparam.i].value.s;
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        free(str);
                    }
                    else {
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                    }
                }
                else {
                    if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                    sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                }
                break;
            case BC_NUM_R:
                ToNumber(&sRegisters[sInstruction->lparam.i]);
                break;
            case BC_INT_R_R:
                if (sInstruction->lparam.i == sInstruction->rparam.i) {
                    if (sRegisters[sInstruction->lparam.i].type == VAR_STR) {
                        char *str = sRegisters[sInstruction->lparam.i].value.s;
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                        free(str);
                    }
                    else {
                        sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->lparam.i]);
                    }
                }
                else {
                    if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                    sRegisters[sInstruction->lparam.i] = ToNewNumber(&sRegisters[sInstruction->rparam.i]);
                }
                sRegisters[sInstruction->lparam.i].value.n = trunc(sRegisters[sInstruction->lparam.i].value.n);
                break;
            case BC_INT_R:
                sRegisters[sInstruction->lparam.i].value.n = trunc(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_SIZE_R_R:
                str = sRegisters[sInstruction->lparam.i].type == VAR_STR ? sRegisters[sInstruction->lparam.i].value.s : 0;
                switch (sRegisters[sInstruction->rparam.i].type) {
                    case VAR_STR:
                    case VAR_NUM:
                        sRegisters[sInstruction->lparam.i].value.n = 1;
                        break;
                    case VAR_LBL:
                        sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->rparam.i].value.l >= 0;
                    case VAR_TBL:
                        sRegisters[sInstruction->lparam.i].value.n = HT_EntryCount(sRegisters[sInstruction->rparam.i].value.t);
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i].value.n = 0;
                }
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                if (str) free(str);
                break;
            case BC_LEN_R_R:
                if (sRegisters[sInstruction->rparam.i].type == VAR_STR) {
                    str = sRegisters[sInstruction->lparam.i].type == VAR_STR ? sRegisters[sInstruction->lparam.i].value.s : 0;
                    sRegisters[sInstruction->lparam.i].value.n = strlen(sRegisters[sInstruction->rparam.i].value.s); 
                    sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                    if (str) free(str);
                }
                else {
                    swap = ToNewString(&sRegisters[sInstruction->rparam.i], 8);
                    str = sRegisters[sInstruction->lparam.i].type == VAR_STR ? sRegisters[sInstruction->lparam.i].value.s : 0;
                    sRegisters[sInstruction->lparam.i].value.n = strlen(swap.value.s); 
                    sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                    free(swap.value.s);
                    if (str) free(str);                    
                }
                break;
            
                
            case BC_NOT_R:
                switch (sRegisters[sInstruction->lparam.i].type) {
                    case VAR_NUM:
                        sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.n == 0.0;
                        break;
                    case VAR_STR:
                        str = sRegisters[sInstruction->lparam.i].value.s;
                        sRegisters[sInstruction->lparam.i].value.n = str[0] == '\0';
                        free(str);
                        break;
                    case VAR_LBL:
                        sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->lparam.i].value.l < 0;
                        break;
                    case VAR_TBL:
                        sRegisters[sInstruction->lparam.i].value.n = HT_EntryCount(sRegisters[sInstruction->lparam.i].value.t) == 0;
                        break;
                    default:
                        sRegisters[sInstruction->lparam.i].value.n = 1;        
                }
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                break;
                
            case BC_MDEL_S:
                if (sMemory->type == VAR_TBL) {
                    if (sMemory->value.t->lock) {
                        RuntimeError("Table is locked (BC_MDEL_S)");
                    }
                    else {
                        HT_Delete(sMemory->value.t, sStrings[sInstruction->lparam.i], 0, DeleteVariable);
                    }
                }
                else {
                    RuntimeError("Variable is not a table (BC_MDEL_S)");
                }
                break;
            case BC_MDEL_N:
                if (sMemory->type == VAR_TBL) {
                    if (sMemory->value.t->lock) {
                        RuntimeError("Table is locked (BC_MDEL_N)");
                    }
                    else {
                        HT_Delete(sMemory->value.t, 0, sInstruction->lparam.i, DeleteVariable);
                    }
                }
                else {
                    RuntimeError("Variable is not a table (BC_MDEL_N)");
                }
                break;
            case BC_MDEL_R:
                    if (sMemory->type == VAR_TBL) {
                        if (sMemory->value.t->lock) {
                            RuntimeError("Table is locked (BC_MDEL_R)");
                        }
                        else {
                            if (sRegisters[sInstruction->lparam.i].type == VAR_STR) {
                                HT_Delete(sMemory->value.t, sRegisters[sInstruction->lparam.i].value.s, 0, DeleteVariable); 
                            }
                            else if (sRegisters[sInstruction->lparam.i].type == VAR_NUM) {
                                HT_Delete(sMemory->value.t, 0, (int)sRegisters[sInstruction->lparam.i].value.n, DeleteVariable);
                            }
                            else {
                                RuntimeError("Register contains no identifier or index (BC_MDEL_R)");
                            }
                        }
                    }
                else {
                    RuntimeError("Variable is not a table (BC_MDEL_R)");
                }
                break;
                
            case BC_LGC:
                //LockGC();
                break;
            case BC_ULGC:
                //UnlockGC();
                break;
            case BC_GC:
                MM_GarbageCollect();
                break;
                
            case BC_CPY_R_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                CopyVariable(&sRegisters[sInstruction->lparam.i], &sRegisters[sInstruction->rparam.i]);
                break;
                
            case BC_ASSERT_R_R:
                if (!ValueTrue(&sRegisters[sInstruction->lparam.i])) {
                    RuntimeError(ToString(&sRegisters[sInstruction->rparam.i], 8));
                }
                break;
            
            case BC_RTE_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) {
                    RuntimeError(sRegisters[sInstruction->lparam.i].value.s);
                }
                else {
                    RuntimeError("Invalid operation (BC_RTE_R)"); 
                }
                break;
            
            case BC_CALL_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_LBL) {
                    sCallStack[sCallStackIndex].instructionIndex = sInstruction - sInstructions;
                    sCallStack[sCallStackIndex].local.type = VAR_TBL;
                    /* Look at BC_RET, when returning we store the capacity of the table. */
                    sCallStack[sCallStackIndex].local.value.t = HT_Create(sInstruction->rparam.i);
                    sCallStackIndex++;
                    sInstruction = &sInstructions[sRegisters[sInstruction->lparam.i].value.l - 1];
                }
                else {
                    RuntimeError("Register is not a label (BC_CALL_R)");
                }
                break;
            case BC_RET:
#ifndef RELEASE_MODE
                if (sCallStackIndex > 0) {
#endif
                    sCallStackIndex--;
                    /* Save capacity for next call table initialisation. Weird? YES! */
                    sInstructions[sCallStack[sCallStackIndex].instructionIndex].rparam.i = sCallStack[sCallStackIndex].local.value.t->capacity; 
                    /* Delete local. */
                    HT_Free(sCallStack[sCallStackIndex].local.value.t, DeleteVariable);
                    sCallStack[sCallStackIndex].local.value.t = 0;
                    sCallStack[sCallStackIndex].local.type = VAR_UNSET;
                    sInstruction = &sInstructions[sCallStack[sCallStackIndex].instructionIndex];
#ifndef RELEASE_MODE
                }
                else {
                    RuntimeError("Call stack is empty (BC_RET)");
                }
#endif
                break;
            case BC_LOCAL:
                sMemoryParent = *sMemory;
                sMemory = &sCallStack[sCallStackIndex - 1].local;
                break;
            case BC_OPT_PVAL:
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                sRegisters[0] = sStack[--sStackIndex];
#ifndef RELEASE_MODE
                if (sRegisters[0].type == VAR_NUM) {
#endif
                    if ((int)sRegisters[0].value.n != sInstruction->lparam.i) {
                        FunctionCallError(sStrings[sInstruction->rparam.i], sInstruction->lparam.i, (int)sRegisters[0].value.n);
                    }
#ifndef RELEASE_MODE
                }
                else {
                    RuntimeError("Register is not a number (BC_OPT_PVAL)");
                }
#endif
                break;
                
            /* Table iterators. */
            case BC_ILOAD:
#ifndef RELEASE_MODE
                if (sMemory->type == VAR_TBL) {
#endif
                    sIteratorStack[sIteratorStackIndex] = (TableIterator *)MM_Malloc(sizeof(TableIterator));
                    /* DUDE! This is a lazy hack, the wrapper flag should be
                       set from a register or constant instead of using the eval
                       flag! Please fix! */
                    sIteratorStack[sIteratorStackIndex]->isWrapper = !eval;
                    sIteratorStack[sIteratorStackIndex]->table = sMemory->value.t;
                    sIteratorStack[sIteratorStackIndex]->list = HT_GetEntriesArray(sMemory->value.t);
                    sIteratorStack[sIteratorStackIndex]->current = sIteratorStack[sIteratorStackIndex]->list;
                    sIteratorStack[sIteratorStackIndex]->table->lock++;
#ifndef RELEASE_MODE
                }
                else {
                    RuntimeError("Variable is not a table (BC_ILOAD)");
                }
#endif
                break;
            case BC_IHAS:
                eval = *sIteratorStack[sIteratorStackIndex]->current != 0;
                break;
            case BC_IVAL_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i] = *(Variable *)((*sIteratorStack[sIteratorStackIndex]->current)->data);
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) sRegisters[sInstruction->lparam.i].value.s = strdup(sRegisters[sInstruction->lparam.i].value.s);
                break;
            case BC_IKEY_R:
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                if (sIteratorStack[sIteratorStackIndex]->isWrapper) {
                    sRegisters[sInstruction->lparam.i].type = VAR_UNSET;
                }
                else {
                    if ((*sIteratorStack[sIteratorStackIndex]->current)->skey) {
                        sRegisters[sInstruction->lparam.i].value.s = strdup((*sIteratorStack[sIteratorStackIndex]->current)->skey);
                        sRegisters[sInstruction->lparam.i].type = VAR_STR;
                    }
                    else {
                        sRegisters[sInstruction->lparam.i].value.n = (*sIteratorStack[sIteratorStackIndex]->current)->ikey;
                        sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                    }
                }
                break;
            case BC_IPUSH:
#ifndef RELEASE_MODE
                if (sIteratorStackIndex >= ITERATOR_STACK_SIZE) {
                    RuntimeError("Iterator stack limit reached (BC_IPUSH)");
                    break;
                }
#endif
                sIteratorStackIndex++;
                break;
            case BC_IPOP:
#ifndef RELEASE_MODE
                if (sIteratorStackIndex <= 0) {
                    RuntimeError("Iterator stack is empty  (BC_IPOP)");
                    break;
                }
#endif
                sIteratorStackIndex--;
                break;
            case BC_ISTEP:
                sIteratorStack[sIteratorStackIndex]->current++;
                break;
            case BC_IDEL:
                if (sIteratorStack[sIteratorStackIndex]) {
                    sIteratorStack[sIteratorStackIndex]->table->lock--;
                    MM_Free(sIteratorStack[sIteratorStackIndex]->list);
                    MM_Free(sIteratorStack[sIteratorStackIndex]);
                    sIteratorStack[sIteratorStackIndex] = 0;
                }
                break;
                
            case BC_ABS_R:
                sRegisters[sInstruction->lparam.i].value.n = fabs(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_COS_R:
                sRegisters[sInstruction->lparam.i].value.n = cos(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_SIN_R:
                sRegisters[sInstruction->lparam.i].value.n = sin(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_TAN_R:
                sRegisters[sInstruction->lparam.i].value.n = tan(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_ACOS_R:
                sRegisters[sInstruction->lparam.i].value.n = acos(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_ASIN_R:
                sRegisters[sInstruction->lparam.i].value.n = asin(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_ATAN_R:
                sRegisters[sInstruction->lparam.i].value.n = atan(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_ATAN2_R_R:
                sRegisters[sInstruction->lparam.i].value.n = atan2(ToNumber(&sRegisters[sInstruction->lparam.i]), ToNumber(&sRegisters[sInstruction->rparam.i]));
                break;
            case BC_LOG_R:
                sRegisters[sInstruction->lparam.i].value.n = log(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_SGN_R:
                sRegisters[sInstruction->lparam.i].value.n = ToNumber(&sRegisters[sInstruction->lparam.i]);
                if (sRegisters[sInstruction->lparam.i].value.n < 0) sRegisters[sInstruction->lparam.i].value.n = -1;
                else if (sRegisters[sInstruction->lparam.i].value.n > 0) sRegisters[sInstruction->lparam.i].value.n = 1;
                else sRegisters[sInstruction->lparam.i].value.n = 0;
                break;
            case BC_SQR_R:
                sRegisters[sInstruction->lparam.i].value.n = sqrt(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_POW_R_R:
                sRegisters[sInstruction->lparam.i].value.n = pow(ToNumber(&sRegisters[sInstruction->lparam.i]), ToNumber(&sRegisters[sInstruction->rparam.i]));
                break;
            case BC_FLOOR_R:
                sRegisters[sInstruction->lparam.i].value.n = floor(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_CEIL_R:
                sRegisters[sInstruction->lparam.i].value.n = ceil(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_ROUND_R:
                sRegisters[sInstruction->lparam.i].value.n = round(ToNumber(&sRegisters[sInstruction->lparam.i]));
                break;
            case BC_RAD_R:
                sRegisters[sInstruction->lparam.i].value.n = ToNumber(&sRegisters[sInstruction->lparam.i])*3.141592653589/180.0;
                break;
            case BC_DEG_R:
                sRegisters[sInstruction->lparam.i].value.n = ToNumber(&sRegisters[sInstruction->lparam.i])*180.0/3.141592653589;
                break;
            case BC_MIN_R_R:
                ToNumber(&sRegisters[sInstruction->lparam.i]);
                ToNumber(&sRegisters[sInstruction->rparam.i]);
                if (sRegisters[sInstruction->rparam.i].value.n < sRegisters[sInstruction->lparam.i].value.n) sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->rparam.i].value.n;
                break;
            case BC_MAX_R_R:
                ToNumber(&sRegisters[sInstruction->lparam.i]);
                ToNumber(&sRegisters[sInstruction->rparam.i]);
                if (sRegisters[sInstruction->rparam.i].value.n > sRegisters[sInstruction->lparam.i].value.n) sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->rparam.i].value.n;
                break;

            case BC_TYPE_R_R:
                str = sRegisters[sInstruction->lparam.i].type == VAR_STR ? sRegisters[sInstruction->lparam.i].value.s : 0;
                sRegisters[sInstruction->lparam.i].value.n = sRegisters[sInstruction->rparam.i].type;
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                if (str) free(str);
                break;
                
            case BC_SYS_N_N:
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                /* Call the system command function. The stack should already be
                   populated with arguments. */
                sRegisters[0] = sSystemCommandFunctions[sInstruction->lparam.i](sInstruction->rparam.i, &sStack[sStackIndex - sInstruction->rparam.i]);
                /* Clean up. */
                for (int i = 0; i < sInstruction->rparam.i; i++) {
                    sStackIndex--;
                    if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                    sStack[sStackIndex].type = VAR_UNSET;
                }
                break;
                
            case BC_FLOAD_R:
                str = ToString(&sRegisters[sInstruction->lparam.i], 0);
                sRegisters[sInstruction->lparam.i].type = VAR_NUM;
                sRegisters[sInstruction->lparam.i].value.n = GetN7CFunctionIndex(str);
                if (sRegisters[sInstruction->lparam.i].value.n < 0) sRegisters[sInstruction->lparam.i].type = VAR_UNSET;
                if (str) free(str);
                break;
            
            case BC_FCALL_N:
#ifndef RELEASE_MODE
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                if ((n7c = GetN7CFunction((int)ToNumber(&sStack[sStackIndex - sInstruction->lparam.i])))) {
                    sRegisters[0] = n7c(sInstruction->lparam.i - 1, &sStack[sStackIndex - sInstruction->lparam.i + 1]);
                    /* Clean up. */
                    for (int i = 0; i < sInstruction->lparam.i; i++) {
                        sStackIndex--;
                        if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                        sStack[sStackIndex].type = VAR_UNSET;
                    }
                }
                else {
                    RuntimeErrorS("External function %s not found (BC_FCALL_N)", ToString(&sStack[sStackIndex - sInstruction->lparam.i], 0));
                }
#else
                if (sRegisters[0].type == VAR_STR) free(sRegisters[0].value.s);
                n7c = GetN7CFunction((int)ToNumber(&sStack[sStackIndex - sInstruction->lparam.i]));
                sRegisters[0] = n7c(sInstruction->lparam.i - 1, &sStack[sStackIndex - sInstruction->lparam.i + 1]);
                for (int i = 0; i < sInstruction->lparam.i; i++) {
                    sStackIndex--;
                    if (sStack[sStackIndex].type == VAR_STR) free(sStack[sStackIndex].value.s);
                    sStack[sStackIndex].type = VAR_UNSET;
                }
#endif
                break;
            
            case BC_OPT_LOADSINGLEVAR_R_S:
#ifndef RELEASE_MODE
                if (sMemory->type == VAR_TBL) {
                    if ((swapRef = (Variable *)HT_GetPH(sMemory->value.t, sStringHashes[sInstruction->rparam.i], sStrings[sInstruction->rparam.i], 0))) {
                        if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                        sRegisters[sInstruction->lparam.i] = *swapRef;
                        if (swapRef->type == VAR_STR) sRegisters[sInstruction->lparam.i].value.s = strdup(swapRef->value.s);
                        sMemoryParent = *swapRef;
                    }
                    else {
                        RuntimeErrorS("Identifier '%s' not found (BC_OPT_LOADSINGLEVAR_R_S)", sStrings[sInstruction->rparam.i]);
                    }
                }
                else {
                    RuntimeErrorS("Can't load identifier '%s', parent is not a table (BC_OPT_LOADSINGLEVAR_R_S)", sStrings[sInstruction->rparam.i]);
                }
#else
                swapRef = (Variable *)HT_GetPH(sMemory->value.t, sStringHashes[sInstruction->rparam.i], sStrings[sInstruction->rparam.i], 0);
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i] = *swapRef;
                if (swapRef->type == VAR_STR) sRegisters[sInstruction->lparam.i].value.s = strdup(swapRef->value.s);
                sMemoryParent = *swapRef;
#endif
                break;
            case BC_OPT_LOADSINGLEVARG_R_S:
#ifndef RELEASE_MODE
                if ((swapRef = (Variable *)HT_GetPH(sProgramMemory->value.t, sStringHashes[sInstruction->rparam.i], sStrings[sInstruction->rparam.i], 0))) {
                    if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                    sRegisters[sInstruction->lparam.i] = *swapRef;
                    if (swapRef->type == VAR_STR) sRegisters[sInstruction->lparam.i].value.s = strdup(swapRef->value.s);
                    sMemoryParent = *swapRef;
                }
                else {
                    RuntimeErrorS("Identifier '%s' not found (BC_OPT_LOADSINGLEVARG_R_S)", sStrings[sInstruction->rparam.i]);
                }
#else
                swapRef = (Variable *)HT_GetPH(sProgramMemory->value.t, sStringHashes[sInstruction->rparam.i], sStrings[sInstruction->rparam.i], 0);
                if (sRegisters[sInstruction->lparam.i].type == VAR_STR) free(sRegisters[sInstruction->lparam.i].value.s);
                sRegisters[sInstruction->lparam.i] = *swapRef;
                if (swapRef->type == VAR_STR) sRegisters[sInstruction->lparam.i].value.s = strdup(swapRef->value.s);
                sMemoryParent = *swapRef;
#endif
                break;
            case BC_OPT_LOADPARAM_S:
#ifndef RELEASE_MODE
                if (sMemory->type != VAR_TBL) {
                    RuntimeErrorS("Can't add identifier '%s', parent is not a table (BC_OPT_LOADPARAM_S)", sStrings[sInstruction->lparam.i]);
                }
                else {
#endif
                    he = HT_GetOrCreateEntryPH(sMemory->value.t, sStringHashes[sInstruction->lparam.i], sStrings[sInstruction->lparam.i], 0);
                    if (!(var = (Variable *)he->data)) {
                        he->data = var = (Variable *)MM_Malloc(sizeof(Variable));
                    }
                    else if (var->type == VAR_STR) free(var->value.s);
                    *var = sStack[--sStackIndex];
                    if (var->type == VAR_STR) {
                        sStack[sStackIndex].type = VAR_UNSET;
                        sStack[sStackIndex].value.s = 0;
                    }
#ifndef RELEASE_MODE
                }
#endif
                break;
        }
        sInstruction++;
    }

    /* Memory might be messed up if thrownError, so terminate without crashing. */
    if (!thrownError) {
        SYS_Release();
        
        if (strcmp(sError, "") == 0) {
            if (sDebugOutput)  {
                printf("\nrenv: Execution finished in %fs\n\n", (double)(TimeMS() - startTime)/1000.0);
                /*DumpMemory(sProgramMemory);
                DumpStack();
                DumpRegisters();*/
                printf("renv: Memory stack index:   %d\n", sMemoryStackIndex);
                printf("renv: Iterator stack index: %d\n", sIteratorStackIndex);
                printf("renv: Stack index:          %d\n", sStackIndex);
                printf("renv: Call stack index:     %d\n\n", sCallStackIndex);
            }
        }
        else {
            if (sDebugOutput) { 
                printf("\nrenv: Execution finished with error in %fs\n\n", (double)(TimeMS() - startTime)/1000.0);
                /*DumpMemory(sProgramMemory);
                DumpStack();
                DumpRegisters();*/
                printf("renv: Memory stack index:   %d\n", sMemoryStackIndex);
                printf("renv: Iterator stack index: %d\n", sIteratorStackIndex);
                printf("renv: Stack index:          %d\n", sStackIndex);
                printf("renv: Call stack index:     %d\n\n", sCallStackIndex);
            }
        }
      
        /* Clear strings from registers. */
        for (int i = 0; i < 10; i++) if (sRegisters[i].type == VAR_STR) free(sRegisters[i].value.s);

        /* Clear strings from stack. */
        for (int i = 0; i < sStackIndex; i++) if (sStack[i].type == VAR_STR) free(sStack[i].value.s);

        /* Free all tables, including program memory. */
        //GarbageCollect(0); /* Just for getting output about dead tables. */
        //FreeAllocatedTables();
        MM_GarbageCollect();

        /* He crashed when you did this BEFORE GarbageCollect becasue the memory
           stack could hold references to call stack tables. */
        for (int i = 0; i < sCallStackIndex; i++) {
            HT_Free(sCallStack[i].local.value.t, DeleteVariable);
        }
        sCallStackIndex = 0;

        sProgramMemory = 0;
    }

    return strcmp(sError, "") == 0 ? RENV_SUCCESS : RENV_FAILURE;
}

/*
 * ThrowError
 * ----------
 */
static void ThrowError(const char *msg) {
    RuntimeError(msg);
    longjmp(sErrorJmpBuf, 1);
}

/*
 * FunctionCallError
 * -----------------
 * Just don't wanna have this one in the instruction loop.
 */
void FunctionCallError(const char *name, int expectedArguments, int actualArguments) {
    char *msg = (char *)malloc(sizeof(char)*128);
    if (expectedArguments == 0) sprintf(msg, "%s expected no arguments but got %d", name, actualArguments);
    else if (expectedArguments == 1) sprintf(msg, "%s expected 1 argument but got %d", name, actualArguments);
    else sprintf(msg, "%s expected %d arguments but got %d", name, expectedArguments, actualArguments);
    RuntimeError(msg);
    free(msg);
}

/*
 * RENV_Error
 * ----------
 */
const char *RENV_Error() {
    return sError;
}

static char *GetFilename() {
    int instructionIndex = sInstruction - sInstructions;
    for (int i = sFilenameCount - 1; i >= 0; i--) {
        if (instructionIndex >= sFilenames[i].instructionIndex) return sFilenames[i].filename; 
    }
    return 0;
}

static int GetLineNumber() {
    int instructionIndex = sInstruction - sInstructions;
    for (int i = sLineNumberCount - 1; i >= 0; i--) {
        if (instructionIndex >= sLineNumbers[i].instructionIndex) return sLineNumbers[i].lineNumber; 
    }
    return 0;
}

/*
 * RuntimeError
 * ------------
 * Construct error message.
 */
void RuntimeError(const char *msg) {
    int lineNumber;
    char tmp[16];
    char *filename;
    char *prevFilename;
    
    filename = GetFilename();
    lineNumber = GetLineNumber();
    sprintf(sError, "%s:%d", filename, lineNumber);
    prevFilename = filename;
    for (int i = sCallStackIndex - 1; i >= 0 && sCallStackIndex - i < 10; i--) {
        sInstruction = &sInstructions[sCallStack[i].instructionIndex];
        filename = GetFilename();
        lineNumber = GetLineNumber();
        if (strcmp(filename, prevFilename)) {
            strcat(sError, ", ");
            strcat(sError, filename);
            strcat(sError, ":");
            sprintf(tmp, "%d", lineNumber);
            strcat(sError, tmp);
        }
        else {
            sprintf(tmp, ",%d", lineNumber);
            strcat(sError, tmp);
        }
        prevFilename = filename;
    }
    strcat(sError, ": runtime error: ");
    strcat(sError, msg);
    
    if (sWin32) {
        WIN_MessageBox("Runtime error", sError);
    }
    
    sRunning = 0;
}

/*
 * RuntimeErrorS
 * -------------
 * Construct error message with string parameter.
 */
void RuntimeErrorS(const char *msg, const char *param) {
    char tmp[512];
    sprintf(tmp, msg, param);
    RuntimeError(tmp);
}

/*
 * TerminateProgram
 * ----------------
 * Terminate program.
 */
void TerminateProgram() {
    sRunning = 0;
}


/*
 * ValueTrue
 * ---------
 * Returns 1 if the variable v should be concidered as true.
 */
static int ValueTrue(Variable *v) {
    switch (v->type) {
        case VAR_NUM:
            return v->value.n != 0;
        case VAR_STR:
            return v->value.s[0] != '\0';
        case VAR_LBL:
            return v->value.l >= 0;
        case VAR_TBL:
            return HT_EntryCount(v->value.t) > 0;
        default:
            return 0;
    }
}

/*
 * ToNewString
 * -----------
 * Convert variable to a new string.
 */
static char sToStrBuf[512];
Variable ToNewString(Variable *v, int maxDecimals) {
    Variable s;
    s.type = VAR_STR;
    
    if (v->type == VAR_STR) {
        s.value.s = strdup(v->value.s);
    }
    else if (v->type == VAR_NUM) {
        int result;
        if (maxDecimals < 0) maxDecimals = 0;
        else if (maxDecimals > 127) maxDecimals = 127;
        result = snprintf(sToStrBuf, 512, "%.*f", maxDecimals, v->value.n);
        if (result < 0 || result >= 512) s.value.s = strdup("Error");
        else {
            /* Trim trailing zeroes. */
            int last = strlen(sToStrBuf) - 1;
            while (sToStrBuf[last] == '0') sToStrBuf[last--] = '\0';
            /* Remove decimal point? */
            if (sToStrBuf[last] == '.') sToStrBuf[last] = '\0';
            s.value.s = strdup(sToStrBuf);
        }
    }
    else if (v->type == VAR_TBL) {
        int cc, mc = 0;
        cc = HT_CollisionCount(v->value.t, &mc);
        sprintf(sToStrBuf, "Table: %p, %d/%d, %d, %d", v->value.t, HT_EntryCount(v->value.t), v->value.t->capacity, cc, mc);
        s.value.s = strdup(sToStrBuf);
    }
    else if (v->type == VAR_LBL) {
        sprintf(sToStrBuf, "Address: %d", v->value.l);
        s.value.s = strdup(sToStrBuf);
    }
    else if (v->type == VAR_UNSET) {
        s.value.s = strdup("Unset");
    }
    return s;
}

/*
 * ToString
 * --------
 * Convert variable to string and returns its value.
 */
char *ToString(Variable *v, int maxDecimals) {
    if (v->type == VAR_STR) {
        return v->value.s;
    }
    else if (v->type == VAR_NUM) {
        int result;
        if (maxDecimals < 0) maxDecimals = 0;
        else if (maxDecimals > 127) maxDecimals = 127;
        result = snprintf(sToStrBuf, 512, "%.*f", maxDecimals, v->value.n);
        if (result < 0 || result >= 512) v->value.s = strdup("NaN");
        else {
            /* Trim trailing zeroes. */
            int last = strlen(sToStrBuf) - 1;
            while (sToStrBuf[last] == '0') sToStrBuf[last--] = '\0';
            /* Remove decimal point? */
            if (sToStrBuf[last] == '.') sToStrBuf[last] = '\0';
            v->value.s = strdup(sToStrBuf);
        }
    }
    else if (v->type == VAR_TBL) {
        int cc, mc = 0;
        cc = HT_CollisionCount(v->value.t, &mc);
        sprintf(sToStrBuf, "Table: %p, %d/%d, %d, %d", v->value.t, HT_EntryCount(v->value.t), v->value.t->capacity, cc, mc);
        v->value.s = strdup(sToStrBuf);
    }
    else if (v->type == VAR_LBL) {
        sprintf(sToStrBuf, "Address: %d", v->value.l);
        v->value.s = strdup(sToStrBuf);
    }
    else if (v->type == VAR_UNSET) {
        v->value.s = strdup("Unset");
    }
    v->type = VAR_STR;
    
    return v->value.s;
}

/*
 * ToNewNumber
 * -----------
 * Convert variable to a new number.
 */
Variable ToNewNumber(Variable *v) {
    Variable n;
    n.type = VAR_NUM;
    n.value.n = 0;
    switch (v->type) {
        case VAR_NUM:
            n.value.n = v->value.n;
            break;
        case VAR_STR:
            n.value.n = atof(v->value.s);
            break;
        default:
            n.value.n = 0;
    }
    return n;
}

/*
 * ToNumber
 * --------
 * Convert variable to number and return its value.
 */
double ToNumber(Variable *v) {
    if (v->type == VAR_NUM) {
        return v->value.n;
    }
    if (v->type == VAR_STR) {
        char *str = v->value.s;
        v->value.n = atof(str);
        free(str);
    }
    else {
        v->value.n = 0;
    }
    v->type = VAR_NUM;
    return v->value.n;
}

/*
 * CopyVariable
 * ------------
 * Make a deep copy of variable.
 */
typedef struct _VarTableCopy VarTableCopy;
struct _VarTableCopy {
    HashTable *org;
    HashTable *cpy;
    VarTableCopy *next;
};
typedef struct {
    HashTable *dst;
    VarTableCopy **copied;
} VarCopyInfo;


/*
 * CopyVariableRec
 * ---------------
 * CopyVariable recursive step.
 */
static void CopyVariableRec(char *skey, int ikey, void *data, void *userData) {
    Variable *v = (Variable *)data;
    VarCopyInfo *info = (VarCopyInfo *)userData;
    Variable *cpy = (Variable *)MM_Malloc(sizeof(Variable));

    *cpy = *v;
    HT_Add(info->dst, skey, ikey, cpy);
    if (v->type == VAR_TBL) {
        int skip = 0;
        if (info->copied) {
            VarTableCopy *vtc = *info->copied;
            while (vtc) {
                if (vtc->org == v->value.t) {
                    cpy->value.t = vtc->cpy;
                    skip = 1;
                    break;
                }
                vtc = vtc->next;
            }
        }
        if (!skip) {
            VarCopyInfo *newInfo;
            cpy->value.t = NewHashTable(1);            
            VarTableCopy *vtc = (VarTableCopy *)malloc(sizeof(VarTableCopy));
            vtc->org = v->value.t;
            vtc->cpy = cpy->value.t;
            vtc->next = 0;
            if (info->copied == 0) {
                info->copied = (VarTableCopy **)malloc(sizeof(VarTableCopy *));
                *info->copied = vtc;
            }
            else {
                vtc->next = *info->copied;
                *info->copied = vtc;
            }            
            newInfo = (VarCopyInfo *)malloc(sizeof(VarCopyInfo));
            newInfo->dst = cpy->value.t;
            newInfo->copied = info->copied;
            HT_ApplyKeyFunction(v->value.t, CopyVariableRec, newInfo);
            free(newInfo);
        }
    }
    else if (v->type == VAR_STR) {
        cpy->value.s = strdup(v->value.s); 
    }
}


static void CopyVariable(Variable *dst, Variable *v) {
    if (v->type == VAR_TBL) {
        VarCopyInfo *info;
        *dst = *v;
        dst->value.t = NewHashTable(1);
        info = (VarCopyInfo *)malloc(sizeof(VarCopyInfo));
        info->dst = dst->value.t;
        info->copied = 0;
        HT_ApplyKeyFunction(v->value.t, CopyVariableRec, info);
        if (info->copied) {
            VarTableCopy *vtc = *info->copied;
            while (vtc) {
                VarTableCopy *next = vtc->next;
                free(vtc);
                vtc = next;
            }
            free(info->copied);
        }
        free(info);
    }
    else if (v->type == VAR_STR) {
        *dst = *v;
        dst->value.s = strdup(v->value.s);
    }
    else {
        *dst = *v;
    }
}

static char *StringConcat(const char *a, const char *b) {
    char *dst = (char *)malloc(sizeof(char)*(strlen(a) + strlen(b) + 1));
    strcpy(dst, a);
    strcat(dst, b);
    return dst;
}

/*
 * DumpMemoryRec
 * -------------
 * Print memory content.
 */
static int mdumpLevel;
static void DumpMemoryRec(char *skey, int ikey, void *data, void *userData) {
    char indent[64];
    Variable *v = (Variable *)data;
    for (int i = 0; i < mdumpLevel; i++) indent[i] = ' ';
    indent[mdumpLevel] = '\0';
    
    if (skey) {
        if (v->type == VAR_UNSET) {
            printf("%s%s: UNSET\n", indent, skey);
        }
        else if (v->type == VAR_STR) {
            printf("%s%s: STR, \"%s\" (%p)\n", indent, skey, v->value.s, v->value.s);
        }
        else if (v->type == VAR_NUM) {
            printf("%s%s: NUM, %.8f\n", indent, skey, v->value.n);
        }
        else if (v->type == VAR_LBL) {
            printf("%s%s: LBL, %d\n", indent, skey, v->value.l);
        }
        else if (v->type == VAR_TBL) {
            printf("%s%s: TBL, %d entries (%p)\n", indent, skey, HT_EntryCount(v->value.t), v->value.t);
            mdumpLevel++;
            HT_ApplyKeyFunction(v->value.t, DumpMemoryRec, userData);
            mdumpLevel--;
        }
    }
    else {
        if (v->type == VAR_UNSET) {
            printf("%s%d: UNSET\n", indent, ikey);
        }
        else if (v->type == VAR_STR) {
            printf("%s%d: STR, \"%s\" (%p)\n", indent, ikey, v->value.s, v->value.s);
        }
        else if (v->type == VAR_NUM) {
            printf("%s%d: NUM, %.8f\n", indent, ikey, v->value.n);
        }
        else if (v->type == VAR_LBL) {
            printf("%s%d: LBL, %d\n", indent, ikey, v->value.l);
        }
        else if (v->type == VAR_TBL) {
            printf("%s%d: TBL, %d entries (%p)\n", indent, ikey, HT_EntryCount(v->value.t), v->value.t);
            mdumpLevel++;
            HT_ApplyKeyFunction(v->value.t, DumpMemoryRec, userData);
            mdumpLevel--;
        }
    }
}

/*
 * DumpMemory
 * ----------
 * Print memory content.
 */
static void DumpMemory(Variable *memory) {
    if (memory->type == VAR_TBL) {
        printf("renv: MEMORY\n");
        mdumpLevel = 2;
        HT_ApplyKeyFunction(memory->value.t, DumpMemoryRec, 0);
    }
}

/* 
 * DumpRegisters
 * -------------
 * Print register content.
 */
static void DumpRegisters() {
    printf("renv: REGISTERS\n");
    for (int i = 0; i < 10; i++) {
        if (sRegisters[i].type == VAR_UNSET) {
            printf("  %d: UNSET\n", i);
        }
        else if (sRegisters[i].type == VAR_STR) {
            printf("  %d: STR, \"%s\" (%p)\n", i, sRegisters[i].value.s, sRegisters[i].value.s);
        }
        else if (sRegisters[i].type == VAR_NUM) {
            printf("  %d: NUM, %.8f\n", i, sRegisters[i].value.n);
        }
        else if (sRegisters[i].type == VAR_LBL) {
            printf("  %d: LBL, %d\n", i, sRegisters[i].value.l);
        }
        else if (sRegisters[i].type == VAR_TBL) {
            printf("  %d: TBL, %d entries (%p)\n", i, HT_EntryCount(sRegisters[i].value.t), sRegisters[i].value.t);
        }
    }
}

/*
 * DumpStack
 * ---------
 * Print stack content.
 */
static void DumpStack() {
    printf("renv: STACK (%d)\n", sStackIndex);
    for (int i = sStackIndex - 1; i >= 0; i--) {
        if (sStack[i].type == VAR_UNSET) {
            printf("  %d: UNSET\n", i);
        }
        else if (sStack[i].type == VAR_STR) {
            printf("  %d: STR, \"%s\" (%p)\n", i, sStack[i].value.s, sStack[i].value.s);
        }
        else if (sStack[i].type == VAR_NUM) {
            printf("  %d: NUM, %.8f\n", i, sStack[i].value.n);
        }
        else if (sStack[i].type == VAR_LBL) {
            printf("  %d: LBL, %d\n", i, sStack[i].value.l);
        }
        else if (sStack[i].type == VAR_TBL) {
            printf("  %d: TBL, %d entries (%p)\n", i, HT_EntryCount(sStack[i].value.t), sStack[i].value.t);
        }
    }
}

int EqualVariables(Variable *a, Variable *b) {
    if (a->type == b->type) {
        switch (a->type) {
            case VAR_NUM: return a->value.n == b->value.n;
            case VAR_STR: return strcmp(a->value.s, b->value.s) == 0;
            case VAR_LBL: return a->value.l == b->value.l;
            case VAR_TBL: return a->value.t == b->value.t;
            default: return 1;
        }
    }
    else {
        return 0;
    }
}

void DeleteVariable(void *data) {
    Variable *v = (Variable *)data;
    if (v) {
        switch (v->type) {
            case VAR_STR:
                free(v->value.s);
                break;
        }
        MM_Free(v);
    }
}

/*
 * InitGC
 * ------
 * Init garbage collector.
 */
/*static void InitGC() {
    sAllocTable = (HashTable **)malloc(sizeof(HashTable *)*DEFAULT_ALLOC_TABLE_SIZE);
    sAllocTableCapacity = DEFAULT_ALLOC_TABLE_SIZE;
    sAllocTableIndex = 0;
    sGCLockIndex = 0;
    sAllocsSinceGC = 0;
    ResetGCPerformance();
}*/

/*
 * FreeAllocatedTables
 * -------------------
 * Free all allocated tables, including the variables in the tables.
 */
/*static void FreeAllocatedTables() {
    for (int i = 0; i < sAllocTableIndex; i++) {
        HT_Free(sAllocTable[i], DeleteVariable);
        sAllocTable[i] = 0;
    }
    free(sAllocTable);
    sAllocTable = 0;
    
    if (sDebugOutput) printf("gc: Disposed %d tables\n", sAllocTableIndex);
}*/

/*static void ResetGCPerformance() {
    sGCPerformanceCycleCount = 0;
    sGCPerformanceDeltaIndex = 0;
    sGCPerformanceTimer = TimeMS();
}

static int MeasureGCPerformance(int releaseCount) {
    unsigned long t = TimeMS();

    sGCPerformanceTimeDeltas[sGCPerformanceDeltaIndex] = t - sGCPerformanceTimer;
    sGCPerformanceReleaseCounts[sGCPerformanceDeltaIndex] = releaseCount;
    sGCPerformanceDeltaIndex = (sGCPerformanceDeltaIndex + 1)%GC_PERFORMANCE_CYCLES;
    sGCPerformanceTimer = t;

    if (sGCPerformanceCycleCount < GC_PERFORMANCE_CYCLES) {
        sGCPerformanceCycleCount++;
    }
    else {
        unsigned long avg = 0;
        for (int i = 0; i < GC_PERFORMANCE_CYCLES; i++) avg += sGCPerformanceTimeDeltas[i]/GC_PERFORMANCE_CYCLES;
        if (avg < GC_PERFORMANCE_MS_LOW) {
            return -1;
        }
    }
    return 0;
}*/

/*static void IncreaseGCCapacity() {
    int newCapacity = sAllocTableCapacity*2;
    if (sDebugOutput) printf("gc: Increasing capacity from %d to %d\n", sAllocTableCapacity, newCapacity);
    sAllocTableCapacity = newCapacity;
    sAllocTable = (HashTable **)realloc(sAllocTable, sizeof(HashTable *)*sAllocTableCapacity);
}*/

/*
 * GarbageCollect
 * --------------
 * Performs garbage collecting and optimizes the allocation table. Set increase
 * to 1 if the allocation table size should be increased in the case where no
 * garbage was found.
 */
/*static int GarbageCollect(int increase) {
    int newIndex = 0;

    if (!sAllocTable) return 0;

    // Don't collect if gc is locked, just increase if needed.
    if (sGCLockIndex) {
        if (increase) {
            IncreaseGCCapacity();
            ResetGCPerformance();
        }
        return 0;
    }
    
    sAllocsSinceGC = 0;
   
    // Mark all tables as dead.
    for (int i = 0; i < sAllocTableIndex; i++) {
        sAllocTable[i]->extra = 0;
    }
    
    // Visit all tables that can be reached from program memory.
    for (int i = 0; i < 10; i++) {
        if (sRegisters[i].type == VAR_TBL && sRegisters[i].value.t->extra == 0) {
            sRegisters[i].value.t->extra = 1;
            HT_ApplyDataFunction(sRegisters[i].value.t, MarkLiveTable, 0);
        }
    }
    // Local variabels. The function memory itself isn't subject for gc, but the
    //  local variables can "only" be reached this way.
    for (int i = 0; i < sCallStackIndex; i++) {
        sCallStack[i].local.value.t->extra = 1;
        HT_ApplyDataFunction(sCallStack[i].local.value.t, MarkLiveTable, 0);
    }
    for (int i = 0; i < sStackIndex; i++) {
        if (sStack[i].type == VAR_TBL && sStack[i].value.t->extra == 0) {
            sStack[i].value.t->extra = 1;
            HT_ApplyDataFunction(sStack[i].value.t, MarkLiveTable, 0);
        }
    }
    if (sProgramMemory->type == VAR_TBL) {
        if (sProgramMemory->value.t->extra == 0) {
            sProgramMemory->value.t->extra = 1;
            HT_ApplyDataFunction(sProgramMemory->value.t, MarkLiveTable, 0);
        }
    }
    for (int i = 0; i < sMemoryStackIndex; i++) {
        if (sMemoryStack[i]->type == VAR_TBL && sMemoryStack[i]->value.t->extra == 0) {
            sMemoryStack[i]->value.t->extra = 1;
            HT_ApplyDataFunction(sMemoryStack[i]->value.t, MarkLiveTable, 0);
        }
    }
    if (sMemory && sMemory->type == VAR_TBL) {
        if (sMemory->value.t->extra == 0) {
            sMemory->value.t->extra = 1;
            HT_ApplyDataFunction(sMemory->value.t, MarkLiveTable, 0);
        }
    }
    for (int i = 0; i <= sIteratorStackIndex; i++) {
        if (sIteratorStack[i] && sIteratorStack[i]->table->extra == 0) {
            sIteratorStack[i]->table->extra = 1;
            HT_ApplyDataFunction(sIteratorStack[i]->table, MarkLiveTable, 0);
        }
    }
    
    // Delete all dead tables and optimize list.
    for (int i = 0; i < sAllocTableIndex; i++) {
        if (sAllocTable[i]->extra) {
            sAllocTable[newIndex++] = sAllocTable[i];
        }
        else {
            HT_Free(sAllocTable[i], DeleteVariable);
            sAllocTable[i] = 0;
        }        
    }
    for (int i = newIndex; i < sAllocTableIndex; i++) sAllocTable[i] = 0;
        
    // No need to increase size of table.
    if (newIndex < sAllocTableIndex) {
        int count = sAllocTableIndex - newIndex;
        int gcPerfResult;
        
        if (sDebugOutput) printf("gc: Released %d tables\n", count);
        sAllocTableIndex = newIndex;

        return count;
    }
    // Increase table size.
    else if (increase) {
        IncreaseGCCapacity();
        ResetGCPerformance();
    }

    return 0;
}*/

/*
 * MarkLiveTables
 * --------------
 * Mark live tables recursively.
 */
static void MarkLiveTable(void *data, void *userData) {
    Variable *v = (Variable *)data;
    if (v->type == VAR_TBL && !MM_Alive(v->value.t)) {
        MM_MarkAlive(v->value.t);
        HT_ApplyDataFunction(v->value.t, MarkLiveTable, userData);
    }
}


static void MarkAndSweep() {
    // Visit all tables that can be reached from program memory.

    // Registers.
    for (int i = 0; i < 10; i++) {
        if (sRegisters[i].type == VAR_TBL && !MM_Alive(sRegisters[i].value.t)) {
            MM_MarkAlive(sRegisters[i].value.t);
            HT_ApplyDataFunction(sRegisters[i].value.t, MarkLiveTable, 0);
        }
    }
    // Local variabels. The function memory itself isn't subject for gc, but the
    // local variables can "only" be reached this way.
    for (int i = 0; i < sCallStackIndex; i++) {
        //sCallStack[i].local.value.t->extra = 1;
        // Ugh ... do we need to mark or check this one, not allocated as collectable.
        HT_ApplyDataFunction(sCallStack[i].local.value.t, MarkLiveTable, 0);
    }
    for (int i = 0; i < sStackIndex; i++) {
        if (sStack[i].type == VAR_TBL && !MM_Alive(sStack[i].value.t)) {
            MM_MarkAlive(sStack[i].value.t);
            HT_ApplyDataFunction(sStack[i].value.t, MarkLiveTable, 0);
        }
    }
    if (sProgramMemory->type == VAR_TBL) {
        if (!MM_Alive(sProgramMemory->value.t)) {
            MM_MarkAlive(sProgramMemory->value.t);
            HT_ApplyDataFunction(sProgramMemory->value.t, MarkLiveTable, 0);
        }
    }
    for (int i = 0; i < sMemoryStackIndex; i++) {
        if (sMemoryStack[i]->type == VAR_TBL && !MM_Alive(sMemoryStack[i]->value.t)) {
            MM_MarkAlive(sMemoryStack[i]->value.t);
            HT_ApplyDataFunction(sMemoryStack[i]->value.t, MarkLiveTable, 0);
        }
    }
    if (sMemory && sMemory->type == VAR_TBL) {
        if (!MM_Alive(sMemory->value.t)) {
            MM_MarkAlive(sMemory->value.t);
            HT_ApplyDataFunction(sMemory->value.t, MarkLiveTable, 0);
        }
    }
    for (int i = 0; i <= sIteratorStackIndex; i++) {
        if (sIteratorStack[i] && !MM_Alive(sIteratorStack[i]->table)) {
            MM_MarkAlive(sIteratorStack[i]->table);
            HT_ApplyDataFunction(sIteratorStack[i]->table, MarkLiveTable, 0);
        }
    }
}

/*
 * AllocTableFull
 * --------------
 * Return 1 if the allocation table is full.
 */
/*static int AllocTableFull() {
    return sAllocTableIndex >= sAllocTableCapacity;
}*/

/*
 * NewHashTable
 * ------------
 * Creates a new hash table and adds it to the allocation table, performs
 * garbage collecting and expands the allocation table if needed.
 */
HashTable *NewHashTable(int capacity) {
    HashTable *ht = HT_Create(capacity);
    MM_SetType(ht, 1);
    
    /*if (AllocTableFull()) {
        
        GarbageCollect(1);
    }
    
    if (AllocTableFull()) IncreaseGCCapacity();
 
    sAllocTable[sAllocTableIndex++] = ht;
    sAllocsSinceGC++;*/
     
    return ht;
}

/*
 * GC
 * --
 * Collect if "needed".
 */
void GC() {
    /* Don't ask, i'm just experimenting. I had a big trail of large dead
       arrays in a test. In combination with an oversized alloc table, the
       program consumed an extreme amount of memory. The solution was to
       clear the tables before they died in the n7 program. But ... that
       can't be expected. So this is just a temporary fix that may not even
       work and probably causes other problems. */
    /*if (sAllocsSinceGC && sAllocsSinceGC > sAllocTableCapacity/4 && TimeMS() - sLastGCTime > 2500) {
        GarbageCollect(0);
    }*/
}

/*
 * LockGC
 * ------
 * Lock garbage collecting.
 */
/*void LockGC() {
    if (sGCLockIndex++ == 0) sAllocTableCapacityBeforeLock = sAllocTableCapacity;
}*/

/*
 * UnlockGC
 * --------
 * Unlock garbage collecting.
 */
/*void UnlockGC() {
    if (--sGCLockIndex == 0 && sAllocTableCapacity > sAllocTableCapacityBeforeLock) {
        GarbageCollect(0);
    }
}*/   

/*
 * TimeMS
 * ------
 */
static unsigned long TimeMS() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000 + ts.tv_nsec/1000000;
}

/*
 * Mod
 * ---
 */
static double Mod(double x, double y) {
    return x - floor(x/y)*y;
}