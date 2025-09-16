/*
 * naalaa_font.c
 * -------------
 *
 * By: Marcus 2021
 */
 
#include "naalaa_font.h"

#include "stdio.h"
#include "windows.h"

#define FONT_BORDER_ADD 4

/*
 * CalculateAlpha3X
 * ----------------
 */
static unsigned char CalculateAlpha3X(unsigned char A, unsigned char B, unsigned char C,
                                      unsigned char D, unsigned char E, unsigned char F,
                                      unsigned char G, unsigned char H, unsigned char I);

/*
 * BF_Create
 * ---------
 */
BitmapFont *BF_Create(const char *name, int size, int bold, int italic, int underline, int smooth) {
    BitmapFont *bf;
    HDC hdc = NULL;
    HBITMAP bmp = NULL;
    HBITMAP OldBmp = NULL;
    
    LPBITMAPINFO lpbi = NULL;
    LPVOID lpvBits = NULL;
    HANDLE BmpFile = INVALID_HANDLE_VALUE;
    HFONT hFont;
    LOGFONT lf;
    TEXTMETRIC tm;
    
    ABC *abc;
    
    int width, height;
        
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = size; 
    if (bold) lf.lfWeight = FW_BOLD;
    else    lf.lfWeight = FW_NORMAL; 
    if (italic) lf.lfItalic = TRUE;
    else    lf.lfItalic = FALSE;
    if (underline)    lf.lfUnderline = TRUE;
    else lf.lfUnderline = FALSE;
    lf.lfCharSet = ANSI_CHARSET; 
    lf.lfOutPrecision = OUT_TT_PRECIS; 
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS; 
    lf.lfQuality = NONANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = FF_DONTCARE;
    strcpy(lf.lfFaceName, name);
     
    hFont = CreateFontIndirect(&lf);
    hdc = CreateCompatibleDC(0);
    
    SelectObject(hdc, hFont);

    abc = (ABC *)malloc(sizeof(ABC)*224);
    GetTextMetrics(hdc, &tm);
    if (!GetCharABCWidths(hdc, 32, 255, abc)) {
        free(abc);
        return 0;
    }
        
    SelectObject(hdc, hFont);

    width = 16*(tm.tmMaxCharWidth + FONT_BORDER_ADD*2);
    height = 14*(tm.tmHeight + FONT_BORDER_ADD*2);
        
    bmp = CreateCompatibleBitmap(hdc, width, height);
    if (!bmp) {
        if (hdc) DeleteDC(hdc);
        free(abc);
        return 0;
    }
        
    OldBmp = (HBITMAP)SelectObject(hdc, bmp);
    if (!OldBmp) {
        if (hdc) DeleteDC(hdc);
        if (bmp) DeleteObject(bmp);
        free(abc);
        return 0;
    }

    SelectObject(hdc, GetStockObject(BLACK_BRUSH));
    Rectangle(hdc, 0, 0, width, height);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    unsigned char c;
    int x, y;
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 16; x++) {
            c = (unsigned char)(y*16+x+32);
            TextOut(hdc, x*(tm.tmMaxCharWidth + FONT_BORDER_ADD*2)-abc[c - 32].abcA + FONT_BORDER_ADD, y*(tm.tmHeight + FONT_BORDER_ADD*2) + FONT_BORDER_ADD, (const char*)&c, 1);
        }
    }

    lpbi = (LPBITMAPINFO)malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    ZeroMemory(&lpbi->bmiHeader, sizeof(BITMAPINFOHEADER));
    lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        
    SelectObject(hdc, OldBmp);
    if (!GetDIBits(hdc, bmp, 0, height, NULL, lpbi, DIB_RGB_COLORS)) {
        if (hdc) DeleteDC(hdc);
        if (bmp) DeleteObject(bmp);
        if (lpbi) free(lpbi);
        free(abc);
        return 0;
    }

    lpvBits = malloc(lpbi->bmiHeader.biSizeImage);
    if (!GetDIBits(hdc, bmp, 0, height, lpvBits, lpbi, DIB_RGB_COLORS)) {
        if (hdc) DeleteDC(hdc);
        if (bmp) DeleteObject(bmp);
        if (lpbi) free(lpbi);
        if (lpvBits) free(lpvBits);
        free(abc);
        return 0;
    }
    
    bf = (BitmapFont *)malloc(sizeof(BitmapFont));
    bf->image = IMG_Create(width, height, 0x00ffffff);
    bf->height = height/14 - FONT_BORDER_ADD*2;    
    bf->abc = (BMABC *)malloc(sizeof(BMABC)*224);
    
    for (int i = 0; i < 224; i++) {
        bf->abc[i].a = abc[i].abcA;
        bf->abc[i].b = abc[i].abcB;
        bf->abc[i].c = abc[i].abcC;
    }
    free(abc);

    BYTE *dword = (BYTE *)lpvBits;
    int ispix;
    int bitcounter = 0;
    for (y = 0; y < height; y++) {
        dword = (BYTE *)lpvBits + y*lpbi->bmiHeader.biSizeImage/height;
        for (x = 0; x < width; x++) {
            switch (bitcounter) {
                case 0: ispix = (*dword) & 128; break;
                case 1: ispix = (*dword) & 64; break;
                case 2: ispix = (*dword) & 32; break;
                case 3: ispix = (*dword) & 16; break;
                case 4: ispix = (*dword) & 8; break;
                case 5: ispix = (*dword) & 4; break;
                case 6: ispix = (*dword) & 2; break;
                case 7: ispix = (*dword) & 1; break;
            }
            if (ispix) IMG_SetPixel(bf->image, x, height - y - 1, ToRGB(255, 255, 255));
            bitcounter++;
            if (bitcounter == 8) {
                bitcounter = 0;
                dword++;
            }
        }
        bitcounter = 0;
    }
    
    if (smooth) BF_ApplySmoothing(bf);
    
    if (hdc) DeleteDC(hdc);
    if (bmp) DeleteObject(bmp);
    if (lpbi) free(lpbi);
    if (lpvBits) free(lpvBits);
    if (BmpFile != INVALID_HANDLE_VALUE) CloseHandle(BmpFile);
 
    return bf;
}

