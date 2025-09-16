/*
 * naalaa_image.c
 * --------------
 *
 * By: Marcus 2021
 */

#include <stdlib.h>
#include <stdio.h> 
#include <math.h>
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "naalaa_image.h"

#define CB_POS(img, x, y) (img->buffer + (y)*img->w + x)

void IMG_UpdateAlphaInfo(Image *img);
void IMG_DrawHLine(Image *dst, int xstart, int xend, int row, unsigned int color, char additive);
void IMG_DrawHLineZBRW(Image *dst, int xstart, int xend, int row, float zstart, float zend, unsigned int color, char additive, int *zbuffer);
void IMG_DrawHLineZBW(Image *dst, int xstart, int xend, int row, float zstart, float zend, unsigned int color, char additive, int *zbuffer);
void IMG_DrawHLineZBR(Image *dst, int xstart, int xend, int row, float zstart, float zend, unsigned int color, char additive, int *zbuffer);
void IMG_DrawHRaster2(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float dstU, float dstV, unsigned int color, int useImageAlpha, char additive);
void IMG_DrawHRaster2Z(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float srcZ, float dstU, float dstV, float dstZ, unsigned int color, int useImageAlpha, char additive);
void IMG_DrawHRaster2ZBRW(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float srcZ, float dstU, float dstV, float dstZ, unsigned int color, int useImageAlpha, char additive, int *zbuffer);
void IMG_DrawHRaster2ZBW(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float srcZ, float dstU, float dstV, float dstZ, unsigned int color, int useImageAlpha, char additive, int *zbuffer);
void IMG_DrawHRaster2ZBR(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float srcZ, float dstU, float dstV, float dstZ, unsigned int color, int useImageAlpha, char additive, int *zbuffer);


void IMG_ScrollUp(Image *img, int step);
void IMG_ScrollDown(Image *img, int step);
void IMG_ScrollLeft(Image *img, int step);
void IMG_ScrollRight(Image *img, int step);
void IMG_ScrollUpLeft(Image *img, int stepX, int stepY);
void IMG_ScrollUpRight(Image *img, int stepX, int stepY);
void IMG_ScrollDownLeft(Image *img, int stepX, int stepY);
void IMG_ScrollDownRight(Image *img, int stepX, int stepY);

typedef struct {
    float x;
    float u;
    float v;
    float z;
} SLPoint;

/* Target image width divisions for perspective correctness. */
static int sPerspectiveDiv = 16;

/*
 * IMG_SetPerspectiveDiv
 * ---------------------
 */
void IMG_SetPerspectiveDiv(int div) {
    sPerspectiveDiv = div;
}
    

/*
 * IMG_Create
 * ----------
 */
Image *IMG_Create(int w, int h, unsigned int color) {
    Image *img;
    int i;

    if (w <= 0 || h <= 0) return 0;
    
    img = (Image *)malloc(sizeof(Image));
    img->buffer = (unsigned int *)malloc(sizeof(unsigned int)*w*h);
    img->zBuffer = 0;
    img->cellInfo = (ImageCellInfo *)malloc(sizeof(ImageCellInfo));
    img->w = w;
    img->h = h;
    img->hasAlpha = ColorAlphaComponent(color) < 128;
    img->cellInfo[0].hasAlpha = img->hasAlpha;
    img->hasColorKey = 0;
    img->colorKey = 0x80ff00ff;
    img->cols = 1;
    img->rows = 1;
    img->cells = 1;
    img->xMin = 0;
    img->yMin = 0;
    img->xMax = w;
    img->yMax = h;
    img->lastDrawX = 0;
    img->lastDrawY = 0;
    
    for (i = 0; i < w*h; i++) img->buffer[i] = color;
    
    return img;
}

/*
 * IMG_FromBuffer
 * --------------
 */
Image *IMG_FromBuffer(unsigned int *buffer, int w, int h) {
    Image *img;
    
    if (!buffer || w <= 0 || h <= 0) return 0;
    
    img = (Image *)malloc(sizeof(Image));
    img->buffer = buffer;
    img->zBuffer = 0;
    img->cellInfo = (ImageCellInfo *)malloc(sizeof(ImageCellInfo));
    img->w = w;
    img->h = h;
    img->hasAlpha = 0;
    img->hasColorKey = 0;
    img->colorKey = 0x80ff00ff;
    img->cols = 1;
    img->rows = 1;
    img->cells = 1;
    img->xMin = 0;
    img->yMin = 0;
    img->xMax = w;
    img->yMax = h;
    img->lastDrawX = 0;
    img->lastDrawY = 0;
    
    return img;
}

/*
 * IMG_Load
 * --------
 */
Image *IMG_Load(const char *filename) {
    Image *img;
    unsigned int *buffer;
    int w, h;
    
    int comp;
    buffer = (unsigned int *)stbi_load(filename, &w, &h, &comp, 4);
    if (buffer) {
        int s = w*h;
        
        img = IMG_FromBuffer(buffer, w, h);
        for (int i = 0; i < s; i++) {
            unsigned char r, g, b, a;
            r = (buffer[i] & 0xff);
            g = (buffer[i] & 0xff00) >> 8;
            b = (buffer[i] & 0xff0000) >> 16;
            a = (buffer[i] & 0xff000000) >> 24;
            if (a == 255) {
                a = 128;
            }
            else {
                img->hasAlpha = 1;
                a /= 2;
            }
            buffer[i] = ToRGBA(r, g, b, a);
        }
        img->cellInfo[0].hasAlpha = img->hasAlpha;
        return img;
    }
    else {
        return 0;
    }
}

/*
 * IMG_Save
 * --------
 */
int IMG_Save(Image *img, const char *filename) {
    unsigned int *pngBuffer = 0;
    
    pngBuffer = (unsigned int *)malloc(sizeof(unsigned int)*img->w*img->h);
    for (int i = 0; i < img->w*img->h; i++) {
        unsigned char r, g, b, a;
        r = (img->buffer[i] & 0xff0000) >> 16;
        g = (img->buffer[i] & 0xff00) >> 8;
        b = (img->buffer[i] & 0xff);
        a = (img->buffer[i] & 0xff000000) >> 24;
        if (a == 128) a = 255;
        else a *= 2;
        pngBuffer[i] = (r) + (g << 8) + (b << 16) + (a << 24);
    }
    int result = stbi_write_png(filename, img->w, img->h, 4, pngBuffer, img->w*4);
    free(pngBuffer);
    
    return result != 0;
}

/*
 * IMG_Free
 * --------
 */
void IMG_Free(Image *img) {
    free(img->buffer);
    free(img->zBuffer);
    free(img->cellInfo);
    free(img);
}

/*
 * IMG_SetColorKey
 * ---------------
 */
void IMG_SetColorKey(Image *img, unsigned int color) {
    img->hasColorKey = 1;
    img->colorKey = color;
    IMG_BufferChanged(img);
}

/*
 * IMG_SetGrid
 * -----------
 */
void IMG_SetGrid(Image *img, int cols, int rows) {
    if (img->w%cols > 0 || img->h%rows > 0) return;
    
    img->cols = cols <= 0 ? cols = 1 : cols;
    img->rows = rows <= 0 ? rows = 1 : rows;
    img->cells = img->cols*img->rows;
    free(img->cellInfo);
    img->cellInfo = (ImageCellInfo *)malloc(sizeof(ImageCellInfo)*img->cells);
    IMG_UpdateAlphaInfo(img);
}


/*
 * IMG_Width
 * ---------
 */
int IMG_Width(Image *img) {
    return img->w;
}

/*
 * IMG_Height
 * ----------
 */
int IMG_Height(Image *img) {
    return img->h;
}

/*
 * IMG_Buffer
 * ----------
 */
unsigned int *IMG_Buffer(Image *img) {
    return img->buffer;
}

/*
 * IMG_AddZBuffer
 * --------------
 */
void IMG_AddZBuffer(Image *img) {
    if (!img->zBuffer) {
        img->zBuffer = (int *)malloc(sizeof(int)*img->w*img->h);
    }
}

/*
 * IMG_ZBuffer
 * -----------
 */
int *IMG_ZBuffer(Image *img) {
    return img->zBuffer;
}

/*
 * IMG_SetClipRect
 * ---------------
 */
void IMG_SetClipRect(Image *img, int x, int y, int w, int h) {
    img->xMin = x;
    img->yMin = y;
    img->xMax = x + w;
    img->yMax = y + h;

    if (img->xMin < 0) img->xMin = 0;
    if (img->xMin >= img->w) img->xMin = img->w - 1;
    if (img->yMin < 0) img->yMin = 0;
    if (img->yMin >= img->h) img->yMin = img->h - 1;

    if (img->xMax < 0) img->xMax = 0;
    if (img->xMax > img->w) img->xMax = img->w;
    if (img->yMax < 0) img->yMax = 0;
    if (img->yMax > img->h) img->yMax = img->h;
}

/*
 * IMG_ClipRect
 * ------------
 */
void IMG_ClipRect(Image *img, int x, int y, int w, int h) {
}

/*
 * IMG_ClearClipRect
 * -----------------
 */
void IMG_ClearClipRect(Image *img) {
    img->xMin = 0;
    img->yMin = 0;
    img->xMax = img->w;
    img->yMax = img->h;
}

/*
 * IMG_ClipX
 * ---------
 */
int IMG_ClipX(Image *img) {
    return img->xMin;
}

/*
 * IMG_ClipY
 * ---------
 */
int IMG_ClipY(Image *img) {
    return img->yMin;
}

/*
 * IMG_ClipWidth
 * -------------
 */
int IMG_ClipWidth(Image *img) {
    return img->xMax - img->xMin;
}

/*
 * IMG_ClipHeight
 * --------------
 */
int IMG_ClipHeight(Image *img) {
    return img->yMax - img->yMin;
}

/*
 * IMG_DrawImage
 * -------------
 */
void IMG_DrawImage(Image *dst, int x, int y, Image *src, int srcX, int srcY, int srcW, int srcH, unsigned int color, int useImageAlpha, char additive) {
    unsigned int *srcBuffer;
    unsigned int *dstBuffer;
    int width, height;
    int sWidth;
    unsigned char srcR, srcG, srcB, srcA, dstR, dstG, dstB, invA;
    int r, g, b, a; /* 220331 ... why are these signed? */
    unsigned char cR, cG, cB, cA;
    int hasAlpha, hasColor;
    int i, j;

    if (srcW <= 0 || srcH <= 0) return;
    if (srcX < 0) {
        srcW += srcX;
        srcX = 0;
        if (srcW <= 0) return;
    }
    if (srcY < 0) {
        srcH += srcY;
        srcY = 0;
        if (srcH <= 0) return;
    }
    if (srcX + srcW > src->w) srcW -= srcX + srcW - src->w;
    if (srcY + srcH > src->h) srcH -= srcY + srcH - src->h;
    if (x + srcW < dst->xMin || x > dst->xMax) return;
    if (y + srcH < dst->yMin || y > dst->yMax) return;
    
    srcBuffer = src->buffer + src->w*srcY + srcX;
    dstBuffer = dst->buffer;
        
    width = srcW;
    height = srcH;

    if (x < dst->xMin) {
        srcBuffer += dst->xMin - x;
        width -= dst->xMin - x;
        dstBuffer += dst->xMin;
    }
    else dstBuffer += x;

    if (y < dst->yMin) {
        srcBuffer += (dst->yMin - y)*src->w;
        height -= dst->yMin - y;
        dstBuffer += dst->yMin*dst->w;
    }
    else dstBuffer += y*dst->w;

    if (x + srcW > dst->xMax) width -= x + srcW - dst->xMax;

    if (y + srcH > dst->yMax) height -= y + srcH - dst->yMax;
        
    ColorToRGBAComponents(color, cR, cG, cB, cA);
    if (cA == 0) return;

    sWidth = src->w;

    hasColor = cR != 255 || cG != 255 || cB != 255;
    hasAlpha = cA != 128 || (src->hasAlpha && useImageAlpha);

    /* Additive mode? */
    if (additive) {
        if (hasColor && hasAlpha) {
            for (i = 0; i < height; i++, srcBuffer += sWidth, dstBuffer += dst->w) {
                for (j = 0; j < width; j++) {
                    ColorToRGBComponents(dstBuffer[j], dstR, dstG, dstB);
                    ColorToRGBAComponents(srcBuffer[j], srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8;
                        g = (srcG * cG) >> 8;
                        b = (srcB * cB) >> 8;
                        a = (srcA * cA) >> 7;                        
                        r = dstR + ((r*a) >> 7);
                        g = dstG + ((g*a) >> 7);
                        b = dstB + ((b*a) >> 7);
                        dstBuffer[j] = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    }
                }
            }
        }
        else if (hasAlpha) {
            for (i = 0; i < height; i++, srcBuffer += sWidth, dstBuffer += dst->w) {
                for (j = 0; j < width; j++) {
                    ColorToRGBComponents(dstBuffer[j], dstR, dstG, dstB);
                    ColorToRGBAComponents(srcBuffer[j], srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        r = dstR + ((srcR*a) >> 7);
                        g = dstG + ((srcG*a) >> 7);
                        b = dstB + ((srcB*a) >> 7);
                        dstBuffer[j] = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    }
                }
            }
        }
        else if (hasColor) {
            for (i = 0; i < height; i++, srcBuffer += sWidth, dstBuffer += dst->w) {
                for (j = 0; j < width; j++) {
                    ColorToRGBComponents(dstBuffer[j], dstR, dstG, dstB);
                    ColorToRGBComponents(srcBuffer[j], srcR, srcG, srcB);
                    r = dstR + ((srcR * cR) >> 8);
                    g = dstG + ((srcG * cG) >> 8);
                    b = dstB + ((srcB * cB) >> 8);
                    dstBuffer[j] = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                }
            }
        }
        else {
            for (i = 0; i < height; i++, srcBuffer += sWidth, dstBuffer += dst->w) {
                for (j = 0; j < width; j++) {
                    ColorToRGBComponents(dstBuffer[j], dstR, dstG, dstB);
                    ColorToRGBComponents(srcBuffer[j], srcR, srcG, srcB);
                    r = dstR + srcR;
                    g = dstG + srcG;
                    b = dstB + srcB;
                    dstBuffer[j] = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                }
            }
        }
    }
    else {
        if (hasColor && hasAlpha) {
            for (i = 0; i < height; i++, srcBuffer += sWidth, dstBuffer += dst->w) {
                for (j = 0; j < width; j++) {
                    ColorToRGBComponents(dstBuffer[j], dstR, dstG, dstB);
                    ColorToRGBAComponents(srcBuffer[j], srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8;
                        g = (srcG * cG) >> 8;
                        b = (srcB * cB) >> 8;
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + r*a) >> 7;
                        dstG = (dstG*invA + g*a) >> 7;
                        dstB = (dstB*invA + b*a) >> 7;
                        dstBuffer[j] = ToRGB(dstR, dstG, dstB);
                    }
                }
            }
        }
        else if (hasAlpha) {
            for (i = 0; i < height; i++, srcBuffer += sWidth, dstBuffer += dst->w) {
                for (j = 0; j < width; j++) {
                    ColorToRGBComponents(dstBuffer[j], dstR, dstG, dstB);
                    ColorToRGBAComponents(srcBuffer[j], srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + srcR*a) >> 7;
                        dstG = (dstG*invA + srcG*a) >> 7;
                        dstB = (dstB*invA + srcB*a) >> 7;
                        dstBuffer[j] = ToRGB(dstR, dstG, dstB);
                    }
                }
            }
        }
        else if (hasColor) {
            for (i = 0; i < height; i++, srcBuffer += sWidth, dstBuffer += dst->w) {
                for (j = 0; j < width; j++) {
                    ColorToRGBComponents(srcBuffer[j], srcR, srcG, srcB);
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    dstBuffer[j] = ToRGB(r, g, b);
                }
            }
        }
        else {
            for (i = 0; i < height; i++, srcBuffer += sWidth, dstBuffer += dst->w) {
                for (j = 0; j < width; j++) {
                    dstBuffer[j] = srcBuffer[j];
                }
            }
        }
    }
}

