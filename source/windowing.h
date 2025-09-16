/*
 * windowing.h
 * -----------
 * Windowing and drawing.
 *
 * By: Marcus 2021
 */

#ifndef __WINDOWING_H__
#define __WINDOWING_H__

#define WIN_SUCCESS     0
#define WIN_ERROR       1
#define WIN_FATAL_ERROR 2

/*
 * WIN_Init
 * --------
 * Called when program starts.
 */
void WIN_Init();

/*
 * WIN_Set
 * -------
 * Create window.
 */
int WIN_Set(const char *title, int width, int height, int fullScreen, int scaleFactor, int minW, int minH);

/*
 * Win_ShowConsole
 * ---------------
 * Show/hide console.
 */
void WIN_ShowConsole(int show);

/*
 * WIN_HasWindow
 * -------------
 */
int WIN_HasWindow();

/*
 * WIN_SetAutoRedraw
 * -----------------
 */
void WIN_SetAutoRedraw(int value);

/*
 * WIN_AutoRedraw
 * --------------
 */
int WIN_AutoRedraw();

/*
 * WIN_Close
 * ---------
 * Close window, called when program terminates.
 */
void WIN_Close();

/*
 * WIN_Update
 * ----------
 * Handle messages.
 */
void WIN_Update();

/*
 * WIN_Redraw
 * ----------
 * Update window content.
 */
void WIN_Redraw();

/*
 * WIN_Active
 * ----------
 * Return 1 if window has focus.
 */
int WIN_Active();

/*
 * WIN_Exists
 * ----------
 * Return 1 if an n7 window with the specified title exists.
 */
int WIN_Exists(const char *title);

/*
 * WIN_SendMessage
 * ---------------
 * Send message to a window.
 */
void WIN_SendMessage(const char *title, char *message);

/*
 * WIN_Show
 * ------------
 * Show window.
 */
void WIN_Show();

/*
 * WIN_Width
 * ---------
 * Return (virtual) width of window.
 */
int WIN_Width();

/*
 * WIN_Height
 * ----------
 * Return (virtual) height of window.
 */
int WIN_Height();

/*
 * WIN_ScreenWidth
 * ---------------
 * Return width of screen.
 */
int WIN_ScreenWidth();

/*
 * WIN_SetMousePosition
 * --------------------
 * Set mouse position.
 */
void WIN_SetMousePosition(int x, int y);

/*
 * WIN_MouseRelX
 * -------------
 * Return mouse x coordinate relative to last set position in SCREEN COORDINATES.
 */
int WIN_MouseRelX();

/*
 * WIN_MouseRelY
 * -------------
 * Return mouse y coordinate relative to last set position in SCREEN COORDINATES.
 */
int WIN_MouseRelY();

/*
 * WIN_SetMouseVisibility
 * ----------------------
 * Set mouse visibility.
 */
void WIN_SetMouseVisibility(int value);

/*
 * WIN_ScreenHeight
 * ----------------
 * Return height of screen.
 */
int WIN_ScreenHeight();

/*
 * WIN_GetImage
 * ------------
 * Get pointer to image data.
 */
void *WIN_GetImage(int id);

/*
 * WIN_SetImage
 * ------------
 * Set destination image, return 1 on success.
 */
int WIN_SetImage(int id, int updateAlpha);

/*
 * WIN_SetClipRect
 * ---------------
 */
void WIN_SetClipRect(int id, int x, int y, int w, int h);

/*
 * WIN_ClearClipRect
 * -----------------
 */
void WIN_ClearClipRect(int id);

/*
 * WIN_CurrentImage
 * ----------------
 */
int WIN_CurrentImage();

/*
 * WIN_SetColor
 * ------------
 * Set draw color.
 */
void WIN_SetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

/*
 * WIN_GetColor
 * ------------
 */
void WIN_GetColor(unsigned char *r, unsigned char *g, unsigned char  *b, unsigned char *a);

/*
 * WIN_SetAdditive
 * ---------------
 */
void WIN_SetAdditive(char value);

/*
 * WIN_SetPixel
 * ------------
 * Set pixel.
 */
void WIN_SetPixel(int x, int y);

