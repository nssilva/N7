/*
 * naalaa_font.h
 * -------------
 *
 * By: Marcus 2021
 */

#ifndef __NAALAA_FONT_H__
#define __NAALAA_FONT_H__

#include "naalaa_image.h"

/* Character info. */
typedef struct {
    int a;
    int b;
    int c;
} BMABC;

/* Bitmap font. */
typedef struct {
  Image *image;
  int height;
  BMABC *abc;
} BitmapFont;

/*
 * BF_Create
 * ---------
 */
BitmapFont *BF_Create(const char *name, int size, int bold, int italic, int underline, int smooth);

/*
 * BF_CreateEmpty
 * --------------
 * Image is not copied, freeing the font will free the image.
 */
BitmapFont *BF_CreateEmpty(Image *image);

/*
 * BF_Load
 * -------
 */
BitmapFont *BF_Load(const char *name);

/*
 * BF_Save
 * -------
 */
int BF_Save(BitmapFont *bf, const char *name);

/*
 * BF_SetABC
 * ---------
 */
void BF_SetABC(BitmapFont *bf, int index, int a, int b, int c);

/*
 * BF_SetABCv
 * ----------
 */
void BF_SetABCv(BitmapFont *bf, int *values);

/*
 * BF_Free
 * -------
 */
void BF_Free(BitmapFont *bf);

/*
 * BF_ApplySmoothing
 * -----------------
 */
void BF_ApplySmoothing(BitmapFont *bf);

/*
 * BF_Write
 * --------
 */
void BF_Write(BitmapFont *bf, Image *dst, const char *text, int *xRef, int *yRef, unsigned int color, char additive);

/*
 * BF_Width
 * --------
 */
int BF_Width(BitmapFont *bf, const char *text);

#endif