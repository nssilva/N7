/*
 * syscmd.c
 * --------
 *
 * By: Marcus 2021
 */
 
#include "syscmd.h"
#include "naalaa_image.h"
#include "n7mm.h"
#include "windowing.h"
#include "audio.h"
#include "w3d.h"
#include "renv.h"
#include "hash_table.h"

#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include "time.h"
#include "string.h"
#include "math.h"
#include "limits.h"
#include "errno.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CROP(a, l, h) ((a) < (l) ? (l) : ((a) > (h) ? (h) : (a)))

#define IMAGE_SNAPSHOT_IMAGE (SYS_PRIMARY_IMAGE + 1)

#define MAX_FONT_INDEX 16384

#define INKEY_BUFFER_SIZE 64

/* Array with all functions. */
static N7CFunction sSystemCommandFunctions[SYS_CMD_COUNT];

/* For random numbers. */
static unsigned long int sRndNext = 1;

static int sRunning = 0;
static int sHasWindow = 0;
static time_t sStartSec = 0;

static int sJustification = -1;

static char *sWindowMessage = 0;

/* For 'draw poly' and 'draw poly xform'. */
static int *sPolyPoints = 0;
static int sPolyPointsSize = 0;
static float *sPolyPointsF = 0;
static int sPolyPointsFSize = 0;

/* For 'draw poly image'. */
static int *sPolyImagePointsi = 0; /* x and y. */
static float *sPolyImagePointsf = 0; /* Always alloc for u, v and z. */
static int sPolyImagePointCount = 0;

/* For 'draw poly image xform', using a different set than above. */
static float *sPolyImageTPointsi = 0; /* x and y. */
static float *sPolyImageTPointsf = 0; /* Always alloc for u, v and z. */
static int sPolyImageTPointCount = 0;

/* When searching for the key of a value in a table. */
typedef struct {
    Variable value; /* What we're searching for. */
    Variable start; /* Start key, index or string. Unset means we should find
                       the lowest of all. */
    HashEntry *found;
} TableSearchEnv;

/* Files. */
typedef struct {
    FILE *file;
    char mode;
    char binary;
    char eof;
} File;
static HashTable *sFiles;
static void DeleteFile(void *data);

/* Zones. */
typedef struct {
    int id;
    int x;
    int y;
    int w;
    int h;
} Zone;
static HashTable *sZones;
static Zone *sActiveZone;
static Zone *sZoneClicked;
static int sZoneMouseDown;
static void DeleteZone(void *data);

/* Window. */

/* Mouse position. */
static int sMouseX = 0;
static int sMouseY = 0;

/* Mouse buttons. */
static int sMouseButton[3] = {0, 0, 0};
/* In case of mouse down and up during the same sleep period, since we don't
   want programs to miss clicks. Same principle should probably be added to 
   sKeyDown. */
static int sMouseButtonCache[2] = {0, 0};

/* Joystick. */
static int sJoyX = 0;
static int sJoyY = 0;
static int sJoyButtons[4] = {0, 0, 0, 0};

/* Circular buffer to catch the last INKEY_BUFFER_SIZE input chars during
   sleep. */
static unsigned int sInkeyBuffer[INKEY_BUFFER_SIZE];
static int sInkeyHead = 0;
static int sInkeyTail = 0;

/* Key status. */
static char sKeyDown[256];

/*
 * NewNumber
 * ---------
 * Create new numeric variable, for table entries.
 */
static Variable *NewNumber(double value) {
    Variable *var;
    var = (Variable *)MM_Malloc(sizeof(Variable));
    var->type = VAR_NUM;
    var->value.n = value;
    return var;
}

/*
 * NewString
 * ---------
 * Create new string variable, for table entries.
 */
static Variable *NewString(char *value) {
    Variable *var;
    var = (Variable *)MM_Malloc(sizeof(Variable));
    var->type = VAR_STR;
    var->value.s = value;
    return var;
}

/*
 * ClearInkeyBuffer
 * ----------------
 */
static void ClearInkeyBuffer() {
    sInkeyHead = 0;
    sInkeyTail = 0;
}

/*
 * ClearKeyDown
 * ------------
 */
static void ClearKeyDown() {
    for (int i = 0; i < 256; i++) sKeyDown[i] = 0;
}

/*
 * ClearMouseButtons
 * -----------------
 */
static void ClearMouseButtons() {
    sMouseButton[0] = 0;
    sMouseButton[1] = 0;
    sMouseButton[2] = 0;
    sMouseButtonCache[0] = 0;
    sMouseButtonCache[1] = 0;
}

/*
 * Pln
 * ---
 * pln <(string)s>
 *
 * Print string to console.
 */
Variable Pln(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;

    if (argc > 0) printf("%s\n", ToString(&argv[0], 8));
    else printf("\n");

    return result;
}

/*
 * CmdRln
 * ------
 */
static char *CmdRln() {
    int growSize = 16;
    int size = growSize;
    int index = 0;
    char c;

    char *str = (char *)malloc(sizeof(char)*(size + 1));
    while ((c = fgetc(stdin)) != '\n') {
        if (c == EOF) break;
        if (index >= size) {
            size += growSize;
            str = (char *)realloc(str, sizeof(char)*(size + 1));
        }
        str[index++] = c;
    }
    str[index] = '\0';

    return str;
}

/*
 * WinRln
 * ------
 */
static char *WinRln(int maxChars, char type) {
    int caretX = WIN_CaretX(), lsCaretX = WIN_LastSetCaretX(), caretY = WIN_CaretY();
    int autoRedraw = WIN_AutoRedraw();
    int currentImage = WIN_CurrentImage();
    unsigned char r, g, b, a;
    char *buf, *res;
    int size = 0;
    int separators = 0;
    int pos = 0;
    int done = 0;
    int blink = 0;
    
    if (maxChars > 0) {
        buf = (char *)malloc(sizeof(char)*(maxChars + 1));
    }
    else {
        maxChars = 0;
        size = 8;
        buf = (char *)malloc(sizeof(char)*(size + 1));
    }
    buf[pos] = '\0';
    
    WIN_GetColor(&r, &g, &b, &a);
    WIN_CreateImage(IMAGE_SNAPSHOT_IMAGE, WIN_Width(), WIN_Height());
    WIN_SetImage(IMAGE_SNAPSHOT_IMAGE, 0);
    WIN_SetColor(255, 255, 255, 255);
    WIN_DrawImage(SYS_PRIMARY_IMAGE, 0, 0);
    WIN_SetImage(SYS_PRIMARY_IMAGE, 0);
    WIN_SetAutoRedraw(0);
    ClearInkeyBuffer();
    do {
        blink = (blink + 1)%500;
        while (sInkeyTail != sInkeyHead) {
            int c = sInkeyBuffer[sInkeyTail];
 
            sInkeyTail = (sInkeyTail + 1)%INKEY_BUFFER_SIZE;
  
            if (c == 13 || c == 10) {
                done = 1;
            }
            else if (c == 8) {
                if (pos > 0) {
                    if (buf[pos - 1] == '.') separators--;
                    buf[--pos] = '\0';
                    /* ... I guess the buffer could shrink, but ... meh. */
                }
                blink = 0;
            }
            else if (c >= 32) {
                int allowed = 1;
                if (type == VAR_NUM) allowed = (c == '-' && pos == 0) || (c == '.' && !separators) || isdigit(c);
                if (allowed) {
                    if (maxChars) {
                        if (pos < maxChars) {
                            buf[pos++] = (char)c;
                            buf[pos] = '\0';
                            if (c == '.') separators++;
                        }
                    }
                    else {
                        if (pos >= size) {
                            size += 8;
                            buf = (char *)realloc(buf, sizeof(char)*(size + 1));
                        }
                        buf[pos++] = (char)c;
                        buf[pos] = '\0';
                        if (c == '.') separators++;
                    }
                    blink = 0;
                }
            }
        }
        WIN_SetColor(255, 255, 255, 255);
        WIN_DrawImage(IMAGE_SNAPSHOT_IMAGE, 0, 0);
        WIN_SetColor(r, g, b, a);
        WIN_SetCaret(caretX, caretY);
        if (blink < 250 && !done) {
            WIN_Write(buf, sJustification, 0);
            if (sJustification == 0) WIN_SetCaret(caretX + WIN_FontWidth(WIN_CurrentFont(), buf)/2, caretY);
            else if (sJustification > 0) WIN_SetCaret(caretX, caretY);
            WIN_Write("_", -1, 1);
        }
        else {
            WIN_Write(buf, sJustification, 1);
        }
        WIN_Redraw();
        WIN_Update();
        WIN_Sleep(1);
    } while (sRunning && !done);
    ClearInkeyBuffer();
    WIN_SetAutoRedraw(autoRedraw);
    WIN_SetCaret(lsCaretX, WIN_CaretY());
    WIN_FreeImage(IMAGE_SNAPSHOT_IMAGE);
    WIN_SetImage(currentImage, 0);

    res = strdup(buf);
    free(buf);

    return res;
}

/*
 * ReadLine
 * --------
 * (string)rln()
 */
Variable ReadLine(int argc, Variable *argv) {
    Variable result;
    int maxChars;
    char type;

    if (argc > 0) {
        maxChars = (int)ToNumber(&argv[0]);
        maxChars = MAX(maxChars, 0);
    }
    else {
        maxChars = 0;
    }
    if (argc > 1) {
        /* Could add support for tables. */
        type = (char)ToNumber(&argv[1]);
        if (type != VAR_NUM) type = VAR_STR;
    }
    else {
        type = VAR_STR;
    }
    
    result.type = VAR_STR;
    if (sHasWindow) result.value.s = WinRln(maxChars, type);
    else result.value.s = CmdRln();
    if (type == VAR_NUM) ToNumber(&result);
    
    return result;
}

/*
 * DateTime
 * --------
 * (table)datetime([(number)s])
 *
 * Return date and time of day for s, which is assumed to be seconds since epoch,
 * as table. If s is omitted, use current time.
 *   [year: (number), month: (number), day: (number),
 *    hour: (number), minute: (number), second: (number)]
 */
Variable DateTime(int argc, Variable *argv) {
    Variable result;
    time_t t = argc == 0 ? time(0) : (time_t)ToNumber(&argv[0]);
    struct tm tm = *localtime(&t); 
      
    /* Always use NewHashTable instead of HT_Create, since NewHashTable makes
       the table visible to the gc. */
    result.type = VAR_TBL;
    /*result.value.t = NewHashTable(8);*/
    result.value.t = HT_Create(8);

    HT_Add(result.value.t, "year", 0, NewNumber(tm.tm_year + 1900));
    HT_Add(result.value.t, "month", 0, NewNumber(tm.tm_mon + 1));
    HT_Add(result.value.t, "day", 0, NewNumber(tm.tm_mday));    
    HT_Add(result.value.t, "hour", 0, NewNumber(tm.tm_hour));
    HT_Add(result.value.t, "minute", 0, NewNumber(tm.tm_min));
    HT_Add(result.value.t, "second", 0, NewNumber(tm.tm_sec));
    HT_Add(result.value.t, "wday", 0, NewNumber(tm.tm_wday == 0 ? 7 : tm.tm_wday));
    HT_Add(result.value.t, "yday", 0, NewNumber(tm.tm_yday + 1));
    
    MM_SetType(result.value.t, 1);
    
    return result;
}

/*
 * Time
 * ----
 * (number)time([(number)year[, (number)month[, (number)day[, (number)hour[,
 *              (number)minute[, (number)second]]]]]])
 *
 * Return seconds since epoch for specified date and time. For zero arguments,
 * the current time is used.
 */
Variable Time(int argc, Variable *argv) {
    Variable result;
    time_t t;
    
    result.type = VAR_NUM;
    if (argc == 0) {
        t = time(0);
    }
    else {
        struct tm tm;
        tm.tm_sec = argc > 5 ? (int)ToNumber(&argv[5]) : 0;
        tm.tm_min = argc > 4 ? (int)ToNumber(&argv[4]) : 0;
        tm.tm_hour = argc > 3 ? (int)ToNumber(&argv[3]) : 0;
        tm.tm_mday = argc > 2 ? (int)ToNumber(&argv[2]) : 1;
        tm.tm_mon = argc > 1 ? (int)ToNumber(&argv[1]) - 1 : 0;
        tm.tm_year = (int)ToNumber(&argv[0]) - 1900;
        tm.tm_wday = 0;
        tm.tm_yday = 0;
        tm.tm_isdst = -1;
        t = mktime(&tm);
    }
    result.value.n = (double)t;
    
    return result;
}

static long long TimeMS() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec - sStartSec)*1000 + ts.tv_nsec/1000000;
}

/*
 * Clock
 * -----
 * (number)clock()
 *
 * Return time in ms since program execution started.
 */
Variable Clock(int argc, Variable *argv) {
    Variable result;
    struct timespec ts;
    
    result.type = VAR_NUM;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    result.value.n = (double)(ts.tv_sec - sStartSec)*1000 + (double)(ts.tv_nsec/1000000);
    
    return result;
}

/*
 * SleepMS
 * -------
 * wait (number)ms
 * This is when we let the windowing system deal with its stuff.
 */
Variable SleepMS(int argc, Variable *argv) {
    Variable result;  
    int ms;
    long long st;
    long long et;
    long long ct;

    ToNumber(&argv[0]);
    ms = MAX((int)argv[0].value.n, 0);
    st = TimeMS();
    ct = st;
    et = st + ms;

    ClearInkeyBuffer();
    
    /* Clear mousewheel. */
    sMouseButton[2] = 0;
    /* Mouse button cache. */
    if (sMouseButtonCache[0]) sMouseButton[0] = 0;
    if (sMouseButtonCache[1]) sMouseButton[1] = 0;
    sMouseButtonCache[0] = 0;
    sMouseButtonCache[1] = 0;

    /* Let renv perform garbage collecting if needed. */
    GC();
    do {
        WIN_Update();
        WIN_Sleep(1);
        ct = TimeMS();
    } while (ct < et && ct >= st && sRunning);

    if (!sMouseButton[0] && sMouseButtonCache[0]) sMouseButton[0] = 1;
    else sMouseButtonCache[0] = 0;
    if (!sMouseButton[1] && sMouseButtonCache[1]) sMouseButton[1] = 1;
    else sMouseButtonCache[1] = 0;

    /* On Windows, if tv_nsec < 1000000 (even 999999) the precision is super
       high, but the CPU goes apeshit. If tv_nsec >= 1000000, the precision all
       of a sudden lands at ~15ms and the CPU usage drops to nothing. We need
       high precision without requiering that 100% of a CPU core is used. So I
       added WIN_Sleep to windowing.h and wrote Windows specific code in 
       windowing_winapi.c. Hopefully nanosleep works better on Linux. */
 
    /*int ms;
    long long st;
    long long et;
    long long ct;
    struct timespec ts;
    int res;

    ToNumber(&argv[0]);
    ms = MAX((int)argv[0].value.n, 0);
    st = TimeMS();
    ct = st;
    et = st + ms;

    ClearInkeyBuffer();
    do {
        WIN_Update();
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000;
        do {
            res = nanosleep(&ts, &ts);
        } while (res && errno == EINTR);
        ct = TimeMS();
    } while (ct < et && ct >= st && sRunning);
    */

    result.type = VAR_UNSET;

    return result;
}

