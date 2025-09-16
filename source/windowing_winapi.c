/*
 * windowing_winapi.c
 * ------------------
 * Windowing implementation for Windows.
 *
 * By: Marcus 2021
 */

#define _WIN32_WINNT 0x0500

#include "windowing.h"
#include "naalaa_image.h"
#include "naalaa_font.h"
#include "syscmd.h"
#include "windows.h"
#include "stdio.h"
#include "shlobj.h"
#include "WinInet.h"
#include "direct.h"
#include "math.h"

#define WND_CLASS_NAME "NAALAA7"
#define SEND_MESSAGE_ID 7108

/* Default font, should probably use non-aa version. */
/*#include "default_font.h"*/

#define DEFAULT_FONT_SMOOTH 0
 
#include "default_font.h"

static int sDefaultFontABCData[] = {
    0, 0, 8, 0, 5, 3, 0, 7, 1, 0, 7, 1, 0, 6, 2, 0, 7, 1, 0, 7, 1, 0, 5, 3,
    0, 6, 2, 0, 6, 2, 0, 7, 1, 0, 8, 0, 0, 6, 2, 0, 8, 0, 0, 5, 3, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 5, 3, 0, 5, 3, 0, 6, 2, 0, 7, 1, 0, 6, 2, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 8, 0, 0, 7, 1, 0, 7, 1,
    0, 7, 1, 0, 8, 0, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 8, 0,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 6, 2, 0, 7, 1, 0, 6, 2, 0, 7, 1, 0, 8, 0,
    0, 6, 2, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1,
    0, 7, 1, 0, 6, 2, 0, 5, 3, 0, 7, 1, 0, 6, 2, 0, 8, 0, 0, 7, 1, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 6, 2, 0, 7, 1, 0, 7, 1, 0, 8, 0,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 6, 2, 0, 5, 3, 0, 6, 2, 0, 8, 0, 0, 6, 2,
    0, 8, 0, 0, 0, 8, 0, 6, 2, 0, 7, 1, 0, 7, 1, 0, 8, 0, 0, 7, 1, 0, 7, 1,
    0, 6, 2, 0, 6, 2, 0, 7, 1, 0, 6, 2, 0, 8, 0, 0, 0, 8, 0, 7, 1, 0, 0, 8,
    0, 0, 8, 0, 6, 2, 0, 6, 2, 0, 7, 1, 0, 7, 1, 0, 5, 3, 0, 7, 1, 0, 8, 0,
    0, 7, 1, 0, 6, 2, 0, 7, 1, 0, 6, 2, 0, 8, 0, 0, 0, 8, 0, 7, 1, 0, 7, 1,
    0, 0, 8, 0, 5, 3, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 5, 3, 0, 8, 0,
    0, 6, 2, 0, 8, 0, 0, 6, 2, 0, 7, 1, 0, 8, 0, 0, 8, 0, 0, 8, 0, 0, 8, 0,
    0, 6, 2, 0, 8, 0, 0, 6, 2, 0, 6, 2, 0, 5, 3, 0, 8, 0, 0, 6, 2, 0, 5, 3,
    0, 6, 2, 0, 6, 2, 0, 6, 2, 0, 7, 1, 0, 6, 2, 0, 6, 2, 0, 6, 2, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 8, 0, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1,
    0, 8, 0, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1,
    0, 8, 0, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 8, 0, 0, 7, 1,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 6, 2, 0, 6, 2, 0, 6, 2, 0, 6, 2,
    0, 6, 2, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 8, 0,
    0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1, 0, 7, 1
}; 

static int sInitialized = 0;
static UINT sTimePeriod = 0;
static int sActive = 0;
static int sFullScreen = 0;
static int sWidth = 0;
static int sHeight = 0;
static int sMinWidth = 0;
static int sMinHeight = 0;
static int sVirtualWidth = 0;
static int sVirtualHeight = 0;
static int sResizable = 0;
static int sResizing = 0;
static double sScaleX = 1;
static double sScaleY = 1;
static int sAutoRedraw = 1;

static int sLastSetMouseX = 0;
static int sLastSetMouseY = 0;
static int sMouseX = 0;
static int sMouseY = 0;


static unsigned int sColor = 0x80ffffff;
static char sAdditive = 0;

static HashTable *sImages = 0;
static Image *sPrimaryImage = 0;
static Image *sDstImage = 0;
static int sDstImageId = 0;
static int sUpdateImageAlpha = 0;

static int *sPolyPoints = 0;
static int sPolyPointCount = 0;

static HashTable *sFonts = 0;
static BitmapFont *sFont = 0;
static int sCurrentFontId = 0;
/* ... Actually caret information should be part of sys. */
static int sCaretBaseX = 0;
static int sCaretX = 0;
static int sCaretY = 0;

static WNDCLASS sWc;
static HWND sWnd = 0;
static HDC sHdc;

static char sBitmapBuffer[sizeof(BITMAPINFO)+16];
static BITMAPINFO *sBitmapInfo;

static void WindowResized();
static void InitFonts();
static void DeleteImage(void *data);
static void DeleteFont(void *data);
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void AutoRedraw();

void WIN__DrawImageRectTransformed(Image *img, float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY, float srcX, float srcY, float srcW, float srcH, int useImageAlpha);


/*
 * WIN_Init
 * --------
 */
void WIN_Init() {
    sImages = HT_Create(1);
    InitFonts();
}

/*
 * Win_ShowConsole
 * ---------------
 * Show/hide console.
 */
void WIN_ShowConsole(int show) {
    if (show) {
        ShowWindow(GetConsoleWindow(), SW_SHOW);
    }
    else {
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    }
}

/*
 * WindowResized
 * -------------
 */
static void WindowResized() {
    RECT rect;
    
    if (GetClientRect(sWnd, &rect)) {
        int newWidth = rect.right;
        int newHeight = rect.bottom;
        if (newWidth != sVirtualWidth || newHeight != sVirtualHeight) {
            sVirtualWidth = sWidth = newWidth;
            sVirtualHeight = sHeight = newHeight;
            sBitmapInfo->bmiHeader.biWidth = sVirtualWidth;
            sBitmapInfo->bmiHeader.biHeight = -sVirtualHeight;

            Image *oldPrimaryImage = (Image *)HT_Get(sImages, 0, SYS_PRIMARY_IMAGE);
            if (oldPrimaryImage) {
                HT_Delete(sImages, 0, SYS_PRIMARY_IMAGE, 0);
            }
            WIN_CreateImage(SYS_PRIMARY_IMAGE, sVirtualWidth, sVirtualHeight);
            sPrimaryImage = (Image *)HT_Get(sImages, 0, SYS_PRIMARY_IMAGE);
            if (!sDstImage || sDstImageId == SYS_PRIMARY_IMAGE) {
                sDstImage = sPrimaryImage;
                sDstImageId = SYS_PRIMARY_IMAGE;
            }
            if (oldPrimaryImage) {
                IMG_DrawImage(sPrimaryImage, 0, 0, oldPrimaryImage, 0, 0, oldPrimaryImage->w, oldPrimaryImage->h, ToRGB(255, 255, 255), 0, 0);
                DeleteImage(oldPrimaryImage);
            }
        }            
    }
}

