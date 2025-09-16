/*
 * renv.h
 * ------
 * N7 runtime.
 *
 * By: Marcus 2021
 */

#ifndef __RENV_H__
#define __RENV_H__

#include "stdio.h"
#include "hash_table.h"

#define RENV_SUCCESS 0
#define RENV_FAILURE 1

#define VAR_UNSET   0
#define VAR_NUM     1
#define VAR_STR     2
#define VAR_LBL     3
#define VAR_TBL     4

/* Instruction parameter. */
typedef union {
    int i;
    double d;
} Parameter;

/* Instruction. */
typedef struct {
    unsigned short cmd;
    Parameter lparam;
    Parameter rparam;
} Instruction;

/* Filename metadata. */
typedef struct {
    int instructionIndex;
    char *filename;
} FilenameMetadata;

/* Line number metadata. */
typedef struct {
    int instructionIndex;
    int lineNumber;
} LineNumberMetadata;

/* Variable. */
typedef struct {
    char type;
    union {
        double n;
        char *s;
        int l;
        HashTable *t;
    } value;
} Variable;

/* C function called from renv, used for system commands and extensions. */
typedef Variable (*N7CFunction)(int, Variable *);

/*
 * RegisterN7CFunction
 * -------------------
 * Register a function with a name, so that a running n7 program can access it
 * through LOAD_FUNCTION.
 */
void RegisterN7CFunction(const char *name, N7CFunction function);

/*
 * RENV_RunFile
 * ------------
 * Run n7b code from file, returns RENV_SUCCESS on success or RENV_FAILURE on
 * failure.
 */
int RENV_RunFile(FILE *File, int argc, char **argv, int win32);

/*
 * RENV_Error
 * ----------
 * Error message, if RENV_RunFile returned RENV_FAILURE.
 */
const char *RENV_Error();

/*
 * ToString
 * --------
 * Convert variable to string and return its value.
 */
char *ToString(Variable *v, int numDecimals);

/*
 * ToNumber
 * --------
 * Convert variable to number and return its value.
 */
double ToNumber(Variable *v);

/*
 * ToNewString
 * -----------
 * Return a new variable that is a string version of v.
 */
Variable ToNewString(Variable *v, int numDecimals);

/*
 * ToNewNumber
 * -----------
 * Return a new variable that is a numeric version of v.
 */
Variable ToNewNumber(Variable *v);

/*
 * NewHashTable
 * ------------
 * Return a new hash table and add it to the garbage collector's allocation
 * table.
 */
HashTable *NewHashTable(int capacity);

/*
 * EqualVariables
 * --------------
 * Return true if variables are concidered equal.
 */
int EqualVariables(Variable *a, Variable *b);

/*
 * DeleteVariable
 * --------------
 * Delete variable, can be used as callback for hash table.
 */
void DeleteVariable(void *data);

/*
 * LockGC
 * ------
 * Prevent garbage collecting until UnlockGC is called.
 */
void LockGC();

/*
 * UnlockGC
 * --------
 * Unlock garbage collecting.
 */
void UnlockGC();

/*
 * GC
 * --
 */
void GC();

/*
 * RuntimeError
 * ------------
 * Force a runtime error and terminate program.
 */
void RuntimeError(const char *msg);

/*
 * RuntimeErrorS
 * -------------
 * Force a runtime error with a string in a format string and terminate program.
 */
void RuntimeErrorS(const char *msg, const char *param);

/*
 * TerminateProgram
 * ----------------
 * Terminate program.
 */
void TerminateProgram();

/* HasConsole
 * ----------
 * Return 1 if not a win32 program.
 */
int HasConsole();

#endif