Variable FrameSleepMS(int argc, Variable *argv) {
    static long long nextFrameTime = 0;
    static int framesSkipped = 0;
    Variable result;
    long long t;
    int hold = (int)ToNumber(&argv[0]);
    int ok;
    
    hold = 1000/MAX(hold, 1);
    
    t = TimeMS();
    if (abs(t - nextFrameTime) > hold*10) nextFrameTime = t;
    
    ok = t < nextFrameTime;
    framesSkipped += !ok;
    if (framesSkipped >= 4) {
        framesSkipped = 0;
        ok = 1;
    }

    /* Clear mousewheel. */
    sMouseButton[2] = 0;
    /* Mouse button cache. */
    if (sMouseButtonCache[0]) sMouseButton[0] = 0;
    if (sMouseButtonCache[1]) sMouseButton[1] = 0;
    sMouseButtonCache[0] = 0;
    sMouseButtonCache[1] = 0;
 
    ClearInkeyBuffer();
    
    /* Let renv perform garbage collecting if needed. */
    GC();
    do {
        WIN_Update();
        WIN_Sleep(1);
        t = TimeMS();
    } while (t < nextFrameTime && sRunning);
    nextFrameTime = nextFrameTime + hold;

    if (!sMouseButton[0] && sMouseButtonCache[0]) sMouseButton[0] = 1;
    else sMouseButtonCache[0] = 0;
    if (!sMouseButton[1] && sMouseButtonCache[1]) sMouseButton[1] = 1;
    else sMouseButtonCache[1] = 0;
    
    result.type = VAR_NUM;
    result.value.n = ok;

    return result;
}

/*
 * Rnd
 * ---
 * (number)rnd([(number)n[, (number)m]])
 *
 * Return a random number in different ways depending on the number of
 * argumens:
 *   rnd() returns a float value in the range [0..1]
 *   rnd(n) returns an int value in the range [0..n - 1]
 *   rnd(n, m) returns an int value in the range [n..m]
 * The arguments are always converted to int.
 */
Variable Rnd(int argc, Variable *argv) {
    Variable result;
    int value;
    
    result.type = VAR_NUM;

    sRndNext = sRndNext*1103515245 + 12345;
    value = (int)((sRndNext/65536)%32768);

    if (argc == 2) {
        int minValue, maxValue;
        int delta;
        
        ToNumber(&argv[0]);
        ToNumber(&argv[1]);
        
        if (argv[0].value.n < argv[1].value.n) {
            minValue = (int)floor(argv[0].value.n);
            maxValue = (int)floor(argv[1].value.n);
        }
        else {
            minValue = (int)floor(argv[1].value.n);
            maxValue = (int)floor(argv[0].value.n);
        }
        delta = maxValue - minValue;
        result.value.n =  (double)(minValue + value%(delta + 1));
    }
    else if (argc == 1) {
        int range = (int)floor(ToNumber(&argv[0]));
        if (range > 0) result.value.n = (double)(value%range);
        else if (range < 0) result.value.n = -(double)(value%(-range));
        else result.value.n = 0;
    }
    else {
        result.value.n = (double)value/32768;
    }
    
    return result;
}

/*
 * Randomize
 * ---------
 * randomize (number)seed
 *
 * Set seed value for rnd.
 */
Variable Randomize(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    sRndNext = (unsigned long int)floor(ToNumber(&argv[0]));
    
    return result;
}

/* Helper. */
int ReadParam(char *dst, const char *src) {
    const char *buf = src;
    while (*buf == ' ' || *buf == '\t') buf++;
    if (*buf == '\"') {
        buf++;
        while (*buf && *buf != '\"') {
            *dst = *buf;
            dst++;
            buf++;
        }
        if (*buf == '\"') buf++;
    }
    else {
        while (*buf && !(*buf == ' ' || *buf == '\t')) {
            *dst = *buf;
            dst++;
            buf++;
        }
    }
    *dst = '\0';
    return buf - src;
}

/*
 * System
 * ------
 * system (string)command
 *
 * Execute system command.
 */
Variable System(int argc, Variable *argv) {
    Variable result;
    char *cmd;
    
    result.type = VAR_UNSET;

    cmd = ToString(&argv[0], 8);

    /* Some stuff that may get their own commands later on. */
    if (strncmp(cmd, "n7:winmsg ", 10) == 0) {
        char window[128];
        char message[256];
        int offset = 10;
        offset += ReadParam(window, cmd + offset);
        ReadParam(message, cmd + offset);
        WIN_SendMessage(window, message);
    }
    else if (strncmp(cmd, "n7:winshow", 10) == 0) {
        WIN_Show();
    }
    else {
        system(cmd);
    }

    return result;
}

/*
 * Capture
 * -------
 * (string)system((string)command)
 *
 * Execute system command and return output.
 */
Variable Capture(int argc, Variable *argv) {
    Variable result;
    char *cmd;

    cmd = ToString(&argv[0], 8);

    if (strncmp(cmd, "n7:winmsg", 9) == 0) {
        if (sWindowMessage) {
            result.type = VAR_STR;
            result.value.s = sWindowMessage;
            sWindowMessage = 0;
            return result;
        }
    }
    else {
        FILE *file;
            
        /* Ugh, this "portable" approach causes an empty console window to appear if
           the executing program was compiled to a win32 application. */
        file = popen(cmd, "r");
        if (file) {
            char buffer[64];
            char *res = 0;
            size_t currentLength = 0;
            while (fgets(buffer, sizeof(buffer), file) != 0) {
                size_t bufferLength = strlen(buffer);
                char *extra = (char *)realloc(res, bufferLength + currentLength + 1);
                if (extra == 0) break;
                res = extra;
                strcpy(res + currentLength, buffer);
                currentLength += bufferLength;
            }
            pclose(file);
            if (res) {
                result.type = VAR_STR;
                result.value.s = res;
                return result;
            }
        }
    }
    
    result.type = VAR_UNSET;

    return result;
}

/*
 * SplitStr
 * --------
 * (array)split((string)s, (string)f)
 *
 * Return an array containing the substrings of s split at every occurence of f.
 */
Variable SplitStr(int argc, Variable *argv) {
    Variable result;
    char *last;
    char *fpos;
    /*char name[16];*/
    int slen, flen;
    int count = 0;
    
    result.type = VAR_TBL;
    result.value.t = HT_Create(1);//NewHashTable(1);
    
    ToString(&argv[0], 8);
    ToString(&argv[1], 8);

    slen = strlen(argv[0].value.s);
    flen = strlen(argv[1].value.s);
    
    last = argv[0].value.s;
    fpos = strstr(last, argv[1].value.s);
    while (fpos) {
        int len = fpos - last;
        char *sub;
        if (len > 0) {
            sub = (char *)malloc(sizeof(char)*(len + 1));
            strncpy(sub, last, len);
            sub[len] = '\0';
            HT_Add(result.value.t, 0, count, NewString(sub));
            count++;
        }
        last = fpos + flen;
        fpos = strstr(last, argv[1].value.s);
    }
    if (last - argv[0].value.s < slen) {
        int len = slen - (last - argv[0].value.s);
        char *sub = (char *)malloc(sizeof(char)*(len + 1));
        strncpy(sub, last, len);
        sub[len] = '\0';
        HT_Add(result.value.t, 0, count, NewString(sub));
    }
    
    MM_SetType(result.value.t, 1);
    
    return result;
}

/*
 * LeftStr
 * -------
 * (string)left((string)s, (number)pos)
 */
Variable LeftStr(int argc, Variable *argv) {
    Variable result;
    char *src;
    int slen;        
    int pos;
    int len;
    char *dst;

    ToString(&argv[0], 8);
    ToNumber(&argv[1]);
    src = argv[0].value.s;
    slen = strlen(src);        
    pos = MAX((int)argv[1].value.n, 0);
    len = MIN(pos, slen);
    dst = (char *)malloc(sizeof(char)*(len + 1));

    
    memcpy(dst, src, len);
    dst[len] = '\0';
    
    result.type = VAR_STR;
    result.value.s = dst;
    
    return result;
}

/*
 * RightStr
 * --------
 * (string)right((string)s, (number)pos)
 */
Variable RightStr(int argc, Variable *argv) {
    Variable result;
    char *src;
    int len;
    int pos;

    ToString(&argv[0], 8);
    ToNumber(&argv[1]);
    src = argv[0].value.s;
    len = strlen(src);
    pos = MAX((int)argv[1].value.n, 0);
  
    result.type = VAR_STR;
    result.value.s = strdup(src + MIN(pos, len)); 
    
    return result;
}

/*
 * MidStr
 * ------
 * (string)mid((string)s, (number)pos[, (number)len])
 */
Variable MidStr(int argc, Variable *argv) {
    Variable result;
    char *src;
    int slen;
    int pos;
    int len;
    char *dst;    

    ToString(&argv[0], 8);
    ToNumber(&argv[1]);

    src = argv[0].value.s;
    slen = strlen(src);
    pos = MAX((int)argv[1].value.n, 0);
    if (argc > 2) {
        ToNumber(&argv[2]);
        len = MAX((int)argv[2].value.n, 0);
    }
    else {
        len = 1;
    }
    
    pos = MIN(pos, slen);
    len = MIN(len, slen - pos);
    dst = (char *)malloc(sizeof(char)*(len + 1));
    memcpy(dst, src + pos, len);
    dst[len] = '\0';
    
    result.type = VAR_STR;
    result.value.s = dst;
    
    return result;
}

/*
 * InStr
 * -----
 * (number)instr((string)s, (string)sub[, pos])
 */
Variable InStr(int argc, Variable *argv) {
    Variable result;
    char *src;
    int len;
    char *sub;
    int pos;
    char *res;

    src = ToString(&argv[0], 8);
    len = strlen(src);
    sub = ToString(&argv[1], 8);
    if (argc > 2) {
        ToNumber(&argv[2]);
        pos = MAX((int)argv[2].value.n, 0);
    }
    else {
        pos = 0;
    }

    result.type = VAR_NUM;

    if (*sub == '\0') {
        result.value.n = -1;
    }
    else {
        pos = MIN(pos, len);    
        result.value.n = (res = strstr(src + pos, sub)) ? res - src : -1;
    }
    
    return result;
}

/*
 * ReplaceStr
 * ----------
 * (string)replace((string)s, (string)sub, (string)rep[, (number)pos])
 *
 * The three arguments version replaces all occurences of sub with rep in s and
 * returns a new string. The four arguments version replaces only one occurence,
 * and the search starts at the specified character position.
 */
Variable ReplaceStr(int argc, Variable *argv) {
    Variable result;
    char *src = ToString(&argv[0], 8);
    char *sub = ToString(&argv[1], 8);
    char *rep = ToString(&argv[2], 8);
    size_t sublen = strlen(sub);
    size_t replen = strlen(rep);
    char *dst = 0;
    
    if (sublen == 0) {
        /* Yes, we can do this, because we know that argv[0] is an expresion. */
        dst = src;
        argv[0].type = VAR_UNSET;
    }
    else {
        char *s;
        size_t len = 0;

        if (argc > 3) {
            size_t pos;
            ToNumber(&argv[3]);
            pos = MAX((int)argv[3].value.n, 0);
            pos = MIN(pos, strlen(src));
            len = pos;
            dst = (char *)malloc(sizeof(char)*(len + 1));
            memcpy(dst, src, len);
            src += pos;
        }
        else {
            dst = (char *)malloc(sizeof(char)*(len + 1));
        }
        s = strstr(src, sub);
        while (s) {
            dst = (char *)realloc(dst, sizeof(char)*(len + s - src + replen + 1));
            memcpy(dst + len, src, s - src);
            memcpy(dst + len + (s - src), rep, replen);
            len += s - src + replen;
            src = s + sublen;
            if (argc > 3) break; /* Meh. */
            s = strstr(src, sub);
        }
        dst[len] = '\0';
        dst = (char *)realloc(dst, sizeof(char)*(len + strlen(src) + 1)); 
        strcat(dst, src);
    }
    
    result.value.s = dst;
    result.type = VAR_STR;
    
    return result;
}

/*
 * LowerStr
 * --------
 * (string)lower((string)s)
 */
Variable LowerStr(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_STR;
    /* argv[0].value.s is the result of a string expression and will be deleted
       after this function call. So we can just assign it to the result and
       unset argv[0]. */
    result.value.s = strlwr(ToString(&argv[0], 8));
    argv[0].type = VAR_UNSET;
    
    return result;
}

/*
 * UpperStr
 * --------
 * (string)upper((string)s)
 */
Variable UpperStr(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_STR;
    result.value.s = strupr(ToString(&argv[0], 8));
    argv[0].type = VAR_UNSET;
    
    return result;
}

/*
 * Chr
 * ---
 * (string)chr((number)n)
 */
Variable Chr(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_STR;
    result.value.s = (char *)malloc(sizeof(char)*2);
    result.value.s[0] = (char)ToNumber(&argv[0]);
    result.value.s[1] = '\0';
    
    return result;
}

/*
 * Asc
 * ---
 * (number)asc((string)s)
 */
Variable Asc(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = ToString(&argv[0], 8)[0];
    
    return result;
}

/*
 * Str
 * ---
 * (string((number)n, int_digits, float_digits)
 *
 * Has replaced ASM_TOSTR, no longer an instruction.
 */
static char sToStrBuf[512];
Variable Str(int argc, Variable *argv) {
    Variable result;
    double value;
    int intDigits, decDigits, totChars;
    int res;
    
    if (argc == 1) {
        ToString(&argv[0], 8);
        result.type = VAR_STR;
        result.value.s = argv[0].value.s;
        argv[0].type = VAR_UNSET;
        return result;
    }

    if (argc > 2) {
        decDigits = ToNumber(&argv[2]);
        decDigits = MIN(MAX(decDigits, 0), 127);
    }
    else {
        decDigits = 0;
    }
    
    value = ToNumber(&argv[0]);
    intDigits = ToNumber(&argv[1]);
    intDigits = MIN(MAX(intDigits, 0), 127);
    
    totChars = intDigits;
    if (value < 0) totChars++;
    if (decDigits > 0) totChars += decDigits + 1;
    
    res = snprintf(sToStrBuf, 512, "%0*.*f", totChars, decDigits, value);
    result.type = VAR_STR;
    if (res < 0 || res >= 512) result.value.s = strdup("Error");
    else result.value.s = strdup(sToStrBuf);
    
    return result;
}

/*
 * TblHasKey
 * ---------
 * (number)key((table)t, (string/number)k)
 */
Variable TblHasKey(int argc, Variable *argv) {
    Variable result;
   
    result.type = VAR_NUM;

    if (argv[0].type == VAR_TBL) {
        if (argv[1].type == VAR_NUM) result.value.n = HT_Exists(argv[0].value.t, 0, (int)argv[1].value.n);
        else if (argv[1].type == VAR_STR) result.value.n = HT_Exists(argv[0].value.t, argv[1].value.s, 0);
        else result.value.n = 0;
    }
    else {
        result.value.n = 0;
    }
    
    return result;
}

int SearchEqualVariable(void *data, void *userData) {
    return EqualVariables((Variable *)data, (Variable *)userData);
}

/*
 * TblHasValue
 * ----------
 * (number)val((table)t, v)
 */
Variable TblHasValue(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    
    if (argv[0].type == VAR_TBL) {
        result.value.n = HT_FindEntry(argv[0].value.t, SearchEqualVariable, &argv[1]) ? 1 : 0;
    }
    /* Uhm ... */
    else {
        result.value.n = EqualVariables(&argv[0], &argv[1]);
    }
    
    return result;
}

void FindLowestStringKey(char key, int ikey, void *data, void *userData) {
}

/*
 * TblKeyOf
 * --------
 * (number/string)keyof((table)t, v[, startKey)
 * If no startIndex: return key with lowest index if numeric keys exist, else
 * the lowest string.
 * With startIndex: return key with lowest index starting at startIndex
 * But how do we handle string keys? If startIndex IS a string, do strcmp
 * comparison, ignore all numeric keys.
 */