/*
 * WIN_Set
 * -------
 */
int WIN_Set(const char *title, int width, int height, int fullScreen, int scaleFactor, int minWidth, int minHeight) {
    RECT rect;
    int cc;

    if (sInitialized) {
        ReleaseDC(sWnd, sHdc);
        DestroyWindow(sWnd);
    }
    else {
        TIMECAPS tc;
        
        sWc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
        sWc.lpfnWndProc = WndProc;
        sWc.cbClsExtra = 0;
        sWc.cbWndExtra = 0;
        sWc.hInstance = 0;
        sWc.hIcon = 0;
        sWc.hCursor = LoadCursor(0, IDC_ARROW);
        sWc.hbrBackground = 0;
        sWc.lpszMenuName = 0;
        sWc.lpszClassName = WND_CLASS_NAME;
        RegisterClassA(&sWc);
        
        if (timeGetDevCaps(&tc, sizeof(tc)) == MMSYSERR_NOERROR) {
            timeBeginPeriod(tc.wPeriodMin);
            sTimePeriod = tc.wPeriodMin;
        }
    }
    
    sFullScreen = fullScreen;
    sVirtualWidth = width;
    sVirtualHeight = height;    
    if (sFullScreen) {
        sWidth = (int)GetSystemMetrics(SM_CXSCREEN);
        sHeight = (int)GetSystemMetrics(SM_CYSCREEN);
        sMinWidth = 0;
        sMinHeight = 0;
        sScaleX = (double)sVirtualWidth/(double)sWidth;
        sScaleY = (double)sVirtualHeight/(double)sHeight;
        sResizable = 0;
    }
    else {
        if (scaleFactor == 0) {
            sWidth = sVirtualWidth;
            sHeight = sVirtualHeight;
            sMinWidth = minWidth;
            sMinHeight = minHeight;
            sResizable = 1;
        }
        else {
            sWidth = sVirtualWidth*scaleFactor;
            sHeight = sVirtualHeight*scaleFactor;
            sMinWidth = 0;
            sMinHeight = 0;
            sResizable = 0;
        }
    }
    sScaleX = (double)sVirtualWidth/(double)sWidth;
    sScaleY = (double)sVirtualHeight/(double)sHeight;
    
    rect.left = 0;
    rect.top = 0;
    rect.right = sWidth;
    rect.bottom = sHeight;

    /*AdjustWindowRect(&rect, WS_POPUP|WS_SYSMENU|WS_CAPTION, 0);*/
    
    if (sFullScreen) {
        AdjustWindowRect(&rect, WS_POPUP, 0);
        sWnd = CreateWindowEx(0, WND_CLASS_NAME, title, WS_POPUP, 0, 0, sWidth, sHeight, 0, 0, 0, 0);        
    }
    else {
        RECT waRect;
        /* Center window in work area. */
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &waRect, 0);
        if (sResizable) {
            AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);
            sMinWidth += (rect.right - rect.left) - sWidth;
            sMinHeight += (rect.bottom - rect.top) - sHeight;
            sWnd = CreateWindowEx(0, WND_CLASS_NAME, title,
                    WS_OVERLAPPEDWINDOW,
                    waRect.left + (waRect.right - waRect.left)/2 - (rect.right - rect.left)/2,
                    waRect.top + (waRect.bottom - waRect.top)/2 - (rect.bottom - rect.top)/2, 
                    rect.right - rect.left, rect.bottom - rect.top,
                    0, 0, 0, 0);
        }
        else {
            AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, 0);
            sWnd = CreateWindowEx(0, WND_CLASS_NAME, title,
                    WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                    waRect.left + (waRect.right - waRect.left)/2 - (rect.right - rect.left)/2,
                    waRect.top + (waRect.bottom - waRect.top)/2 - (rect.bottom - rect.top)/2, 
                    rect.right - rect.left, rect.bottom - rect.top,
                    0, 0, 0, 0);
        }
    }

    for (cc = 0; cc < sizeof(BITMAPINFOHEADER)+16; cc++) sBitmapBuffer[cc] = 0;
    sBitmapInfo = (BITMAPINFO *)&sBitmapBuffer;
    sBitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    sBitmapInfo->bmiHeader.biPlanes = 1;
    sBitmapInfo->bmiHeader.biBitCount = 32;
    sBitmapInfo->bmiHeader.biCompression = BI_BITFIELDS;
    sBitmapInfo->bmiHeader.biWidth = sVirtualWidth;
    sBitmapInfo->bmiHeader.biHeight = -sVirtualHeight;
    ((unsigned long *)sBitmapInfo->bmiColors)[0] = 0x00FF0000;
    ((unsigned long *)sBitmapInfo->bmiColors)[1] = 0x0000FF00;
    ((unsigned long *)sBitmapInfo->bmiColors)[2] = 0x000000FF;

    sHdc = GetDC(sWnd);
    
    ShowWindow(sWnd, SW_SHOW);
    SetForegroundWindow(sWnd);
    SetFocus(sWnd);

    /* Create back buffer. */
    Image *oldPrimaryImage = (Image *)HT_Get(sImages, 0, SYS_PRIMARY_IMAGE);
    if (oldPrimaryImage) {
        HT_Delete(sImages, 0, SYS_PRIMARY_IMAGE, 0);
    }
    WIN_CreateImage(SYS_PRIMARY_IMAGE, sVirtualWidth, sVirtualHeight);
    sPrimaryImage = (Image *)HT_Get(sImages, 0, SYS_PRIMARY_IMAGE);
    sDstImage = sPrimaryImage;
    sDstImageId = SYS_PRIMARY_IMAGE;
    if (oldPrimaryImage) {
        IMG_DrawImage(sPrimaryImage, 0, 0, oldPrimaryImage, 0, 0, oldPrimaryImage->w, oldPrimaryImage->h, ToRGB(255, 255, 255), 0, 0);
        DeleteImage(oldPrimaryImage);
    }
    
    sInitialized = 1;

    WIN_Redraw();
    WIN_Update();

    return WIN_SUCCESS;
}

/*
 * WIN_HasWindow
 * -------------
 */
int WIN_HasWindow() {
    return sInitialized;
}

/*
 * InitFonts
 * ---------
 */
