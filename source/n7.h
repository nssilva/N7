/*
 * n7.h
 * ----
 * N7 compiler.
 *
 * By: Marcus 2021-2025
 */
 
#ifndef __N7_H__
#define __N7_H__

/* Yes, the version is a date. */
#define N7_VERSION_STRING "25.09.14b"

#define N7_SUCCESS  0
#define N7_FAILURE  1

#define N7_WIN32_FLAG (unsigned int)1
#define N7_DBG_FLAG   (unsigned int)2

/*
 * N7_Compile
 * ----------
 */
int N7_Compile(const char *srcFilename, const char *dstFilename);

/*
 * N7_Error
 * --------
 */
const char *N7_Error();

/*
 * N7_GetRuntimeFlags
 * ------------------
 */
unsigned int N7_GetRuntimeFlags();

/*
 * N7_SetRuntimeFlags
 * ------------------
 */
void N7_SetRuntimeFlags(unsigned int flags);

/*
 * N7_MemoryRequest
 * ----------------
 */
int N7_MemoryRequest();

/*
 * N7_SetLibPath
 * -------------
 */
void N7_SetLibPath(const char *path);

/*
 * N7_SetUserLibPath
 * -----------------
 */
void N7_SetUserLibPath(const char *path);

#endif