/*
 * IMG_DrawImageCel
 * ----------------
 */
void IMG_DrawImageCel(Image *dst, int x, int y, Image *src, int cel, unsigned int color, char additive) {
    int celW = src->w/src->cols;
    int celH = src->h/src->rows;
    if (cel >= 0 || cel < src->cells) IMG_DrawImage(dst, x, y, src, celW*(cel%src->cols), celH*(cel/src->cols), celW, celH, color, src->cellInfo[cel].hasAlpha, additive);
}

/*
 * IMG_DrawPixel
 * -------------
 */
void IMG_DrawPixel(Image *dst, int x, int y, unsigned int color, char additive) {
    unsigned char srcR, srcG, srcB, srcA, dstR, dstG, dstB, invA;
    int ai;

    dst->lastDrawX = x;
    dst->lastDrawY = y;
    
    if (x < dst->xMin || x >= dst->xMax || y < dst->yMin || y >= dst->yMax) return;
        
    ai = y*dst->w + x;
    
    if (additive) {
        int r, g, b;        
        ColorToRGBAComponents(color, srcR, srcG, srcB, srcA);
        ColorToRGBComponents(dst->buffer[ai], dstR, dstG, dstB);
        if (ColorAlphaComponent(color) < 128) {
            srcR = (srcR*srcA) >> 7;
            srcG = (srcG*srcA) >> 7;
            srcB = (srcB*srcA) >> 7;
        }
        r = dstR + srcR;
        g = dstG + srcG;
        b = dstB + srcB;
        color = ToRGB((r > 255 ? 255 : r), (g > 255 ? 255 : g), (b > 255 ? 255 : b));
    }
    else {
        if (ColorAlphaComponent(color) < 128) {
            ColorToRGBAComponents(color, srcR, srcG, srcB, srcA);
            ColorToRGBComponents(dst->buffer[ai], dstR, dstG, dstB);
            invA = 128 - srcA;
            dstR = (dstR*invA + srcR*srcA) >> 7;
            dstG = (dstG*invA + srcG*srcA) >> 7;
            dstB = (dstB*invA + srcB*srcA) >> 7;            
            color = ToRGB(dstR, dstG, dstB);
        }
    }
    
    dst->buffer[ai] = color;
}

/*
 * IMG_SetPixel
 * ------------
 */
void IMG_SetPixel(Image *dst, int x, int y, unsigned int color) {
    dst->lastDrawX = x;
    dst->lastDrawY = y;
    
    if (x < dst->xMin || x >= dst->xMax || y < dst->yMin || y >= dst->yMax) return;
        
    dst->buffer[y*dst->w + x] = color;
}

/*
 * IMG_GetPixel
 * ------------
 */
int IMG_GetPixel(Image *img, int x, int y, unsigned int *color) {
    if (x < img->xMin || x >= img->xMax || y < img->yMin || y >= img->yMax) return 0;
    *color = img->buffer[y*img->w + x];
    return 1;
}


/*
 * IMG_DrawRect
 * ------------
 */
void IMG_DrawRect(Image *dst, int x, int y, int w, int h, unsigned int color, char additive) {
    IMG_FillRect(dst, x, y, w, 1, color, additive);
    IMG_FillRect(dst, x, y + h - 1, w, 1, color, additive);
    IMG_FillRect(dst, x, y + 1, 1, h - 1, color, additive);
    IMG_FillRect(dst, x + w - 1, y + 1, 1, h - 1, color, additive);
}

/*
 * IMG_FillRect
 * ------------
 */
void IMG_FillRect(Image *dst, int x, int y, int w, int h, unsigned int color, char additive) {
    unsigned int *cb;
    int dx;
    int x1, y1;

    if (w <= 0 || h <= 0) return;
    if (ColorAlphaComponent(color) == 0) return;

    x1 = x + w;
    y1 = y + h;
        
    if (x >= dst->xMax || y >= dst->yMax || x1 < dst->xMin || y1 < dst->yMin) return;
                
    if (x < dst->xMin) x = dst->xMin;
    if (x1 > dst->xMax) x1 = dst->xMax;
    if (y < dst->yMin) y = dst->yMin;
    if (y1 > dst->yMax) y1 = dst->yMax;
        
    cb = dst->buffer + y*dst->w + x;
    dx = x1 - x;

    if (additive) {
        unsigned char srcR, srcG, srcB;
        unsigned char srcA, dstR, dstG, dstB;
        int r, g, b;
            
        ColorToRGBAComponents(color, srcR, srcG, srcB, srcA);
        if (srcR == 0 && srcG == 0 && srcB == 0) return; 
        
        if (ColorAlphaComponent(color) < 128) {
            srcR = (srcR*srcA) >> 7;
            srcG = (srcG*srcA) >> 7;
            srcB = (srcB*srcA) >> 7;
        }
        for (int ys = y; ys < y1; ys++, cb += dst->w) {
            for (int xs = 0; xs < dx; xs++) {
                ColorToRGBComponents(cb[xs], dstR, dstG, dstB);
                r = dstR + srcR;
                g = dstG + srcG;
                b = dstB + srcB;
                cb[xs] = ToRGB((r > 255 ? 255 : r), (g > 255 ? 255 : g), (b > 255 ? 255 : b));                            
            }
        }
    }
    else {
        if (ColorAlphaComponent(color) < 128) {
            unsigned char srcR, srcG, srcB;
            unsigned char srcA, dstR, dstG, dstB, invA;
            
            ColorToRGBAComponents(color, srcR, srcG, srcB, srcA);
            
            srcR = (srcR*srcA) >> 7;
            srcG = (srcG*srcA) >> 7;
            srcB = (srcB*srcA) >> 7;
            invA = 128 - srcA;
            for (int ys = y; ys < y1; ys++, cb += dst->w) {
                for (int xs = 0; xs < dx; xs++) {
                    ColorToRGBComponents(cb[xs], dstR, dstG, dstB);
                    dstR = ((dstR*invA) >> 7) + srcR;
                    dstG = ((dstG*invA) >> 7) + srcG;
                    dstB = ((dstB*invA) >> 7) + srcB;
                    cb[xs] = ToRGB(dstR, dstG, dstB);                            
                }
            }
        }
        else {
            for (int ys = y; ys < y1; ys++, cb += dst->w) {
                for (int xs = 0; xs < dx; xs++) {
                    cb[xs] = color;
                }
            }
        }
    }
}

/*
 * IMG_SetRect
 * -----------
 */
void IMG_SetRect(Image *dst, int x, int y, int w, int h, unsigned int color) {
    unsigned int *cb;
    int dx;
    int x1, y1;

    if (w <= 0 || h <= 0) return;

    x1 = x + w;
    y1 = y + h;
        
    if (x >= dst->xMax || y >= dst->yMax || x1 < dst->xMin || y1 < dst->yMin) return;
                
    if (x < dst->xMin) x = dst->xMin;
    if (x1 > dst->xMax) x1 = dst->xMax;
    if (y < dst->yMin) y = dst->yMin;
    if (y1 > dst->yMax) y1 = dst->yMax;
        
    cb = dst->buffer + y*dst->w + x;
    dx = x1 - x;
        
    for (int ys = y; ys < y1; ys++, cb += dst->w) {
        for (int xs = 0; xs < dx; xs++) {
            cb[xs] = color;
        }
    }
}

/*
 * IMG_DrawLine
 * ------------
 * Lines should be clipped and an unsafe version of IMG_DrawPixel should be used.
 */
void IMG_DrawLine(Image *dst, int x0, int y0, int x1, int y1, unsigned int color, char additive) {
    int dx, sx, dy, sy, err;
    
    dst->lastDrawX = x1;
    dst->lastDrawY = y1;
    
    if (ColorAlphaComponent(color) == 0) return;
    
    if (y0 == y1) {
        if (x1 >= x0) IMG_FillRect(dst, x0, y0, x1 - x0 + 1, 1, color, additive);
        else  IMG_FillRect(dst, x1, y0, x0 - x1 + 1, 1, color, additive);
        return;
    }
    else if (x0 == x1) {
        if (y1 >= y0) IMG_FillRect(dst, x0, y0, 1, y1 - y0 + 1, color, additive);
        else  IMG_FillRect(dst, x0, y1, 1, y0 - y1 + 1, color, additive);
        return;
    }
    
    dx = x1 > x0 ? x1 - x0 : x0 - x1;
    sx = x0 < x1 ? 1 : -1;
    dy = y1 > y0 ? y0 - y1 : y1 - y0;
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;
    while (1) {
        int e2;
        IMG_DrawPixel(dst, x0, y0, color, additive);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2*err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/*
 * IMG_DrawLineTo
 * --------------
 */
void IMG_DrawLineTo(Image *dst, int toX, int toY, unsigned int color, char additive) {
    IMG_DrawLine(dst, dst->lastDrawX, dst->lastDrawY, toX, toY, color, additive);
}

/*
 * IMG_FillPolygon
 * ---------------
 */
void IMG_FillPolygon(Image *dst, int numPoints, int *points, unsigned int color, char additive) {
    int minY, maxY, minX, maxX;
    int i, y;
    int *p;
    int xval[64];
    int numxval;
    
    if (numPoints <= 0) return;
    if (ColorAlphaComponent(color) == 0) return;

    p = points;
    minY = *(p + 1);
    maxY = minY;
    minX = *p;
    maxX = minX;
    p += 2;
    for (i = 1; i < numPoints; i++, p += 2) {
        if (*(p + 1) < minY) minY = *(p + 1);
        if (*(p + 1) > maxY) maxY = *(p + 1);
        if (*p < minX) minX = *p;
        if (*p > maxX) maxX = *p;
    }
    if (maxX < dst->xMin || minX >= dst->xMax || maxY < dst->yMin || minY >= dst->yMax) return;

    if (minY < dst->yMin) minY = dst->yMin;
    if (maxY > dst->yMax) maxY = dst->yMax;

    for (y = minY; y <= maxY; y++) {
        numxval = 0;
        for (i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                int dx, dy;
                dy = p1[1] - p0[1];
                dx = p1[0] - p0[0];
                dy = p1[1] - p0[1];
                xval[numxval++] = (p0[0] + dx*(y - p0[1])/dy);
            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    int dx, dy;
                    dy = p1[1] - p0[1];
                    dx = p1[0] - p0[0];
                    dy = p1[1] - p0[1];
                    xval[numxval++] = (p0[0] + dx*(y - p0[1])/dy);
                }
            }
        }
        if (numxval) {
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (i = 0; i < last; i++) {
                    if (xval[i] > xval[i + 1]) {
                        sorted = 0;
                        int tmp = xval[i];
                        xval[i] = xval[i + 1];
                        xval[i + 1] = tmp;
                    }
                }
                last--;
            } while (!sorted);
            for (i = 0; i < numxval; i += 2) {
                IMG_DrawHLine(dst, xval[i], xval[i + 1], y, color, additive);
            }
        }
    }
}

/*
 * IMG_FillPolygonZBRW
 * -------------------
 */