static void InitFonts() {
    BitmapFont *bf;
    Image *img = IMG_Create(default_font.width, default_font.height, 0x00000000);
    
    for (int i = 0; i < default_font.width*default_font.height; i++) {
        unsigned char a = (unsigned char)((int)default_font.pixel_data[i*4 + 3]*128/255);
        img->buffer[i] = ToRGBA(default_font.pixel_data[i*4], default_font.pixel_data[i*4 + 1], default_font.pixel_data[i*4 + 2], a);
    }
    
    bf = BF_CreateEmpty(img);
    BF_SetABCv(bf, sDefaultFontABCData);
    if (DEFAULT_FONT_SMOOTH) BF_ApplySmoothing(bf);
    
    sFonts = HT_Create(1);
    HT_Add(sFonts, 0, 0, bf);
    sFont = bf;
}

/*
 * WIN_SetAutoRedraw
 * -----------------
 */
void WIN_SetAutoRedraw(int value) {
    sAutoRedraw = value;
}

/*
 * WIN_AutoRedraw
 * --------------
 */
int WIN_AutoRedraw() {
    return sAutoRedraw;
}

void AutoRedraw() {
    if (sAutoRedraw && sDstImage == sPrimaryImage) {
        WIN_Redraw();
        WIN_Update();
    }
}



static void DeleteImage(void *data) {
    IMG_Free((Image *)data);
}

static void DeleteFont(void *data) {
    BitmapFont *bf = (BitmapFont *)data;
    if (bf == sFont) sFont = 0;
    BF_Free(bf);
}

/*
 * WIN_Close
 * ---------
 */
void WIN_Close() {
    if (!sInitialized) return;

    ReleaseDC(sWnd, sHdc);
    DestroyWindow(sWnd);
    UnregisterClass(WND_CLASS_NAME, 0);
    timeEndPeriod(sTimePeriod);
    HT_Free(sImages, DeleteImage);
    HT_Free(sFonts, DeleteFont);
    sImages = 0;
    sPrimaryImage = 0;
    sDstImage = 0;
    if (sPolyPoints) {
        sPolyPoints = 0;
        sPolyPointCount = 0;
    }
    sInitialized = 0;
    sActive = 0;
}

/*
 * WndProc
 * -------
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* On some keyboard layouts, pressing ALT GR causes two WM_KEYDOWN messages,
       where the first one is VK_CONTROL and the second one VK_MENU. This may
       mess things up, especially if you are using the CTRL key for "commands"
       in your program. As a hack, I send a SYS_KeyUp message for VK_CONTROL to
       n7 whenever VK_MENU follows VK_CONTROL. It may of course cause other
       weird behaviors, but most likely they're rare. N7 doesn't process
       VK_MENU (ALT) because it's such bloody mess. */
    static unsigned int lastKeyDown = 0;
    unsigned int key;
    int x, y;
    
    switch (msg) {
        case WM_CREATE:
            joySetCapture(hWnd, JOYSTICKID1, 16, TRUE);
            break;
        case WM_DESTROY:
            joyReleaseCapture(JOYSTICKID1);
            break;
            
        case WM_PAINT: {
            if (sPrimaryImage) {
                StretchDIBits(sHdc, 0, 0, sWidth, sHeight,
                              0, 0, sVirtualWidth, sVirtualHeight,
                              IMG_Buffer(sPrimaryImage), sBitmapInfo, DIB_RGB_COLORS, SRCCOPY);
            }
            ValidateRect(sWnd, NULL);
            return 0;
        }

        case WM_CHAR:
            SYS_KeyChar((unsigned int)wParam);
            return 0;
        
        case WM_KEYDOWN:
            key = (unsigned int) wParam;
            /* A hack to remove the VK_CONTOL generated when pressing ALT GR on
               SOME keyboard layouts ... */
            if ((HIWORD(lParam) & KF_EXTENDED) == KF_EXTENDED) {
                if (key == VK_MENU) {
                    if (lastKeyDown == VK_CONTROL) SYS_KeyUp(VK_CONTROL);
                }
                else {
                    SYS_KeyDown(key);
                }                
            }
            else {
                SYS_KeyDown(key);
            }
            /* I want the DEL char in n7's inkey buffer.  */
            if (key == VK_DELETE) SYS_KeyChar(127);
            lastKeyDown = key;
            break;
            
        case WM_KEYUP:
            SYS_KeyUp((unsigned int)wParam);
            break;
            
        /* Mouse. */
        case WM_LBUTTONDOWN:
            SetCapture(sWnd);
            SYS_MouseDown(0);
            return 0;
        case WM_LBUTTONUP: 
            ReleaseCapture();
            SYS_MouseUp(0);
            return 0;
        case WM_RBUTTONDOWN:
            SetCapture(sWnd);
            SYS_MouseDown(1);
            return 0;
        case WM_RBUTTONUP:
            ReleaseCapture();
            SYS_MouseUp(1);
            return 0;
        case WM_MOUSEWHEEL:
            SYS_MouseWheel(GET_WHEEL_DELTA_WPARAM(wParam)/120);
            return 0;
        case WM_MOUSEMOVE:
            sMouseX = (int)((short)LOWORD(lParam));
            sMouseY = (int)((short)HIWORD(lParam));
            if (sFullScreen) {
                x = (int)((float)sMouseX*sScaleX);
                y = (int)((float)sMouseY*sScaleY);
            }
            else {
                x = sMouseX*sVirtualWidth/sWidth;
                y = sMouseY*sVirtualHeight/sHeight;
            }
            SYS_MouseMove(x < 0 ? 0 : (x >= sVirtualWidth ? sVirtualWidth - 1 : x),
                    y < 0 ? 0 : (y >= sVirtualHeight ? sVirtualHeight - 1 : y));
            return 0;
            
        /* Joystick. */
        case MM_JOY1BUTTONDOWN:
            if (wParam & JOY_BUTTON1CHG) SYS_JoyButtonDown(0);
            else if (wParam & JOY_BUTTON2CHG) SYS_JoyButtonDown(1);
            else if (wParam & JOY_BUTTON3CHG) SYS_JoyButtonDown(2);
            else if (wParam & JOY_BUTTON4CHG) SYS_JoyButtonDown(3);
            break;
        case MM_JOY1BUTTONUP:
            if (wParam & JOY_BUTTON1CHG) SYS_JoyButtonUp(0);
            else if (wParam & JOY_BUTTON2CHG) SYS_JoyButtonUp(1);
            else if (wParam & JOY_BUTTON3CHG) SYS_JoyButtonUp(2);
            else if (wParam & JOY_BUTTON4CHG) SYS_JoyButtonUp(3);
            break;
        case MM_JOY1MOVE:
            x = (int)((LOWORD(lParam) - 32767)/320.768);
            y = (int)((HIWORD(lParam) - 32767)/320.768);
            
            if (x <= -20)   x = x < -100 ? -100 : x;
            else if (x >= 20) x = x > 100 ? 100 : x;
            else x = 0;
            
            if (y <= -20)   y = y < -100 ? -100 : y;
            else if (y >= 20) y = y > 100 ? 100 : y;
            else y = 0;
            
            SYS_JoyMove(x, y);
            break;

        /* Window size. */
        case WM_SIZE:
            if (sResizable) {
                if (wParam == SIZE_MAXIMIZED || (wParam == SIZE_RESTORED && !sResizing)) {
                    WindowResized();
                }
            }
            break;

        case WM_ENTERSIZEMOVE:
            sResizing = 1;
            break;
            
        case WM_EXITSIZEMOVE:
            sResizing = 0;
            if (sResizable) WindowResized();
            break;
            
        case WM_GETMINMAXINFO:
            if (sResizable) {
                ((LPMINMAXINFO)lParam)->ptMinTrackSize.x = sMinWidth;
                ((LPMINMAXINFO)lParam)->ptMinTrackSize.y = sMinHeight;
                return 0;
            }
            break;
            
        case WM_ACTIVATEAPP:
            sActive = wParam == TRUE ? 1 : 0;
            SYS_WindowFocusChanged(sActive);
            break;            
        
        case WM_COPYDATA:
            if (((PCOPYDATASTRUCT)lParam)->dwData == SEND_MESSAGE_ID) {
                SYS_WindowMessageReceived((const char *)((PCOPYDATASTRUCT)lParam)->lpData);
                return TRUE;
            }
            break;
        
        case WM_CLOSE:
            SYS_TerminateProgram();
            return 0;
            
    }
    
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/*
 * WIN_Update
 * ----------
 */