/*
 * BF_Load
 * -------
 * Ugh, using the old n6 format after all.
 */
BitmapFont *BF_Load(const char *name) {
    BitmapFont *bf = 0;
    FILE *file;
    char *filename;
    size_t len;
    
    len = strlen(name);
    filename = (char *)malloc(sizeof(char)*(len + 5));
    memcpy(filename, name, len + 1);
    strcat(filename, ".txt");
    
    file = fopen(filename, "r");
    
    if (file) {
        int success = 1;
        bf = (BitmapFont *)malloc(sizeof(BitmapFont));
        bf->image = 0;        
        /* txt. */
        bf->abc = (BMABC *)malloc(sizeof(BMABC)*224);
        for (int i = 0; i < 224; i++) {
            success &= fscanf(file, "%d%d%d", &bf->abc[i].a, &bf->abc[i].b, &bf->abc[i].c) == 3;
        }
        success &= fscanf(file, "%d", &bf->height) == 1;
        fclose(file);
        if (success) {
            /* png. */
            memcpy(filename, name, len + 1);
            strcat(filename, ".png");
            success &= (bf->image = IMG_Load(filename)) != 0;
        }
        if (!success) {
            free(bf->abc);
            free(bf);
            bf = 0;
        }
    }
    
    free(filename);
    
    return bf;
}

/*
 * BF_Save
 * -------
 * Ugh, using the old n6 format after all.
 */
int BF_Save(BitmapFont *bf, const char *name) {
 	FILE *file;
    char *filename;
    size_t len;
    int success = 0;
	
	if (!bf) return 0;
    
    len = strlen(name);
    filename = (char *)malloc(sizeof(char)*(len + 5));
    memcpy(filename, name, len + 1);
    strcat(filename, ".txt");	
	file = fopen(filename, "w");
    /* txt. */
    if (file) {
        for (int i = 0; i < 224; i++) {
            fprintf(file, "%d %d %d\n", bf->abc[i].a, bf->abc[i].b, bf->abc[i].c);
        }
        fprintf(file, "%d\n", bf->height);	
        fclose(file);
        /* png. */
        memcpy(filename, name, len + 1);
        strcat(filename, ".png");
        IMG_Save(bf->image, filename);
        success = 1;
    }

    free(filename);
    return success;
}

/*
 * BF_CreateEmpty
 * --------------
 */
BitmapFont *BF_CreateEmpty(Image *image) {
    BitmapFont *bf = (BitmapFont *)malloc(sizeof(BitmapFont));
    int w;

    bf->image = image;
    bf->height = bf->image->h/14 - FONT_BORDER_ADD*2;
    w = bf->image->w/16 - FONT_BORDER_ADD*2;
    bf->abc = (BMABC *)malloc(sizeof(BMABC)*224);
    for (int i = 0; i < 224; i++) {
        bf->abc[i].a = 0;
        bf->abc[i].b = w;
        bf->abc[i].c = 0;
    }
    
    return bf;
}

/*
 * BF_SetABC
 * ---------
 */
void BF_SetABC(BitmapFont *bf, int index, int a, int b, int c) {
    bf->abc[index].a = a;
    bf->abc[index].b = b;
    bf->abc[index].c = c;
}

/*
 * BF_SetABCv
 * ----------
 */