void IMG_FillPolygonZBRW(Image *dst, int numPoints, int *points, float *uvz, unsigned int color, char additive, int *zbuffer) {
    int minY, maxY, minX, maxX;
    int i, y;
    int *p;
    int xval[16];
    float zval[16];
    int numxval;
    
    if (numPoints <= 0) return;
    if (ColorAlphaComponent(color) == 0) return;

    p = points;
    minY = *(p + 1);
    maxY = minY;
    minX = *p;
    maxX = minX;
    p += 2;
    for (i = 1; i < numPoints; i++, p += 2) {
        if (*(p + 1) < minY) minY = *(p + 1);
        if (*(p + 1) > maxY) maxY = *(p + 1);
        if (*p < minX) minX = *p;
        if (*p > maxX) maxX = *p;
    }
    if (maxX < dst->xMin || minX >= dst->xMax || maxY < dst->yMin || minY >= dst->yMax) return;

    if (minY < dst->yMin) minY = dst->yMin;
    if (maxY > dst->yMax) maxY = dst->yMax;

    for (y = minY; y <= maxY; y++) {
        numxval = 0;
        for (i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            float z0 = uvz[i*3 + 2];
            float z1 = uvz[((i + 1)%numPoints)*3 + 2];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                xval[numxval] = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                zval[numxval++] = z0 + (z1 - z0)*k;

            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                float z0 = uvz[i*3 + 2];
                float z1 = uvz[((i + 1)%numPoints)*3 + 2];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                    xval[numxval] = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                    zval[numxval++] = z0 + (z1 - z0)*k;
                }
            }
        }
        if (numxval) {
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (i = 0; i < last; i++) {
                    if (xval[i] > xval[i + 1]) {
                        int xtmp;
                        float ztmp;
                        xtmp = xval[i];
                        xval[i] = xval[i + 1];
                        xval[i + 1] = xtmp;
                        ztmp = zval[i];
                        zval[i] = zval[i + 1];
                        zval[i + 1] = ztmp;
                        sorted = 0;
                    }
                }
                last--;
            } while (!sorted);
            for (i = 0; i < numxval; i += 2) {
                IMG_DrawHLineZBRW(dst, xval[i], xval[i + 1], y, zval[i], zval[i + 1], color, additive, zbuffer);
            }
        }
    }
}

/*
 * IMG_FillPolygonZBW
 * ------------------
 */
void IMG_FillPolygonZBW(Image *dst, int numPoints, int *points, float *uvz, unsigned int color, char additive, int *zbuffer) {
    int minY, maxY, minX, maxX;
    int i, y;
    int *p;
    int xval[16];
    float zval[16];
    int numxval;
    
    if (numPoints <= 0) return;
    if (ColorAlphaComponent(color) == 0) return;

    p = points;
    minY = *(p + 1);
    maxY = minY;
    minX = *p;
    maxX = minX;
    p += 2;
    for (i = 1; i < numPoints; i++, p += 2) {
        if (*(p + 1) < minY) minY = *(p + 1);
        if (*(p + 1) > maxY) maxY = *(p + 1);
        if (*p < minX) minX = *p;
        if (*p > maxX) maxX = *p;
    }
    if (maxX < dst->xMin || minX >= dst->xMax || maxY < dst->yMin || minY >= dst->yMax) return;

    if (minY < dst->yMin) minY = dst->yMin;
    if (maxY > dst->yMax) maxY = dst->yMax;

    for (y = minY; y <= maxY; y++) {
        numxval = 0;
        for (i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            float z0 = uvz[i*3 + 2];
            float z1 = uvz[((i + 1)%numPoints)*3 + 2];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                xval[numxval] = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                zval[numxval++] = z0 + (z1 - z0)*k;

            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                float z0 = uvz[i*3 + 2];
                float z1 = uvz[((i + 1)%numPoints)*3 + 2];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                    xval[numxval] = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                    zval[numxval++] = z0 + (z1 - z0)*k;
                }
            }
        }
        if (numxval) {
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (i = 0; i < last; i++) {
                    if (xval[i] > xval[i + 1]) {
                        int xtmp;
                        float ztmp;
                        xtmp = xval[i];
                        xval[i] = xval[i + 1];
                        xval[i + 1] = xtmp;
                        ztmp = zval[i];
                        zval[i] = zval[i + 1];
                        zval[i + 1] = ztmp;
                        sorted = 0;
                    }
                }
                last--;
            } while (!sorted);
            for (i = 0; i < numxval; i += 2) {
                IMG_DrawHLineZBW(dst, xval[i], xval[i + 1], y, zval[i], zval[i + 1], color, additive, zbuffer);
            }
        }
    }
}

/*
 * IMG_FillPolygonZBR
 * ------------------
 */
void IMG_FillPolygonZBR(Image *dst, int numPoints, int *points, float *uvz, unsigned int color, char additive, int *zbuffer) {
    int minY, maxY, minX, maxX;
    int i, y;
    int *p;
    int xval[16];
    float zval[16];
    int numxval;
    
    if (numPoints <= 0) return;
    if (ColorAlphaComponent(color) == 0) return;

    p = points;
    minY = *(p + 1);
    maxY = minY;
    minX = *p;
    maxX = minX;
    p += 2;
    for (i = 1; i < numPoints; i++, p += 2) {
        if (*(p + 1) < minY) minY = *(p + 1);
        if (*(p + 1) > maxY) maxY = *(p + 1);
        if (*p < minX) minX = *p;
        if (*p > maxX) maxX = *p;
    }
    if (maxX < dst->xMin || minX >= dst->xMax || maxY < dst->yMin || minY >= dst->yMax) return;

    if (minY < dst->yMin) minY = dst->yMin;
    if (maxY > dst->yMax) maxY = dst->yMax;

    for (y = minY; y <= maxY; y++) {
        numxval = 0;
        for (i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            float z0 = uvz[i*3 + 2];
            float z1 = uvz[((i + 1)%numPoints)*3 + 2];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                xval[numxval] = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                zval[numxval++] = z0 + (z1 - z0)*k;

            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                float z0 = uvz[i*3 + 2];
                float z1 = uvz[((i + 1)%numPoints)*3 + 2];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                    xval[numxval] = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                    zval[numxval++] = z0 + (z1 - z0)*k;
                }
            }
        }
        if (numxval) {
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (i = 0; i < last; i++) {
                    if (xval[i] > xval[i + 1]) {
                        int xtmp;
                        float ztmp;
                        xtmp = xval[i];
                        xval[i] = xval[i + 1];
                        xval[i + 1] = xtmp;
                        ztmp = zval[i];
                        zval[i] = zval[i + 1];
                        zval[i + 1] = ztmp;
                        sorted = 0;
                    }
                }
                last--;
            } while (!sorted);
            for (i = 0; i < numxval; i += 2) {
                IMG_DrawHLineZBR(dst, xval[i], xval[i + 1], y, zval[i], zval[i + 1], color, additive, zbuffer);
            }
        }
    }
}


/*
 * IMG_TexturePolygon
 * ------------------
 * points: [x0, y0, x1, y1 .. xn, yn]
 * uv: [u0, v0, u1, v1 .. un, vn]
 */
void IMG_TexturePolygon(Image *dst, int numPoints, int *points, float *uv, Image *src, unsigned int color, int useImageAlpha, char additive) {
    SLPoint xval[16];
    int numxval;
    int minx, miny, maxx, maxy;
    
    if (ColorAlphaComponent(color) == 0) return;
    
    minx = maxx = points[0];
    miny = maxy = points[1];
    for (int i = 1; i < numPoints; i++) {
        int *p = &points[i*2];
        if (p[0] < minx) minx = p[0];
        if (p[0] > maxx) maxx = p[0];
        if (p[1] < miny) miny = p[1];
        if (p[1] > maxy) maxy = p[1];
    }
    if (maxx < (float)dst->xMin || minx >= (float)dst->xMax || maxy < (float)dst->yMin || miny >= (float)dst->yMax) return;

    if (miny < dst->yMin) miny = dst->yMin;
    if (maxy > dst->yMax) maxy = dst->yMax;
    
    for (int y = miny; y <= maxy; y++) {
        numxval = 0;
        for (int i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            float *uv0 = &uv[i*2];
            float *uv1 = &uv[((i + 1)%numPoints)*2];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                xval[numxval].u = uv0[0] + (uv1[0] - uv0[0])*k;
                xval[numxval].v = uv0[1] + (uv1[1] - uv0[1])*k;
                numxval++;
            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                float *uv0 = &uv[i*2];
                float *uv1 = &uv[((i + 1)%numPoints)*2];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                    xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                    xval[numxval].u = uv0[0] + (uv1[0] - uv0[0])*k;
                    xval[numxval].v = uv0[1] + (uv1[1] - uv0[1])*k;
                    numxval++;
                }
            }
        }
        if (numxval) {
            // instead of sorting struct, sort an index array of the same size.
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (int i = 0; i < last; i++) {
                    if (xval[i].x > xval[i + 1].x) {
                        SLPoint tmp = xval[i];
                        sorted = 0;
                        xval[i] = xval[i + 1];
                        xval[i + 1] = tmp;
                    }
                }
                last--;
            } while (!sorted);
            for (int i = 0; i < numxval; i += 2) {
                IMG_DrawHRaster2(dst, src, y, xval[i].x, xval[i + 1].x, xval[i].u, xval[i].v, xval[i + 1].u, xval[i + 1].v, color, useImageAlpha, additive);
            }
        }
    }
}

/*
 * IMG_TexturePolygonZ
 * -------------------
 * points: [x0, y0, x1, y1 .. xn, yn]
 * uv: [u0, v0, z0, u1, v1, z1 .. un, vn, zn]
 */
void IMG_TexturePolygonZ(Image *dst, int numPoints, int *points, float *uvz, Image *src, unsigned int color, int useImageAlpha, char additive) {
    SLPoint xval[16];
    int numxval;
    int minx, miny, maxx, maxy;
    
    if (ColorAlphaComponent(color) == 0) return;
    
    minx = maxx = points[0];
    miny = maxy = points[1];
    for (int i = 1; i < numPoints; i++) {
        int *pi = &points[i*2];
        if (pi[0] < minx) minx = pi[0];
        if (pi[0] > maxx) maxx = pi[0];
        if (pi[1] < miny) miny = pi[1];
        if (pi[1] > maxy) maxy = pi[1];
    }
    if (maxx < (float)dst->xMin || minx >= (float)dst->xMax || maxy < (float)dst->yMin || miny >= (float)dst->yMax) return;
    
    if (miny < dst->yMin) miny = dst->yMin;
    if (maxy > dst->yMax) maxy = dst->yMax;
    
    for (int i = 0; i < numPoints; i++) {
        float *pf = &uvz[i*3];
        pf[2] = 1.0f/pf[2];
        pf[0] *= pf[2];
        pf[1] *= pf[2];
    }
    
    for (int y = miny; y <= maxy; y++) {
        numxval = 0;
        for (int i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            float *uvz0 = &uvz[i*3];
            float *uvz1 = &uvz[((i + 1)%numPoints)*3];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                xval[numxval].u = uvz0[0] + (uvz1[0] - uvz0[0])*k;
                xval[numxval].v = uvz0[1] + (uvz1[1] - uvz0[1])*k;
                xval[numxval].z = uvz0[2] + (uvz1[2] - uvz0[2])*k; 
                numxval++;
            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                float *uvz0 = &uvz[i*3];
                float *uvz1 = &uvz[((i + 1)%numPoints)*3];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                    xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                    xval[numxval].u = uvz0[0] + (uvz1[0] - uvz0[0])*k;
                    xval[numxval].v = uvz0[1] + (uvz1[1] - uvz0[1])*k;
                    xval[numxval].z = uvz0[2] + (uvz1[2] - uvz0[2])*k;
                    numxval++;
                }
            }
        }
        if (numxval) {
            // instead of sorting struct, sort an index array of the same size.
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (int i = 0; i < last; i++) {
                    if (xval[i].x > xval[i + 1].x) {
                        SLPoint tmp = xval[i];
                        sorted = 0;
                        xval[i] = xval[i + 1];
                        xval[i + 1] = tmp;
                    }
                }
                last--;
            } while (!sorted);
            for (int i = 0; i < numxval; i += 2) {
                IMG_DrawHRaster2Z(dst, src, y, xval[i].x, xval[i + 1].x,
                        xval[i].u, xval[i].v, xval[i].z,
                        xval[i + 1].u, xval[i + 1].v, xval[i + 1].z,
                        color, useImageAlpha, additive);

            }
        }
    }
}

void IMG_TexturePolygonZBRW(Image *dst, int numPoints, int *points, float *uvz, Image *src, unsigned int color, int useImageAlpha, char additive, int *zbuffer) {
    SLPoint xval[16];
    int numxval;
    int minx, miny, maxx, maxy;
    
    if (ColorAlphaComponent(color) == 0) return;
    
    minx = maxx = points[0];
    miny = maxy = points[1];
    for (int i = 1; i < numPoints; i++) {
        int *pi = &points[i*2];
        if (pi[0] < minx) minx = pi[0];
        if (pi[0] > maxx) maxx = pi[0];
        if (pi[1] < miny) miny = pi[1];
        if (pi[1] > maxy) maxy = pi[1];
    }
    if (maxx < (float)dst->xMin || minx >= (float)dst->xMax || maxy < (float)dst->yMin || miny >= (float)dst->yMax) return;
    
    if (miny < dst->yMin) miny = dst->yMin;
    if (maxy > dst->yMax) maxy = dst->yMax;
    
    for (int i = 0; i < numPoints; i++) {
        float *pf = &uvz[i*3];
        pf[2] = 1.0f/pf[2];
        pf[0] *= pf[2];
        pf[1] *= pf[2];
    }
    
    for (int y = miny; y <= maxy; y++) {
        numxval = 0;
        for (int i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            float *uvz0 = &uvz[i*3];
            float *uvz1 = &uvz[((i + 1)%numPoints)*3];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                xval[numxval].u = uvz0[0] + (uvz1[0] - uvz0[0])*k;
                xval[numxval].v = uvz0[1] + (uvz1[1] - uvz0[1])*k;
                xval[numxval].z = uvz0[2] + (uvz1[2] - uvz0[2])*k; 
                numxval++;
            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                float *uvz0 = &uvz[i*3];
                float *uvz1 = &uvz[((i + 1)%numPoints)*3];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                    xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                    xval[numxval].u = uvz0[0] + (uvz1[0] - uvz0[0])*k;
                    xval[numxval].v = uvz0[1] + (uvz1[1] - uvz0[1])*k;
                    xval[numxval].z = uvz0[2] + (uvz1[2] - uvz0[2])*k;
                    numxval++;
                }
            }
        }
        if (numxval) {
            // instead of sorting struct, sort an index array of the same size.
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (int i = 0; i < last; i++) {
                    if (xval[i].x > xval[i + 1].x) {
                        SLPoint tmp = xval[i];
                        sorted = 0;
                        xval[i] = xval[i + 1];
                        xval[i + 1] = tmp;
                    }
                }
                last--;
            } while (!sorted);
            for (int i = 0; i < numxval; i += 2) {
                IMG_DrawHRaster2ZBRW(dst, src, y, xval[i].x, xval[i + 1].x,
                        xval[i].u, xval[i].v, xval[i].z,
                        xval[i + 1].u, xval[i + 1].v, xval[i + 1].z,
                        color, useImageAlpha, additive, zbuffer);

            }
        }
    }
}