void WIN_Update() {
    MSG msg;    
    int didRedraw = 0;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        if (msg.message == WM_PAINT) {
            if (!didRedraw) {
                DispatchMessage(&msg);
                didRedraw = 1;
            }
        }
        else {
            DispatchMessage(&msg);
        }
    }
}

/*
 * WIN_Redraw
 * ----------
 */
void WIN_Redraw() {
    if (sInitialized) {
        InvalidateRect(sWnd, NULL, TRUE);
        SendMessage(sWnd, WM_PAINT, 0, 0);
    }
}

/*
 * WIN_Active
 * ----------
 */
int WIN_Active() {
    return sActive;
}

/*
 * WIN_Exists
 * ----------
 * Return 1 if an n7 window with the specified title exists.
 */
int WIN_Exists(const char *title) {
    return FindWindowA(WND_CLASS_NAME, title) != NULL;
}

/*
 * WIN_SendMessage
 * ---------------
 * Send message to a window.
 */
void WIN_SendMessage(const char *title, char *message) {
    HWND wnd = FindWindowA(WND_CLASS_NAME, title);
    if (wnd) {
        COPYDATASTRUCT data;
        data.dwData = SEND_MESSAGE_ID;
        data.cbData = strlen(message) + 1;
        data.lpData = message;
        SendMessage(wnd, WM_COPYDATA, 0, (LPARAM) (LPVOID) &data);
    }
}

/*
 * WIN_Show
 * ------------
 * Show a window.
 */