Variable TblKeyOf(int argc, Variable *argv) {
    Variable result;
    
    
    result.type = VAR_UNSET;
    /*if (argv[0].type == VAR_TBL) {
        if (argc > 2) {
            if (argv[2].type == VAR_STR) {
                HT_ApplyKeyFunction(argv[0].value.t, void (*func)(char *, int, void *, void *), void *userData)
            }
            else {
                
            }
        }
        else {
        }
    }*/
    
    return result;
}

static void DeleteTableIndex(HashTable *ht, int index) {
    HashEntry *e = HT_GetEntry(ht, 0, index);
    if (e) {
        HashEntry *next;

        DeleteVariable(e->data);
        e->data = 0;
                    
        while ((next = HT_GetEntry(ht, 0, ++index))) {
            e->data = next->data;
            next->data = 0;
            e = next;
        }
                    
        HT_Delete(ht, 0, e->ikey, DeleteVariable);
    }
 }

/*
 * TblFreeKey
 * ----------
 * free key (table)t, (string/number)k
 */
Variable TblFreeKey(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    if (argv[0].type == VAR_TBL) {
        if (!argv[0].value.t->lock) {
            if (argv[1].type == VAR_NUM) {
                /* Delete and re-arrange. */
                int n = (int)argv[1].value.n;
                DeleteTableIndex(argv[0].value.t, n);
            }
            else if (argv[1].type == VAR_STR) {
                HT_Delete(argv[0].value.t, argv[1].value.s, 0, DeleteVariable);
            }
        }
        else {
            RuntimeError("Table is locked (SYS_FREE_KEY)");
        }
    }
    
    return result;
}

/*
 * TblFreeValue
 * ------------
 * free value (table)t, v
 */
Variable TblFreeValue(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    if (argv[0].type == VAR_TBL) {
        if (!argv[0].value.t->lock) { 
            /* This works, but we don't get re-indexing of arrays. */
            HashEntry **entries = HT_GetEntriesArray(argv[0].value.t);
            if (entries) {
                int indexedCount = 0;
                int minIndex = INT_MAX;
                int maxIndex = INT_MIN;
                int indexedRemoved = 0;
                int minIndexRemoved = INT_MAX;
                int maxIndexRemoved = INT_MIN;
                
                HashEntry **e = entries;
                while (*e) {
                    if (!(*e)->skey) {
                        indexedCount++;
                        minIndex = MIN(minIndex, (*e)->ikey);
                        maxIndex = MAX(maxIndex, (*e)->ikey);
                    }
                    if (EqualVariables((Variable *)(*e)->data, &argv[1])) {
                        if (!(*e)->skey) {
                            indexedRemoved++;
                            minIndexRemoved = MIN(minIndexRemoved, (*e)->ikey);
                            maxIndexRemoved = MAX(maxIndexRemoved, (*e)->ikey);
                        }
                        HT_Delete(argv[0].value.t, (*e)->skey, (*e)->ikey, DeleteVariable);
                    }
                    e++;
                }
                MM_Free(entries);
                /* Reindex if it was without holes before only? Idk. */
                if (indexedRemoved > 0 && maxIndex - minIndex == indexedCount - 1) {
                    HT_ReIndex(argv[0].value.t, minIndex, maxIndex);
                }
            }
        }
        else {
            RuntimeError("Table is locked (SYS_FREE_VALUE)");
        }
    }
    
    return result;
}

/*
 * TblClear
 * --------
 */
Variable TblClear(int argc, Variable *argv) {
    Variable result;
    
    if (argv[0].type == VAR_TBL) {
        HT_Clear(argv[0].value.t, DeleteVariable);
    }
    
    result.type = VAR_UNSET;
    
    return result;
}

static void InsertTableIndex(HashTable *ht, int index, Variable *v) {
    HashEntry *e = HT_GetEntry(ht, 0, index);
    while (e) {
        Variable *prev = (Variable *)e->data;
        e->data = v;
        index++;
        e = HT_GetEntry(ht, 0, index);
        v = prev;
    }
    if (v) HT_Add(ht, 0, index, v);
}

/*static void InsertTableLast(HashTable *ht, Variable *v) {
    if (ht->entries == 0 || (HT_Exists(ht, 0, ht->entries - 1) && !HT_Exists(ht, 0, ht->entries)) {
        HT_Add(ht, 0, ht->entries, v);
    }
    else {
        
    }
}*/

/*
 * TblInsert
 * ---------
 */
