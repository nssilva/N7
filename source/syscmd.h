/*
 * syscmd.h
 * --------
 *
 * By: Marcus 2021
 */

#ifndef __SYS_CMD_H__
#define __SYS_CMD_H__

#include "renv.h"

#define SYS_PRIMARY_IMAGE 65537

typedef enum {
    SYS_PLN = 0,
    SYS_READ_LINE,
    SYS_DATE_TIME,
    SYS_TIME,
    SYS_CLOCK,
    SYS_SLEEP,
    SYS_FRAME_SLEEP,
    SYS_RND,
    SYS_RANDOMIZE,
    SYS_SYSTEM,
    SYS_CAPTURE,
    SYS_SPLIT_STR,
    SYS_LEFT_STR,
    SYS_RIGHT_STR,
    SYS_MID_STR,
    SYS_IN_STR,
    SYS_REPLACE_STR,
    SYS_LOWER_STR,
    SYS_UPPER_STR,
    SYS_CHR,
    SYS_ASC,
    SYS_STR,
    SYS_TBL_HAS_KEY,
    SYS_TBL_HAS_VALUE,
    SYS_TBL_KEY_OF,
    SYS_TBL_FREE_KEY,
    SYS_TBL_FREE_VALUE,
    SYS_TBL_CLEAR,
    SYS_TBL_INSERT,
    SYS_SET_CLIPBOARD,
    SYS_GET_CLIPBOARD,
    SYS_CREATE_FILE,
    SYS_CREATE_FILE_LEGACY,
    SYS_OPEN_FILE,
    SYS_OPEN_FILE_LEGACY,
    SYS_FREE_FILE,
    SYS_FILE_EXISTS,
    SYS_FILE_WRITE,
    SYS_FILE_WRITE_LINE,
    SYS_FILE_READ,
    SYS_FILE_READ_CHAR,
    SYS_FILE_READ_LINE,
    SYS_OPEN_FILE_DIALOG,
    SYS_SAVE_FILE_DIALOG,
    SYS_CHECK_FILE_EXISTS,
    SYS_SET_WINDOW,
    SYS_SET_REDRAW,
    SYS_WIN_ACTIVE,
    SYS_WIN_EXISTS,
    SYS_SCREEN_W,
    SYS_SCREEN_H,
    SYS_WIN_REDRAW,
    SYS_MOUSE_X,
    SYS_MOUSE_Y,
    SYS_MOUSE_DOWN,
    SYS_SET_MOUSE,
    SYS_CREATE_ZONE,
    SYS_CREATE_ZONE_LEGACY,
    SYS_FREE_ZONE,
    SYS_ZONE,
    SYS_ZONE_X,
    SYS_ZONE_Y,
    SYS_ZONE_W,
    SYS_ZONE_H,
    SYS_INKEY,
    SYS_KEY_DOWN,
    SYS_SET_IMAGE,
    SYS_SET_IMAGE_CLIP_RECT,
    SYS_CLEAR_IMAGE_CLIP_RECT,
    SYS_SET_COLOR,
    SYS_SET_ADDITIVE,
    SYS_CLS,
    SYS_SET_PIXEL,
    SYS_GET_PIXEL,
    SYS_DRAW_PIXEL,
    SYS_DRAW_LINE,
    SYS_DRAW_RECT,
    SYS_DRAW_ELLIPSE,
    SYS_DRAW_POLYGON,
    SYS_DRAW_VRASTER,
    SYS_DRAW_HRASTER,
    SYS_LOAD_IMAGE,
    SYS_LOAD_IMAGE_LEGACY,
    SYS_SAVE_IMAGE,
    SYS_CREATE_IMAGE,
    SYS_CREATE_IMAGE_LEGACY,
    SYS_FREE_IMAGE,
    SYS_SET_IMAGE_COLOR_KEY,
    SYS_SET_IMAGE_GRID,
    SYS_IMAGE_EXISTS,
    SYS_IMAGE_WIDTH,
    SYS_IMAGE_HEIGHT,
    SYS_IMAGE_COLS,
    SYS_IMAGE_ROWS,
    SYS_IMAGE_CELLS,
    SYS_DRAW_IMAGE,
    SYS_CREATE_FONT,
    SYS_CREATE_FONT_LEGACY,
    SYS_LOAD_FONT,
    SYS_LOAD_FONT_LEGACY,
    SYS_SAVE_FONT,
    SYS_FREE_FONT,
    SYS_SET_FONT,
    SYS_FONT_EXISTS,
    SYS_FONT_WIDTH,
    SYS_FONT_HEIGHT,
    SYS_SCROLL,
    SYS_WRITE,
    SYS_WRITE_LINE,
    SYS_CENTER,
    SYS_SET_JUSTIFICATION,
    SYS_SET_CARET,
    SYS_LOAD_SOUND,
    SYS_LOAD_SOUND_LEGACY,
    SYS_FREE_SOUND,
    SYS_SOUND_EXISTS,
    SYS_PLAY_SOUND,
    SYS_LOAD_MUSIC,
    SYS_LOAD_MUSIC_LEGACY,
    SYS_FREE_MUSIC,
    SYS_MUSIC_EXISTS,
    SYS_PLAY_MUSIC,
    SYS_STOP_MUSIC,
    SYS_SET_MUSIC_VOLUME,
    SYS_W3D_RENDER, // 124, ugh, called by id from wolf3d until now, don't add before.
    SYS_CREATE_SOUND,
    SYS_CREATE_SOUND_LEGACY,
    SYS_DOWNLOAD,
    SYS_CONSOLE,
    SYS_DRAW_IMAGE_TRANSFORMED,
    SYS_DRAW_POLYGON_IMAGE,
    SYS_MOUSE_DX,
    SYS_MOUSE_DY,
    SYS_GET_PIXEL_INT,
    SYS_SET_COLOR_INT,
    SYS_DRAW_POLYGON_TRANSFORMED,
    SYS_DRAW_POLYGON_IMAGE_TRANSFORMED,
    SYS_JOY_X,
    SYS_JOY_Y,
    SYS_JOY_BUTTON,
    SYS_FILE_TELL,
    SYS_FILE_SEEK,
    
    SYS_CMD_COUNT
} SystemCommand;