void BF_SetABCv(BitmapFont *bf, int *values) {
    for (int i = 0; i < 224; i++) {
        bf->abc[i].a = values[i*3 + 0];
        bf->abc[i].b = values[i*3 + 1];
        bf->abc[i].c = values[i*3 + 2];
    }
}

/*
 * BF_Free
 * -------
 */
void BF_Free(BitmapFont *bf) {
    IMG_Free(bf->image);
    free(bf->abc);
    free(bf);
}


/*
 * BF_ApplySmoothing
 * -----------------
 */
void BF_ApplySmoothing(BitmapFont *bf) {
    int width = bf->image->w;
    int height = bf->image->h;
    unsigned int *buffer = (unsigned int *)malloc(sizeof(unsigned int)*width*height);

    for (int i = 0; i < width*height; i++) buffer[i] = bf->image->buffer[i];
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            unsigned char alpha = CalculateAlpha3X(ColorAlphaComponent(bf->image->buffer[(y - 1)*width + x - 1]),
                                                   ColorAlphaComponent(bf->image->buffer[(y - 1)*width + x]),
                                                   ColorAlphaComponent(bf->image->buffer[(y - 1)*width + x + 1]),
                                                   ColorAlphaComponent(bf->image->buffer[y*width + x - 1]),
                                                   ColorAlphaComponent(bf->image->buffer[y*width + x]),
                                                   ColorAlphaComponent(bf->image->buffer[y*width + x + 1]),
                                                   ColorAlphaComponent(bf->image->buffer[(y + 1)*width + x - 1]),
                                                   ColorAlphaComponent(bf->image->buffer[(y + 1)*width + x]),
                                                   ColorAlphaComponent(bf->image->buffer[(y + 1)*width + x + 1]));
            buffer[y*width + x] = ToRGBA(255, 255, 255, alpha);
        }
    }
    free(bf->image->buffer);
    bf->image->buffer = buffer;
}

/*
 * BF_Write
 * --------
 */
void BF_Write(BitmapFont *bf, Image *dst, const char *text, int *xRef, int *yRef, unsigned int color, char additive) {
    const char *c;
    int col, row;
    unsigned char ch;
    int celw, celh;
    int x = *xRef, y = *yRef;

    if (!bf) return;
    
    celw = bf->image->w/16;
    celh = bf->image->h/14;
    
    c = text;
    while (*c != '\0') {
        if (*c == '\r') {
            x = *xRef;
            y += bf->height;
        }
        else {
            ch = *(unsigned char*)(c);
            ch -= 32;
            if (ch >= 0 && ch < 224) {
                x += bf->abc[ch].a;
                row = ch >> 4;
                col = ch - (row << 4);
                IMG_DrawImage(dst, x - FONT_BORDER_ADD, y - FONT_BORDER_ADD, bf->image, col*celw, row*celh, celw, celh, color, 1, additive);
                x += bf->abc[ch].b + bf->abc[ch].c;
            }
        }
        c++;
    }
    *xRef = x;
    *yRef = y;
}

/*
 * BF_Width
 * --------
 */
int BF_Width(BitmapFont *bf, const char *text) {
    const char *c;
    unsigned char ch;
    int w;

    if (!bf) return 0;
    
    w = 0;
    c = text;
    while (*c != '\0') {
        ch = *(unsigned char*)(c);//*c;
        ch -= 32;
        if (ch >= 0 && ch < 224) {
            w += bf->abc[ch].a + bf->abc[ch].b + bf->abc[ch].c;
        }
        c++;
    }
    return w;
}

/*
 * CalculateAlpha3X
 * ----------------
 * Use the Scale3X zoom algorithm and weighted average to calculate a new alpha
 * value for E.
 */
static unsigned char CalculateAlpha3X(unsigned char A, unsigned char B, unsigned char C,
                                      unsigned char D, unsigned char E, unsigned char F,
                                      unsigned char G, unsigned char H, unsigned char I) {
    int E0, E1, E2, E3, E4, E5, E6, E7, E8;

    if (B != H && D != F) {
        E0 = D == B ? D : E;
        E1 = (D == B && E != C) || (B == F && E != A) ? B : E;
        E2 = B == F ? F : E;
        E3 = (D == B && E != G) || (D == H && E != A) ? D : E;
        E4 = E;
        E5 = (B == F && E != I) || (H == F && E != C) ? F : E;
        E6 = D == H ? D : E;
        E7 = (D == H && E != I) || (H == F && E != G) ? H : E;
        E8 = H == F ? F : E;
    }
    else {
        E0 = E;
        E1 = E;
        E2 = E;
        E3 = E;
        E4 = E;
        E5 = E;
        E6 = E;
        E7 = E;
        E8 = E;
    }
    
    return (unsigned char)((E0*1 + E1*2 + E2*1 + E3*2 + E4*4 + E5*2 + E6*1 + E7*2 + E8*1)/16);
}