void IMG_TexturePolygonZBW(Image *dst, int numPoints, int *points, float *uvz, Image *src, unsigned int color, int useImageAlpha, char additive, int *zbuffer) {
    SLPoint xval[16];
    int numxval;
    int minx, miny, maxx, maxy;
    
    if (ColorAlphaComponent(color) == 0) return;
    
    minx = maxx = points[0];
    miny = maxy = points[1];
    for (int i = 1; i < numPoints; i++) {
        int *pi = &points[i*2];
        if (pi[0] < minx) minx = pi[0];
        if (pi[0] > maxx) maxx = pi[0];
        if (pi[1] < miny) miny = pi[1];
        if (pi[1] > maxy) maxy = pi[1];
    }
    if (maxx < (float)dst->xMin || minx >= (float)dst->xMax || maxy < (float)dst->yMin || miny >= (float)dst->yMax) return;
    
    if (miny < dst->yMin) miny = dst->yMin;
    if (maxy > dst->yMax) maxy = dst->yMax;
    
    for (int i = 0; i < numPoints; i++) {
        float *pf = &uvz[i*3];
        pf[2] = 1.0f/pf[2];
        pf[0] *= pf[2];
        pf[1] *= pf[2];
    }
    
    for (int y = miny; y <= maxy; y++) {
        numxval = 0;
        for (int i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            float *uvz0 = &uvz[i*3];
            float *uvz1 = &uvz[((i + 1)%numPoints)*3];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                xval[numxval].u = uvz0[0] + (uvz1[0] - uvz0[0])*k;
                xval[numxval].v = uvz0[1] + (uvz1[1] - uvz0[1])*k;
                xval[numxval].z = uvz0[2] + (uvz1[2] - uvz0[2])*k; 
                numxval++;
            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                float *uvz0 = &uvz[i*3];
                float *uvz1 = &uvz[((i + 1)%numPoints)*3];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                    xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                    xval[numxval].u = uvz0[0] + (uvz1[0] - uvz0[0])*k;
                    xval[numxval].v = uvz0[1] + (uvz1[1] - uvz0[1])*k;
                    xval[numxval].z = uvz0[2] + (uvz1[2] - uvz0[2])*k;
                    numxval++;
                }
            }
        }
        if (numxval) {
            // instead of sorting struct, sort an index array of the same size.
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (int i = 0; i < last; i++) {
                    if (xval[i].x > xval[i + 1].x) {
                        SLPoint tmp = xval[i];
                        sorted = 0;
                        xval[i] = xval[i + 1];
                        xval[i + 1] = tmp;
                    }
                }
                last--;
            } while (!sorted);
            for (int i = 0; i < numxval; i += 2) {
                IMG_DrawHRaster2ZBW(dst, src, y, xval[i].x, xval[i + 1].x,
                        xval[i].u, xval[i].v, xval[i].z,
                        xval[i + 1].u, xval[i + 1].v, xval[i + 1].z,
                        color, useImageAlpha, additive, zbuffer);

            }
        }
    }
}

void IMG_TexturePolygonZBR(Image *dst, int numPoints, int *points, float *uvz, Image *src, unsigned int color, int useImageAlpha, char additive, int *zbuffer) {
    SLPoint xval[16];
    int numxval;
    int minx, miny, maxx, maxy;
    
    if (ColorAlphaComponent(color) == 0) return;
    
    minx = maxx = points[0];
    miny = maxy = points[1];
    for (int i = 1; i < numPoints; i++) {
        int *pi = &points[i*2];
        if (pi[0] < minx) minx = pi[0];
        if (pi[0] > maxx) maxx = pi[0];
        if (pi[1] < miny) miny = pi[1];
        if (pi[1] > maxy) maxy = pi[1];
    }
    if (maxx < (float)dst->xMin || minx >= (float)dst->xMax || maxy < (float)dst->yMin || miny >= (float)dst->yMax) return;
    
    if (miny < dst->yMin) miny = dst->yMin;
    if (maxy > dst->yMax) maxy = dst->yMax;
    
    for (int i = 0; i < numPoints; i++) {
        float *pf = &uvz[i*3];
        pf[2] = 1.0f/pf[2];
        pf[0] *= pf[2];
        pf[1] *= pf[2];
    }
    
    for (int y = miny; y <= maxy; y++) {
        numxval = 0;
        for (int i = 0; i < numPoints; i++) {
            int *p0 = &points[i*2];
            int *p1 = &points[((i + 1)%numPoints)*2];
            float *uvz0 = &uvz[i*3];
            float *uvz1 = &uvz[((i + 1)%numPoints)*3];
            if ((p0[1] <= y && p1[1] > y) || (p0[1] > y && p1[1] <= y)) {
                float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                xval[numxval].u = uvz0[0] + (uvz1[0] - uvz0[0])*k;
                xval[numxval].v = uvz0[1] + (uvz1[1] - uvz0[1])*k;
                xval[numxval].z = uvz0[2] + (uvz1[2] - uvz0[2])*k; 
                numxval++;
            }
        }
        if (!numxval) {
            for (int i = 0; i < numPoints; i++) {
                int *p0 = &points[i*2];
                int *p1 = &points[((i + 1)%numPoints)*2];
                float *uvz0 = &uvz[i*3];
                float *uvz1 = &uvz[((i + 1)%numPoints)*3];
                if ((p0[1] < y && p1[1] >= y) || (p0[1] >= y && p1[1] < y)) {
                    float k = (float)(y - p0[1])/(float)(p1[1] - p0[1]);
                    xval[numxval].x = (int)roundf((float)p0[0] + (float)(p1[0] - p0[0])*k);
                    xval[numxval].u = uvz0[0] + (uvz1[0] - uvz0[0])*k;
                    xval[numxval].v = uvz0[1] + (uvz1[1] - uvz0[1])*k;
                    xval[numxval].z = uvz0[2] + (uvz1[2] - uvz0[2])*k;
                    numxval++;
                }
            }
        }
        if (numxval) {
            // instead of sorting struct, sort an index array of the same size.
            char sorted;
            int last = numxval - 1;
            do {
                sorted = 1;
                for (int i = 0; i < last; i++) {
                    if (xval[i].x > xval[i + 1].x) {
                        SLPoint tmp = xval[i];
                        sorted = 0;
                        xval[i] = xval[i + 1];
                        xval[i + 1] = tmp;
                    }
                }
                last--;
            } while (!sorted);
            for (int i = 0; i < numxval; i += 2) {
                IMG_DrawHRaster2ZBR(dst, src, y, xval[i].x, xval[i + 1].x,
                        xval[i].u, xval[i].v, xval[i].z,
                        xval[i + 1].u, xval[i + 1].v, xval[i + 1].z,
                        color, useImageAlpha, additive, zbuffer);

            }
        }
    }
}


/*
 * IMG_DrawPolygon
 * ---------------
 */
void IMG_DrawPolygon(Image *dst, int numPoints, int *points, unsigned int color, char additive) {
    if (numPoints <= 0) return;
    if (ColorAlphaComponent(color) == 0) return;
    
    for (int i = 0; i < numPoints; i++) {
        int *p0 = &points[i*2];
        int *p1 = &points[((i + 1)%numPoints)*2];
        IMG_DrawLine(dst, p0[0], p0[1], p1[0], p1[1], color, additive);
    }
}

/*
 * IMG_DrawEllipse
 * ---------------
 */
void IMG_DrawEllipse(Image *dst, int cx, int cy, int xr, int yr, unsigned int color, char additive) {
    if (xr == 0 || yr == 0) return;
    if (ColorAlphaComponent(color) == 0) return;
    if (xr < 0) xr = -xr;
    if (yr < 0) yr = -yr;

    if (cx - xr >= dst->xMax || cy - yr >= dst->yMax || cx + xr < dst->xMin || cy + yr < dst->yMin)
        return;

    int twoasqr = 2*xr*xr;
    int twobsqr = 2*yr*yr;

    int x = xr;
    int y = 0;
    int xc = yr*yr*(1 - 2*xr);
    int yc = xr*xr;
    int err = 0;
    int sx = twobsqr*xr;
    int sy = 0;
    while (sx >= sy) {
        IMG_DrawPixel(dst, cx + x, cy + y, color, additive);
        IMG_DrawPixel(dst, cx - x, cy + y, color, additive);
        IMG_DrawPixel(dst, cx - x, cy - y, color, additive);
        IMG_DrawPixel(dst, cx + x, cy - y, color, additive);
        y++;
        sy += twoasqr;
        err += yc;
        yc += twoasqr;
        if (2*err + xc > 0) {
            x--;
            sx -= twobsqr;
            err += xc;
            xc += twobsqr;
        }
    }

    x = 0;
    y = yr;
    xc = yr*yr;
    yc = xr*xr*(1 - 2*yr);
    err = 0;
    sx = 0;
    sy = twoasqr*yr;
    while (sx <= sy) {
        IMG_DrawPixel(dst, cx + x, cy + y, color, additive);
        IMG_DrawPixel(dst, cx - x, cy + y, color, additive);
        IMG_DrawPixel(dst, cx - x, cy - y, color, additive);
        IMG_DrawPixel(dst, cx + x, cy - y, color, additive);
        x++;
        sx += twobsqr;
        err += xc;
        xc += twobsqr;
        if (2*err + yc > 0) {
            y--;
            sy -= twoasqr;
            err += yc;
            yc += twoasqr;
        }
    }
}

/*
 * IMG_FillEllipse
 * ---------------
 */
void IMG_FillEllipse(Image *dst, int cx, int cy, int xr, int yr, unsigned int color, char additive) {
    if (xr == 0 || yr == 0) return;
    if (ColorAlphaComponent(color) == 0) return;
    if (xr < 0) xr = -xr;
    if (yr < 0) yr = -yr;

    if (cx - xr >= dst->xMax || cy - yr >= dst->yMax || cx + xr < dst->xMin || cy + yr < dst->yMin)
        return;


    int twoasqr = 2*xr*xr;
    int twobsqr = 2*yr*yr;

    int x = xr;
    int y = 0;
    int xc = yr*yr*(1 - 2*xr);
    int yc = xr*xr;
    int err = 0;
    int sx = twobsqr*xr;
    int sy = 0;
    while (sx >= sy) {
        IMG_FillRect(dst, cx - x, cy - y, x*2 + 1, 1, color, additive);
        if (y) IMG_FillRect(dst, cx - x, cy + y, x*2 + 1, 1, color, additive);

        y++;
        sy += twoasqr;
        err += yc;
        yc += twoasqr;
        if (2*err + xc > 0) {
            x--;
            sx -= twobsqr;
            err += xc;
            xc += twobsqr;
        }
    }
    int h = y - 1;

    x = 0;
    y = yr;
    xc = yr*yr;
    yc = xr*xr*(1 - 2*yr);
    err = 0;
    sx = 0;
    sy = twoasqr*yr;
    while (sx <= sy) {
        IMG_FillRect(dst, cx - x, cy - y, 1, y - h, color, additive);
        IMG_FillRect(dst, cx - x, cy + h + 1, 1, y - h, color, additive);
        if (x) {
            IMG_FillRect(dst, cx + x, cy - y, 1, y - h, color, additive);
            IMG_FillRect(dst, cx + x, cy + h + 1, 1, y - h, color, additive);
        }

        x++;
        sx += twobsqr;
        err += xc;
        xc += twobsqr;
        if (2*err + yc > 0) {
            y--;
            sy -= twoasqr;
            err += yc;
            yc += twoasqr;
        }
    }
}

void IMG_FillRec(Image *dst, int x, int y, unsigned int bg, unsigned int color) {
/*    int xs, xe;
    if (x < dst->xMin || x >= dst->xMax || y < dst->yMin || y >= dst->yMax || dst->buffer[y*dst->w + x] != bg) return;
    
    xs = x;
    xe = x;
    
    while (xstart >= dst->xMin && *CB_POS(dst, xstart, y) == bg) xstart--;
    xstart++;
    while (xend <= dst->xMax && *CB_POS(dst, xend, y) == bg) xend++;
    xend--;*/  
}

/*
 * IMG_Fill
 * --------
 */
void IMG_Fill(Image *dst, int x, int y, unsigned int color, char additive) {
/*    unsigned int bg;
    
    if (x < dst->xMin || x >= dst->xMax || y < dst->yMin || y >= dst->yMax) return;
    
    bg = dst->buffer[y*dst->w + x];
    
    IMG_FillRec(dst, x, y, bg, color);*/
    
}

/*
 * IMG_DrawHLine
 * -------------
 */
void IMG_DrawHLine(Image *dst, int xstart, int xend, int row, unsigned int color, char additive) {
    int w;
    unsigned char dstR, dstG, dstB, invA;
    unsigned char cR, cG, cB, cA;
    unsigned int *cb;
        
    if (row < dst->yMin || row >= dst->yMax) return;
    
    w = xend - xstart;
    if (w <= 0) return;
    if (xstart >= dst->xMax) return;
    if (xend < dst->xMin) return;
    
    if (xstart < dst->xMin) xstart = dst->xMin;
    if (xend >= dst->xMax) xend = dst->xMax - 1;

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    cb = &dst->buffer[row*dst->w + xstart];

    if (additive) {
        int r, g, b;
        if (cA < 128) {
            cR = (cR*cA) >> 7;
            cG = (cG*cA) >> 7;
            cB = (cB*cA) >> 7;
        }
        for (int i = xstart; i <= xend; i++, cb++) {
            ColorToRGBComponents(*cb, dstR, dstG, dstB);
            r = dstR + cR;
            g = dstG + cG;
            b = dstB + cB;
            *cb = ToRGB((r > 255 ? 255 : r), (g > 255 ? 255 : g), (b > 255 ? 255 : b));
        }
    }
    else {
        if (cA < 128) {
            invA = 128 - cA;
            for (int i = xstart; i <= xend; i++, cb++) {
                ColorToRGBComponents(*cb, dstR, dstG, dstB);
                dstR = (dstR*invA + cR*cA) >> 7;
                dstG = (dstG*invA + cG*cA) >> 7;
                dstB = (dstB*invA + cB*cA) >> 7;
                *cb = ToRGB(dstR, dstG, dstB);
            }
        }
        else {
            for (int i = xstart; i <= xend; i++, cb++) *cb = color;
        }
    }
}