/*
 * SYS_Init
 * --------
 * Init and return array with system command functions.
 */
N7CFunction *SYS_Init();

/*
 * SYS_Release
 * -----------
 * Close window, release resources.
 */
void SYS_Release();

/*
 * SYS_TerminateProgram
 * --------------------
 */
void SYS_TerminateProgram();

/*
 * SYS_WindowFocusChanged
 * ----------------------
 * Windowing should call this one if window loses/regains focus.
 */
void SYS_WindowFocusChanged(int value);

/*
 * SYS_WindowMessageReceived
 * -------------------------
 * Windowing should call this one when receiving a message from another n7 window.
 */
void SYS_WindowMessageReceived(const char *msg);

/*
 * SYS_MouseMove
 * -------------
 */
void SYS_MouseMove(int x, int y);

/*
 * SYS_MouseDown
 * -------------
 */
void SYS_MouseDown(int button);

/*
 * SYS_MouseUp
 * -----------
 */
void SYS_MouseUp(int button);

/*
 * SYS_MouseWheel
 * --------------
 */
void SYS_MouseWheel(int step);

/*
 * SYS_JoyMove
 * -----------
 */
void SYS_JoyMove(int x, int y);

/*
 * SYS_JoyButtonDown
 * -----------------
 */
void SYS_JoyButtonDown(int button);

/*
 * SYS_JoyButtonUp
 * ---------------
 */
void SYS_JoyButtonUp(int button);

/*
 * SYS_KeyChar
 * -----------
 */
void SYS_KeyChar(unsigned int c);

/*
 * SYS_KeyDown
 * -----------
 */
void SYS_KeyDown(unsigned int c);

/*
 * SYS_KeyUp
 * ---------
 */
void SYS_KeyUp(unsigned int c);

#endif