/*
 * WIN_GetPixel
 * ------------
 */
int WIN_GetPixel(int id, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a);

/*
 * WIN_GetPixelCurrent
 * -------------------
 */
int WIN_GetPixelCurrent(int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a);

/*
 * WIN_DrawPixel
 * -------------
 * Draw pixel.
 */
void WIN_DrawPixel(int x, int y);

/*
 * WIN_DrawLine
 * ------------
 * Draw line.
 */
void WIN_DrawLine(int x1, int y1, int x2, int y2);

/*
 * WIN_DrawLineTo
 * --------------
 * Draw line to.
 */
void WIN_DrawLineTo(int x, int y);

/*
 * WIN_DrawRect
 * ------------
 * Draw rectangle.
 */
void WIN_DrawRect(int x, int y, int w, int h);

/*
 * WIN_FillRect
 * ------------
 * Draw filled rectangle.
 */
void WIN_FillRect(int x, int y, int w, int h);

/*
 * WIN_DrawEllipse
 * ---------------
 * Draw ellipse.
 */
void WIN_DrawEllipse(int centerX, int centerY, int xRadius, int yRadius);

/*
 * WIN_FillEllipse
 * ---------------
 * Draw filled ellipse.
 */
void WIN_FillEllipse(int centerX, int centerY, int xRadius, int yRadius);

/*
 * WIN_Cls
 * -------
 * Draw rectangle over the entire image. 
 */
void WIN_Cls(int setColor);

/*
 * WIN_DrawPolygon
 * ---------------
 * Draw polygon.
 */
void WIN_DrawPolygon(int pointCount, int *points);

/*
 * WIN_FillPolygon
 * ---------------
 * Draw filled polygon.
 */
void WIN_FillPolygon(int pointCount, int *points);

/*
 * WIN_DrawPolygonTransformed
 * --------------------------
 */
void WIN_DrawPolygonTransformed(int pointCount, float *points,
        float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY);

/*
 * WIN_FillPolygonTransformed
 * --------------------------
 */
void WIN_FillPolygonTransformed(int pointCount, float *points,
        float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY);

/*
 * WIN_TexturePolygon
 * ------------------
 * Draw a textured polygon.
 */
void WIN_TexturePolygon(int imageId, int fields, int pointCount, int *points, float *uv);

/*
 * WIN_TexturePolygonTransformed
 * -----------------------------
 */
void WIN_TexturePolygonTransformed(int imageId, int fields, int pointCount, float *points, float *uv,
        float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY);
 
/*
 * WIN_CreateImage
 * ---------------
 * Create image.
 */
int WIN_CreateImage(int id, int width, int height);

/*
 * WIN_LoadImage
 * -------------
 * Load image, return 1 on success.
 */
int WIN_LoadImage(int id, const char *filename);

/*
 * WIN_SaveImage
 * -------------
 */
int WIN_SaveImage(int id, const char *filename);

/*
 * WIN_FreeImage
 * -------------
 */
void WIN_FreeImage(int id);

/*
 * WIN_ImageExists
 * ---------------
 * Return 1 if image exists.
 */
int WIN_ImageExists(int id);

/*
 * WIN_ImageWidth
 * --------------
 * Return image/cell width.
 */
int WIN_ImageWidth(int id);

/*
 * WIN_ImageHeight
 * ---------------
 * Return image/cell height.
 */
int WIN_ImageHeight(int id);

/*
 * WIN_SetImageColorKey
 * --------------------
 * Set image color key.
 */
void WIN_SetImageColorKey(int id, unsigned char r, unsigned char g, unsigned char b);

/*
 * WIN_SetImageColorKey
 * --------------------
 * Set image cell grid.
 */
void WIN_SetImageGrid(int id, int cols, int rows);

/*
 * WIN_ImageCols
 * -------------
 * Return number of columns in image grid.
 */
int WIN_ImageCols(int id);

/*
 * WIN_ImageRows
 * -------------
 * Return number of rows in image grid.
 */
int WIN_ImageRows(int id);

/*
 * WIN_ImageCells
 * -------------
 * Return number of cells in image grid.
 */
int WIN_ImageCells(int id);