/*
 * IMG_DrawHLineZBRW
 * -----------------
 */
void IMG_DrawHLineZBRW(Image *dst, int xstart, int xend, int row, float zstart, float zend, unsigned int color, char additive, int *zbuffer) {
    int w;
    unsigned char dstR, dstG, dstB, invA;
    unsigned char cR, cG, cB, cA;
    unsigned int *cb;
    float dz = 0;
        
    if (row < dst->yMin || row >= dst->yMax) return;
    
    w = xend - xstart;
    if (w <= 0) return;
    if (xstart >= dst->xMax) return;
    if (xend < dst->xMin) return;
    
    if (w > 0) dz = (zend - zstart)/(float)w;
    
    if (xstart < dst->xMin) {
        zstart += (dst->xMin - xstart)*dz;
        xstart = dst->xMin;
    }
    if (xend >= dst->xMax) {
        xend = dst->xMax - 1;
    }

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    cb = &dst->buffer[row*dst->w + xstart];
    zbuffer += row*dst->w + xstart;
    
    int zf = (int)(zstart*65536.0f);
    int dzf = ((int)(zend*65536.0f) - zf)/(w + 1);

    if (additive) {
        int r, g, b;
        if (cA < 128) {
            cR = (cR*cA) >> 7;
            cG = (cG*cA) >> 7;
            cB = (cB*cA) >> 7;
        }
        for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
            if (zf < *zbuffer) {
                ColorToRGBComponents(*cb, dstR, dstG, dstB);
                r = dstR + cR;
                g = dstG + cG;
                b = dstB + cB;
                *cb = ToRGB((r > 255 ? 255 : r), (g > 255 ? 255 : g), (b > 255 ? 255 : b));
                *zbuffer = zf;
            }
            zf += dzf;
        }
    }
    else {
        if (cA < 128) {
            invA = 128 - cA;
            for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
                if (zf < *zbuffer) {
                    ColorToRGBComponents(*cb, dstR, dstG, dstB);
                    dstR = (dstR*invA + cR*cA) >> 7;
                    dstG = (dstG*invA + cG*cA) >> 7;
                    dstB = (dstB*invA + cB*cA) >> 7;
                    *cb = ToRGB(dstR, dstG, dstB);
                    *zbuffer = zf;
                }
                zf += dzf;
            }
        }
        else {
            for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
                if (zf < *zbuffer) {
                    *cb = color;
                    *zbuffer = zf;
                }
                zf += dzf;
            }
        }
    }
}

/*
 * IMG_DrawHLineZBW
 * ----------------
 */
void IMG_DrawHLineZBW(Image *dst, int xstart, int xend, int row, float zstart, float zend, unsigned int color, char additive, int *zbuffer) {
    int w;
    unsigned char dstR, dstG, dstB, invA;
    unsigned char cR, cG, cB, cA;
    unsigned int *cb;
    float dz = 0;
        
    if (row < dst->yMin || row >= dst->yMax) return;
    
    w = xend - xstart;
    if (w <= 0) return;
    if (xstart >= dst->xMax) return;
    if (xend < dst->xMin) return;
    
    if (w > 0) dz = (zend - zstart)/(float)w;
    
    if (xstart < dst->xMin) {
        zstart += (dst->xMin - xstart)*dz;
        xstart = dst->xMin;
    }
    if (xend >= dst->xMax) {
        xend = dst->xMax - 1;
    }

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    cb = &dst->buffer[row*dst->w + xstart];
    zbuffer += row*dst->w + xstart;
    
    int zf = (int)(zstart*65536.0f);
    int dzf = ((int)(zend*65536.0f) - zf)/(w + 1);

    if (additive) {
        int r, g, b;
        if (cA < 128) {
            cR = (cR*cA) >> 7;
            cG = (cG*cA) >> 7;
            cB = (cB*cA) >> 7;
        }
        for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
            ColorToRGBComponents(*cb, dstR, dstG, dstB);
            r = dstR + cR;
            g = dstG + cG;
            b = dstB + cB;
            *cb = ToRGB((r > 255 ? 255 : r), (g > 255 ? 255 : g), (b > 255 ? 255 : b));
            *zbuffer = zf;
            zf += dzf;
        }
    }
    else {
        if (cA < 128) {
            invA = 128 - cA;
            for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
                ColorToRGBComponents(*cb, dstR, dstG, dstB);
                dstR = (dstR*invA + cR*cA) >> 7;
                dstG = (dstG*invA + cG*cA) >> 7;
                dstB = (dstB*invA + cB*cA) >> 7;
                *cb = ToRGB(dstR, dstG, dstB);
                *zbuffer = zf;
                zf += dzf;
            }
        }
        else {
            for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
                *cb = color;
                *zbuffer = zf;
                zf += dzf;
            }
        }
    }
}

/*
 * IMG_DrawHLineZBR
 * ----------------
 */
void IMG_DrawHLineZBR(Image *dst, int xstart, int xend, int row, float zstart, float zend, unsigned int color, char additive, int *zbuffer) {
    int w;
    unsigned char dstR, dstG, dstB, invA;
    unsigned char cR, cG, cB, cA;
    unsigned int *cb;
    float dz = 0;
        
    if (row < dst->yMin || row >= dst->yMax) return;
    
    w = xend - xstart;
    if (w <= 0) return;
    if (xstart >= dst->xMax) return;
    if (xend < dst->xMin) return;
    
    if (w > 0) dz = (zend - zstart)/(float)w;
    
    if (xstart < dst->xMin) {
        zstart += (dst->xMin - xstart)*dz;
        xstart = dst->xMin;
    }
    if (xend >= dst->xMax) {
        xend = dst->xMax - 1;
    }

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    cb = &dst->buffer[row*dst->w + xstart];
    zbuffer += row*dst->w + xstart;
    
    int zf = (int)(zstart*65536.0f);
    int dzf = ((int)(zend*65536.0f) - zf)/(w + 1);

    if (additive) {
        int r, g, b;
        if (cA < 128) {
            cR = (cR*cA) >> 7;
            cG = (cG*cA) >> 7;
            cB = (cB*cA) >> 7;
        }
        for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
            if (zf < *zbuffer) {
                ColorToRGBComponents(*cb, dstR, dstG, dstB);
                r = dstR + cR;
                g = dstG + cG;
                b = dstB + cB;
                *cb = ToRGB((r > 255 ? 255 : r), (g > 255 ? 255 : g), (b > 255 ? 255 : b));
            }
            zf += dzf;
        }
    }
    else {
        if (cA < 128) {
            invA = 128 - cA;
            for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
                if (zf < *zbuffer) {
                    ColorToRGBComponents(*cb, dstR, dstG, dstB);
                    dstR = (dstR*invA + cR*cA) >> 7;
                    dstG = (dstG*invA + cG*cA) >> 7;
                    dstB = (dstB*invA + cB*cA) >> 7;
                    *cb = ToRGB(dstR, dstG, dstB);
                }
                zf += dzf;
            }
        }
        else {
            for (int i = xstart; i <= xend; i++, cb++, zbuffer++) {
                if (zf < *zbuffer) {
                    *cb = color;
                }
                zf += dzf;
            }
        }
    }
}


/*
 * IMG_DrawVRaster
 * ---------------
 */
void IMG_DrawVRaster(Image *dst, Image *src,
        int x, int y0, int y1,
        float srcU, float srcV, float dstU, float dstV, unsigned int color) {
    
    float u, v;
    int ui, vi, dui, dvi, h;
    unsigned int *buffer;
    unsigned char cR, cG, cB, cA;
 
    /* Trivial clipping. */
    if (x < dst->xMin || x >= dst->xMax) return;
    if (y1 < y0) {
        int tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    if (y0 >= dst->yMax || y1 < dst->yMin) return;
    
    h = y1 - y0 + 1;
    
    srcU = srcU < 0.0f ? 0.0f: (srcU > 1.0f ? 1.0f : srcU);
    srcV = srcV < 0.0f ? 0.0f: (srcV > 1.0f ? 1.0f : srcV);
    dstU = dstU < 0.0f ? 0.0f: (dstU > 1.0f ? 1.0f : dstU);
    dstV = dstV < 0.0f ? 0.0f: (dstV > 1.0f ? 1.0f : dstV);
    
    /* Yeah, rather than adding code for crop/wrap in the loop, I just pretend
       that the last pixel is 1% smaller than the others ... */
    srcU *= (float)src->w - 0.01f;
    dstU *= (float)src->w - 0.01f;
    srcV *= (float)src->h - 0.01f;
    dstV *= (float)src->h - 0.01f;
    
    u = srcU;
    v = srcV;
    
    /* Clipping. */
    int step = 0;
    if (y0 < dst->yMin) {
        step = (dst->yMin - y0);
        y0 = dst->yMin;
    }
    if (y1 >= dst->yMax) y1 = dst->yMax - 1;

    /* Fog color. */
    ColorToRGBAComponents(color, cR, cG, cB, cA);
    
    /* Start position in destination buffer. */
    buffer = dst->buffer + y0*dst->w + x;
 
    ui = (int)(524288.0*(double)u);
    vi = (int)(524288.0*(double)v);
    dui = ((int)(524288.0*(double)dstU) - ui)/h;
    dvi = ((int)(524288.0*(double)dstV) - vi)/h;
    
    ui += dui*step;
    vi += dvi*step;
 
    /* In n6 I used fixed point maths for u and v, but idk if it's worth it. */
    if (src->hasAlpha) {
        if (cA == 0) {
            /* Src image has transparency, but no fog effect. */
            unsigned int c;
            unsigned char srcR, srcG, srcB, srcA, invSrcA;
            unsigned char dstR, dstG, dstB;
            if (dui == 0) {
                ui = ui >> 19;
                for (int y = y0; y <= y1; y++) {
                    c = src->buffer[(vi >> 19)*src->w + ui];
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA > 0) {
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        invSrcA = 128 - srcA;                
                        dstR = (unsigned char)((srcR*srcA + dstR*invSrcA) >> 7);
                        dstG = (unsigned char)((srcG*srcA + dstG*invSrcA) >> 7);
                        dstB = (unsigned char)((srcB*srcA + dstB*invSrcA) >> 7);
                        *buffer = ToRGB(dstR, dstG, dstB);
                    }
                    buffer += dst->w;
                    vi += dvi;
                }
            }
            else {
                for (int y = y0; y <= y1; y++) {
                    c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA > 0) {
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        invSrcA = 128 - srcA;                
                        dstR = (unsigned char)((srcR*srcA + dstR*invSrcA) >> 7);
                        dstG = (unsigned char)((srcG*srcA + dstG*invSrcA) >> 7);
                        dstB = (unsigned char)((srcB*srcA + dstB*invSrcA) >> 7);
                        *buffer = ToRGB(dstR, dstG, dstB);
                    }
                    buffer += dst->w;
                    ui += dui;
                    vi += dvi;
                }
            }
        }
        else {
            /* Src image has transparency, and there's a fog effect. So we blend
               the src color with the fog and the resulting color with dst. I
               THINK that's what makes sense ... n6 didn't support alpha in its
               raster commands, only color keys. */
            unsigned char invA = 128 - cA;
            int r = (int)cR*cA;
            int g = (int)cG*cA;
            int b = (int)cB*cA;
            unsigned int c;
            unsigned char srcR, srcG, srcB, srcA, invSrcA;
            unsigned char dstR, dstG, dstB;
            if (dui == 0) {
                ui = ui >> 19;
                for (int y = y0; y <= y1; y++) {
                    c = src->buffer[(vi >> 19)*src->w + ui];
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA > 0) {
                        invSrcA = 128 - srcA;
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        dstR = (dstR*invSrcA + ((r + srcR*invA) >> 7)*srcA) >> 7;
                        dstG = (dstG*invSrcA + ((g + srcG*invA) >> 7)*srcA) >> 7;
                        dstB = (dstB*invSrcA + ((b + srcB*invA) >> 7)*srcA) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                    }
                    buffer += dst->w;
                    vi += dvi;
                }
            }
            else {
                for (int y = y0; y <= y1; y++) {
                    c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA > 0) {
                        invSrcA = 128 - srcA;
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        dstR = (dstR*invSrcA + ((r + srcR*invA) >> 7)*srcA) >> 7;
                        dstG = (dstG*invSrcA + ((g + srcG*invA) >> 7)*srcA) >> 7;
                        dstB = (dstB*invSrcA + ((b + srcB*invA) >> 7)*srcA) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                    }
                    buffer += dst->w;
                    ui += dui;
                    vi += dvi;
                }
            }
        }
    }
    else {
        if (cA == 0) {
            /* Src image has no transparency, and there's no fog effect, just
               copy from src to dst. */
            if (dui == 0) {
                ui = ui >> 19;
                for (int y = y0; y <= y1; y++) {
                    *buffer = src->buffer[(vi >> 19)*src->w + ui];
                    buffer += dst->w;
                    vi += dvi;
                }
            }
            else {
                for (int y = y0; y <= y1; y++) {
                    *buffer = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                    buffer += dst->w;
                    ui += dui;
                    vi += dvi;
                }
            }
        }
        else {
            /* Src image has no transparency, but there's a fog effect. */
            unsigned char invA = 128 - cA;
            int r = (int)cR*cA;
            int g = (int)cG*cA;
            int b = (int)cB*cA;
            unsigned int c;
            unsigned char srcR, srcG, srcB;
            unsigned char dstR, dstG, dstB;
            if (dui == 0) {
                ui = ui >> 19;
                for (int y = y0; y <= y1; y++) {
                    c = src->buffer[(vi >> 19)*src->w + ui];
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    dstR = (unsigned char)((r + srcR*invA) >> 7);
                    dstG = (unsigned char)((g + srcG*invA) >> 7);
                    dstB = (unsigned char)((b + srcB*invA) >> 7);
                    *buffer = ToRGB(dstR, dstG, dstB);
                    buffer += dst->w;
                    vi += dvi;
                }
            }
            else {
                for (int y = y0; y <= y1; y++) {
                    c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    dstR = (unsigned char)((r + srcR*invA) >> 7);
                    dstG = (unsigned char)((g + srcG*invA) >> 7);
                    dstB = (unsigned char)((b + srcB*invA) >> 7);
                    *buffer = ToRGB(dstR, dstG, dstB);
                    buffer += dst->w;
                    ui += dui;
                    vi += dvi;
                }
            }
        }
    }
}

