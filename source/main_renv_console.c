/*
 * main_renv_console.c
 * -------------------
 *
 * By: Marcus 2021
 */

#include "renv.h" 
#include "renv_mark.h"
#include "stdlib.h"
#include "stdio.h"

/*
 * main
 * ----
 */
int main(int argc, char **argv) {
    FILE *file;

    /* Load executing file. */
    if (argc > 0 && (file = fopen(argv[0], "rb"))) {
        char mark[7] = {0, 0, 0, 0, 0, 0, 0};
        char c;
        int found = 0;

        /* Look for binary data marker. */
        while (fread(&c, sizeof(char), 1, file)) {
            for (int i = 0; i < 6; i++) mark[i] = mark[i + 1];
            mark[6] = c;
            if (mark[0] == RENV_MARKER_0 &&
                    mark[1] == RENV_MARKER_1 &&
                    mark[2] == RENV_MARKER_2 &&
                    mark[3] == RENV_MARKER_3 &&
                    mark[4] == RENV_MARKER_4 &&
                    mark[5] == RENV_MARKER_5 &&
                    mark[6] == RENV_MARKER_6) {
                found = 1;
                break;
            }
        }
        if (found) {
            /* Run the bytecode, renv closes the file. */
            if (RENV_RunFile(file, argc, argv, 0) == RENV_SUCCESS) {
                return EXIT_SUCCESS;
            }
            else {
                printf("(renv) %s\n", RENV_Error());
                system("pause");
            }
        }
    }
    
    return EXIT_FAILURE;
}