Variable TblInsert(int argc, Variable *argv) {
    Variable result;

    if (argv[0].type == VAR_TBL) {
        if (argc == 3) {
            Variable *v = (Variable *)MM_Malloc(sizeof(Variable));
            *v = argv[2];
            if (v->type == VAR_STR) v->value.s = strdup(v->value.s);
            InsertTableIndex(argv[0].value.t, (int)ToNumber(&argv[1]), v);
        }
        else {
        }
    }
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * SetClipboard
 * ------------
 */
Variable SetClipboard(int argc, Variable *argv) {
    Variable result;
    
    WIN_SetClipboardText(ToString(&argv[0], 8));
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * GetClipboard
 * ------------
 */
Variable GetClipboard(int argc, Variable *argv) {
    Variable result;
    char *txt;
   
    result.type = VAR_STR;
    if ((txt = WIN_GetClipboardText())) result.value.s = txt;
    else result.value.s = strdup("");
    
    return result;
}

/*
 * CreateFile
 * ----------
 */
Variable CreateFile(int argc, Variable *argv) {
    Variable result;
    FILE *f;
    char binary = argc > 1 && (int)ToNumber(&argv[1]);
    
    f = fopen(ToString(&argv[0], 8), binary ? "wb" : "w");
    if (f) {
        File *file;
        int i = 1;
        
        while (HT_Exists(sFiles, 0, i)) i++;
        file = (File *)malloc(sizeof(File));
        file->file = f;
        file->mode = 'w';
        file->binary = binary;
        file->eof = 0;
        HT_Add(sFiles, 0, i, file);
        result.type = VAR_NUM;
        result.value.n = i;
        
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * CreateFileLegacy
 * ----------------
 */
Variable CreateFileLegacy(int argc, Variable *argv) {
    Variable result;
    int id = (int)ToNumber(&argv[0]);
    
    if (HT_Exists(sFiles, 0, id)) {
        char tmp[64];

        sprintf(tmp, "File %d is already in use (SYS_CREATE_FILE_LEGACY)", id);
        RuntimeError(tmp);
    }
    else {
        char binary = argc > 2 && (int)ToNumber(&argv[2]);
        FILE *f = fopen(ToString(&argv[1], 8), binary ? "wb" : "w");

        if (f) {
            File *file = (File *)malloc(sizeof(File));
            file->file = f;
            file->mode = 'w';
            file->binary = binary;
            file->eof = 0;
            HT_Add(sFiles, 0, id, file);
        }
    }
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * OpenFile
 * --------
 */
Variable OpenFile(int argc, Variable *argv) {
    Variable result;
    FILE *f;
    char binary = argc > 1 && (int)ToNumber(&argv[1]);
    
    f = fopen(ToString(&argv[0], 8), binary ? "rb" : "r");
    if (f) {
        File *file;
        int i = 1;
        
        while (HT_Exists(sFiles, 0, i)) i++;
        file = (File *)malloc(sizeof(File));
        file->file = f;
        file->mode = 'r';
        file->binary = binary;
        file->eof = 0;
        HT_Add(sFiles, 0, i, file);
        result.type = VAR_NUM;
        result.value.n = i;
        
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * OpenFileLegacy
 * --------------
 */
Variable OpenFileLegacy(int argc, Variable *argv) {
    Variable result;
    int id = (int)ToNumber(&argv[0]);
    
    if (HT_Exists(sFiles, 0, id)) {
        char tmp[64];

        sprintf(tmp, "File %d is already in use (SYS_OPEN_FILE_LEGACY)", id);
        RuntimeError(tmp);
    }
    else {
        char binary = argc > 2 && (int)ToNumber(&argv[2]);
        FILE *f = fopen(ToString(&argv[1], 8), binary ? "rb" : "r");

        if (f) {
            File *file = (File *)malloc(sizeof(File));
            file->file = f;
            file->mode = 'r';
            file->binary = binary;
            file->eof = 0;
            HT_Add(sFiles, 0, id, file);
        }
    }
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * FreeFile
 * --------
 */
Variable FreeFile(int argc, Variable *argv) {
    Variable result;
    
    HT_Delete(sFiles, 0, (int)ToNumber(&argv[0]), DeleteFile);
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * FileExists
 * ----------
 */
Variable FileExists(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = HT_Exists(sFiles, 0, (int)ToNumber(&argv[0]));
    
    return result;
}

/*
 * FileWrite
 * ---------
 */
Variable FileWrite(int argc, Variable *argv) {
    Variable result;
    File *file;
    int id = (int)ToNumber(&argv[0]);
    
    if ((file = (File *)HT_Get(sFiles, 0, id))) {
        if (file->mode == 'w') {
            if (file->binary) {
                if (argc > 2) {
                    int bytes = (int)ToNumber(&argv[2]);
                    int bytesError = 0;
                    if (argc > 3 && !(int)ToNumber(&argv[3])) { /* Unsigned? */
                        switch (bytes) {
                            case 8: {
                                unsigned char data = (unsigned char)ToNumber(&argv[1]);
                                fwrite(&data, sizeof(unsigned char), 1, file->file);
                                break;
                            }
                            case 16: {
                                unsigned short data = (unsigned short)ToNumber(&argv[1]);
                                fwrite(&data, sizeof(unsigned short), 1, file->file);
                                break;
                            }
                            case 32: {
                                unsigned int data = (unsigned int)ToNumber(&argv[1]);
                                fwrite(&data, sizeof(unsigned int), 1, file->file);
                                break;
                            }
                            case 64: {
                                float data = (float)ToNumber(&argv[1]);
                                fwrite(&data, sizeof(float), 1, file->file);
                                break;
                            }
                            default: {
                                bytesError = 1;
                                break;
                            }
                        }
                    }
                    else {
                        switch (bytes) {
                            case 8: {
                                char data = (char)ToNumber(&argv[1]);
                                fwrite(&data, sizeof(char), 1, file->file);
                                break;
                            }
                            case 16: {
                                short data = (short)ToNumber(&argv[1]);
                                fwrite(&data, sizeof(short), 1, file->file);
                                break;
                            }
                            case 32: {
                                int data = (int)ToNumber(&argv[1]);
                                fwrite(&data, sizeof(int), 1, file->file);
                                break;
                            }
                            case 64: {
                                double data = ToNumber(&argv[1]);
                                fwrite(&data, sizeof(double), 1, file->file);
                                break;
                            }
                            default: {
                                bytesError = 1;
                                break;
                            }
                        }
                    }
                    if (bytesError) {
                        char tmp[64];
                        sprintf(tmp, "%d is an invalid data size (SYS_FILE_WRITE)", bytes);
                        RuntimeError(tmp);
                    }
                }
                else {
                    /* Write as string, include the null character. */
                    char *s = ToString(&argv[1], 8);
                    fwrite(s, sizeof(char), strlen(s) + 1, file->file);
                }
            }
            else {
                fprintf(file->file, "%s", ToString(&argv[1], 8));
            }
        }
        else {
            char tmp[64];
            sprintf(tmp, "File %d is read only (SYS_FILE_WRITE)", id);
            RuntimeError(tmp);
        }
    }
    else {
        char tmp[64];
        sprintf(tmp, "File %d doesn't exist (SYS_FILE_WRITE)", id);
        RuntimeError(tmp);
    }
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * FileWriteLine
 * -------------
 */
Variable FileWriteLine(int argc, Variable *argv) {
    Variable result;
    File *file;
    int id = (int)ToNumber(&argv[0]);
    
    if ((file = (File *)HT_Get(sFiles, 0, id))) {
        if (file->mode == 'w') {
            if (file->binary) {
                const char *eol = "\n";
                if (argc > 1) {
                    char *s = ToString(&argv[1], 8);
                    fwrite(s, sizeof(char), strlen(s), file->file);
                }
                fwrite(eol, sizeof(char), strlen(eol) + 1, file->file);
            }
            else {
                if (argc > 1) fprintf(file->file, "%s\n", ToString(&argv[1], 8));
                else fprintf(file->file, "\n");
            }
        }
        else {
            char tmp[96];
            sprintf(tmp, "File %d is read only (SYS_FILE_WRITE_LINE)", id);
            RuntimeError(tmp);
        }
    }
    else {
        char tmp[96];
        sprintf(tmp, "File %d doesn't exist (SYS_FILE_WRITE_LINE)", id);
        RuntimeError(tmp);
    }
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * FileTell
 * --------
 * filetell((number)file_id)
 */
Variable FileTell(int argc, Variable *argv) {
    Variable result;
    File *file;
    int id = (int)ToNumber(&argv[0]);
    
    result.type = VAR_UNSET;
    
    if ((file = (File *)HT_Get(sFiles, 0, id))) {
        result.type = VAR_NUM;
        result.value.n = (double)ftell(file->file);
    }
    else {
        char tmp[64];
        sprintf(tmp, "File %d doesn't exist (SYS_FILE_TELL)", id);
        RuntimeError(tmp);
    }
    
    return result;
}

/*
 * FileSeek
 * --------
 * file seek (number)file_id, (number)offs[, (number)whence = 0]
 * SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2
 */
Variable FileSeek(int argc, Variable *argv) {
    Variable result;
    File *file;
    int id = (int)ToNumber(&argv[0]);
    long int offs = (long int)ToNumber(&argv[1]);
    int whence = argc > 2 ? (int)ToNumber(&argv[2]) : -1;
    
    result.type = VAR_NUM;
    result.value.n = 0;
    if ((file = (File *)HT_Get(sFiles, 0, id))) {
        if (file->mode == 'r') {
            if (whence == 0) {
                result.value.n = (double)fseek(file->file, offs, SEEK_SET) == 0 ? 1 : 0;
            }
            else if (whence == 1) {
                result.value.n = (double)fseek(file->file, offs, SEEK_CUR) == 0 ? 1 : 0;
            }
            else if (whence == 2) {
                result.value.n = (double)fseek(file->file, offs, SEEK_END) == 0 ? 1 : 0;
            }
        }
        else {
            char tmp[64];
            sprintf(tmp, "File %d is write only (SYS_FILE_SEEK)", id);
            RuntimeError(tmp);
        }
    }
    else {
        char tmp[64];
        sprintf(tmp, "File %d doesn't exist (SYS_FILE_SEEK)", id);
        RuntimeError(tmp);
    }
    
    return result;
}

/*
 * FileRead
 * --------
 */
Variable FileRead(int argc, Variable *argv) {
    Variable result;    
    File *file;
    int id = (int)ToNumber(&argv[0]);

    result.type = VAR_UNSET;

    if ((file = (File *)HT_Get(sFiles, 0, id))) {
        if (file->mode == 'r') {
            if (!file->eof) {
                if (file->binary) {
                    if (argc > 1) {
                        int bytes = (int)ToNumber(&argv[1]);
                        int bytesError = 0;
                        if (argc > 2 && !(int)ToNumber(&argv[2])) { /* Unsigned? */
                            switch (bytes) {
                                case 8: {
                                    unsigned char data;
                                    if (!(file->eof = !fread(&data, sizeof(unsigned char), 1, file->file))) {
                                        result.type = VAR_NUM;
                                        result.value.n = data;
                                    }
                                    break;
                                }
                                case 16: {
                                    unsigned short data;
                                    if (!(file->eof = !fread(&data, sizeof(unsigned short), 1, file->file))) {
                                        result.type = VAR_NUM;
                                        result.value.n = data;
                                    }
                                    break;
                                }
                                case 32: {
                                    unsigned int data;
                                    if (!(file->eof = !fread(&data, sizeof(unsigned int), 1, file->file))) {
                                        result.type = VAR_NUM;
                                        result.value.n = data;
                                    }
                                    break;
                                }
                                case 64: {
                                    float data;
                                    if (!(file->eof = !fread(&data, sizeof(float), 1, file->file))) {
                                        result.type = VAR_NUM;
                                        result.value.n = data;
                                    }
                                    break;
                                }
                                default: {
                                    bytesError = 1;
                                    break;
                                }
                            }
                        }
                        else {
                            switch (bytes) {
                                case 8: {
                                    char data;
                                    if (!(file->eof = !fread(&data, sizeof(char), 1, file->file))) {
                                        result.type = VAR_NUM;
                                        result.value.n = data;
                                    }
                                    break;
                                }
                                case 16: {
                                    short data;
                                    if (!(file->eof = !fread(&data, sizeof(short), 1, file->file))) {
                                        result.type = VAR_NUM;
                                        result.value.n = data;
                                    }
                                    break;
                                }
                                case 32: {
                                    int data;
                                    if (!(file->eof = !fread(&data, sizeof(int), 1, file->file))) {
                                        result.type = VAR_NUM;
                                        result.value.n = data;
                                    }
                                    break;
                                }
                                case 64: {
                                    double data;
                                    if (!(file->eof = !fread(&data, sizeof(double), 1, file->file))) {
                                        result.type = VAR_NUM;
                                        result.value.n = data;
                                    }
                                    break;
                                }
                                default: {
                                    bytesError = 1;
                                    break;
                                }
                            }
                        }
                        if (bytesError) {
                            char tmp[64];
                            sprintf(tmp, "%d is an invalid data size (SYS_FILE_READ)", bytes);
                            RuntimeError(tmp);
                        }
                    }
                    else { /* String. */
                        int growSize = 16;
                        int size = growSize;
                        int index = 0;
                        char c;
                        char *str = (char *)malloc(sizeof(char)*(size + 1));
                        
                        while (!(file->eof = !fread(&c, sizeof(char), 1, file->file)) && c != '\0') {
                            if (index >= size) {
                                size += growSize;
                                str = (char *)realloc(str, sizeof(char)*(size + 1));
                            }
                            str[index++] = c;
                        }
                        str[index] = '\0';                        
                        if (index > 0) {
                            result.type = VAR_STR;
                            result.value.s = strdup(str);
                        }
                        free(str);
                    }
                }
                else {
                    int growSize = 16;
                    int size = growSize;
                    int index = 0;
                    int quoted = 0;
                    int c;

                    char *str = (char *)malloc(sizeof(char)*(size + 1));
                    c = fgetc(file->file);
                    while (c == '\n' || c == '\t' || c == ' ') c = fgetc(file->file);
                    if (c == '"') {
                        c = fgetc(file->file);
                        quoted = 1;
                        while (!(c == '"' || c == EOF)) {
                            if (index >= size) {
                                size += growSize;
                                str = (char *)realloc(str, sizeof(char)*(size + 1));
                            }
                            str[index++] = c;
                            c = fgetc(file->file);
                        }
                    }
                    else {
                        while (!(c == '\n' || c == '\t' || c == ' ' || c == EOF)) {
                            if (index >= size) {
                                size += growSize;
                                str = (char *)realloc(str, sizeof(char)*(size + 1));
                            }
                            str[index++] = c;
                            c = fgetc(file->file);
                        }
                    }
                    file->eof = c == EOF;
                    str[index] = '\0';
                    if (index > 0 || quoted) {
                        result.type = VAR_STR;
                        result.value.s = strdup(str);
                    }
                    free(str);
                }
            }
        }
        else {
            char tmp[64];
            sprintf(tmp, "File %d is write only (SYS_FILE_READ)", id);
            RuntimeError(tmp);
        }
    }
    else {
        char tmp[64];
        sprintf(tmp, "File %d doesn't exist (SYS_FILE_READ)", id);
        RuntimeError(tmp);
    }
    
    return result;
}

Variable FileReadChar(int argc, Variable *argv) {
    Variable result;
    
    File *file;
    int id = (int)ToNumber(&argv[0]);

    result.type = VAR_UNSET;

    if ((file = (File *)HT_Get(sFiles, 0, id))) {
        if (file->mode == 'r') {
            if (!file->eof) {
                if (file->binary) {
                    char data;
                    if (!(file->eof = !fread(&data, sizeof(char), 1, file->file))) {
                        result.type = VAR_NUM;
                        result.value.n = data;
                    }
                }
                else {
                    int c = fgetc(file->file);
                    if (!(file->eof = c == EOF)) {
                        result.type = VAR_NUM;
                        result.value.n = c;
                    }
                }
            }
        }
        else {
            char tmp[64];
            sprintf(tmp, "File %d is write only (SYS_FILE_READ_CHAR)", id);
            RuntimeError(tmp);
        }
    }
    else {
        char tmp[64];
        sprintf(tmp, "File %d doesn't exist (SYS_FILE_READ_CHAR)", id);
        RuntimeError(tmp);
    }
    
    
    return result;
}

Variable FileReadLine(int argc, Variable *argv) {
    Variable result;    
    File *file;
    int id = (int)ToNumber(&argv[0]);

    result.type = VAR_UNSET;

    if ((file = (File *)HT_Get(sFiles, 0, id))) {
        if (file->mode == 'r') {
            if (!file->eof) {
                if (file->binary) {
                    int growSize = 16;
                    int size = growSize;
                    int index = 0;
                    char c;
                    char *str = (char *)malloc(sizeof(char)*(size + 1));
                       
                    while (!(file->eof = !fread(&c, sizeof(char), 1, file->file)) && !(c == '\0' || c == 10)) {
                        if (index >= size) {
                            size += growSize;
                            str = (char *)realloc(str, sizeof(char)*(size + 1));
                        }
                        str[index++] = c;
                    }
                    str[index] = '\0';                        
                    if (index > 0) {
                        result.type = VAR_STR;
                        result.value.s = strdup(str);
                    }
                    free(str);
                }
                else {
                    int growSize = 16;
                    int size = growSize;
                    int index = 0;
                    int c;

                    char *str = (char *)malloc(sizeof(char)*(size + 1));
                    c = fgetc(file->file);
                    while (!(c == '\n' || c == EOF)) {
                        if (index >= size) {
                            size += growSize;
                            str = (char *)realloc(str, sizeof(char)*(size + 1));
                        }
                        str[index++] = c;
                        c = fgetc(file->file);
                    }
                    file->eof = c == EOF;
                    str[index] = '\0';
                    result.type = VAR_STR;
                    result.value.s = strdup(str);
                    free(str);
                }
            }
        }
        else {
            char tmp[64];
            sprintf(tmp, "File %d is write only (SYS_FILE_READ_LINE)", id);
            RuntimeError(tmp);
        }
    }
    else {
        char tmp[64];
        sprintf(tmp, "File %d doesn't exist (SYS_FILE_READ_LINE)", id);
        RuntimeError(tmp);
    }
    
    return result;
}

Variable OpenFileDialog(int argc, Variable *argv) {
    Variable result;
    char *fn;
    
    result.type = VAR_STR;
    
    fn = WIN_OpenFileDialog(argc >= 1 ? ToString(&argv[0], 8) : 0);
    result.value.s = fn ? fn : strdup("");

    ClearInkeyBuffer();
    ClearKeyDown();
    ClearMouseButtons();
    
    return result;
}

Variable SaveFileDialog(int argc, Variable *argv) {
    Variable result;
    char *fn;
    
    result.type = VAR_STR;
    
    fn = WIN_SaveFileDialog(argc >= 1 ? ToString(&argv[0], 8) : 0);
    result.value.s = fn ? fn : strdup("");

    ClearInkeyBuffer();
    ClearKeyDown();
    ClearMouseButtons();
    
    return result;
}

Variable CheckFileExists(int argc, Variable *argv) {
    Variable result;
    FILE *file;
    
    result.type = VAR_NUM;
    if ((file = fopen(ToString(&argv[0], 8), "r"))) {
        fclose(file);
        result.value.n = 1;
    }
    else {
        result.value.n = 0;
    }
    
    return result;
}

/*
 * SetWindow
 * ---------
 * set window (string)title, (number)w, (number)h[, (number)fullScreen[,
 *            (number)scale]]
 */
Variable SetWindow(int argc, Variable *argv) {
    Variable result;
    int w = ToNumber(&argv[1]);
    int h = ToNumber(&argv[2]);
    int fs = argc > 3 ? (int)ToNumber(&argv[3]) : 0;
    int s = argc > 4 ? (int)ToNumber(&argv[4]) : 1;
    int minW = argc > 5 ? (int)ToNumber(&argv[5]) : w;
    int minH = argc > 6 ? (int)ToNumber(&argv[6]) : h;
   
    w = MAX(w, 64);
    h = MAX(h, 48);
    minW = MAX(minW, 64);
    minH = MAX(minH, 48);

    if (s < 0) s = 0;
    
    sHasWindow = WIN_Set(ToString(&argv[0], 8), w, h, fs, s, minW, minH) == WIN_SUCCESS;
    result.type = VAR_UNSET;
    
    return result;
}

Variable SetRedraw(int argc, Variable *argv) {
    Variable result;
    
    WIN_SetAutoRedraw((int)ToNumber(&argv[0]));
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * WindowActive
 * ------------
 * (number)active()
 */
Variable WindowActive(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = WIN_Active();
    
    return result;
}

/*
 * WindowExists
 * ------------
 * (number)window((string)title)
 */
Variable WindowExists(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = WIN_Exists(ToString(&argv[0], 8));
    
    return result;
}

/*
 * ScreenW
 * -------
 * (number)screenw()
 */
Variable ScreenW(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = WIN_ScreenWidth();
    
    return result;
}

/*
 * ScreenH
 * -------
 * (number)screenh()
 */
Variable ScreenH(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = WIN_ScreenHeight();
    
    return result;
}

/*
 * RedrawWindow
 * ------------
 * redraw
 */
Variable RedrawWindow(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    WIN_Redraw();
    
    return result;
}

/*
 * MouseX
 * ------
 * (number)mousex()
 */
Variable MouseX(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = sMouseX;
    
    return result;
}

/*
 * MouseY
 * ------
 * (number)mousey()
 */
Variable MouseY(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = sMouseY;
    
    return result;
}

/*
 * MouseDX
 * -------
 * (number)mouserelx()
 */
Variable MouseDX(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = WIN_MouseRelX();
    
    return result;
}

/*
 * MouseDY
 * -------
 * (number)mouserely()
 */
Variable MouseDY(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = WIN_MouseRelY();
    
    return result;
}

/*
 * MouseDown
 * ---------
 * (number)mousebutton((number)button[, (number)unflag])
 */
Variable MouseDown(int argc, Variable *argv) {
    Variable result;
    int button = (int)ToNumber(&argv[0]);
    
    result.type = VAR_NUM;
    /* Mousewheel. */
    if (button == 2) {
        result.value.n = sMouseButton[2];
        if (argc > 1 && (int)ToNumber(&argv[1]) != 0) sMouseButton[button] = 0;
    }
    /* Buttons. */
    else if (button >= 0 && button <= 1) {
        result.value.n = sMouseButton[button];
        if (argc > 1 && (int)ToNumber(&argv[1]) != 0) sMouseButton[button] = 0;
    }
    else {
        result.value.n = 0;
    }
    
    return result;
}

/*
 * SetMouse
 * --------
 * set mouse (number)visibility
 * set mouse (number)x, (number)y
 */
Variable SetMouse(int argc, Variable *argv) {
    Variable result;
    result.type = VAR_UNSET;
    
    if (argc == 2) {
        WIN_SetMousePosition((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]));
    }
    else {
        WIN_SetMouseVisibility((int)ToNumber(&argv[0]));
    }
    
    return result;
}

/*
 * MouseOverZone
 * -------------
 */
int MouseOverZone(void *data, void *user) {
    Zone *zone = (Zone*)data;
    return sMouseX >= zone->x && sMouseX < zone->x + zone->w && sMouseY >= zone->y && sMouseY < zone->y + zone->h;
}

/*
 * DeleteZone
 * ----------
 */
void DeleteZone(void *data) {
    free(data);
}

/*
 * CreateZone
 * ----------
 * (number)createzone((number)x, (number)y, (number)w, (number)h)
 */
Variable CreateZone(int argc, Variable *argv) {
    Zone *zone = (Zone *)malloc(sizeof(Zone));
    Variable result;
    
    result.type = VAR_NUM;
    
    zone->id = 1;
    while (HT_Exists(sZones, 0, zone->id)) zone->id++;
    zone->x = (int)ToNumber(&argv[0]);
    zone->y = (int)ToNumber(&argv[1]);
    zone->w = (int)ToNumber(&argv[2]);
    zone->h = (int)ToNumber(&argv[3]);
    if (zone->w < 0) {
        zone->x += zone->w;
        zone->w = -zone->w;
    }
    if (zone->h < 0) {
        zone->y += zone->h;
        zone->h = -zone->h;
    }
    HT_Add(sZones, 0, zone->id, zone);

    result.value.n = zone->id;

    return result;
}

/*
 * CreateZoneLegacy
 * ----------------
 * create zone (number)id, (number)x, (number)y, (number)w, (number)h
 */
Variable CreateZoneLegacy(int argc, Variable *argv) {
    HashEntry *he;
    Zone *zone;
    Variable result;
    int id = (int)ToNumber(&argv[0]);
    
    he = HT_GetOrCreateEntry(sZones, 0, id);
    if (!(zone = (Zone *)he->data)) {
        zone = (Zone*)malloc(sizeof(Zone));
        zone->id = id;
        he->data = zone;
    }
    zone->x = (int)ToNumber(&argv[1]);
    zone->y = (int)ToNumber(&argv[2]);
    zone->w = (int)ToNumber(&argv[3]);
    zone->h = (int)ToNumber(&argv[4]);
    if (zone->w < 0) {
        zone->x += zone->w;
        zone->w = -zone->w;
    }
    if (zone->h < 0) {
        zone->y += zone->h;
        zone->h = -zone->h;
    }
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * FreeZone
 * --------
 * free zone (number)id
 */
Variable FreeZone(int argc, Variable *argv) {
    Variable result;

    int id = (int)ToNumber(&argv[0]);
    if (sActiveZone && sActiveZone->id == id) sActiveZone = 0;
    if (sZoneClicked && sZoneClicked->id == id) sZoneClicked = 0;
    HT_Delete(sZones, 0, id, DeleteZone);

    result.type = VAR_UNSET;
    return result;
}

/*
 * ZoneInfo
 * --------
 * (number)zone()
 * (number)zone((number)id)
 * (number)zone((number)x, (number)y)
 */
Variable ZoneInfo(int argc, Variable *argv) {
    Variable result;
    
    if (argc == 0) {
        if (sZoneClicked) {
            result.type = VAR_NUM;
            result.value.n = sZoneClicked->id;
            sZoneClicked = 0;
        }
        else {
            result.type = VAR_UNSET;
        }
    }
    else if (argc == 1) {
        Zone *zone = (Zone *)HT_Get(sZones, 0, (int)ToNumber(&argv[0]));
        if (zone) {
            result.type = VAR_NUM;
            if (sActiveZone == zone) {
                if (sZoneMouseDown) result.value.n = MouseOverZone(sActiveZone, 0) ? 2 : 0;
                else result.value.n = 1;
            }
            else {
                result.value.n = 0;
            }
        }
        else {
            result.type = VAR_UNSET;
        }
    }
    else if (argc == 2) {
        HashEntry *he = HT_FindEntry(sZones, MouseOverZone, 0);
        if (he) {
            result.type = VAR_NUM;
            result.value.n = he->ikey;
        }
        else {
            result.type = VAR_UNSET;
        }
    }
    
    return result;
}

/*
 * ZoneX
 * -----
 * (number)zonex((number)id)
 */
Variable ZoneX(int argc, Variable *argv) {
    Zone *zone;
    Variable result;
    
    if ((zone = (Zone *)HT_Get(sZones, 0, (int)ToNumber(&argv[0])))) {
        result.type = VAR_NUM;
        result.value.n = zone->x;
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * ZoneY
 * -----
 * (number)zoney((number)id)
 */
Variable ZoneY(int argc, Variable *argv) {
    Zone *zone;
    Variable result;
    
    if ((zone = (Zone *)HT_Get(sZones, 0, (int)ToNumber(&argv[0])))) {
        result.type = VAR_NUM;
        result.value.n = zone->y;
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * ZoneW
 * -----
 * (number)zonew((number)id)
 */
Variable ZoneW(int argc, Variable *argv) {
    Zone *zone;
    Variable result;
    
    if ((zone = (Zone *)HT_Get(sZones, 0, (int)ToNumber(&argv[0])))) {
        result.type = VAR_NUM;
        result.value.n = zone->w;
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * ZoneH
 * -----
 * (number)zoneh((number)id)
 */
Variable ZoneH(int argc, Variable *argv) {
    Zone *zone;
    Variable result;
    
    if ((zone = (Zone *)HT_Get(sZones, 0, (int)ToNumber(&argv[0])))) {
        result.type = VAR_NUM;
        result.value.n = zone->h;
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * JoyX
 * ----
 * (number)joyx()
 */
Variable JoyX(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = 0.01*(double)sJoyX;
    
    return result;
}

/*
 * JoyY
 * ----
 * (number)joyy()
 */
Variable JoyY(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = 0.01*(double)sJoyY;
    
    return result;
}

/*
 * JoyButton
 * ---------
 * (number)joybutton([button = 0[, unflag = 0])
 */
Variable JoyButton(int argc, Variable *argv) {
    Variable result;
    int button = argc > 0 ? (int)ToNumber(&argv[0]) : 0;
    int unflag = argc > 1 ? (int)ToNumber(&argv[1]) : 0;;
    int value = 0;

    result.type = VAR_NUM;
    if (button == 0) {        
        for (int i = 0; i < 4; i++) {
            if (sJoyButtons[i]) {
                value = 1;
                if (unflag) sJoyButtons[i] = 0;
            }
        }
    }
    else if (button >= 1 && button <= 4) {
        value = sJoyButtons[button - 1];
        if (unflag) sJoyButtons[button - 1] = 0;
    }
    
    result.value.n = value;
    
    return result;
}


/*
 * Inkey
 * -----
 * (number)inkey()
 */
Variable Inkey(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    if (sInkeyTail == sInkeyHead) {
        result.value.n = 0;      
    }
    else {
        result.value.n = sInkeyBuffer[sInkeyTail];
        sInkeyTail = (sInkeyTail + 1)%INKEY_BUFFER_SIZE;
    }
    
    return result;
}

/*
 * KeyDown
 * -------
 * (number)keydown((number)key[, (number)unflag])
 */ 
Variable KeyDown(int argc, Variable *argv) {
    Variable result;
    int key = ToNumber(&argv[0]);
    
    result.type = VAR_NUM;
    if (key >= 0 && key < 256) {
        result.value.n = sKeyDown[key];
        if (argc > 1 && (int)ToNumber(&argv[1]) != 0) sKeyDown[key] = 0;
    }
    else {
        result.value.n = 0;
    }
    
    return result;
}

/*
 * SetImage
 * --------
 * set image (number)id
 */
Variable SetImage(int argc, Variable *argv) {
    Variable result;
    int id = (int)ToNumber(&argv[0]);
    int updateAlpha = 1;

    if (id == SYS_PRIMARY_IMAGE || (argc > 1 && !(int)ToNumber(&argv[1]))) updateAlpha = 0;
    
    if (!WIN_SetImage(id, updateAlpha)) {
        RuntimeError("Image does not exist (SYS_SET_IMAGE)");
    }
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * SetImageClipRect
 * ----------------
 */
Variable SetImageClipRect(int argc, Variable *argv) {
    Variable result;

    WIN_SetClipRect(WIN_CurrentImage(), ToNumber(&argv[0]), ToNumber(&argv[1]), ToNumber(&argv[2]), ToNumber(&argv[3]));
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * ClearImageClipRect
 * ------------------
 */
Variable ClearImageClipRect(int argc, Variable *argv) {
    Variable result;
    
    WIN_ClearClipRect(WIN_CurrentImage());
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * SetColor
 * --------
 * set color (number)r, (number)g, (number)b[, (number)a]
 */
Variable SetColor(int argc, Variable *argv) {
    Variable result;
    int r, g, b, a;
    
    result.type = VAR_UNSET;
    
    if (argc == 1) {
        if (argv[0].type == VAR_TBL) {
            Variable *vr = (Variable *)HT_Get(argv[0].value.t, 0, 0);
            Variable *vg = (Variable *)HT_Get(argv[0].value.t, 0, 1);
            Variable *vb = (Variable *)HT_Get(argv[0].value.t, 0, 2);
            Variable *va = (Variable *)HT_Get(argv[0].value.t, 0, 3);
            if (vr && vr->type == VAR_NUM && vg && vg->type == VAR_NUM && vb && vb->type == VAR_NUM && (!va || (va && va->type == VAR_NUM))) {
                r = (int)vr->value.n;
                g = (int)vg->value.n;
                b = (int)vb->value.n;
                a = va ? (int)va->value.n : 255;
            }
            else {
                RuntimeError("Invalid color table (SYS_SET_COLOR)");
                return result;
            }
        }
        else {
            RuntimeError("Argument is not a table (SYS_SET_COLOR)");
            return result;
        }
    }
    else {
        r = (int)ToNumber(&argv[0]);
        g = (int)ToNumber(&argv[1]);
        b = (int)ToNumber(&argv[2]);
        a = argc > 3 ? (int)ToNumber(&argv[3]) : 255;
    }
    
    r = CROP(r, 0, 255);
    g = CROP(g, 0, 255);
    b = CROP(b, 0, 255);
    a = CROP(a, 0, 255);

    WIN_SetColor((unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a);
    
    return result;
}

/*
 * SetColorInt
 * -----------
 * set colori (number)n
 */
Variable SetColorInt(int argc, Variable *argv) {
    Variable result;
    unsigned int c;
    unsigned char r, g, b, a;
    
    result.type = VAR_UNSET;

    c = (unsigned int)ToNumber(&argv[0]);
    //ColorToRGBComponents(c, r, g, b);
    //WIN_SetColor(r, g, b, 255);
    ColorToRGBAComponents(c, r, g, b, a);
    WIN_SetColor(r, g, b, a);
    
    return result;
}


/*
 * SetAdditive
 * -----------
 * set additive (number)value
 */
Variable SetAdditive(int argc, Variable *argv) {
    Variable result;
    
    WIN_SetAdditive((char)ToNumber(&argv[0]));
    
    result.type = VAR_UNSET;
    
    return result;    
}

/*
 * Cls
 * ---
 * cls
 */
Variable Cls(int argc, Variable *argv) {
    Variable result;
    
    WIN_Cls(argc > 0 ? (int)ToNumber(&argv[0]) : 0);
    result.type = 0;
    
    return result;
}

/*
 * SetPixel
 * --------
 * set pixel (number)x, (number)y
 */
Variable SetPixel(int argc, Variable *argv) {
    Variable result;

    WIN_SetPixel((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]));
    result.type = 0;
    
    return result;
}

/*
 * DrawPixel
 * ---------
 * draw pixel (number)x, (number)y
 */
Variable DrawPixel(int argc, Variable *argv) {
    Variable result;

    WIN_DrawPixel((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]));
    result.type = 0;
    
    return result;
}

Variable GetPixel(int argc, Variable *argv) {
    Variable result;
    unsigned char r, g, b, a;
    
    if (argc == 3) {
        if (WIN_GetPixel((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), &r, &g, &b, &a)) {
            result.type = VAR_TBL;
            result.value.t = HT_Create(4); /*NewHashTable(4);*/
            HT_Add(result.value.t, 0, 0, NewNumber(r));
            HT_Add(result.value.t, 0, 1, NewNumber(g));
            HT_Add(result.value.t, 0, 2, NewNumber(b));
            HT_Add(result.value.t, 0, 3, NewNumber(a));
            MM_SetType(result.value.t, 1);
        }
        else {
            result.type = VAR_UNSET;
        }        
    }
    else {
        if (WIN_GetPixelCurrent((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), &r, &g, &b, &a)) {
            result.type = VAR_TBL;
            result.value.t = HT_Create(4);//NewHashTable(4);
            HT_Add(result.value.t, 0, 0, NewNumber(r));
            HT_Add(result.value.t, 0, 1, NewNumber(g));
            HT_Add(result.value.t, 0, 2, NewNumber(b));
            HT_Add(result.value.t, 0, 3, NewNumber(a));
            MM_SetType(result.value.t, 1);
        }
        else {
            result.type = VAR_UNSET;
        }
    }
    return result;
}

/*
 * GetPixelInt
 * -----------
 */
Variable GetPixelInt(int argc, Variable *argv) {
    Variable result;
    unsigned char r, g, b, a;
    
    if (argc == 3) {
        if (WIN_GetPixel((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), &r, &g, &b, &a)) {
            result.type = VAR_NUM;
            //result.value.n = ((r << 16) + (g << 8) + b);
            result.value.n = (((long long)a << 24) + ((long long)r << 16) + ((long long)g << 8) + (long long)b);
        }
        else {
            result.type = VAR_UNSET;
        }        
    }
    else {
        if (WIN_GetPixelCurrent((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), &r, &g, &b, &a)) {
            result.type = VAR_NUM;
            result.value.n = (((long long)a << 24) + ((long long)r << 16) + ((long long)g << 8) + (long long)b);
        }
        else {
            result.type = VAR_UNSET;
        }
    }
    return result;
}

/*
 * DrawLine
 * --------
 * draw line (number)x1, (number)y1, (number)x2, (number)y2
 */
Variable DrawLine(int argc, Variable *argv) {
    Variable result;

    if (argc == 2) WIN_DrawLineTo((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]));
    else WIN_DrawLine((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), (int)ToNumber(&argv[3]));
    result.type = 0;
    
    return result;
}

/*
 * DrawRect
 * --------
 * draw rect (number)x, (number)y, (number)w, (number)h[, (number)fill]
 */
Variable DrawRect(int argc, Variable *argv) {
    Variable result;

    if (argc > 4 && (int)ToNumber(&argv[4])) {
        WIN_FillRect((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), (int)ToNumber(&argv[3]));
    }
    else {
        WIN_DrawRect((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), (int)ToNumber(&argv[3]));
    }
    result.type = 0;
    
    return result;
}

/*
 * DrawEllipse
 * -----------
 * draw ellipse (number)x, (number)y, (number)rx, (number)ry[, (number)fill]
 */
Variable DrawEllipse(int argc, Variable *argv) {
    Variable result;

    if (argc > 4 && (int)ToNumber(&argv[4])) {
        WIN_FillEllipse((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), (int)ToNumber(&argv[3]));
    }
    else {
        WIN_DrawEllipse((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), (int)ToNumber(&argv[3]));
    }

    result.type = 0; 
    
    return result;
}

/*
 * DrawPolygon
 * -----------
 * draw polygon (array)points[, (number)fill[, (number)pointCount]]
 */
Variable DrawPolygon(int argc, Variable *argv) {
    Variable result;
    
    result.type = 0;
    
    if (!sPolyPoints) {
        sPolyPointsSize = 128;
        sPolyPoints = (int *)malloc(sizeof(int)*sPolyPointsSize);
        if (!sPolyPoints) {
            RuntimeError("Could not allocate memory for points (SYS_DRAW_POLYGON)");
            return result;
        }
    }

    if (argv[0].type == VAR_TBL) {
        Variable *p;
        int count = 0;
        
        while ((p = (Variable *)HT_Get(argv[0].value.t, 0, count))) {
            if (count >= sPolyPointsSize) {
                sPolyPointsSize *= 2;
                sPolyPoints = (int *)realloc(sPolyPoints, sizeof(int)*sPolyPointsSize);
                if (!sPolyPoints) {
                    RuntimeError("Could not reallocate memory for points (SYS_DRAW_POLYGON)");
                    return result;
                }
            }
            if (p->type == VAR_NUM) sPolyPoints[count] = (int)round(p->value.n);
            else sPolyPoints[count] = (int)ToNewNumber(p).value.n;
            count++;
        }
        count /= 2;
        if (argc > 2) {
            int c = ToNumber(&argv[2]);
            count = MIN(count, c);
        }
        
        if (argc > 1 && (int)ToNumber(&argv[1])) WIN_FillPolygon(count, sPolyPoints);
        else WIN_DrawPolygon(count, sPolyPoints);
    }
    
    return result;
}

/*
 * DrawPolygonTransformed
 * ----------------------
 * draw poly transformed points, x, y, scaleX, scaleY, angle, pivotX, pivotY[, filled[, pointCount]]
 */
Variable DrawPolygonTransformed(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
            
    if (!sPolyPointsF) {
        sPolyPointsFSize = 128;
        sPolyPointsF = (float *)malloc(sizeof(float)*sPolyPointsFSize);
        if (!sPolyPointsF) {
            RuntimeError("Could not allocate memory for points (SYS_DRAW_POLYGON_TRANSFORMED)");
            return result;
        }
    }

    if (argv[0].type == VAR_TBL) {
        Variable *p;
        int count = 0;
        
        while ((p = (Variable *)HT_Get(argv[0].value.t, 0, count))) {
            if (count >= sPolyPointsFSize) {
                sPolyPointsFSize *= 2;
                sPolyPointsF = (float *)realloc(sPolyPointsF, sizeof(float)*sPolyPointsFSize);
                if (!sPolyPointsF) {
                    RuntimeError("Could not reallocate memory for points (SYS_DRAW_POLYGON_TRANSFORMED)");
                    return result;
                }
            }
            if (p->type == VAR_NUM) sPolyPointsF[count] = (float)p->value.n;
            else sPolyPointsF[count] = (float)ToNewNumber(p).value.n;
            count++;
        }
        count /= 2;
        
        if (argc > 9) {
            int c = (int)ToNumber(&argv[9]);
            count = MIN(count, c);
        }

        if (argc > 8 && (int)ToNumber(&argv[8])) WIN_FillPolygonTransformed(
                count, sPolyPointsF,
                (float)ToNumber(&argv[1]), (float)ToNumber(&argv[2]),
                (float)ToNumber(&argv[3]), (float)ToNumber(&argv[4]),
                (float)ToNumber(&argv[5]),
                (float)ToNumber(&argv[6]), (float)ToNumber(&argv[7]));
        else WIN_DrawPolygonTransformed(
                count, sPolyPointsF,
                (float)ToNumber(&argv[1]), (float)ToNumber(&argv[2]),
                (float)ToNumber(&argv[3]), (float)ToNumber(&argv[4]),
                (float)ToNumber(&argv[5]),
                (float)ToNumber(&argv[6]), (float)ToNumber(&argv[7]));        
    }
    
    return result;
}

/*
 * DrawPolygonImage
 * ----------------
 * draw poly image img, points[, pointFields[, pointCount]]
 */
Variable DrawPolygonImage(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
        
    if (sPolyImagePointCount == 0) {
        sPolyImagePointCount = 64;
        sPolyImagePointsi = (int *)malloc(sizeof(int)*sPolyImagePointCount*2);
        sPolyImagePointsf = (float *)malloc(sizeof(float)*sPolyImagePointCount*3);
    }

    if (argv[1].type == VAR_TBL) {
        Variable *p;
        int count = 0;
        int fields = 4;
        int pcount = 0;
        
        if (argc > 2) fields = (int)ToNumber(&argv[2]);
        
        if (!(fields == 4 || fields == 5)) {
            RuntimeError("Invalid point fields count (SYS_DRAW_POLYGON_IMAGE)");
            return result;
        }
        
        while ((p = (Variable *)HT_Get(argv[1].value.t, 0, count))) {
            double value;
            int m = count%fields;
            if (p->type == VAR_NUM) value = p->value.n;
            else value = ToNewNumber(p).value.n;
            
            if (m == 0) {
                if (pcount >= sPolyImagePointCount) {
                    sPolyImagePointCount *= 2;
                    sPolyImagePointsi = (int *)realloc(sPolyImagePointsi, sizeof(int)*sPolyImagePointCount*2);
                    sPolyImagePointsf = (float *)realloc(sPolyImagePointsf, sizeof(float)*sPolyImagePointCount*3);
                }
                sPolyImagePointsi[pcount*2] = (int)round(value); /* x. */
            }
            else if (m == 1) {
                sPolyImagePointsi[pcount*2 + 1] = (int)round(value); /* y. */
            }
            else {
                if (fields == 4) {
                    if (m == 2) {
                        sPolyImagePointsf[pcount*2] = (float)value; /* u. */
                    }
                    else {
                        sPolyImagePointsf[pcount*2 + 1] = (float)value; /* v. */
                        pcount++;
                    }
                }
                else {
                    if (m == 2) {
                        sPolyImagePointsf[pcount*3 + 2] = (float)value; /* z */
                    }
                    else if (m == 3) {
                        sPolyImagePointsf[pcount*3] = (float)value; /* u */
                    }
                    else {
                        sPolyImagePointsf[pcount*3 + 1] = (float)value; /* v */
                        pcount++;
                    }
                }
            }
            count++;
        }
       
        if (count%fields) {
            RuntimeError("Invalid point array (SYS_DRAW_POLYGON_IMAGE)");
            return result;
        }
        
        if (argc > 3) {
            int c = (int)ToNumber(&argv[3]);
            pcount = MIN(pcount, c);
        }
                
        WIN_TexturePolygon((int)ToNumber(&argv[0]), fields, pcount, sPolyImagePointsi, sPolyImagePointsf);
    }
    
    return result;
}

/*
 * DrawPolygonImageTransformed
 * ---------------------------
 * draw poly image xform image_id, points, x, y, scaleX, scaleY, angle, pivotX, pivotY[, fieldCount[, pointCount]]
 */
Variable DrawPolygonImageTransformed(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
        
    if (sPolyImageTPointCount == 0) {
        sPolyImageTPointCount = 64;
        sPolyImageTPointsi = (float *)malloc(sizeof(float)*sPolyImageTPointCount*2);
        sPolyImageTPointsf = (float *)malloc(sizeof(float)*sPolyImageTPointCount*3);
    }

    if (argv[1].type == VAR_TBL) {
        Variable *p;
        int count = 0;
        int fields = 4;
        int pcount = 0;
        
        if (argc > 9) fields = (int)ToNumber(&argv[9]);
        
        if (!(fields == 4 || fields == 5)) {
            RuntimeError("Invalid point fields count (SYS_DRAW_POLYGON_IMAGE_TRANSFORM)");
            return result;
        }
        
        while ((p = (Variable *)HT_Get(argv[1].value.t, 0, count))) {
            double value;
            int m = count%fields;
            if (p->type == VAR_NUM) value = p->value.n;
            else value = ToNewNumber(p).value.n;
            
            if (m == 0) {
                if (pcount >= sPolyImageTPointCount) {
                    sPolyImageTPointCount *= 2;
                    sPolyImageTPointsi = (float *)realloc(sPolyImageTPointsi, sizeof(float)*sPolyImageTPointCount*2);
                    sPolyImageTPointsf = (float *)realloc(sPolyImageTPointsf, sizeof(float)*sPolyImageTPointCount*3);
                }
                sPolyImageTPointsi[pcount*2] = (float)value; /* x. */
            }
            else if (m == 1) {
                sPolyImageTPointsi[pcount*2 + 1] = (float)value; /* y. */
            }
            else {
                if (fields == 4) {
                    if (m == 2) {
                        sPolyImageTPointsf[pcount*2] = (float)value; /* u. */
                    }
                    else {
                        sPolyImageTPointsf[pcount*2 + 1] = (float)value; /* v. */
                        pcount++;
                    }
                }
                else {
                    if (m == 2) {
                        sPolyImageTPointsf[pcount*3 + 2] = (float)value; /* z */
                    }
                    else if (m == 3) {
                        sPolyImageTPointsf[pcount*3] = (float)value; /* u */
                    }
                    else {
                        sPolyImageTPointsf[pcount*3 + 1] = (float)value; /* v */
                        pcount++;
                    }
                }
            }
            count++;
        }
       
        if (count%fields) {
            RuntimeError("Invalid point array (SYS_DRAW_POLYGON_IMAGE_TRANSFORMED)");
            return result;
        }
        
        if (argc > 10) {
            int c = (int)ToNumber(&argv[10]);
            pcount = MIN(pcount, c);
        }
        
        WIN_TexturePolygonTransformed(
                (int)ToNumber(&argv[0]), fields, pcount, sPolyImageTPointsi, sPolyImageTPointsf,
                (float)ToNumber(&argv[2]), (float)ToNumber(&argv[3]),
                (float)ToNumber(&argv[4]), (float)ToNumber(&argv[5]),
                (float)ToNumber(&argv[6]),
                (float)ToNumber(&argv[7]), (float)ToNumber(&argv[8]));
    }
    
    return result;
}

/*
 * DrawVRaster
 * -----------
 * draw vraster img, x, y0, y1, u0, v0, u1, v1
 */
Variable DrawVRaster(int argc, Variable *argv) {
    Variable result;
    result.type = VAR_UNSET;
    
    WIN_DrawVRaster((int)ToNumber(&argv[0]),
            (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), (int)ToNumber(&argv[3]),
            (float)ToNumber(&argv[4]), (float)ToNumber(&argv[5]), (float)ToNumber(&argv[6]), (float)ToNumber(&argv[7]));
    
    return result;
}

/*
 * DrawHRaster
 * -----------
 * draw hraster img, y, x0, x1, u0, v0, u1, v1
 */
Variable DrawHRaster(int argc, Variable *argv) {
    Variable result;
    result.type = VAR_UNSET;

    WIN_DrawHRaster((int)ToNumber(&argv[0]),
            (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), (int)ToNumber(&argv[3]),
            (float)ToNumber(&argv[4]), (float)ToNumber(&argv[5]), (float)ToNumber(&argv[6]), (float)ToNumber(&argv[7]));
    
    return result;
}


/*
 * LoadImage
 * ---------
 * (number)loadimage((string)filename)
 */
Variable LoadImage(int argc, Variable *argv) {
    Variable result;
    
    int id;
    for (id = 1; id < SYS_PRIMARY_IMAGE; id++) if (!WIN_ImageExists(id)) break;
    
    if (id < SYS_PRIMARY_IMAGE && WIN_LoadImage(id, ToString(&argv[0], 8))) {
        if (argc == 3) {
            int cols = (int)ToNumber(&argv[1]);
            int rows = (int)ToNumber(&argv[2]);
            cols = MAX(cols, 1);
            rows = MAX(rows, 1);
            WIN_SetImageGrid(id, cols, rows);
        }
        result.type = VAR_NUM;
        result.value.n = id;
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * LoadImageLegacy
 * ---------------
 * load image (number)id, (string)filename
 */
Variable LoadImageLegacy(int argc, Variable *argv) {
    Variable result;
    int id = (int)ToNumber(&argv[0]);

    if (id < SYS_PRIMARY_IMAGE) {
        if (WIN_LoadImage(id, ToString(&argv[1], 8))) {
            if (argc == 4) {
                int cols = (int)ToNumber(&argv[2]);
                int rows = (int)ToNumber(&argv[3]);
                cols = MAX(cols, 1);
                rows = MAX(rows, 1);
                WIN_SetImageGrid(id, cols, rows);
            }
        }
    }
    else {
        RuntimeError("Invalid image identifier (SYS_LOAD_IMAGE_LEGACY)");
    }
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * SaveImage
 * ---------
 */
Variable SaveImage(int argc, Variable *argv) {
    Variable result; 
    char *filename = ToString(&argv[1], 8);
    char *newFilename;
    size_t len = strlen(filename);
    int i;
    
    if (len > 0) {    
        /* Remove extension. */
        for (i = len - 1; i >= 0; i--) if (filename[i] == '.') break;
        if (i >= 0) len = i;
        newFilename = (char *)malloc(sizeof(char)*(len + 5));
        memcpy(newFilename, filename, len);
        newFilename[len] = '\0';
        strcat(newFilename, ".png");
        WIN_SaveImage(ToNumber(&argv[0]), newFilename);
        free(newFilename);
    }
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * CreateImage
 * -----------
 * (number)createimage((number)w, (number)h)
 */
Variable CreateImage(int argc, Variable *argv) {
    Variable result;

    int id;
    for (id = 1; id < SYS_PRIMARY_IMAGE; id++) if (!WIN_ImageExists(id)) break;
    
    if (id < SYS_PRIMARY_IMAGE && WIN_CreateImage(id, (int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]))) {
        result.type = VAR_NUM;
        result.value.n = id;
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * CreateImageLegacy
 * -----------------
 * create image (number)id, (number)w, (number)h
 */
Variable CreateImageLegacy(int argc, Variable *argv) {
    Variable result;
    int id = (int)ToNumber(&argv[0]);

    if (id < SYS_PRIMARY_IMAGE) WIN_CreateImage(id, (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]));
    else RuntimeError("Invalid image identifier (SYS_CREATE_IMAGE_LEGACY)");

    result.type = VAR_UNSET;
    
    return result;
}

Variable FreeImage(int argc, Variable *argv) {
    Variable result;
    int id = (int)ToNumber(&argv[0]);
    
    if (id < SYS_PRIMARY_IMAGE) WIN_FreeImage(id);
    else RuntimeError("Invalid image identifier (SYS_FREE_IMAGE)");
    
    result.type = VAR_UNSET;
    
    return result;
}

/*
 * SetImageColorKey
 * ----------------
 * set image colorkey (number)id, (number)r, (number)g, (number)b
 */
Variable SetImageColorKey(int argc, Variable *argv) {
    Variable result;
    int r, g, b;
    
    r = (int)ToNumber(&argv[1]);
    g = (int)ToNumber(&argv[2]);
    b = (int)ToNumber(&argv[3]);
    r = CROP(r, 0, 255);
    g = CROP(g, 0, 255);
    b = CROP(b, 0, 255);
    WIN_SetImageColorKey((int)ToNumber(&argv[0]), r, g, b);
    result.type = VAR_UNSET;
    
    return result;
}

Variable SetImageGrid(int argc, Variable *argv) {
    Variable result;
    int cols = (int)ToNumber(&argv[1]);
    int rows = (int)ToNumber(&argv[2]);
    
    cols = MAX(cols, 1);
    rows = MAX(rows, 1);
    
    WIN_SetImageGrid((int)ToNumber(&argv[0]), cols, rows);
    
    result.type = VAR_UNSET;
    
    return result;
}

Variable ImageExists(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = WIN_ImageExists((int)ToNumber(&argv[0]));
    
    return result;
}

Variable ImageWidth(int argc, Variable *argv) {
    Variable result;

    result.type = VAR_NUM;
    if (argc > 0) result.value.n = WIN_ImageWidth((int)ToNumber(&argv[0]));
    else result.value.n = WIN_ImageWidth(WIN_CurrentImage());
 
    return result;
}

Variable ImageHeight(int argc, Variable *argv) {
    Variable result;

    result.type = VAR_NUM;
    if (argc > 0) result.value.n = WIN_ImageHeight((int)ToNumber(&argv[0]));
    else result.value.n = WIN_ImageHeight(WIN_CurrentImage());
 
    return result;
}

Variable ImageCols(int argc, Variable *argv) {
    Variable result;

    result.type = VAR_NUM;
    if (argc > 0) result.value.n = WIN_ImageCols((int)ToNumber(&argv[0]));
    else result.value.n = WIN_ImageCols(WIN_CurrentImage());
 
    return result;
}

Variable ImageRows(int argc, Variable *argv) {
    Variable result;

    result.type = VAR_NUM;
    if (argc > 0) result.value.n = WIN_ImageRows((int)ToNumber(&argv[0]));
    else result.value.n = WIN_ImageRows(WIN_CurrentImage());
 
    return result;
}

Variable ImageCells(int argc, Variable *argv) {
    Variable result;

    result.type = VAR_NUM;
    if (argc > 0) result.value.n = WIN_ImageCells((int)ToNumber(&argv[0]));
    else result.value.n = WIN_ImageCells(WIN_CurrentImage());
 
    return result;
}

/*
 * DrawImage
 * ---------
 * draw image id, x, y
 * draw image id, x, y, cel
 * draw image id, x, y, src_x, src_y, src_w, src_h
 */
Variable DrawImage(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    /* draw image id, x, y */
    if (argc == 3) {
        WIN_DrawImage((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2])); 
    }
    else if (argc == 4) {
        WIN_DrawImageCel((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]), (int)ToNumber(&argv[3])); 
    }
    else {
        WIN_DrawImageRect((int)ToNumber(&argv[0]),
                          (int)ToNumber(&argv[1]), (int)ToNumber(&argv[2]),
                          (int)ToNumber(&argv[3]), (int)ToNumber(&argv[4]),
                          (int)ToNumber(&argv[5]), (int)ToNumber(&argv[6])); 
    }
    
    return result;
}

/*
 * DrawImageTransformed
 * --------------------
 * draw image transformed id, x, y, scale_x, scale_y, angle, pivot_x, pivot_y
 * draw image transformed id, x, y, scale_x, scale_y, angle, pivot_x, pivot_y, cel
 * draw image transformed id, x, y, scale_x, scale_y, angle, pivot_x, pivot_y, src_x, src_y, src_w, src_h
 */
Variable DrawImageTransformed(int argc, Variable *argv) {
    Variable result;
    int img = (int)ToNumber(&argv[0]);
    float x = (float)ToNumber(&argv[1]);
    float y = (float)ToNumber(&argv[2]);
    float scaleX = (float)ToNumber(&argv[3]);
    float scaleY = (float)ToNumber(&argv[4]);
    float angle = (float)ToNumber(&argv[5]);
    float pivotX = (float)ToNumber(&argv[6]);
    float pivotY = (float)ToNumber(&argv[7]);
    
    result.type = VAR_UNSET;
    
    if (argc == 8) {
        WIN_DrawImageTransformed(img, x, y, scaleX, scaleY, angle, pivotX, pivotY);
    }
    else if (argc == 9) {
        WIN_DrawImageCelTransformed(img, x, y, scaleX, scaleY, angle, pivotX, pivotY,
                (int)ToNumber(&argv[8]));
    }
    else {
        WIN_DrawImageRectTransformed(img, x, y, scaleX, scaleY, angle, pivotX, pivotY,
                (float)ToNumber(&argv[8]), (float)ToNumber(&argv[9]),
                (float)ToNumber(&argv[10]), (float)ToNumber(&argv[11]));
    }
    
    return result;
}

Variable CreateFont(int argc, Variable *argv) {
    Variable result;
    
    int id;
    for (id = 1; id < MAX_FONT_INDEX; id++) if (!WIN_FontExists(id)) break;
    
    if (id < MAX_FONT_INDEX && WIN_CreateFont(
            id,
            ToString(&argv[0], 8),
            (int)ToNumber(&argv[1]),
            argc > 2 ? (int)ToNumber(&argv[2]) : 0,
            argc > 3 ? (int)ToNumber(&argv[3]) : 0,
            argc > 4 ? (int)ToNumber(&argv[4]) : 0,
            argc > 5 ? (int)ToNumber(&argv[5]) : 0)) {
        result.type = VAR_NUM;
        result.value.n = id;
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

Variable CreateFontLegacy(int argc, Variable *argv) {
    Variable result;
    
    int id = (int)ToNumber(&argv[0]);
    
    if (id < MAX_FONT_INDEX) {
        WIN_CreateFont(
                id,
                ToString(&argv[1], 8),
                (int)ToNumber(&argv[2]),
                argc > 3 ? (int)ToNumber(&argv[3]) : 0,
                argc > 4 ? (int)ToNumber(&argv[4]) : 0,
                argc > 5 ? (int)ToNumber(&argv[5]) : 0,
                argc > 6 ? (int)ToNumber(&argv[6]) : 0);
    }
    else {
        RuntimeError("Invalid font identifier (SYS_CREATE_FONT_LEGACY)");
    }
    result.type = VAR_UNSET;
    
    return result;
}

/*
    if (len > 0) {    
        for (i = len - 1; i >= 0; i--) if (filename[i] == '.') break;
        if (i >= 0) len = i;
        newFilename = (char *)malloc(sizeof(char)*(len + 5));
        memcpy(newFilename, filename, len);
        newFilename[len] = '\0';
        strcat(newFilename, ".png");
        WIN_SaveImage(ToNumber(&argv[0]), newFilename);
        free(newFilename);
    }

*/
Variable LoadFont(int argc, Variable *argv) {
    Variable result;
    char *filename;
    int id;
    
    for (id = 1; id < MAX_FONT_INDEX; id++) if (!WIN_FontExists(id)) break;
    
    /* Remove any extension from filename. */
    filename = ToString(&argv[0], 8);
    for (int i = strlen(filename) - 1; i >= 0; i--) {
        if (filename[i] == '.') {
            filename[i] = '\0';
            break;
        }
    }
    
    if (id < MAX_FONT_INDEX && WIN_LoadFont(id, filename)) {
        result.type = VAR_NUM;
        result.value.n = id;
    }
    else {
        result.type = VAR_UNSET;
    }
    
    return result;
}

Variable LoadFontLegacy(int argc, Variable *argv) {
    Variable result;
    
    int id = (int)ToNumber(&argv[0]);
    
    if (id < MAX_FONT_INDEX) {
        WIN_LoadFont(id, ToString(&argv[1], 8));
    }
    else {
        RuntimeError("Invalid font identifier (SYS_LOAD_FONT_LEGACY)");
    }
    result.type = VAR_UNSET;
    
    return result;
}

Variable SaveFont(int argc, Variable *argv) {
    Variable result;
    
    WIN_SaveFont((int)ToNumber(&argv[0]), ToString(&argv[1], 8));
    
    result.type = VAR_UNSET;
    
    return result;
}

Variable FreeFont(int argc, Variable *argv) {
    Variable result;
    
    WIN_FreeFont((int)ToNumber(&argv[0]));
                   
    result.type = VAR_UNSET;
    
    return result;
}

Variable SetFont(int argc, Variable *argv) {
    Variable result;
    
    WIN_SetFont(ToNumber(&argv[0]));
    result.type = VAR_UNSET;
    
    return result;
}

Variable FontExists(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = WIN_FontExists((int)ToNumber(&argv[0]));
    
    return result;
}

Variable FontWidth(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    if (argc > 1) result.value.n = WIN_FontWidth((int)ToNumber(&argv[0]), ToString(&argv[1], 8));
    else result.value.n = WIN_FontWidth(WIN_CurrentFont(), ToString(&argv[0], 8));
    
    return result;
}

Variable FontHeight(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    if (argc > 0) result.value.n = WIN_FontHeight((int)ToNumber(&argv[0]));
    else result.value.n = WIN_FontHeight(WIN_CurrentFont());
    
    return result;
}

Variable Write(int argc, Variable *argv) {
    Variable result;
    
    if (sHasWindow) WIN_Write(ToString(&argv[0], 8), sJustification, 0);
    else printf("%s", ToString(&argv[0], 8));
    
    result.type = VAR_UNSET;
    
    return result;
}

Variable WriteLine(int argc, Variable *argv) {
    Variable result;

    if (sHasWindow) {
        if (argc > 0) WIN_Write(ToString(&argv[0], 8), sJustification, 1);
        else WIN_Write("", sJustification, 1);
    }
    else {
        if (argc > 0) printf("%s\n", ToString(&argv[0], 8));
        else printf("\n");
    }

    result.type = VAR_UNSET;
    
    return result;
}

Variable Center(int argc, Variable *argv) {
    Variable result;

    if (argc > 0) WIN_Write(ToString(&argv[0], 8), 0, 1);
    else WIN_Write("", 0, 1);

    result.type = VAR_UNSET;
    
    return result;
}

Variable SetJustification(int argc, Variable *argv) {
    Variable result;

    sJustification = (int)ToNumber(&argv[0]);
    
    result.type = VAR_UNSET;
    
    return result;
}

Variable SetCaret(int argc, Variable *argv) {
    Variable result;
    
    WIN_SetCaret((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1]));

    result.type = VAR_UNSET;
    
    return result;
}

Variable Scroll(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    WIN_Scroll((int)ToNumber(&argv[0]), (int)ToNumber(&argv[1])); 
    
    return result;
}

Variable LoadSound(int argc, Variable *argv) {
    Variable result;
    int id = 1;
    
    while (AUD_SoundExists(id)) id++;
    
    if (AUD_LoadSound(id, ToString(&argv[0], 8))) {
        result.type = VAR_NUM;
        result.value.n = id;
    }
    else result.type = VAR_UNSET;
  
    return result;    
}

Variable LoadSoundLegacy(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    AUD_LoadSound((int)ToNumber(&argv[0]), ToString(&argv[1], 8));
    
    return result;    
}


Variable CreateSound(int argc, Variable *argv) {
    Variable result;
    int id = 1;
    
    while (AUD_SoundExists(id)) id++;
    
    int lsize = 0, rsize = 0;
    float *ldata = 0, *rdata = 0;

    if (argv[0].type == VAR_TBL) {
        while (HT_Get(argv[0].value.t, 0, lsize)) lsize++;
        if (lsize) {
            ldata = (float *)malloc(sizeof(float)*lsize);
            for (int i = 0; i < lsize; i++) {
                ldata[i] = (float)ToNewNumber((Variable *)HT_Get(argv[0].value.t, 0, i)).value.n;
                if (ldata[i] > 1.0f) ldata[i] = 1.0f;
                else if (ldata[i] < -1.0f) ldata[i] = -1.0f;
            }
        }
    }
    if (argv[1].type == VAR_TBL) {
        while (HT_Get(argv[1].value.t, 0, rsize)) rsize++;
        if (rsize) {
            rdata = (float *)malloc(sizeof(float)*rsize);
            for (int i = 0; i < rsize; i++) {
                rdata[i] = (float)ToNewNumber((Variable *)HT_Get(argv[1].value.t, 0, i)).value.n;
                if (rdata[i] > 1.0f) rdata[i] = 1.0f;
                else if (rdata[i] < -1.0f) rdata[i] = -1.0f;
            }
        }
    }
    
    int sampleRate = (int)ToNumber(&argv[2]);
    
    if (sampleRate >= 1) {
        if (ldata && rdata && lsize == rsize) {
            if (AUD_CreateSound(id, ldata, rdata, lsize, sampleRate)) {
                result.type = VAR_NUM;
                result.value.n = id;
            }
        }
        else {
            RuntimeError("Invalid sound data (SYS_CREATE_SOUND)");
        }
    }
    else {
        RuntimeError("Invalid sample rate (SYS_CREATE_SOUND)");
    }

    if (ldata) free(ldata);
    if (rdata) free(rdata);
    
    return result;
}

// jeje, just copying code from above for now, lazy ...
Variable CreateSoundLegacy(int argc, Variable *argv) {
    Variable result;

    int id;
    int lsize = 0, rsize = 0;
    float *ldata = 0, *rdata = 0;
    
    id = (int)ToNumber(&argv[0]);

    if (argv[1].type == VAR_TBL) {
        while (HT_Get(argv[1].value.t, 0, lsize)) lsize++;
        if (lsize) {
            ldata = (float *)malloc(sizeof(float)*lsize);
            for (int i = 0; i < lsize; i++) {
                ldata[i] = (float)ToNewNumber((Variable *)HT_Get(argv[1].value.t, 0, i)).value.n;
                if (ldata[i] > 1.0f) ldata[i] = 1.0f;
                else if (ldata[i] < -1.0f) ldata[i] = -1.0f;
            }
        }
    }
    if (argv[2].type == VAR_TBL) {
        while (HT_Get(argv[2].value.t, 0, rsize)) rsize++;
        if (rsize) {
            rdata = (float *)malloc(sizeof(float)*rsize);
            for (int i = 0; i < rsize; i++) {
                rdata[i] = (float)ToNewNumber((Variable *)HT_Get(argv[2].value.t, 0, i)).value.n;
                if (rdata[i] > 1.0f) rdata[i] = 1.0f;
                else if (rdata[i] < -1.0f) rdata[i] = -1.0f;
            }
        }
    }
    
    int sampleRate = (int)ToNumber(&argv[3]);
    
    if (sampleRate >= 1) {
        if (ldata && rdata && lsize == rsize) {
            AUD_CreateSound(id, ldata, rdata, lsize, sampleRate);
        }
        else {
            RuntimeError("Invalid sound data (SYS_CREATE_SOUND_LEGACY)");
        }
    }
    else {
        RuntimeError("Invalid sample rate (SYS_CREATE_SOUND_LEGACY)");
    }

    if (ldata) free(ldata);
    if (rdata) free(rdata);
    
    result.type = VAR_UNSET;
    
    return result;
}

Variable FreeSound(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;

    AUD_FreeSound((int)ToNumber(&argv[0]));
    
    return result;    
}

Variable SoundExists(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = AUD_SoundExists((int)ToNumber(&argv[0]));
    
    return result;    
}

Variable PlaySound(int argc, Variable *argv) {
    Variable result;

    float vol = argc > 1 ? (float)ToNumber(&argv[1]) : 1.0f;
    float pan = argc > 2 ? (float)ToNumber(&argv[2]) : 0.0f;
 
    AUD_PlaySound((int)ToNumber(&argv[0]), vol, pan);

    result.type = VAR_UNSET;
    
    return result;    
}

Variable LoadMusic(int argc, Variable *argv) {
    Variable result;
    int id = 1;
    
    while (AUD_MusicExists(id)) id++;
    
    if (AUD_LoadMusic(id, ToString(&argv[0], 8))) {
        result.type = VAR_NUM;
        result.value.n = id;
    }
    else result.type = VAR_UNSET;
  
    return result;    
}

Variable LoadMusicLegacy(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    AUD_LoadMusic((int)ToNumber(&argv[0]), ToString(&argv[1], 8));
    
    return result;    
}

Variable FreeMusic(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;

    AUD_FreeMusic((int)ToNumber(&argv[0]));
    
    return result;    
}

Variable MusicExists(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_NUM;
    result.value.n = AUD_MusicExists((int)ToNumber(&argv[0]));
    
    return result;    
}

Variable PlayMusic(int argc, Variable *argv) {
    Variable result;
    
    int loop = argc > 1 ? (int)ToNumber(&argv[1]) : 0;
    
    result.type = VAR_UNSET;
    
    AUD_PlayMusic((int)ToNumber(&argv[0]), loop);
    
    return result;    
}

Variable StopMusic(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    AUD_StopMusic((int)ToNumber(&argv[0]));
    
    return result;    
}

Variable SetMusicVolume(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    AUD_SetMusicVolume((int)ToNumber(&argv[0]), (float)ToNumber(&argv[1]));
    
    return result;    
}

/*
 * Download((str)url, (str)filename/(num)type)
 * -------------------------------------------
 */
Variable Download(int argc, Variable *argv) {
    Variable result;

    /* Download to filename. */
    if (argv[1].type == VAR_STR) {
        int dataSize = 0;
        char *data = WIN_DownloadFile(ToString(&argv[0], 8), &dataSize);
        result.type = VAR_NUM;
        if (data) {
            FILE *file = fopen(argv[1].value.s, "wb");
            if (file) {
                fwrite(data, sizeof(char), (size_t)dataSize, file);
                fclose(file);
                result.value.n = 1;
            }
            else {
                result.value.n = 0;
            }
            free(data);
        }
        else {
            result.value.n = 0;
        }
    }
    /* Download to string. */
    else if (argv[1].type == VAR_NUM && (int)argv[1].value.n == VAR_STR) {
        int dataSize = 0;
        char *data = WIN_DownloadFile(ToString(&argv[0], 8), &dataSize);
        if (data) {
            result.type = VAR_STR;
            result.value.s = data;
        }
        else {
            result.type = VAR_UNSET;
        }
    }
    /* Download to "byte" array. */
    else if (argv[1].type == VAR_NUM && (int)argv[1].value.n == VAR_TBL) {
        int dataSize = 0;
        char *data = WIN_DownloadFile(ToString(&argv[0], 8), &dataSize);
        if (data) {
            result.type = VAR_TBL;
            result.value.t = HT_Create(8);
            for (int i = 0; i < dataSize; i++) HT_Add(result.value.t, 0, i, NewNumber((double)data[i]));
            MM_SetType(result.value.t, 1);
        }
        else {
            result.type = VAR_UNSET;
        }
        
    }
    else {
        RuntimeError("Invalid destination parameter (SYS_DOWNLOAD)");
        result.type = VAR_UNSET;
    }
    
    return result;
}

/*
 * console on/off
 * --------------
 */
Variable Console(int argc, Variable *argv) {
    Variable result;
    result.type = VAR_UNSET;
    WIN_ShowConsole((int)ToNumber(&argv[0]));
    return result;
}

/*
 * SYS_Init
 * --------
 * Init and return array with system command functions.
 */
N7CFunction *SYS_Init() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    sStartSec = ts.tv_sec;
    sRunning = 1;
 
    sInkeyHead = 0;
    sInkeyTail = 0;
    for (int i = 0; i < 256; i++) sKeyDown[i] = 0;

    sFiles = HT_Create(1);
    
    sZones = HT_Create(1);
    sActiveZone = 0;
    sZoneClicked = 0;
    sZoneMouseDown = 0;

    WIN_Init();
    AUD_Init();
 
    sSystemCommandFunctions[SYS_PLN] = Pln;
    sSystemCommandFunctions[SYS_READ_LINE] = ReadLine;
    sSystemCommandFunctions[SYS_DATE_TIME] = DateTime;
    sSystemCommandFunctions[SYS_TIME] = Time;
    sSystemCommandFunctions[SYS_CLOCK] = Clock;
    sSystemCommandFunctions[SYS_SLEEP] = SleepMS;
    sSystemCommandFunctions[SYS_FRAME_SLEEP] = FrameSleepMS;
    sSystemCommandFunctions[SYS_RND] = Rnd;
    sSystemCommandFunctions[SYS_RANDOMIZE] = Randomize;
    sSystemCommandFunctions[SYS_SYSTEM] = System;
    sSystemCommandFunctions[SYS_CAPTURE] = Capture;
    sSystemCommandFunctions[SYS_SPLIT_STR] = SplitStr;
    sSystemCommandFunctions[SYS_LEFT_STR] = LeftStr;
    sSystemCommandFunctions[SYS_RIGHT_STR] = RightStr;
    sSystemCommandFunctions[SYS_MID_STR] = MidStr;
    sSystemCommandFunctions[SYS_IN_STR] = InStr;
    sSystemCommandFunctions[SYS_REPLACE_STR] = ReplaceStr;
    sSystemCommandFunctions[SYS_LOWER_STR] = LowerStr;
    sSystemCommandFunctions[SYS_UPPER_STR] = UpperStr;
    sSystemCommandFunctions[SYS_CHR] = Chr;
    sSystemCommandFunctions[SYS_ASC] = Asc;
    sSystemCommandFunctions[SYS_STR] = Str;
    sSystemCommandFunctions[SYS_TBL_HAS_KEY] = TblHasKey;
    sSystemCommandFunctions[SYS_TBL_HAS_VALUE] = TblHasValue;
    sSystemCommandFunctions[SYS_TBL_FREE_KEY] = TblFreeKey;
    sSystemCommandFunctions[SYS_TBL_FREE_VALUE] = TblFreeValue;
    sSystemCommandFunctions[SYS_TBL_CLEAR] = TblClear;
    sSystemCommandFunctions[SYS_TBL_INSERT] = TblInsert;
    sSystemCommandFunctions[SYS_SET_CLIPBOARD] = SetClipboard;
    sSystemCommandFunctions[SYS_GET_CLIPBOARD] = GetClipboard;

    sSystemCommandFunctions[SYS_CREATE_FILE] = CreateFile;
    sSystemCommandFunctions[SYS_CREATE_FILE_LEGACY] = CreateFileLegacy;
    sSystemCommandFunctions[SYS_OPEN_FILE] = OpenFile;
    sSystemCommandFunctions[SYS_OPEN_FILE_LEGACY] = OpenFileLegacy;
    sSystemCommandFunctions[SYS_FREE_FILE] = FreeFile;
    sSystemCommandFunctions[SYS_FILE_EXISTS] = FileExists;
    sSystemCommandFunctions[SYS_FILE_WRITE] = FileWrite;
    sSystemCommandFunctions[SYS_FILE_WRITE_LINE] = FileWriteLine;
    sSystemCommandFunctions[SYS_FILE_READ] = FileRead;
    sSystemCommandFunctions[SYS_FILE_READ_CHAR] = FileReadChar;
    sSystemCommandFunctions[SYS_FILE_READ_LINE] = FileReadLine;
    sSystemCommandFunctions[SYS_OPEN_FILE_DIALOG] = OpenFileDialog;
    sSystemCommandFunctions[SYS_SAVE_FILE_DIALOG] = SaveFileDialog;
    sSystemCommandFunctions[SYS_CHECK_FILE_EXISTS] = CheckFileExists;
    sSystemCommandFunctions[SYS_SET_WINDOW] = SetWindow;
    sSystemCommandFunctions[SYS_SET_REDRAW] = SetRedraw;
    sSystemCommandFunctions[SYS_WIN_REDRAW] = RedrawWindow;
    sSystemCommandFunctions[SYS_WIN_ACTIVE] = WindowActive;
    sSystemCommandFunctions[SYS_WIN_EXISTS] = WindowExists;
    sSystemCommandFunctions[SYS_SCREEN_W] = ScreenW;
    sSystemCommandFunctions[SYS_SCREEN_H] = ScreenH;
    sSystemCommandFunctions[SYS_MOUSE_X] = MouseX;
    sSystemCommandFunctions[SYS_MOUSE_Y] = MouseY;
    sSystemCommandFunctions[SYS_MOUSE_DX] = MouseDX;
    sSystemCommandFunctions[SYS_MOUSE_DY] = MouseDY;
    sSystemCommandFunctions[SYS_MOUSE_DOWN] = MouseDown;
    sSystemCommandFunctions[SYS_SET_MOUSE] = SetMouse;
    sSystemCommandFunctions[SYS_CREATE_ZONE] = CreateZone;
    sSystemCommandFunctions[SYS_CREATE_ZONE_LEGACY] = CreateZoneLegacy;
    sSystemCommandFunctions[SYS_FREE_ZONE] = FreeZone;
    sSystemCommandFunctions[SYS_ZONE] = ZoneInfo;
    sSystemCommandFunctions[SYS_ZONE_X] = ZoneX;
    sSystemCommandFunctions[SYS_ZONE_Y] = ZoneY;
    sSystemCommandFunctions[SYS_ZONE_W] = ZoneW;
    sSystemCommandFunctions[SYS_ZONE_H] = ZoneH;
    sSystemCommandFunctions[SYS_JOY_X] = JoyX;
    sSystemCommandFunctions[SYS_JOY_Y] = JoyY;
    sSystemCommandFunctions[SYS_JOY_BUTTON] = JoyButton;
    sSystemCommandFunctions[SYS_INKEY] = Inkey;
    sSystemCommandFunctions[SYS_KEY_DOWN] = KeyDown;
    sSystemCommandFunctions[SYS_SET_IMAGE] = SetImage;
    sSystemCommandFunctions[SYS_SET_IMAGE_CLIP_RECT] = SetImageClipRect;
    sSystemCommandFunctions[SYS_CLEAR_IMAGE_CLIP_RECT] = ClearImageClipRect;
    sSystemCommandFunctions[SYS_SET_COLOR] = SetColor;
    sSystemCommandFunctions[SYS_SET_COLOR_INT] = SetColorInt;
    sSystemCommandFunctions[SYS_SET_ADDITIVE] = SetAdditive;
    sSystemCommandFunctions[SYS_CLS] = Cls;
    sSystemCommandFunctions[SYS_SET_PIXEL] = SetPixel;
    sSystemCommandFunctions[SYS_GET_PIXEL] = GetPixel;
    sSystemCommandFunctions[SYS_GET_PIXEL_INT] = GetPixelInt;
    sSystemCommandFunctions[SYS_DRAW_PIXEL] = DrawPixel;
    sSystemCommandFunctions[SYS_DRAW_LINE] = DrawLine;
    sSystemCommandFunctions[SYS_DRAW_RECT] = DrawRect;
    sSystemCommandFunctions[SYS_DRAW_ELLIPSE] = DrawEllipse;
    sSystemCommandFunctions[SYS_DRAW_POLYGON] = DrawPolygon;
    sSystemCommandFunctions[SYS_DRAW_POLYGON_IMAGE] = DrawPolygonImage;
    sSystemCommandFunctions[SYS_DRAW_VRASTER] = DrawVRaster;
    sSystemCommandFunctions[SYS_DRAW_HRASTER] = DrawHRaster;
    sSystemCommandFunctions[SYS_LOAD_IMAGE] = LoadImage;
    sSystemCommandFunctions[SYS_LOAD_IMAGE_LEGACY] = LoadImageLegacy;
    sSystemCommandFunctions[SYS_SAVE_IMAGE] = SaveImage;
    sSystemCommandFunctions[SYS_CREATE_IMAGE] = CreateImage;
    sSystemCommandFunctions[SYS_CREATE_IMAGE_LEGACY] = CreateImageLegacy;
    sSystemCommandFunctions[SYS_FREE_IMAGE] = FreeImage;
    sSystemCommandFunctions[SYS_SET_IMAGE_COLOR_KEY] = SetImageColorKey;
    sSystemCommandFunctions[SYS_SET_IMAGE_GRID] = SetImageGrid;
    sSystemCommandFunctions[SYS_IMAGE_EXISTS] = ImageExists;
    sSystemCommandFunctions[SYS_IMAGE_WIDTH] = ImageWidth;
    sSystemCommandFunctions[SYS_IMAGE_HEIGHT] = ImageHeight;
    sSystemCommandFunctions[SYS_IMAGE_COLS] = ImageCols;
    sSystemCommandFunctions[SYS_IMAGE_ROWS] = ImageRows;
    sSystemCommandFunctions[SYS_IMAGE_CELLS] = ImageCells;

    sSystemCommandFunctions[SYS_DRAW_IMAGE] = DrawImage;
    sSystemCommandFunctions[SYS_CREATE_FONT] = CreateFont;
    sSystemCommandFunctions[SYS_CREATE_FONT_LEGACY] = CreateFontLegacy;
    sSystemCommandFunctions[SYS_LOAD_FONT] = LoadFont;
    sSystemCommandFunctions[SYS_LOAD_FONT_LEGACY] = LoadFontLegacy;
    sSystemCommandFunctions[SYS_SAVE_FONT] = SaveFont;
    sSystemCommandFunctions[SYS_FREE_FONT] = FreeFont;
    sSystemCommandFunctions[SYS_SET_FONT] = SetFont;
    sSystemCommandFunctions[SYS_FONT_EXISTS] = FontExists;
    sSystemCommandFunctions[SYS_FONT_WIDTH] = FontWidth;
    sSystemCommandFunctions[SYS_FONT_HEIGHT] = FontHeight;
    sSystemCommandFunctions[SYS_SCROLL] = Scroll;
    sSystemCommandFunctions[SYS_WRITE] = Write;
    sSystemCommandFunctions[SYS_WRITE_LINE] = WriteLine;
    sSystemCommandFunctions[SYS_CENTER] = Center;
    sSystemCommandFunctions[SYS_SET_JUSTIFICATION] = SetJustification;
    sSystemCommandFunctions[SYS_SET_CARET] = SetCaret;

    sSystemCommandFunctions[SYS_LOAD_SOUND] = LoadSound;
    sSystemCommandFunctions[SYS_LOAD_SOUND_LEGACY] = LoadSoundLegacy;
    sSystemCommandFunctions[SYS_CREATE_SOUND] = CreateSound;
    sSystemCommandFunctions[SYS_CREATE_SOUND_LEGACY] = CreateSoundLegacy;
    sSystemCommandFunctions[SYS_FREE_SOUND] = FreeSound;
    sSystemCommandFunctions[SYS_SOUND_EXISTS] = SoundExists;
    sSystemCommandFunctions[SYS_PLAY_SOUND] = PlaySound;
    sSystemCommandFunctions[SYS_LOAD_MUSIC] = LoadMusic;
    sSystemCommandFunctions[SYS_LOAD_MUSIC_LEGACY] = LoadMusicLegacy;
    sSystemCommandFunctions[SYS_FREE_MUSIC] = FreeMusic;
    sSystemCommandFunctions[SYS_MUSIC_EXISTS] = MusicExists;
    sSystemCommandFunctions[SYS_PLAY_MUSIC] = PlayMusic;
    sSystemCommandFunctions[SYS_STOP_MUSIC] = StopMusic;
    sSystemCommandFunctions[SYS_SET_MUSIC_VOLUME] = SetMusicVolume;
    sSystemCommandFunctions[SYS_DOWNLOAD] = Download;
    sSystemCommandFunctions[SYS_CONSOLE] = Console;
    sSystemCommandFunctions[SYS_DRAW_IMAGE_TRANSFORMED] = DrawImageTransformed;
    sSystemCommandFunctions[SYS_DRAW_POLYGON_TRANSFORMED] = DrawPolygonTransformed;
    sSystemCommandFunctions[SYS_DRAW_POLYGON_IMAGE_TRANSFORMED] = DrawPolygonImageTransformed;
    sSystemCommandFunctions[SYS_W3D_RENDER] = W3D_Render;
    sSystemCommandFunctions[SYS_FILE_TELL] = FileTell;
    sSystemCommandFunctions[SYS_FILE_SEEK] = FileSeek;

    return sSystemCommandFunctions;
}

void DeleteFile(void *data) {
    File *f = (File *)data;
    fclose(f->file);
    free(f);
}

/*
 * SYS_Release
 * -----------
 * Close window, release resources.
 */
void SYS_Release() {
    HT_Free(sFiles, DeleteFile);
    HT_Free(sZones, DeleteZone);
    if (sWindowMessage) free(sWindowMessage);
    WIN_Close();
    AUD_Close();
}

/*
 * SYS_TerminateProgram
 * --------------------
 */
void SYS_TerminateProgram() {
    sRunning = 0;
    TerminateProgram();
}

/*
 * SYS_WindowFocusChanged
 * ----------------------
 */
void SYS_WindowFocusChanged(int value) {
    /* Just clear all input. */
    ClearInkeyBuffer();
    ClearKeyDown();
    ClearMouseButtons();
}

/*
 * SYS_WindowMessageReceived
 * -------------------------
 */
void SYS_WindowMessageReceived(const char *msg) {
    if (sWindowMessage) free(sWindowMessage);
    sWindowMessage = strdup(msg);
}

/*
 * SYS_MouseMove
 * -------------
 */
void SYS_MouseMove(int x, int y) {
    sMouseX = x;
    sMouseY = y;
   
    if (!sZoneMouseDown) {
        if (sActiveZone && !MouseOverZone(sActiveZone, 0)) sActiveZone = 0;
        if (!sActiveZone) {
            /* Add a FindValue, dammet! */
            HashEntry *he = HT_FindEntry(sZones, MouseOverZone, 0);
            sActiveZone = he ? (Zone*)he->data : 0;
        }
    }
}

/*
 * SYS_MouseDown
 * -------------
 */
void SYS_MouseDown(int button) {
    sMouseButton[button] = 1;
    sMouseButtonCache[button] = 1;

    if (button == 0) sZoneMouseDown = 1;
}

/*
 * SYS_MouseUp
 * -----------
 */
void SYS_MouseUp(int button) {
    sMouseButton[button] = 0;

    if (button == 0) {
        HashEntry *he = HT_FindEntry(sZones, MouseOverZone, 0);
        sZoneMouseDown = 0;
        sZoneClicked = sActiveZone && MouseOverZone(sActiveZone, 0) ? sActiveZone : 0;
        sActiveZone = he ? (Zone *)he->data : 0;
    }
}

/*
 * SYS_MouseWheel
 * --------------
 */
void SYS_MouseWheel(int step) {
    sMouseButton[2] = step;
}

/*
 * SYS_JoyMove
 * -----------
 */
void SYS_JoyMove(int x, int y) {
    sJoyX = x;
    sJoyY = y;
}

/*
 * SYS_JoyButtonDown
 * -----------------
 */
void SYS_JoyButtonDown(int button) {
    if (button >= 0 && button <= 3) sJoyButtons[button] = 1;
}

/*
 * SYS_JoyButtonUp
 * ---------------
 */
void SYS_JoyButtonUp(int button) {
    if (button >= 0 && button <= 3) sJoyButtons[button] = 0;
}

/*
 * SYS_KeyChar
 * -----------
 */
void SYS_KeyChar(unsigned int c) {
    sInkeyBuffer[sInkeyHead] = c;
    sInkeyHead = (sInkeyHead + 1)%INKEY_BUFFER_SIZE;
    if (sInkeyHead == sInkeyTail) sInkeyTail = (sInkeyTail + 1)%INKEY_BUFFER_SIZE;
}

/*
 * SYS_KeyDown
 * -----------
 */
void SYS_KeyDown(unsigned int c) {
    if (c > 255) return;
    sKeyDown[c] = 1;
}

/*
 * SYS_KeyUp
 * ---------
 */
void SYS_KeyUp(unsigned int c) {
    if (c > 255) return;
    sKeyDown[c] = 0;
}