/*
 * IMG_DrawHRaster
 * ---------------
 */
void IMG_DrawHRaster(Image *dst, Image *src,
        int y, int x0, int x1,
        float srcU, float srcV, float dstU, float dstV, unsigned int color) {
    float u, v;
    int ui, vi, dui, dvi;
    unsigned int *buffer;
    unsigned char cR, cG, cB, cA;
    int w;
    
    /* Trivial clipping. */
    if (y < dst->yMin || y >= dst->yMax) return;
    if (x1 < x0) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (x0 >= dst->xMax || x1 < dst->xMin) return;
    
    srcU = srcU < 0.0f ? 0.0f: (srcU > 1.0f ? 1.0f : srcU);
    srcV = srcV < 0.0f ? 0.0f: (srcV > 1.0f ? 1.0f : srcV);
    dstU = dstU < 0.0f ? 0.0f: (dstU > 1.0f ? 1.0f : dstU);
    dstV = dstV < 0.0f ? 0.0f: (dstV > 1.0f ? 1.0f : dstV);
  
    /* Yeah, rather than adding code for crop/wrap in the loop, I just pretend
       that the last pixel is 1% smaller than the others ... */
    srcU *= (float)src->w - 0.01f;
    dstU *= (float)src->w - 0.01f;
    srcV *= (float)src->h - 0.01f;
    dstV *= (float)src->h - 0.01f;
    
    w = x1 - x0 + 1;
    u = srcU;
    v = srcV;
    
    /* Clipping. */
    int step = 0;
    if (x0 < dst->xMin) {
        step = (dst->xMin - x0);
        x0 = dst->xMin;
    }
    if (x1 >= dst->xMax) x1 = dst->xMax - 1;

    /* Fog color. */
    ColorToRGBAComponents(color, cR, cG, cB, cA);
    
    /* Start position in destination buffer. */
    buffer = dst->buffer + y*dst->w + x0;

    ui = (int)(524288.0*(double)u);
    vi = (int)(524288.0*(double)v);
    dui = ((int)((double)dstU*524288.0) - ui)/w;
    dvi = ((int)((double)dstV*524288.0) - vi)/w;
        
    ui += dui*step;
    vi += dvi*step;

    if (src->hasAlpha) {
        if (cA == 0) {
            /* Src image has transparency, but no fog effect. */
            unsigned int c;
            unsigned char srcR, srcG, srcB, srcA, invSrcA;
            unsigned char dstR, dstG, dstB;
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA > 0) {
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    invSrcA = 128 - srcA;                
                    dstR = (unsigned char)((srcR*srcA + dstR*invSrcA) >> 7);
                    dstG = (unsigned char)((srcG*srcA + dstG*invSrcA) >> 7);
                    dstB = (unsigned char)((srcB*srcA + dstB*invSrcA) >> 7);
                    *buffer = ToRGB(dstR, dstG, dstB);
                }
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
        else {
            /* Src image has transparency, and there's a fog effect. So we blend
               the src color with the fog and the resulting color with dst. I
               THINK that's what makes sense ... n6 didn't support alpha in its
               raster commands, only color keys. */
            unsigned char invA = 128 - cA;
            int r = (int)cR*cA;
            int g = (int)cG*cA;
            int b = (int)cB*cA;
            unsigned int c;
            unsigned char srcR, srcG, srcB, srcA, invSrcA;
            unsigned char dstR, dstG, dstB;
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA > 0) {
                    invSrcA = 128 - srcA;
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    dstR = (dstR*invSrcA + ((r + srcR*invA) >> 7)*srcA) >> 7;
                    dstG = (dstG*invSrcA + ((g + srcG*invA) >> 7)*srcA) >> 7;
                    dstB = (dstB*invSrcA + ((b + srcB*invA) >> 7)*srcA) >> 7;
                    *buffer = ToRGB(dstR, dstG, dstB);
                }
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
    }
    else {
        if (cA == 0) {
            /* Src image has no transparency, and there's no fog effect, just
               copy from src to dst. */
            for (int x = x0; x <= x1; x++) {
                *buffer = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
        else {
            /* Src image has no transparency, but there's a fog effect. */
            unsigned char invA = 128 - cA;
            int r = (int)cR*cA;
            int g = (int)cG*cA;
            int b = (int)cB*cA;
            unsigned int c;
            unsigned char srcR, srcG, srcB;
            unsigned char dstR, dstG, dstB;
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBComponents(c, srcR, srcG, srcB);
                dstR = (unsigned char)((r + srcR*invA) >> 7);
                dstG = (unsigned char)((g + srcG*invA) >> 7);
                dstB = (unsigned char)((b + srcB*invA) >> 7);
                *buffer = ToRGB(dstR, dstG, dstB);
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
    }
}

/*
 * IMG_DrawHRaster2
 * ----------------
 */
void IMG_DrawHRaster2(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float dstU, float dstV, unsigned int color, int useImageAlpha, char additive) {
    float u, v;
    int ui, vi, dui, dvi;
    unsigned int *buffer;
    unsigned char cR, cG, cB, cA;
    int hasColor, hasAlpha;
    int w;

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    if (cA == 0) return;
    
    /* Trivial clipping. */
    if (y < dst->yMin || y >= dst->yMax) return;
    if (x1 < x0) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;        
    }
    if (x0 >= dst->xMax || x1 < dst->xMin) return;
    
    u = srcU;
    v = srcV;
    w = x1 - x0 + 1;

    /* Clipping. */
    int step = 0;
    if (x0 < dst->xMin) {
        step = (dst->xMin - x0);
        x0 = dst->xMin;
    }
    if (x1 >= dst->xMax) x1 = dst->xMax - 1;

     /* Start position in destination buffer. */
    buffer = dst->buffer + y*dst->w + x0;

    ui = (int)(524288.0*(double)u);
    vi = (int)(524288.0*(double)v);
    dui = ((int)(524288.0*(double)dstU) - ui)/w;
    dvi = ((int)(524288.0*(double)dstV) - vi)/w;
    ui += dui*step;
    vi += dvi*step;
    
    hasColor = cR != 255 || cG != 255 || cB != 255;
    hasAlpha = cA != 128 || (src->hasAlpha && useImageAlpha);

    unsigned int c;
    unsigned char srcR, srcG, srcB, srcA, dstR, dstG, dstB, invA;
    int r, g, b, a;
    
    if (additive) {
        if (hasColor && hasAlpha) {
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    a = (srcA * cA) >> 7;                        
                    r = dstR + ((r*a) >> 7);
                    g = dstG + ((g*a) >> 7);
                    b = dstB + ((b*a) >> 7);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                }
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
        else if (hasAlpha) {
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    a = (srcA * cA) >> 7;
                    r = dstR + ((srcR*a) >> 7);
                    g = dstG + ((srcG*a) >> 7);
                    b = dstB + ((srcB*a) >> 7);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                }
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
        else if (hasColor) {
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = dstR + ((srcR * cR) >> 8);
                g = dstG + ((srcG * cG) >> 8);
                b = dstB + ((srcB * cB) >> 8);
                *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
        else {
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = dstR + srcR;
                g = dstG + srcG;
                b = dstB + srcB;
                *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
    }
    else {
        if (hasColor && hasAlpha) {
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    a = (srcA * cA) >> 7;
                    invA = 128 - a;
                    dstR = (dstR*invA + r*a) >> 7;
                    dstG = (dstG*invA + g*a) >> 7;
                    dstB = (dstB*invA + b*a) >> 7;
                    *buffer = ToRGB(dstR, dstG, dstB);
                }
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
        else if (hasAlpha) {
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    a = (srcA * cA) >> 7;
                    invA = 128 - a;
                    dstR = (dstR*invA + srcR*a) >> 7;
                    dstG = (dstG*invA + srcG*a) >> 7;
                    dstB = (dstB*invA + srcB*a) >> 7;
                    *buffer = ToRGB(dstR, dstG, dstB);
                }
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
        else if (hasColor) {
            for (int x = x0; x <= x1; x++) {
                c = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = (srcR * cR) >> 8;
                g = (srcG * cG) >> 8;
                b = (srcB * cB) >> 8;
                *buffer = ToRGB(r, g, b);
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
        else {
            for (int x = x0; x <= x1; x++) {
                *buffer = src->buffer[(vi >> 19)*src->w + (ui >> 19)];
                buffer++;
                ui += dui;
                vi += dvi;
            }
        }
    }
}

/*
 * IMG_DrawHRaster2Z
 * -----------------
 */

void IMG_DrawHRaster2Z(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float srcZ, float dstU, float dstV, float dstZ, unsigned int color, int useImageAlpha, char additive) {
    unsigned char cR, cG, cB, cA;
    int hasColor, hasAlpha;

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    if (cA == 0) return;

    /* Trivial clipping. */
    if (y < dst->yMin || y >= dst->yMax) return;
    if (x1 < x0) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;        
    }
    if (x0 >= dst->xMax || x1 < dst->xMin) return;

    hasColor = cR != 255 || cG != 255 || cB != 255;
    hasAlpha = cA != 128 || (src->hasAlpha && useImageAlpha);
    
    int w = x1 - x0;
    
    float u = srcU;
    float v = srcV;
    float z = srcZ;
    float du = 0, dv = 0, dz = 0;
    if (w > 0) {
        du = (dstU - srcU)/(float)w;
        dv = (dstV - srcV)/(float)w;
        dz = (dstZ - srcZ)/(float)w;
    }
    
    if (x0 < dst->xMin) {
        u += (dst->xMin - x0)*du;
        v += (dst->xMin - x0)*dv;
        z += (dst->xMin - x0)*dz;
        x0 = dst->xMin;
    }
    if (x1 >= dst->xMax) {
        dstZ -= (x1 - dst->xMax + 1)*dz;
        dstU -= (x1 - dst->xMax + 1)*du;
        dstV -= (x1 - dst->xMax + 1)*dv;
        x1 = dst->xMax - 1;
    }
    
    unsigned int *buffer = dst->buffer + y*dst->w + x0;

    /* Perspective correct works. */
    /*for (int x = x0; x <= x1; x++) {
        float zinv = 1.0f/z;
        int ui = (int)(u*zinv);
        int vi = (int)(v*zinv);
                
        *buffer = src->buffer[vi*src->w + ui];

        buffer++;
        u += du;
        v += dv;
        z += dz;
    }*/

    /* floats, no color effect. */
    /*int lerp = dst->w/16;
    float lerpf = (float)lerp;
    int divs = (x1 - x0)/lerp;
    float zinv = 1.0f/z;
    float uf = u*zinv;
    float vf = v*zinv;
    float duf = 0.0f, dvf = 0.0f;
    for (int i = 0; i < divs; i++) {
        z += dz*lerpf;
        u += du*lerpf;
        v += dv*lerpf;
        
        zinv = 1.0f/z;
        duf = (u*zinv - uf)/lerpf;
        dvf = (v*zinv - vf)/lerpf;
        for (int j = 0; j < lerp; j++) {
            int ui = (int)uf;
            int vi = (int)vf;
            *buffer = src->buffer[vi*src->w + ui];
            buffer++;
            uf += duf;
            vf += dvf;
        }
    }
    int rest = (x1 - x0)%lerp;
    zinv = 1.0f/dstZ;
    if (rest) {
        duf = (dstU*zinv - uf)/(float)rest;
        dvf = (dstV*zinv - vf)/(float)rest;
    }
    for (int j = 0; j <= rest; j++) {
        int ui = (int)uf;
        int vi = (int)vf;
        *buffer = src->buffer[vi*src->w + ui];
        buffer++;
        uf += duf;
        vf += dvf;
    }*/
    
    int lerp = dst->w/sPerspectiveDiv;
    float lerpf = (float)lerp;
    int divs = (x1 - x0)/lerp;
    float zinv = 1.0f/z;
    int uf = (int)(u*zinv*65536.0f);
    int vf = (int)(v*zinv*65536.0f);
    int duf = 0, dvf = 0;
    
    unsigned int c;
    unsigned char srcR, srcG, srcB, srcA, dstR, dstG, dstB, invA;
    int r, g, b, a;
    
    if (additive) {
        if (hasColor && hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8; g = (srcG * cG) >> 8; b = (srcB * cB) >> 8; a = (srcA * cA) >> 7;                        
                        r = dstR + ((r*a) >> 7); g = dstG + ((g*a) >> 7); b = dstB + ((b*a) >> 7);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    }
                    buffer++;
                    uf += duf;
                    vf += dvf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    r = (srcR * cR) >> 8; g = (srcG * cG) >> 8; b = (srcB * cB) >> 8; a = (srcA * cA) >> 7;                        
                    r = dstR + ((r*a) >> 7); g = dstG + ((g*a) >> 7); b = dstB + ((b*a) >> 7);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                }
                buffer++;
                uf += duf;
                vf += dvf;
            }
        }
        else if (hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        r = dstR + ((srcR*a) >> 7);
                        g = dstG + ((srcG*a) >> 7);
                        b = dstB + ((srcB*a) >> 7);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    }
                    buffer++;
                    uf += duf;
                    vf += dvf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    a = (srcA * cA) >> 7;
                    r = dstR + ((srcR*a) >> 7);
                    g = dstG + ((srcG*a) >> 7);
                    b = dstB + ((srcB*a) >> 7);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                }
                buffer++;
                uf += duf;
                vf += dvf;
            }
        }
        else if (hasColor) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = dstR + ((srcR * cR) >> 8);
                    g = dstG + ((srcG * cG) >> 8);
                    b = dstB + ((srcB * cB) >> 8);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    buffer++;
                    uf += duf;
                    vf += dvf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = dstR + ((srcR * cR) >> 8);
                g = dstG + ((srcG * cG) >> 8);
                b = dstB + ((srcB * cB) >> 8);
                *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                buffer++;
                uf += duf;
                vf += dvf;
            }
        }
        else {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = dstR + srcR;
                    g = dstG + srcG;
                    b = dstB + srcB;
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    buffer++;
                    uf += duf;
                    vf += dvf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = dstR + srcR;
                g = dstG + srcG;
                b = dstB + srcB;
                *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                buffer++;
                uf += duf;
                vf += dvf;
            }
        }
    }
    else {
        if (hasColor && hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8;
                        g = (srcG * cG) >> 8;
                        b = (srcB * cB) >> 8;
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + r*a) >> 7;
                        dstG = (dstG*invA + g*a) >> 7;
                        dstB = (dstB*invA + b*a) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                    }
                    buffer++;
                    uf += duf;
                    vf += dvf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    a = (srcA * cA) >> 7;
                    invA = 128 - a;
                    dstR = (dstR*invA + r*a) >> 7;
                    dstG = (dstG*invA + g*a) >> 7;
                    dstB = (dstB*invA + b*a) >> 7;
                    *buffer = ToRGB(dstR, dstG, dstB);
                }
                buffer++;
                uf += duf;
                vf += dvf;
            }
        }
        else if (hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + srcR*a) >> 7;
                        dstG = (dstG*invA + srcG*a) >> 7;
                        dstB = (dstB*invA + srcB*a) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                    }
                    buffer++;
                    uf += duf;
                    vf += dvf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    a = (srcA * cA) >> 7;
                    invA = 128 - a;
                    dstR = (dstR*invA + srcR*a) >> 7;
                    dstG = (dstG*invA + srcG*a) >> 7;
                    dstB = (dstB*invA + srcB*a) >> 7;
                    *buffer = ToRGB(dstR, dstG, dstB);
                }
                buffer++;
                uf += duf;
                vf += dvf;
            }
        }
        else if (hasColor) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    *buffer = ToRGB(r, g, b);
                    buffer++;
                    uf += duf;
                    vf += dvf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = (srcR * cR) >> 8;
                g = (srcG * cG) >> 8;
                b = (srcB * cB) >> 8;
                *buffer = ToRGB(r, g, b);
                buffer++;
                uf += duf;
                vf += dvf;
            }
        }
        else {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    *buffer = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    buffer++;
                    uf += duf;
                    vf += dvf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                *buffer = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                buffer++;
                uf += duf;
                vf += dvf;
            }
        }
    }
}