void WIN_Show() {
    if (sWnd) {
        /*if (IsIconic(sWnd)) ShowWindow(sWnd, SW_RESTORE);
        else ShowWindow(sWnd, SW_SHOW);
        BringWindowToTop(sWnd);
        SetFocus(sWnd);*/
        /* To be honest ... I have no fucking idea ... struggled for hours with
           different combinations without success. Stackoverflow to the rescue:
           https://stackoverflow.com/questions/916259/win32-bring-a-window-to-top
        */
        HWND hCurWnd = GetForegroundWindow();
        DWORD dwMyID = GetCurrentThreadId();
        DWORD dwCurID = GetWindowThreadProcessId(hCurWnd, NULL);
        AttachThreadInput(dwCurID, dwMyID, TRUE);
        SetWindowPos(sWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        SetWindowPos(sWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
        SetForegroundWindow(sWnd);
        SetFocus(sWnd);
        SetActiveWindow(sWnd);
        AttachThreadInput(dwCurID, dwMyID, FALSE);
        if (IsIconic(sWnd)) ShowWindow(sWnd, SW_RESTORE);
    }
}

/*
 * WIN_Width
 * ---------
 */
int WIN_Width() {
    return sVirtualWidth;
}

/*
 * WIN_Height
 * ----------
 */
int WIN_Height() {
    return sVirtualHeight;
}

/*
 * WIN_ScreenWidth
 * ---------------
 */
int WIN_ScreenWidth() {
    return (int)GetSystemMetrics(SM_CXSCREEN);
}

/*
 * WIN_ScreenHeight
 * ----------------
 */
int WIN_ScreenHeight() {
    return GetSystemMetrics(SM_CYSCREEN);
}

/*
 * WIN_SetMousePosition
 * --------------------
 * Set mouse position.
 */
void WIN_SetMousePosition(int x, int y) {
    if (sInitialized && sWnd) {
        if (x >= 0 && x < sVirtualWidth && y >= 0 && y < sVirtualHeight) {
            POINT pt;
            pt.x = (int)(((double)x + 0.5)/sScaleX);
            pt.y = (int)(((double)y + 0.5)/sScaleY);
            sLastSetMouseX = pt.x;
            sLastSetMouseY = pt.y;
            sMouseX = sLastSetMouseX;
            sMouseY = sLastSetMouseY;
            ClientToScreen(sWnd, &pt);
            SetCursorPos(pt.x, pt.y);
            SYS_MouseMove(x, y);
        }
    }
}

int WIN_MouseRelX() {
    return sMouseX - sLastSetMouseX;
}

int WIN_MouseRelY() {
    return sMouseY - sLastSetMouseY;
}

/*
 * WIN_SetMouseVisibility
 * ----------------------
 * Set mouse visibility.
 */
void WIN_SetMouseVisibility(int value) {
    /* Uhm ... idk, I copied this from n6. */
    if (value) while (ShowCursor(TRUE) < 0);
    else while (ShowCursor(FALSE) > 0);
}

/*
 * WIN_SetImage
 * ------------
 */
int WIN_SetImage(int id, int updateAlpha) {
    Image *prevImage = sDstImage;
    Image *img;
    
    if ((img = (Image *)HT_Get(sImages, 0, id))) {
        sDstImage = img;
        sDstImageId = id;
        if (sUpdateImageAlpha && prevImage && prevImage != sPrimaryImage && prevImage != sDstImage) {
            IMG_BufferChanged(prevImage);
        }
        sUpdateImageAlpha = updateAlpha;
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * WIN_SetClipRect
 * ---------------
 */
void WIN_SetClipRect(int id, int x, int y, int w, int h) {
    Image *img;

    if ((img = (Image *)HT_Get(sImages, 0, id))) IMG_SetClipRect(img, x, y, w, h);
}

/*
 * WIN_ClearClipRect
 * -----------------
 */
void WIN_ClearClipRect(int id) {
    Image *img;

    if ((img = (Image *)HT_Get(sImages, 0, id))) IMG_ClearClipRect(img);
}


/*
 * WIN_CurrentImage
 * ----------------
 */
int WIN_CurrentImage() {
    return sDstImageId;
}

/*
 * WIN_SetColor
 * ------------
 */
void WIN_SetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    a = (unsigned char)((int)a*128/255);
    sColor = ToRGBA(r, g, b, a);
}

/*
 * WIN_GetColor
 * ------------
 */
void WIN_GetColor(unsigned char *r, unsigned char *g, unsigned char  *b, unsigned char *a) {
    ColorToRGBAComponents(sColor, *r, *g, *b, *a);
    *a = (unsigned char)((int)*a*255/128);
}

/*
 * WIN_SetAdditive
 * ---------------
 */
void WIN_SetAdditive(char value) {
    sAdditive = value;
}


/*
 * WIN_SetPixel
 * ------------
 */
void WIN_SetPixel(int x, int y) {
    if (!(sImages && sDstImage)) return;
    
    IMG_SetPixel(sDstImage, x, y, sColor);

    AutoRedraw();
}



/*
 * WIN_GetPixel
 * ------------
 */
int WIN_GetPixel(int id, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a) {
    Image *img;
    unsigned int color;
    
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return 0;
    if (IMG_GetPixel(img, x, y, &color)) {
        ColorToRGBAComponents(color, *r, *g, *b, *a);
        *a = (unsigned char)((int)*a*255/128);
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * WIN_GetPixelCurrent
 * -------------------
 */
int WIN_GetPixelCurrent(int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a) {
    unsigned int color;
    
    if (!(sImages && sDstImage)) return 0;
    if (IMG_GetPixel(sDstImage, x, y, &color)) {
        ColorToRGBAComponents(color, *r, *g, *b, *a);
        *a = (unsigned char)((int)*a*255/128);
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * WIN_DrawPixel
 * -------------
 */
void WIN_DrawPixel(int x, int y) {
    if (!(sImages && sDstImage)) return;
    
    IMG_DrawPixel(sDstImage, x, y, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_DrawLine
 * ------------
 */
void WIN_DrawLine(int x1, int y1, int x2, int y2) {
    if (!(sImages && sDstImage)) return;
    
    IMG_DrawLine(sDstImage, x1, y1, x2, y2, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_DrawLineTo
 * --------------
 */
void WIN_DrawLineTo(int x, int y) {
    if (!(sImages && sDstImage)) return;
    
    IMG_DrawLineTo(sDstImage, x, y, sColor, sAdditive);

    AutoRedraw();
}


/*
 * WIN_DrawRect
 * ------------
 */
void WIN_DrawRect(int x, int y, int w, int h) {
    if (!(sImages && sDstImage)) return;
    
    IMG_DrawRect(sDstImage, x, y, w, h, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_FillRect
 * ------------
 */
void WIN_FillRect(int x, int y, int w, int h) {
    if (!(sImages && sDstImage)) return;
    
    IMG_FillRect(sDstImage, x, y, w, h, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_Cls
 * -------
 */
void WIN_Cls(int setColor) {
    if (!(sImages && sDstImage)) return;

    if (setColor) {
        IMG_SetRect(sDstImage, 0, 0, IMG_Width(sDstImage), IMG_Height(sDstImage), sColor);
    }
    else { 
        IMG_FillRect(sDstImage, 0, 0, IMG_Width(sDstImage), IMG_Height(sDstImage), sColor, sAdditive);
    }

    AutoRedraw();
}

/*
 * WIN_DrawEllipse
 * ---------------
 * Draw ellipse.
 */
void WIN_DrawEllipse(int centerX, int centerY, int xRadius, int yRadius) {
    if (!(sImages && sDstImage)) return;
    
    IMG_DrawEllipse(sDstImage, centerX, centerY, xRadius, yRadius, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_FillEllipse
 * ---------------
 * Draw filled ellipse.
 */
void WIN_FillEllipse(int centerX, int centerY, int xRadius, int yRadius) {
    if (!(sImages && sDstImage)) return;
    
    IMG_FillEllipse(sDstImage, centerX, centerY, xRadius, yRadius, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_DrawPolygon
 * ---------------
 */
void WIN_DrawPolygon(int pointCount, int *points) {
    if (!(sImages && sDstImage)) return;
    
    IMG_DrawPolygon(sDstImage, pointCount, points, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_FillPolygon
 * ---------------
 */
void WIN_FillPolygon(int pointCount, int *points) {
    if (!(sImages && sDstImage)) return;
    
    IMG_FillPolygon(sDstImage, pointCount, points, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_DrawPolygonTransformed
 * --------------------------
 */
void WIN_DrawPolygonTransformed(int pointCount, float *points,
        float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY) {
    if (!(sImages && sDstImage)) return;    
    if (pointCount > sPolyPointCount) {
        sPolyPointCount = pointCount;
        sPolyPoints = (int *)realloc(sPolyPoints, sizeof(int)*sPolyPointCount*2);
        if (!sPolyPoints) {
            sPolyPoints = 0;
            return;
        }
    }
    for (int i = 0; i < pointCount; i++) {
        float px = (points[i*2] - pivotX)*scaleX;
        float py = (points[i*2 + 1] - pivotY)*scaleY;
        sPolyPoints[i*2] = (int)roundf(x + px*cosf(angle) - py*sinf(angle));
        sPolyPoints[i*2 + 1] = (int)roundf(y + py*cosf(angle) + px*sinf(angle));
    }
    IMG_DrawPolygon(sDstImage, pointCount, sPolyPoints, sColor, sAdditive);    

    AutoRedraw();
}

/*
 * WIN_FillPolygonTransformed
 * --------------------------
 */
void WIN_FillPolygonTransformed(int pointCount, float *points,
        float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY) {
    if (!(sImages && sDstImage)) return;
    if (pointCount > sPolyPointCount) {
        sPolyPointCount = pointCount;
        sPolyPoints = (int *)realloc(sPolyPoints, sizeof(int)*sPolyPointCount*2);
        if (!sPolyPoints) {
            sPolyPoints = 0;
            return;
        }
    }
    for (int i = 0; i < pointCount; i++) {
        float px = (points[i*2] - pivotX)*scaleX;
        float py = (points[i*2 + 1] - pivotY)*scaleY;
        sPolyPoints[i*2] = (int)roundf(x + px*cosf(angle) - py*sinf(angle));
        sPolyPoints[i*2 + 1] = (int)roundf(y + py*cosf(angle) + px*sinf(angle));
    }
    IMG_FillPolygon(sDstImage, pointCount, sPolyPoints, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_TexturePolygon
 * ------------------
 */
void WIN_TexturePolygon(int imageId, int fields, int pointCount, int *points, float *uvz) {
    Image *img;
    
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, imageId)))) return;
    
    float maxW = (float)IMG_Width(img) - 0.01f;
    float maxH = (float)IMG_Height(img) - 0.01f;
    float *point = uvz;
    int step = fields == 4 ? 2 : 3;
    for (int i = 0; i < pointCount; i++, point += step) {
        if (point[0] < 0.01f) point[0] = 0.01f;
        if (point[0] > maxW) point[0] = maxW;
        if (point[1] < 0.01f) point[1] = 0.01f;
        if (point[1] >= maxH) point[1] = maxH;
    }
    if (fields == 5) IMG_TexturePolygonZ(sDstImage, pointCount, points, uvz, img, sColor, 1, sAdditive);
    else IMG_TexturePolygon(sDstImage, pointCount, points, uvz, img, sColor, 1, sAdditive);
    AutoRedraw();
}

/*
 * WIN_TexturePolygonTransformed
 * -----------------------------
 */
void WIN_TexturePolygonTransformed(int imageId, int fields, int pointCount, float *points, float *uvz,
        float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY) {
    Image *img;
    
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, imageId)))) return;
    
    float maxW = (float)IMG_Width(img) - 0.01f;
    float maxH = (float)IMG_Height(img) - 0.01f;
    float *point = uvz;
    int step = fields == 4 ? 2 : 3;
    
    if (pointCount > sPolyPointCount) {
        sPolyPointCount = pointCount;
        sPolyPoints = (int *)realloc(sPolyPoints, sizeof(int)*sPolyPointCount*2);
        if (!sPolyPoints) {
            sPolyPoints = 0;
            return;
        }
    }
    
    for (int i = 0; i < pointCount; i++) {
        float px = (points[i*2] - pivotX)*scaleX;
        float py = (points[i*2 + 1] - pivotY)*scaleY;
        sPolyPoints[i*2] = (int)roundf(x + px*cosf(angle) - py*sinf(angle));
        sPolyPoints[i*2 + 1] = (int)roundf(y + py*cosf(angle) + px*sinf(angle));
    }

    for (int i = 0; i < pointCount; i++, point += step) {
        if (point[0] < 0.01f) point[0] = 0.01f;
        if (point[0] > maxW) point[0] = maxW;
        if (point[1] < 0.01f) point[1] = 0.01f;
        if (point[1] >= maxH) point[1] = maxH;
    }
    
    if (fields == 5) IMG_TexturePolygonZ(sDstImage, pointCount, sPolyPoints, uvz, img, sColor, 1, sAdditive);
    else IMG_TexturePolygon(sDstImage, pointCount, sPolyPoints, uvz, img, sColor, 1, sAdditive);
    AutoRedraw();
}


/*
 * WIN_GetImage
 * ------------
 * Get pointer to image data.
 */
void *WIN_GetImage(int id) {
    return HT_Get(sImages, 0, id);
}

/*
 * WIN_CreateImage
 * ---------------
 */
int WIN_CreateImage(int id, int width, int height) {
    Image *img;
    
    if ((img = (Image *)HT_Get(sImages, 0, id))) {
        if (img == sDstImage) return 0;
        else HT_Delete(sImages, 0, id, DeleteImage);
    }

    if ((img = IMG_Create(width, height, 0x80000000))) {    
        HT_Add(sImages, 0, id, img);
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * WIN_LoadImage
 * -------------
 * Load image, return 1 on success.
 */
int WIN_LoadImage(int id, const char *filename) {
    Image *img;
    
    if ((img = (Image *)HT_Get(sImages, 0, id))) {
        if (img == sDstImage) return 0;
        else HT_Delete(sImages, 0, id, DeleteImage);
    }

    if ((img = IMG_Load(filename))) {    
        HT_Add(sImages, 0, id, img);
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * WIN_SaveImage
 * -------------
 */
int WIN_SaveImage(int id, const char *filename) {
    Image *img = (Image *)HT_Get(sImages, 0, id);
    return img && IMG_Save(img, filename);
}

/*
 * WIN_FreeImage
 * -------------
 */
void WIN_FreeImage(int id) {
    Image *img;
    
    img = (Image *)HT_Get(sImages, 0, id);
    if ((img = (Image *)HT_Get(sImages, 0, id))) {
        if (img == sDstImage) return;
        else HT_Delete(sImages, 0, id, DeleteImage);
    }
}

/*
 * WIN_ImageExists
 * ---------------
 * Return 1 if image exists.
 */
int WIN_ImageExists(int id) {
    return sImages ? HT_Exists(sImages, 0, id) : 0;
}

/*
 * WIN_ImageWidth
 * --------------
 */
int WIN_ImageWidth(int id) {
    Image *img;
    
    return sImages && (img = (Image *)HT_Get(sImages, 0, id)) ? img->w/img->cols : 0;
}

/*
 * WIN_ImageHeight
 * ---------------
 */
int WIN_ImageHeight(int id) {
    Image *img;
    
    return sImages && (img = (Image *)HT_Get(sImages, 0, id)) ? img->h/img->rows : 0;
}

/*
 * WIN_ImageCols
 * -------------
 * Return number of columns in image grid.
 */
int WIN_ImageCols(int id) {
    Image *img;
    
    return sImages && (img = (Image *)HT_Get(sImages, 0, id)) ? img->cols : 0;
}

/*
 * WIN_ImageRows
 * -------------
 * Return number of rows in image grid.
 */
int WIN_ImageRows(int id) {
    Image *img;
    
    return sImages && (img = (Image *)HT_Get(sImages, 0, id)) ? img->rows : 0;
}

/*
 * WIN_ImageCols
 * -------------
 * Return number of cells in image grid.
 */
int WIN_ImageCells(int id) {
    Image *img;
    
    return sImages && (img = (Image *)HT_Get(sImages, 0, id)) ? img->cells : 0;
}


/*
 * WIN_SetImageColorKey
 * --------------------
 */
void WIN_SetImageColorKey(int id, unsigned char r, unsigned char g, unsigned char b) {
    Image *img;
    
    if (!(sImages && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    
    IMG_SetColorKey(img, ToRGB(r, g, b));
}

/*
 * WIN_SetImageGrid
 * ----------------
 */
void WIN_SetImageGrid(int id, int cols, int rows) {
    Image *img;
    
    if (!(sImages && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    
    IMG_SetGrid(img, cols, rows);
}

/*
 * WIN_DrawImage
 * -------------
 * Draw image.
 */
void WIN_DrawImage(int id, int x, int y) {
    Image *img;
    
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    
    IMG_DrawImage(sDstImage, x, y, img, 0, 0, IMG_Width(img), IMG_Height(img), sColor, 1, sAdditive);

    AutoRedraw();
}

/*
 * WIN_DrawImageCel
 * ----------------
 * Draw image cell.
 */
void WIN_DrawImageCel(int id, int x, int y, int cel) {
    Image *img;
    
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    
    IMG_DrawImageCel(sDstImage, x, y, img, cel, sColor, sAdditive);

    AutoRedraw();
}

/*
 * WIN_DrawImageRect
 * -----------------
 * Draw rectangular selection of image.
 */
void WIN_DrawImageRect(int id, int x, int y, int srcX, int srcY, int w, int h) {
    Image *img;
    
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    
    IMG_DrawImage(sDstImage, x, y, img, srcX, srcY, w, h, sColor, 1, sAdditive);

    AutoRedraw();
}

/*
 * WIN_DrawImageTransformed
 * ------------------------
 */
void WIN_DrawImageTransformed(int id, float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY) {
    Image *img;
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    WIN__DrawImageRectTransformed(img, x, y, scaleX, scaleY, angle, pivotX, pivotY, 0.0f, 0.0f, (float)IMG_Width(img), (float)IMG_Height(img), 1);
}

/*
 * WIN_DrawImageCelTransformed
 * ---------------------------
 */
void WIN_DrawImageCelTransformed(int id, float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY, int cel) {
    Image *img;
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    if (cel < 0 || cel >= img->cells) return;
    int col = cel%img->cols, row = cel/img->cols;
    int celw = img->w/img->cols, celh = img->h/img->rows;
    WIN__DrawImageRectTransformed(img, x, y, scaleX, scaleY, angle, pivotX, pivotY, (float)col*celw, (float)row*celh, (float)celw, (float)celh, img->cellInfo[cel].hasAlpha);
}

/*
 * WIN_DrawImageRectTransformed
 * ----------------------------
 */
void WIN_DrawImageRectTransformed(int id, float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY, float srcX, float srcY, float srcW, float srcH) {
    Image *img;
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    if (srcX < 0.0f) srcX = 0.0f;
    if (srcX + srcW > (float)IMG_Width(img)) srcW = (float)IMG_Width(img) - srcX;
    if (srcY < 0.0f) srcY = 0.0f;
    if (srcY + srcH > (float)IMG_Height(img)) srcH = (float)IMG_Height(img) - srcY; 
    WIN__DrawImageRectTransformed(img, x, y, scaleX, scaleY, angle, pivotX, pivotY, srcX, srcY, srcW, srcH, 1);
}

void WIN__DrawImageRectTransformed(Image *img, float x, float y, float scaleX, float scaleY, float angle, float pivotX, float pivotY, float srcX, float srcY, float srcW, float srcH, int useImageAlpha) {
    int points[8];
    int pointsf[8];
    float uv[8];
    float xLeft, xRight, yTop, yBottom;
   
    if (fabsf(scaleX*srcW) < 1 || fabsf(scaleY*srcH) < 1) return;
   
    xLeft = -pivotX*scaleX;
    xRight = (srcW - pivotX)*scaleX - 1;
    yTop = -pivotY*scaleY;
    yBottom = (srcH - pivotY)*scaleY - 1;
    
    pointsf[0] = xLeft; pointsf[1] = yTop;
    pointsf[2] = xRight; pointsf[3] = yTop;
    pointsf[4] = xRight; pointsf[5] = yBottom;
    pointsf[6] = xLeft; pointsf[7] = yBottom;
    
    uv[0] = srcX + 0.01f; uv[1] = srcY + 0.01f;
    uv[2] = srcX + srcW - 0.01f; uv[3] = srcY + 0.01f;
    uv[4] = srcX + srcW - 0.01f; uv[5] = srcY + srcH - 0.01f;
    uv[6] = srcX + 0.01f; uv[7] = srcY + srcH - 0.01f;
        
    for (int i = 0; i < 4; i++) {
        float rx = pointsf[i*2]*cosf(angle) - pointsf[i*2 + 1]*sinf(angle);
        float ry = pointsf[i*2 + 1]*cosf(angle) + pointsf[i*2]*sinf(angle);
        points[i*2] = (int)roundf(x + rx);
        points[i*2 + 1] = (int)roundf(y + ry);
    }
    
    IMG_TexturePolygon(sDstImage, 4, points, uv, img, sColor, useImageAlpha, sAdditive);

    AutoRedraw();
}


/*
 * WIN_DrawVRaster
 * ---------------
 * Draw vertical textured line.
 */
void WIN_DrawVRaster(int id, int x, int y0, int y1, float u0, float v0, float u1, float v1) {
    Image *img;
    
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    
    IMG_DrawVRaster(sDstImage, img, x, y0, y1, u0, v0, u1, v1, sColor);
    
    AutoRedraw();
}

/*
 * WIN_DrawHRaster
 * ---------------
 * Draw horizontal textured line.
 */
void WIN_DrawHRaster(int id, int y, int x0, int x1, float u0, float v0, float u1, float v1) {
    Image *img;
    
    if (!(sImages && sDstImage && (img = (Image *)HT_Get(sImages, 0, id)))) return;
    
    IMG_DrawHRaster(sDstImage, img, y, x0, x1, u0, v0, u1, v1, sColor);
    
    AutoRedraw();
}


/*
 * WIN_CreateFont
 * --------------
 */
int WIN_CreateFont(int id, const char *name, int size, int bold, int italic, int underline, int smooth) {
    BitmapFont *bf;
    
    HT_Delete(sFonts, 0, id, DeleteFont);
    
    bf = BF_Create(name, size, bold, italic, underline, smooth);
    if (bf) {
        HT_Add(sFonts, 0, id, bf);
        if (sCurrentFontId == id) sFont = bf;
    }
    
    return bf != 0;
}

/*
 * WIN_LoadFont
 * ------------
 */
int WIN_LoadFont(int id, const char *name) {
    BitmapFont *bf;
    
    HT_Delete(sFonts, 0, id, DeleteFont);
    
    bf = BF_Load(name);
    if (bf) {
        HT_Add(sFonts, 0, id, bf);
        if (sCurrentFontId == id) sFont = bf;
    }
    
    return bf != 0;
}

/*
 * WIN_SaveFont
 * ------------
 */
int WIN_SaveFont(int id, const char *name) {
    BitmapFont *bf = (BitmapFont *)HT_Get(sFonts, 0, id);
    return bf && BF_Save(bf, name);
}

/*
 * WIN_FreeFont
 * ------------
 */
void WIN_FreeFont(int id) {
    HT_Delete(sFonts, 0, id, DeleteFont);
}

/*
 * WIN_SetFont
 * -----------
 */
void WIN_SetFont(int id) {
    sCurrentFontId = id;
    sFont = (BitmapFont *)HT_Get(sFonts, 0, id);
}

/*
 * WIN_CurrentFont
 * ---------------
 */
int WIN_CurrentFont() {
    return sCurrentFontId;
}

/*
 * WIN_FontExists
 * --------------
 */
int WIN_FontExists(int id) {
    return HT_Exists(sFonts, 0, id);
}

/*
 * WIN_FontWidth
 * -------------
 */
int WIN_FontWidth(int id, const char *s) {
    BitmapFont *bf;
    
    if ((bf = (BitmapFont *)HT_Get(sFonts, 0, id))) return BF_Width(bf, s);
    else return 0;
}

/*
 * WIN_FontHeight
 * --------------
 */
int WIN_FontHeight(int id) {
    BitmapFont *bf;
    
    if ((bf = (BitmapFont *)HT_Get(sFonts, 0, id))) return bf->height;
    else return 0;
}

/*
 * WIN_Write
 * ---------
 */
void WIN_Write(const char *s, int justification, int addNewLine) {
    if (!(sDstImage && sFont)) return;

    if (justification < 0) {
        BF_Write(sFont, sDstImage, s, &sCaretX, &sCaretY, sColor, sAdditive);
    }
    else if (justification == 0) {
        int len = BF_Width(sFont, s);
        int x = sCaretX - len/2;
        BF_Write(sFont, sDstImage, s, &x, &sCaretY, sColor, sAdditive);
    }
    else if (justification > 0) {
        int len = BF_Width(sFont, s);
        int x = sCaretX - len;
        sCaretX -= len;
        BF_Write(sFont, sDstImage, s, &x, &sCaretY, sColor, sAdditive);
    }
    
    if (addNewLine) {
        sCaretX = sCaretBaseX;
        sCaretY += sFont->height;
    }
    
    AutoRedraw();
}

/*
 * WIN_SetCaret
 * ------------
 */
void WIN_SetCaret(int x, int y) {
    sCaretBaseX = x;
    sCaretX = x;
    sCaretY = y;
}

/*
 * WIN_CaretX
 * ----------
 */
int WIN_CaretX() {
    return sCaretX;
}

/*
 * WIN_LastSetCaretX
 * -----------------
 */
int WIN_LastSetCaretX() {
    return sCaretBaseX;
}

/*
 * WIN_CaretY
 * ----------
 */
int WIN_CaretY() {
    return sCaretY;
}

/*
 * WIN_Scroll
 * ----------
 * Scroll image.
 */
void WIN_Scroll(int dx, int dy) {
    if (!(sImages && sDstImage)) return;
    
    IMG_Scroll(sDstImage, dx, dy);

    AutoRedraw();
}

/*
 * WIN_Sleep
 * ---------
 * Sleep.
 */
void WIN_Sleep(int ms) {
    //Sleep(ms);
    SleepEx(ms, 0);
}

/*
 * WIN_SetClipboardText
 * --------------------
 * Copy txt to clipboard.
 */
void WIN_SetClipboardText(const char *txt) {
    size_t len = strlen(txt);

    if (len > 0 && OpenClipboard(0)) {
        HGLOBAL m = GlobalAlloc(GMEM_MOVEABLE, len + 1);
        memcpy(GlobalLock(m), txt, len + 1);
        GlobalUnlock(m);
        EmptyClipboard();
        SetClipboardData(CF_TEXT, m);
        CloseClipboard();
    }
}

/*
 * WIN_GetClipboardText
 * --------------------
 * Return text from clipboard if present, 0 otherwise.
 */
char *WIN_GetClipboardText() {
    char *txt = 0;
    if (OpenClipboard(0)) {
        HANDLE data = GetClipboardData(CF_TEXT);
        if (data) {
            char *txtData = (char *)GlobalLock(data);
            if (txtData) txt = strdup(txtData);
            GlobalUnlock(txtData);
        }
        CloseClipboard();
    }
    
    return txt;
}

/*
 * WIN_OpenFileDialog
 * ------------------
 * Show a dialog for selecting a file for opening, return filename or 0.
 */
char *WIN_OpenFileDialog(const char *ext) {
	OPENFILENAME ofn;
	char filename[MAX_PATH] = "";
	char filter[64] = "";
    size_t filterlen;

	if (ext && (filterlen = strlen(ext))) {
        strcpy(filter, ext);
        filter[filterlen + 1] = '*';
        filter[filterlen + 2] = '.';
        strcpy(filter + filterlen + 3, ext);
        filter[filterlen*2 + 4] = '\0';
	}
	
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = 0;
	ofn.lpstrFilter = strlen(filter) ? filter : NULL;
	ofn.lpstrFile = filename;
	ofn.lpstrFile[0] = '\0';
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;

	if(GetOpenFileName(&ofn)) return strdup(filename);
	else return 0;
}

/*
 * WIN_SaveFileDialog
 * ------------------
 * Show a dialog for selecting a file for saving, return filename or 0.
 */
char *WIN_SaveFileDialog(const char *ext) {
	OPENFILENAME ofn;
	char filename[MAX_PATH] = "";
	char filter[64] = "";
    size_t filterlen;

	if (ext && (filterlen = strlen(ext))) {
        strcpy(filter, ext);
        filter[filterlen + 1] = '*';
        filter[filterlen + 2] = '.';
        strcpy(filter + filterlen + 3, ext);
        filter[filterlen*2 + 4] = '\0';
	}
	
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = 0;
	ofn.lpstrFilter = strlen(filter) ? filter : NULL;
	ofn.lpstrFile = filename;
	ofn.lpstrFile[0] = '\0';
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER;

	if(GetSaveFileName(&ofn)) return strdup(filename);
	else return 0;
}

/*
 * WIN_DownloadFile
 * ----------------
 * Download file and return as char array
 */
char *WIN_DownloadFile(const char* url, int *bytesDownloaded) {
	HINTERNET connection;
	HINTERNET openAddress;

	if (!(connection = InternetOpen(
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4501.0 Safari/537.36 Edg/91.0.866.0",
		INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0))) {
		return 0;
	}
	if (!(openAddress = InternetOpenUrl(connection, url, NULL, 0, INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_KEEP_CONNECTION, 0))) {
		InternetCloseHandle(connection);
		return 0;
	}


	int bufferSize = 4069;
	int totalBufferSize;
	DWORD totalBytesRead = 0;
	DWORD bytesRead = 0;
	totalBufferSize = 4096;
	char* buffer = (char *)malloc(sizeof(char) * (totalBufferSize + 1));
	while (InternetReadFile(openAddress, &buffer[totalBytesRead], bufferSize, &bytesRead) && bytesRead) {
		totalBytesRead += bytesRead;
		if (totalBytesRead >= totalBufferSize - bufferSize) {
			totalBufferSize += bufferSize;
			buffer = (char *)realloc(buffer, sizeof(char) * totalBufferSize + 1);
		}
	}
	buffer[totalBytesRead] = '\0';
    *bytesDownloaded = totalBytesRead;
	
	InternetCloseHandle(openAddress);
	InternetCloseHandle(connection);

	return buffer;
 }

/*
 * WIN_MessageBox
 * --------------
 */
int WIN_MessageBox(const char *title, const char *msg) {
    MessageBox(sWnd, msg, title, MB_OK);
    return WIN_SUCCESS;
}
