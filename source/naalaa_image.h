/*
 * naalaa_image.h
 * --------------
 *
 * 20250913: Added possibility to allocate a z-buffer for an image. Until I have the time to rewrite
 *           the code, S3D still handles everything about these bufferes. Earlier, S3D reallocated
 *           ITS z-buffer every time the render target changed, but then came the sudden need for
 *           concurrent targets.
 *
 * By: Marcus 2021
 */
 

#ifndef __PL_IMAGE_H__
#define __PL_IMAGE_H__

typedef struct {
    char hasAlpha;
    /*char hasColorKey;*/
} ImageCellInfo;

/* Image. */
typedef struct {
    int id;
    unsigned int *buffer;
    int *zBuffer;
    ImageCellInfo *cellInfo;
    char hasAlpha;
    char hasColorKey;
    unsigned int colorKey;
    int w;
    int h;
    int cols;
    int rows;
    int cells;
    int xMin;
    int yMin;
    int xMax;
    int yMax;
    int lastDrawX;
    int lastDrawY;
} Image;

/* Macros for color components. */

#define ToRGB(r, g, b) \
    (unsigned int)(b + ((g) << 8) + ((r) << 16) + (128 << 24))

#define ToRGBA(r, g, b, a) \
    (unsigned int)(b + ((g) << 8) + ((r) << 16) + ((a) << 24))

#define ColorRedComponent(c) \
    (unsigned char)((c) >> 16)

#define ColorGreenComponent(c) \
    (unsigned char)((c) >> 8)

#define ColorBlueComponent(c) \
    (unsigned char)(c)

#define ColorAlphaComponent(c) \
    (unsigned char)((c) >> 24)

#define ColorToRGBComponents(c, r, g, b) \
    (r) = (c) >> 16;\
    (g) = (c) >> 8;\
    (b) = (c)
    
#define ColorToRGBAComponents(c, r, g, b, a) \
    (r) = (c) >> 16;\
    (g) = (c) >> 8;\
    (b) = (c);\
    (a) = (c) >> 24

/*
 * IMG_SetPerspectiveDiv
 * ---------------------
 */
void IMG_SetPerspectiveDiv(int div);

/*
 * IMG_Create
 * ----------
 */
Image *IMG_Create(int w, int h, unsigned int color);

/*
 * IMG_FromBuffer
 * --------------
 */
Image *IMG_FromBuffer(unsigned int *buffer, int w, int h);

/*
 * IMG_Load
 * --------
 */
Image *IMG_Load(const char *filename);

/*
 * IMG_Save
 * --------
 */
int IMG_Save(Image *img, const char *filename);

/*
 * IMG_Free
 * --------
 */
void IMG_Free(Image *img);

/*
 * IMG_BufferChanged
 * -----------------
 */
void IMG_BufferChanged(Image *img);

/*
 * IMG_SetColorKey
 * ---------------
 */
void IMG_SetColorKey(Image *img, unsigned int color);

/*
 * IMG_SetGrid
 * -----------
 */
void IMG_SetGrid(Image *img, int cols, int rows);

/*
 * IMG_Width
 * ---------
 */
int IMG_Width(Image *img);

/*
 * IMG_Height
 * ----------
 */
int IMG_Height(Image *img);

/*
 * IMG_Buffer
 * ----------
 */
unsigned int *IMG_Buffer(Image *img);

/*
 * IMG_AddZBuffer
 * --------------
 */
void IMG_AddZBuffer(Image *img);

/*
 * IMG_ZBuffer
 * -----------
 */
int *IMG_ZBuffer(Image *img);

/*
 * IMG_SetClipRect
 * ---------------
 */
void IMG_SetClipRect(Image *img, int x, int y, int w, int h);

/*
 * IMG_ClipRect
 * ------------
 */
void IMG_ClipRect(Image *img, int x, int y, int w, int h);

/*
 * IMG_ClearClipRect
 * -----------------
 */
void IMG_ClearClipRect(Image *img);

/*
 * IMG_ClipX
 * ---------
 */
int IMG_ClipX(Image *img);

/*
 * IMG_ClipY
 * ---------
 */
int IMG_ClipY(Image *img);

/*
 * IMG_ClipWidth
 * -------------
 */
int IMG_ClipWidth(Image *img);

/*
 * IMG_ClipHeight
 * --------------
 */
int IMG_ClipHeight(Image *img);

/*
 * IMG_DrawImage
 * -------------
 * Maybe change additive to drawMode and support other modes?
 */
void IMG_DrawImage(Image *dst, int x, int y, Image *src, int srcX, int srcY, int srcW, int srcH, unsigned int color, int useImageAlpha, char additive);