void IMG_DrawHRaster2ZBRW(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float srcZ, float dstU, float dstV, float dstZ, unsigned int color, int useImageAlpha, char additive, int *zbuffer) {
    unsigned char cR, cG, cB, cA;
    int hasColor, hasAlpha;

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    if (cA == 0) return;

    /* Trivial clipping. */
    if (y < dst->yMin || y >= dst->yMax) return;
    if (x1 < x0) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;        
    }
    if (x0 >= dst->xMax || x1 < dst->xMin) return;

    hasColor = cR != 255 || cG != 255 || cB != 255;
    hasAlpha = cA != 128 || (src->hasAlpha && useImageAlpha);
    
    int w = x1 - x0;
    
    float u = srcU;
    float v = srcV;
    float z = srcZ;
    float du = 0, dv = 0, dz = 0;
    if (w > 0) {
        du = (dstU - srcU)/(float)w;
        dv = (dstV - srcV)/(float)w;
        dz = (dstZ - srcZ)/(float)w;
    }
    
    if (x0 < dst->xMin) {
        u += (dst->xMin - x0)*du;
        v += (dst->xMin - x0)*dv;
        z += (dst->xMin - x0)*dz;
        x0 = dst->xMin;
    }
    if (x1 >= dst->xMax) {
        dstZ -= (x1 - dst->xMax + 1)*dz;
        dstU -= (x1 - dst->xMax + 1)*du;
        dstV -= (x1 - dst->xMax + 1)*dv;
        x1 = dst->xMax - 1;
    }
    
    unsigned int *buffer = dst->buffer + y*dst->w + x0;
    zbuffer += y*dst->w + x0;
    
    int lerp = dst->w/sPerspectiveDiv;
    float lerpf = (float)lerp;
    int divs = (x1 - x0)/lerp;
    float zinv = 1.0f/z;
    int uf = (int)(u*zinv*65536.0f);
    int vf = (int)(v*zinv*65536.0f);
    int zf = (int)(zinv*65536.0f);
    int duf = 0, dvf = 0, dzf = 0;
    
    unsigned int c;
    unsigned char srcR, srcG, srcB, srcA, dstR, dstG, dstB, invA;
    int r, g, b, a;
    
    if (additive) {
        if (hasColor && hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                        if (srcA != 0) {
                            r = (srcR * cR) >> 8; g = (srcG * cG) >> 8; b = (srcB * cB) >> 8; a = (srcA * cA) >> 7;                        
                            r = dstR + ((r*a) >> 7); g = dstG + ((g*a) >> 7); b = dstB + ((b*a) >> 7);
                            *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                            *zbuffer = zf;
                        }
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8; g = (srcG * cG) >> 8; b = (srcB * cB) >> 8; a = (srcA * cA) >> 7;                        
                        r = dstR + ((r*a) >> 7); g = dstG + ((g*a) >> 7); b = dstB + ((b*a) >> 7);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                        *zbuffer = zf;
                    }
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                        if (srcA != 0) {
                            a = (srcA * cA) >> 7;
                            r = dstR + ((srcR*a) >> 7);
                            g = dstG + ((srcG*a) >> 7);
                            b = dstB + ((srcB*a) >> 7);
                            *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                            *zbuffer = zf;
                        }
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        r = dstR + ((srcR*a) >> 7);
                        g = dstG + ((srcG*a) >> 7);
                        b = dstB + ((srcB*a) >> 7);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                        *zbuffer = zf;
                    }
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasColor) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBComponents(c, srcR, srcG, srcB);
                        r = dstR + ((srcR * cR) >> 8);
                        g = dstG + ((srcG * cG) >> 8);
                        b = dstB + ((srcB * cB) >> 8);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                        *zbuffer = zf;
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = dstR + ((srcR * cR) >> 8);
                    g = dstG + ((srcG * cG) >> 8);
                    b = dstB + ((srcB * cB) >> 8);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    *zbuffer = zf;
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBComponents(c, srcR, srcG, srcB);
                        r = dstR + srcR;
                        g = dstG + srcG;
                        b = dstB + srcB;
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                        *zbuffer = zf;
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = dstR + srcR;
                    g = dstG + srcG;
                    b = dstB + srcB;
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    *zbuffer = zf;
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
    }
    else {
        if (hasColor && hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                        if (srcA != 0) {
                            r = (srcR * cR) >> 8;
                            g = (srcG * cG) >> 8;
                            b = (srcB * cB) >> 8;
                            a = (srcA * cA) >> 7;
                            invA = 128 - a;
                            dstR = (dstR*invA + r*a) >> 7;
                            dstG = (dstG*invA + g*a) >> 7;
                            dstB = (dstB*invA + b*a) >> 7;
                            *buffer = ToRGB(dstR, dstG, dstB);
                            *zbuffer = zf;
                        }
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8;
                        g = (srcG * cG) >> 8;
                        b = (srcB * cB) >> 8;
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + r*a) >> 7;
                        dstG = (dstG*invA + g*a) >> 7;
                        dstB = (dstB*invA + b*a) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                        *zbuffer = zf;
                    }
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                        if (srcA != 0) {
                            a = (srcA * cA) >> 7;
                            invA = 128 - a;
                            dstR = (dstR*invA + srcR*a) >> 7;
                            dstG = (dstG*invA + srcG*a) >> 7;
                            dstB = (dstB*invA + srcB*a) >> 7;
                            *buffer = ToRGB(dstR, dstG, dstB);
                            *zbuffer = zf;
                        }
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + srcR*a) >> 7;
                        dstG = (dstG*invA + srcG*a) >> 7;
                        dstB = (dstB*invA + srcB*a) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                        *zbuffer = zf;
                    }
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasColor) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(c, srcR, srcG, srcB);
                        r = (srcR * cR) >> 8;
                        g = (srcG * cG) >> 8;
                        b = (srcB * cB) >> 8;
                        *buffer = ToRGB(r, g, b);
                        *zbuffer = zf;
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    *buffer = ToRGB(r, g, b);
                    *zbuffer = zf;
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        *buffer = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        *zbuffer = zf;
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    *buffer = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                     *zbuffer = zf;
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
    }
}

void IMG_DrawHRaster2ZBW(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float srcZ, float dstU, float dstV, float dstZ, unsigned int color, int useImageAlpha, char additive, int *zbuffer) {
    unsigned char cR, cG, cB, cA;
    int hasColor, hasAlpha;

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    if (cA == 0) return;

    /* Trivial clipping. */
    if (y < dst->yMin || y >= dst->yMax) return;
    if (x1 < x0) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;        
    }
    if (x0 >= dst->xMax || x1 < dst->xMin) return;

    hasColor = cR != 255 || cG != 255 || cB != 255;
    hasAlpha = cA != 128 || (src->hasAlpha && useImageAlpha);
    
    int w = x1 - x0;
    
    float u = srcU;
    float v = srcV;
    float z = srcZ;
    float du = 0, dv = 0, dz = 0;
    if (w > 0) {
        du = (dstU - srcU)/(float)w;
        dv = (dstV - srcV)/(float)w;
        dz = (dstZ - srcZ)/(float)w;
    }
    
    if (x0 < dst->xMin) {
        u += (dst->xMin - x0)*du;
        v += (dst->xMin - x0)*dv;
        z += (dst->xMin - x0)*dz;
        x0 = dst->xMin;
    }
    if (x1 >= dst->xMax) {
        dstZ -= (x1 - dst->xMax + 1)*dz;
        dstU -= (x1 - dst->xMax + 1)*du;
        dstV -= (x1 - dst->xMax + 1)*dv;
        x1 = dst->xMax - 1;
    }
    
    unsigned int *buffer = dst->buffer + y*dst->w + x0;
    zbuffer += y*dst->w + x0;
    
    int lerp = dst->w/sPerspectiveDiv;
    float lerpf = (float)lerp;
    int divs = (x1 - x0)/lerp;
    float zinv = 1.0f/z;
    int uf = (int)(u*zinv*65536.0f);
    int vf = (int)(v*zinv*65536.0f);
    int zf = (int)(zinv*65536.0f);
    int duf = 0, dvf = 0, dzf = 0;
    
    unsigned int c;
    unsigned char srcR, srcG, srcB, srcA, dstR, dstG, dstB, invA;
    int r, g, b, a;
    
    if (additive) {
        if (hasColor && hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8; g = (srcG * cG) >> 8; b = (srcB * cB) >> 8; a = (srcA * cA) >> 7;                        
                        r = dstR + ((r*a) >> 7); g = dstG + ((g*a) >> 7); b = dstB + ((b*a) >> 7);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                        *zbuffer = zf;
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    r = (srcR * cR) >> 8; g = (srcG * cG) >> 8; b = (srcB * cB) >> 8; a = (srcA * cA) >> 7;                        
                    r = dstR + ((r*a) >> 7); g = dstG + ((g*a) >> 7); b = dstB + ((b*a) >> 7);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    *zbuffer = zf;
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        r = dstR + ((srcR*a) >> 7);
                        g = dstG + ((srcG*a) >> 7);
                        b = dstB + ((srcB*a) >> 7);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                        *zbuffer = zf;
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    a = (srcA * cA) >> 7;
                    r = dstR + ((srcR*a) >> 7);
                    g = dstG + ((srcG*a) >> 7);
                    b = dstB + ((srcB*a) >> 7);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    *zbuffer = zf;
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasColor) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = dstR + ((srcR * cR) >> 8);
                    g = dstG + ((srcG * cG) >> 8);
                    b = dstB + ((srcB * cB) >> 8);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    *zbuffer = zf;
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = dstR + ((srcR * cR) >> 8);
                g = dstG + ((srcG * cG) >> 8);
                b = dstB + ((srcB * cB) >> 8);
                *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                *zbuffer = zf;
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = dstR + srcR;
                    g = dstG + srcG;
                    b = dstB + srcB;
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    *zbuffer = zf;
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = dstR + srcR;
                g = dstG + srcG;
                b = dstB + srcB;
                *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                *zbuffer = zf;
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
    }
    else {
        if (hasColor && hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8;
                        g = (srcG * cG) >> 8;
                        b = (srcB * cB) >> 8;
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + r*a) >> 7;
                        dstG = (dstG*invA + g*a) >> 7;
                        dstB = (dstB*invA + b*a) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                        *zbuffer = zf;
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    a = (srcA * cA) >> 7;
                    invA = 128 - a;
                    dstR = (dstR*invA + r*a) >> 7;
                    dstG = (dstG*invA + g*a) >> 7;
                    dstB = (dstB*invA + b*a) >> 7;
                    *buffer = ToRGB(dstR, dstG, dstB);
                    *zbuffer = zf;
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + srcR*a) >> 7;
                        dstG = (dstG*invA + srcG*a) >> 7;
                        dstB = (dstB*invA + srcB*a) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                        *zbuffer = zf;
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                if (srcA != 0) {
                    a = (srcA * cA) >> 7;
                    invA = 128 - a;
                    dstR = (dstR*invA + srcR*a) >> 7;
                    dstG = (dstG*invA + srcG*a) >> 7;
                    dstB = (dstB*invA + srcB*a) >> 7;
                    *buffer = ToRGB(dstR, dstG, dstB);
                    *zbuffer = zf;
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasColor) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    *buffer = ToRGB(r, g, b);
                    *zbuffer = zf;
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                ColorToRGBComponents(c, srcR, srcG, srcB);
                r = (srcR * cR) >> 8;
                g = (srcG * cG) >> 8;
                b = (srcB * cB) >> 8;
                *buffer = ToRGB(r, g, b);
                *zbuffer = zf;
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    *buffer = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    *zbuffer = zf;
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                *buffer = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                 *zbuffer = zf;
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
    }
}

