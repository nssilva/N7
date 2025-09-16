/*
 * main_compiler.c
 * ---------------
 * Compiles n7 source code to n7 assembler, then compiles the assembler source
 * code to n7 bytecode and builds an executable.
 *
 * By: Marcus 2021
 */

#include "asm.h"
#include "n7.h"
#include "renv_mark.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#define ERROR_RENV 1
#define ERROR_BIN 2
#define ERROR_EXE 3
#define ERROR_WRITE 4

/*
 * BuildExecutable
 * ---------------
*/
int BuildExecutable(const char *renvFilename, const char *binFilename, const char *exeFilename, unsigned int flags, unsigned int heapSize);

/*
 * FindCharLast
 * ------------
 */
int FindCharLast(const char *s, char c);

/*
 * GetPath
 * -------
 */
char *GetPath(const char *s);

/*
 * main
 * ----
 */
int main(int argc, char **argv) {
    int result = EXIT_FAILURE;

    if (argc >= 2) {
        char *cmpPath = 0;
        char *srcPath = 0;
        char *libPath = 0;
        char *asmFilename = 0;
        char *binFilename = 0;
        char *exeFilename = 0;
        char *renvFilename = 0;
        unsigned int cmdFlags = 0;
        int cmdMemory = 0;
        int opt = 1;
        int i;

        /* Get path to executable and source. */
        cmpPath = GetPath(argv[0]);
        srcPath = GetPath(argv[1]);
        
        /* Construct path to standard libraries. */
        libPath = (char *)malloc(strlen(cmpPath) + 5);
        strcpy(libPath, cmpPath);
        strcat(libPath, "lib\\");

        /* Look for flags. */
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "win32") == 0) {
                cmdFlags |= N7_WIN32_FLAG;
            }
            else if (strcmp(argv[i], "dbg") == 0) {
                cmdFlags |= N7_DBG_FLAG;
            }
            else if (strncmp(argv[i], "mem", 3) == 0) {
                if (strlen(argv[i]) == 3) {
                    printf("n7: 'mem' missing value\n");
                    return EXIT_FAILURE;
                }
                else {
                    int mr = atoi(argv[i] + 3);
                    if (mr > 0) cmdMemory = mr;
                }
            }
            else if (strcmp(argv[i], "no_opt") == 0) {
                opt = 0;
            }
            else {
                printf("n7: unknown flag \"%s\"\n", argv[i]); 
                return EXIT_FAILURE;
            }
        }
       
        /* Find extension in source code filename. */
        i = FindCharLast(argv[1], '.');
        if (i < 0) i = strlen(argv[1]);
        
        /* Build filenames. */
        asmFilename = (char *)malloc(sizeof(char)*(i + 5));
        binFilename = (char *)malloc(sizeof(char)*(i + 5));
        exeFilename = (char *)malloc(sizeof(char)*(i + 5));
        memcpy(asmFilename, argv[1], i);
        memcpy(binFilename, argv[1], i);
        memcpy(exeFilename, argv[1], i);
        asmFilename[i] = '\0';
        binFilename[i] = '\0';
        exeFilename[i] = '\0';     
        strcat(asmFilename, ".n7a");
        strcat(binFilename, ".n7b");
        strcat(exeFilename, ".exe");

        /* Set compiler library paths. */
        N7_SetLibPath(libPath);
        N7_SetUserLibPath(srcPath);
        
        /* Compile n7 to n7a. */
        if (N7_Compile(argv[1], asmFilename) == N7_SUCCESS) {
            unsigned int flags = N7_GetRuntimeFlags() | cmdFlags;
            const char *renv;
          
            if (flags & N7_WIN32_FLAG) renv = "renv_win.exe";
            else renv = "renv_console.exe";
            cmdMemory = N7_MemoryRequest() ? N7_MemoryRequest() : cmdMemory;
            renvFilename = (char *)malloc(strlen(cmpPath) + strlen(renv) + 1);
            sprintf(renvFilename, "%s%s", cmpPath, renv);
            
            printf("n7: success\n");
            /* Compile n7a to n7b. */
            if (ASM_Compile(asmFilename, binFilename, opt) == ASM_SUCCESS) {
                int err;
                printf("n7a: success\n");
                /* Build executable. */
                if (!(err = BuildExecutable(renvFilename, binFilename, exeFilename, flags, (unsigned int)cmdMemory))) {
                    printf("n7b: success\n");
                    result = EXIT_SUCCESS;
                }
                else {
                    if (err == ERROR_RENV) printf("n7b: error: could not load runtime file\n");
                    else if (err == ERROR_BIN) printf("n7b: error: could not load n7b file\n");
                    else if (err == ERROR_EXE) printf("n7b: error: could not create exe file\n");
                    else printf("n7b: error: failed writing to exe file\n");
                }
            }
            else {
                printf("n7a: %s\n", ASM_Error());
            }
        }
        else {           
            printf("n7: %s\n", N7_Error());
        }
        
        free(cmpPath);
        free(srcPath);
        free(libPath);
        free(asmFilename);
        free(binFilename);
        free(exeFilename);
        free(renvFilename);
    }
    else {
        printf("n7: n7 <source_file> [win] [dbg] [mem<bytes>]\n");
        printf("    win32      - create a win32- instead of console-application\n");
        printf("    dbg        - output debug info\n");
        printf("    mem<bytes> - set memory heap size\n");
    }
    
    return result;
}

