/*
 * main_renv_win.c
 * ---------------
 *
 * By: Marcus 2021
 */
 
#include "renv.h" 
#include "renv_mark.h"
#include "stdlib.h"
#include "stdio.h"
#include "windows.h"

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	char fn[MAX_PATH];
	FILE *file;
    
	GetModuleFileName(NULL, fn, MAX_PATH);
    
    if ((file = fopen(fn, "rb"))) {
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
            /* Ugh, if the progam has no console window, popen and system will
               cause consoles to pop up. So, we ... yeah. */
            AllocConsole();
            ShowWindow(GetConsoleWindow(), SW_HIDE);
            /* Run the bytecode, renv closes the file. */
            if (RENV_RunFile(file, __argc, __argv, 1) == RENV_SUCCESS) {
                return EXIT_SUCCESS;
            }
            else {
                printf("(renv) %s\n", RENV_Error());
            }
        }
    }
    
    return EXIT_FAILURE;
}