/*
 * IMG_DrawImageCel
 * ----------------
 */
void IMG_DrawImageCel(Image *dst, int x, int y, Image *src, int cel, unsigned int color, char additive);

/*
 * IMG_DrawVRaster
 * ---------------
 * Color as fog.
 */
void IMG_DrawVRaster(Image *dst, Image *src,
        int x, int y0, int y1,
        float srcU, float srcV, float dstU, float dstV, unsigned int color);

/*
 * IMG_DrawHRaster
 * ---------------
 * Color as fog.
 */
void IMG_DrawHRaster(Image *dst, Image *src,
        int y, int x0, int x1,
        float srcU, float srcV, float dstU, float dstV, unsigned int color);

/*
 * IMG_DrawPixel
 * -------------
 */
void IMG_DrawPixel(Image *dst, int x, int y, unsigned int color, char additive);

/*
 * IMG_SetPixel
 * ------------
 */
void IMG_SetPixel(Image *dst, int x, int y, unsigned int color);

/*
 * IMG_GetPixel
 * ------------
 */
int IMG_GetPixel(Image *img, int x, int y, unsigned int *color);

/*
 * IMG_DrawRect
 * ------------
 */
void IMG_DrawRect(Image *dst, int x, int y, int w, int h, unsigned int color, char additive);

/*
 * IMG_FillRect
 * ------------
 */
void IMG_FillRect(Image *dst, int x, int y, int w, int h, unsigned int color, char additive);

/*
 * IMG_SetRect
 * ------------
 */
void IMG_SetRect(Image *dst, int x, int y, int w, int h, unsigned int color);

/*
 * IMG_ClearRect
 * -------------
 */
/*void IMG_ClearRect(Image *dst, int x, int y, int w, int h);*/

/*
 * IMG_DrawLine
 * ------------
 */
void IMG_DrawLine(Image *dst, int x0, int y0, int x1, int y1, unsigned int color, char additive);

/*
 * IMG_DrawLineTo
 * --------------
 */
void IMG_DrawLineTo(Image *dst, int toX, int toY, unsigned int color, char additive);

/*
 * IMG_DrawEllipse
 * ---------------
 */
void IMG_DrawEllipse(Image *dst, int cx, int cy, int xr, int yr, unsigned int color, char additive);

/*
 * IMG_FillEllipse
 * ---------------
 */
void IMG_FillEllipse(Image *dst, int cx, int cy, int xr, int yr, unsigned int color, char additive);

/*
 * IMG_DrawPolygon
 * ---------------
 */
void IMG_DrawPolygon(Image *dst, int numPoints, int *points, unsigned int color, char additive);

/*
 * IMG_FillPolygon
 * ---------------
 */
void IMG_FillPolygon(Image *dst, int numPoints, int *points, unsigned int color, char additive);

/*
 * IMG_FillPolygonZ
 * ----------------
 */
void IMG_FillPolygonZBRW(Image *dst, int numPoints, int *points, float *uvz, unsigned int color, char additive, int *zbuffer);
void IMG_FillPolygonZBW(Image *dst, int numPoints, int *points, float *uvz, unsigned int color, char additive, int *zbuffer);
void IMG_FillPolygonZBR(Image *dst, int numPoints, int *points, float *uvz, unsigned int color, char additive, int *zbuffer);

/*
 * IMG_TexturePolygon
 * ------------------
 */
void IMG_TexturePolygon(Image *dst, int numPoints, int *points, float *uv, Image *src, unsigned int color, int useImageAlpha, char additive);

/*
 * IMG_TexturePolygonZ
 * -------------------
 */
void IMG_TexturePolygonZ(Image *dst, int numPoints, int *points, float *uvz, Image *src, unsigned int color, int useImageAlpha, char additive);
void IMG_TexturePolygonZBRW(Image *dst, int numPoints, int *points, float *uvz, Image *src, unsigned int color, int useImageAlpha, char additive, int *zbuffer);
void IMG_TexturePolygonZBW(Image *dst, int numPoints, int *points, float *uvz, Image *src, unsigned int color, int useImageAlpha, char additive, int *zbuffer);
void IMG_TexturePolygonZBR(Image *dst, int numPoints, int *points, float *uvz, Image *src, unsigned int color, int useImageAlpha, char additive, int *zbuffer);

/*
 * IMG_Fill
 * --------
 */
void IMG_Fill(Image *dst, int x, int y, unsigned int color, char additive);

/*
 * IMG_Scroll
 * ----------
 */
void IMG_Scroll(Image *img, int stepX, int stepY);

#endif