/*
 * BuildExecutable
 * ---------------
*/
int BuildExecutable(const char *renvFilename, const char *binFilename, const char *exeFilename, unsigned int flags, unsigned int heapSize) {
    FILE *renvFile = 0;
    FILE *binFile = 0;
    FILE *exeFile = 0;
    int error = 0;
    char dbgFlag = flags & N7_DBG_FLAG ? 1 : 0;
    
    if (!(renvFile = fopen(renvFilename, "rb"))) error = ERROR_RENV;
    if (!(binFile = fopen(binFilename, "rb"))) error = ERROR_BIN;
    if (!(exeFile = fopen(exeFilename, "wb"))) error = ERROR_EXE;

    if (!error) {
        char c;
        while (fread(&c, sizeof(char), 1, renvFile)) {
            if (!fwrite(&c, sizeof(char), 1, exeFile)) {
                error = ERROR_WRITE;
                break;
            }
        }
        if (!error) {
            char m6 = RENV_MARKER_6;
            char m5 = RENV_MARKER_5;
            char m4 = RENV_MARKER_4;
            char m3 = RENV_MARKER_3;
            char m2 = RENV_MARKER_2;
            char m1 = RENV_MARKER_1;
            char m0 = RENV_MARKER_0;
            if (fwrite(&m0, sizeof(char), 1, exeFile) &&
                    fwrite(&m1, sizeof(char), 1, exeFile) &&
                    fwrite(&m2, sizeof(char), 1, exeFile) &&
                    fwrite(&m3, sizeof(char), 1, exeFile) &&
                    fwrite(&m4, sizeof(char), 1, exeFile) &&
                    fwrite(&m5, sizeof(char), 1, exeFile) &&
                    fwrite(&m6, sizeof(char), 1, exeFile) &&
                    fwrite(&dbgFlag, sizeof(char), 1, exeFile) &&
                    fwrite(&heapSize, sizeof(unsigned int), 1, exeFile)) {
                while (fread(&c, sizeof(char), 1, binFile)) {
                    if (!fwrite(&c, sizeof(char), 1, exeFile)) {
                        error = ERROR_WRITE;
                        break;
                    }
                }
            }
            else {
                error = ERROR_WRITE;
            }
        }
    }
    
    if (renvFile) fclose(renvFile);
    if (binFile) fclose(binFile);
    if (exeFile) fclose(exeFile);
    
    return error;
}

/*
 * FindCharLast
 * ------------
 */
int FindCharLast(const char *s, char c) {
    int len;
    int i = -1;

    if (s && (len = strlen(s)))
        for (i = len; i >= 0; i--) if (s[i] == c) break;
    
    return i;
}

/*
 * GetPath
 * -------
 */
char *GetPath(const char *filename) {
    char *path;
    int i = FindCharLast(filename, '\\');
    int j = FindCharLast(filename, '/');
    if (j > i) i = j;
    
    if (i >= 0) {
        path = (char *)malloc(sizeof(char)*(i + 2));
        memcpy(path, filename, i + 1);
        path[i + 1] = '\0';
    }
    else {
        path = (char *)malloc(sizeof(char));
        path[0] = '\0';
    }
    
    return path;
}