void IMG_DrawHRaster2ZBR(Image *dst, Image *src, int y, int x0, int x1, float srcU, float srcV, float srcZ, float dstU, float dstV, float dstZ, unsigned int color, int useImageAlpha, char additive, int *zbuffer) {
    unsigned char cR, cG, cB, cA;
    int hasColor, hasAlpha;

    ColorToRGBAComponents(color, cR, cG, cB, cA);
    if (cA == 0) return;

    /* Trivial clipping. */
    if (y < dst->yMin || y >= dst->yMax) return;
    if (x1 < x0) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;        
    }
    if (x0 >= dst->xMax || x1 < dst->xMin) return;

    hasColor = cR != 255 || cG != 255 || cB != 255;
    hasAlpha = cA != 128 || (src->hasAlpha && useImageAlpha);
    
    int w = x1 - x0;
    
    float u = srcU;
    float v = srcV;
    float z = srcZ;
    float du = 0, dv = 0, dz = 0;
    if (w > 0) {
        du = (dstU - srcU)/(float)w;
        dv = (dstV - srcV)/(float)w;
        dz = (dstZ - srcZ)/(float)w;
    }
    
    if (x0 < dst->xMin) {
        u += (dst->xMin - x0)*du;
        v += (dst->xMin - x0)*dv;
        z += (dst->xMin - x0)*dz;
        x0 = dst->xMin;
    }
    if (x1 >= dst->xMax) {
        dstZ -= (x1 - dst->xMax + 1)*dz;
        dstU -= (x1 - dst->xMax + 1)*du;
        dstV -= (x1 - dst->xMax + 1)*dv;
        x1 = dst->xMax - 1;
    }
    
    unsigned int *buffer = dst->buffer + y*dst->w + x0;
    zbuffer += y*dst->w + x0;
    
    int lerp = dst->w/sPerspectiveDiv;
    float lerpf = (float)lerp;
    int divs = (x1 - x0)/lerp;
    float zinv = 1.0f/z;
    int uf = (int)(u*zinv*65536.0f);
    int vf = (int)(v*zinv*65536.0f);
    int zf = (int)(zinv*65536.0f);
    int duf = 0, dvf = 0, dzf = 0;
    
    unsigned int c;
    unsigned char srcR, srcG, srcB, srcA, dstR, dstG, dstB, invA;
    int r, g, b, a;
    
    if (additive) {
        if (hasColor && hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                        if (srcA != 0) {
                            r = (srcR * cR) >> 8; g = (srcG * cG) >> 8; b = (srcB * cB) >> 8; a = (srcA * cA) >> 7;                        
                            r = dstR + ((r*a) >> 7); g = dstG + ((g*a) >> 7); b = dstB + ((b*a) >> 7);
                            *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                        }
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8; g = (srcG * cG) >> 8; b = (srcB * cB) >> 8; a = (srcA * cA) >> 7;                        
                        r = dstR + ((r*a) >> 7); g = dstG + ((g*a) >> 7); b = dstB + ((b*a) >> 7);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    }
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                        if (srcA != 0) {
                            a = (srcA * cA) >> 7;
                            r = dstR + ((srcR*a) >> 7);
                            g = dstG + ((srcG*a) >> 7);
                            b = dstB + ((srcB*a) >> 7);
                            *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                        }
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        r = dstR + ((srcR*a) >> 7);
                        g = dstG + ((srcG*a) >> 7);
                        b = dstB + ((srcB*a) >> 7);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    }
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasColor) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBComponents(c, srcR, srcG, srcB);
                        r = dstR + ((srcR * cR) >> 8);
                        g = dstG + ((srcG * cG) >> 8);
                        b = dstB + ((srcB * cB) >> 8);
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = dstR + ((srcR * cR) >> 8);
                    g = dstG + ((srcG * cG) >> 8);
                    b = dstB + ((srcB * cB) >> 8);
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBComponents(c, srcR, srcG, srcB);
                        r = dstR + srcR;
                        g = dstG + srcG;
                        b = dstB + srcB;
                        *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = dstR + srcR;
                    g = dstG + srcG;
                    b = dstB + srcB;
                    *buffer = ToRGB((r > 255 ? 255: r), (g > 255 ? 255: g), (b > 255 ? 255: b));
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
    }
    else {
        if (hasColor && hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                        if (srcA != 0) {
                            r = (srcR * cR) >> 8;
                            g = (srcG * cG) >> 8;
                            b = (srcB * cB) >> 8;
                            a = (srcA * cA) >> 7;
                            invA = 128 - a;
                            dstR = (dstR*invA + r*a) >> 7;
                            dstG = (dstG*invA + g*a) >> 7;
                            dstB = (dstB*invA + b*a) >> 7;
                            *buffer = ToRGB(dstR, dstG, dstB);
                        }
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        r = (srcR * cR) >> 8;
                        g = (srcG * cG) >> 8;
                        b = (srcB * cB) >> 8;
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + r*a) >> 7;
                        dstG = (dstG*invA + g*a) >> 7;
                        dstB = (dstB*invA + b*a) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                    }
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasAlpha) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                        ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                        if (srcA != 0) {
                            a = (srcA * cA) >> 7;
                            invA = 128 - a;
                            dstR = (dstR*invA + srcR*a) >> 7;
                            dstG = (dstG*invA + srcG*a) >> 7;
                            dstB = (dstB*invA + srcB*a) >> 7;
                            *buffer = ToRGB(dstR, dstG, dstB);
                        }
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(*buffer, dstR, dstG, dstB);
                    ColorToRGBAComponents(c, srcR, srcG, srcB, srcA);
                    if (srcA != 0) {
                        a = (srcA * cA) >> 7;
                        invA = 128 - a;
                        dstR = (dstR*invA + srcR*a) >> 7;
                        dstG = (dstG*invA + srcG*a) >> 7;
                        dstB = (dstB*invA + srcB*a) >> 7;
                        *buffer = ToRGB(dstR, dstG, dstB);
                    }
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else if (hasColor) {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                        ColorToRGBComponents(c, srcR, srcG, srcB);
                        r = (srcR * cR) >> 8;
                        g = (srcG * cG) >> 8;
                        b = (srcB * cB) >> 8;
                        *buffer = ToRGB(r, g, b);
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    c = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    ColorToRGBComponents(c, srcR, srcG, srcB);
                    r = (srcR * cR) >> 8;
                    g = (srcG * cG) >> 8;
                    b = (srcB * cB) >> 8;
                    *buffer = ToRGB(r, g, b);
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
        else {
            for (int i = 0; i < divs; i++) {
                z += dz*lerpf;
                u += du*lerpf;
                v += dv*lerpf;
                zinv = 1.0f/z;
                duf = ((int)(u*zinv*65536.0f) - uf)/(lerp + 1);
                dvf = ((int)(v*zinv*65536.0f) - vf)/(lerp + 1);
                dzf = ((int)(zinv*65536.0f) - zf)/(lerp + 1);
                for (int j = 0; j < lerp; j++) {
                    if (zf < *zbuffer) {
                        *buffer = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                    }
                    buffer++;
                    zbuffer++;
                    uf += duf;
                    vf += dvf;
                    zf += dzf;
                }
            }
            int rest = (x1 - x0)%lerp;
            zinv = 1.0f/dstZ;
            if (rest) {
                duf = ((int)(dstU*zinv*65536.0f) - uf)/rest;
                dvf = ((int)(dstV*zinv*65536.0f) - vf)/rest;
                dzf = ((int)(zinv*65536.0f) - zf)/rest;
            }
            for (int j = 0; j <= rest; j++) {
                if (zf < *zbuffer) {
                    *buffer = src->buffer[(vf >> 16)*src->w + (uf >> 16)];
                }
                buffer++;
                zbuffer++;
                uf += duf;
                vf += dvf;
                zf += dzf;
            }
        }
    }
}

 
/*
 * IMG_Scroll
 * ----------
 */
void IMG_Scroll(Image *img, int stepX, int stepY) {
	if (stepX > 0) {
		if (stepY > 0) IMG_ScrollDownRight(img, stepX, stepY);
		else if (stepY < 0) IMG_ScrollUpRight(img, stepX, -stepY);
		else IMG_ScrollRight(img, stepX);
	}
	else if (stepX < 0) {
		if (stepY > 0) IMG_ScrollDownLeft(img, -stepX, stepY);
		else if (stepY < 0) IMG_ScrollUpLeft(img, -stepX, -stepY);
		else IMG_ScrollLeft(img, -stepX);
	}
	else {
		if (stepY > 0) IMG_ScrollDown(img, stepY);
		else if (stepY < 0) IMG_ScrollUp(img, -stepY);
	}
}

/*
 * IMG_ScrollUp
 * ------------
 */
void IMG_ScrollUp(Image *img, int step) {
	unsigned int *src;
	unsigned int *dst;
	int d;
	int i;
	
	d = img->w*(img->h - step);
	src = CB_POS(img, 0, step);
	dst = CB_POS(img, 0, 0);
	for (i = 0; i < d; i++, src++, dst++) *dst = *src;
}

/*
 * IMG_ScrollDown
 * --------------
 */
void IMG_ScrollDown(Image *img, int step) {
	unsigned int *src;
	unsigned int *dst;
	int i, d;
	
	src = CB_POS(img, img->w - 1, img->h - step - 1);
	dst = CB_POS(img, img->w - 1, img->h - 1);
	d = img->w*(img->h - step);
	for (i = 0; i < d; i++, src--, dst--) *dst = *src;	
}

/*
 * IMG_ScrollLeft
 * --------------
 */
void IMG_ScrollLeft(Image *img, int step) {
	unsigned int *src;
	unsigned int *dst;
	int x, y;
	
	src = CB_POS(img, step, 0);
	dst = CB_POS(img, 0, 0);
	for (y = 0; y < img->h; y++, src += step, dst += step) 
		for (x = 0; x < img->w - step; x++, src++, dst++) *dst = *src;
}

/*
 * IMG_ScrollRight
 * ---------------
 */
void IMG_ScrollRight(Image *img, int step) {
	unsigned int *src;
	unsigned int *dst;
	int x, y, d;
	
	src = CB_POS(img, img->w - step - 1, 0);
	dst = CB_POS(img, img->w - 1, 0);
	d = img->w*2 - step;
	for (y = 0; y < img->h; y++, src += d, dst += d)
		for (x = 0; x < img->w - step; x++, src--, dst--) *dst = *src;
}

/*
 * IMG_ScrollUpLeft
 * ----------------
 */
 void IMG_ScrollUpLeft(Image *img, int stepX, int stepY) {
	unsigned int *src;
	unsigned int *dst;
	int x, y, w, h;
	
	src = CB_POS(img, stepX, stepY);
	dst = CB_POS(img, 0, 0);
	
	h = img->h - stepY;
	w = img->w - stepX;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++, src++, dst++) *dst = *src;
		src  = src - w + img->w;
		dst = dst - w + img->w;
	}
}

/*
 * IMG_ScrollUpRight
 * -----------------
 */
void IMG_ScrollUpRight(Image *img, int stepX, int stepY) {
	unsigned int *src;
	unsigned int *dst;
	int x, y, w, h;

	src = CB_POS(img, img->w - stepX - 1, stepY);
	dst = CB_POS(img, img->w - 1, 0);
	w = img->w - stepX;
	h = img->h - stepY;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++, src--, dst--) *dst = *src;
		src = src + w + img->w;
		dst = dst + w + img->w;
	}
}

/*
 * IMG_ScrollDownLeft
 * ------------------
 */
void IMG_ScrollDownLeft(Image *img, int stepX, int stepY) {
	unsigned int *src;
	unsigned int *dst;
	int x, y, w, h;
	
	src = CB_POS(img, stepX, img->h - stepY - 1);
	dst = CB_POS(img, 0, img->h - 1);
	w = img->w - stepX;
	h = img->h - stepY;
	
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++, src++, dst++) *dst = *src;
		src = src - w - img->w;
		dst = dst - w - img->w;
	}
	
}

/*
 * IMG_ScrollDownRight
 * -------------------
 */
void IMG_ScrollDownRight(Image *img, int stepX, int stepY) {
	unsigned int *src;
	unsigned int *dst;
	int x, y, w, h;
	
	
	src = CB_POS(img, img->w - stepX - 1, img->h - stepY - 1);
	dst = CB_POS(img, img->w - 1, img->h - 1);
	w = img->w - stepX;
	h = img->h - stepY;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++, src--, dst--) *dst = *src;
		src = src + w - img->w;
		dst = dst + w - img->w;
	}
}

void IMG_UpdateAlphaInfo(Image *img) {
    int rowh = img->h/img->rows;
    int colw = img->w/img->cols;

    for (int row = 0; row < img->rows; row++) {
        for (int col = 0; col < img->cols; col++) {
            unsigned int *cb = img->buffer + row*rowh*img->w + col*colw;
            char hasAlpha = 0;
            for (int y = 0; y < rowh && !hasAlpha; y++, cb += img->w) {
                for (int x = 0; x < colw && !hasAlpha; x++) {
                    hasAlpha = ColorAlphaComponent(cb[x]) < 128;
                }
            }
            if (hasAlpha) {
                img->hasAlpha = 1;
                img->cellInfo[row*img->cols + col].hasAlpha = 1;
            }
            else {
                img->cellInfo[row*img->cols + col].hasAlpha = 0;
            }
        }
    }
}

void IMG_BufferChanged(Image *img) {
    if (img->hasColorKey) {
        for (int i = 0; i < img->w*img->h; i++) {
            if (img->buffer[i] == img->colorKey) img->buffer[i] = 0x0;
        }
    }
    IMG_UpdateAlphaInfo(img);
}