/*
 * WIN_DrawImage
 * -------------
 * Draw image.
 */
void WIN_DrawImage(int id, int x, int y);

/*
 * WIN_DrawImageCel
 * ----------------
 * Draw image cell.
 */
void WIN_DrawImageCel(int id, int x, int y, int cel);

/*
 * WIN_DrawImageRect
 * -----------------
 * Draw rectangular selection of image.
 */
void WIN_DrawImageRect(int id, int x, int y, int srcX, int srcY, int w, int h);

/*
 * WIN_DrawImageTransformed
 * ------------------------
 */
void WIN_DrawImageTransformed(int id, float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY);

/*
 * WIN_DrawImageCelTransformed
 * ---------------------------
 */
void WIN_DrawImageCelTransformed(int id, float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY, int cel);

/*
 * WIN_DrawImageRectTransformed
 * ----------------------------
 */
void WIN_DrawImageRectTransformed(int id, float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY, float srcX, float srcY, float srcW, float srcH);

/*
 * WIN_DrawVRaster
 * ---------------
 * Draw vertical textured line.
 */
void WIN_DrawVRaster(int id, int x, int y0, int y1, float u0, float v0, float u1, float v1);

/*
 * WIN_DrawHRaster
 * ---------------
 * Draw horizontal textured line.
 */
void WIN_DrawHRaster(int id, int y, int x0, int x1, float u0, float v0, float u1, float v1);

/*
 * WIN_CreateFont
 * --------------
 */
int WIN_CreateFont(int id, const char *name, int size, int bold, int italic, int underline, int smooth);

/*
 * WIN_LoadFont
 * ------------
 */
int WIN_LoadFont(int id, const char *name);

/*
 * WIN_SaveFont
 * ------------
 */
int WIN_SaveFont(int id, const char *name);

/*
 * WIN_FreeFont
 * ------------
 */
void WIN_FreeFont(int id);

/*
 * WIN_SetFont
 * -----------
 */
void WIN_SetFont(int id);

/*
 * WIN_CurrentFont
 * ---------------
 */
int WIN_CurrentFont();

/*
 * WIN_FontExists
 * --------------
 */
int WIN_FontExists(int id);

/*
 * WIN_FontWidth
 * -------------
 */
int WIN_FontWidth(int id, const char *s);

/*
 * WIN_FontHeight
 * --------------
 */
int WIN_FontHeight(int id);

/*
 * WIN_Write
 * ---------
 */
void WIN_Write(const char *s, int justification, int addNewLine);

/*
 * WIN_SetCaret
 * ------------
 */
void WIN_SetCaret(int x, int y);

/*
 * WIN_CaretX
 * ----------
 */
int WIN_CaretX();

/*
 * WIN_LastSetCaretX
 * -----------------
 */
int WIN_LastSetCaretX();

/*
 * WIN_CaretY
 * ----------
 */
int WIN_CaretY();

/*
 * WIN_Scroll
 * ----------
 * Scroll image.
 */
void WIN_Scroll(int dx, int dy);

/*
 * WIN_Sleep
 * ---------
 * Sleep.
 */
void WIN_Sleep(int ms);

/*
 * WIN_SetClipboardText
 * --------------------
 * Copy txt to clipboard.
 */
void WIN_SetClipboardText(const char *txt);

/*
 * WIN_GetClipboardText
 * --------------------
 * Return text from clipboard if present, 0 otherwise.
 */
char *WIN_GetClipboardText();

/*
 * WIN_OpenFileDialog
 * ------------------
 * Show a dialog for selecting a file for opening, return filename or 0.
 */
char *WIN_OpenFileDialog(const char *ext);

/*
 * WIN_SaveFileDialog
 * ------------------
 * Show a dialog for selecting a file for saving, return filename or 0.
 */
char *WIN_SaveFileDialog(const char *ext);

/*
 * WIN_DownloadFile
 * ----------------
 * Download file and return as char array
 */
char *WIN_DownloadFile(const char* url, int *bytesDownloaded);

/*
 * WIN_MessageBox
 * --------------
 * Show a message box.
 */
int WIN_MessageBox(const char *title, const char *msg);


#endif