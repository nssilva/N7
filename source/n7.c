/*
 * n7.c
 * ----
 * N7 compiler. Compiles n7 text files to n7a (n7 assembler) text files. 
 *
 * By: Marcus 2021
 */

#include "n7.h"
#include "asm.h"
#include "renv.h"
#include "syscmd.h"
#include "keycodes.h"
#include "hash_table.h"

#include "stdlib.h"
#include "stdio.h"
#include "ctype.h"
#include "string.h"
#include "setjmp.h"

/* Basic optimization flags. */
#define OPT_MALS
#define OPT_MSSP
#define OPT_LOADPARAM
#define OPT_PVAL

#define IDENTIFIER_STACK_SIZE 64
#define BLOCK_STACK_SIZE 128
#define N7_NUMBER_MAX_CHARS 64

/* Keywords, no specific order. */
typedef enum {
    N7_NONE,
    N7_END,
    N7_GC,
    N7_ASSERT,
    N7_INCLUDE,
    N7_ASM,
    N7_ENDASM,
    N7_AND,
    N7_OR,
    N7_XOR,
    N7_NOT,
    N7_IF,
    N7_THEN,
    N7_ELSE,
    N7_ELSEIF,
    N7_ENDIF,
    N7_SELECT,
    N7_CASE,
    N7_DEFAULT,
    N7_ENDSELECT,
    N7_WHILE,
    N7_WEND,
    N7_DO,
    N7_LOOP,
    N7_UNTIL,
    N7_FOR,
    N7_TO,
    N7_STEP,
    N7_NEXT,
    N7_FOREACH,
    N7_IN,
    N7_BREAK,
    N7_TOSTRING,
    N7_TONUMBER,
    N7_TOINTEGER,
    N7_UNSET,
    N7_SIZEOF,
    N7_LEN,
    N7_FREE,
    N7_DIM,
    N7_FILL,
    N7_COPY,
    N7_FUNCTION,
    N7_ENDFUNC,
    N7_RETURN,
    N7_VISIBLE,
    N7_CONSTANT,
    N7_TYPEOF,
    N7_ABS,
    N7_SGN,
    N7_COS,
    N7_SIN,
    N7_TAN,
    N7_ACOS,
    N7_ASIN,
    N7_ATAN,
    N7_ATAN2,
    N7_SQR,
    N7_POW,
    N7_FLOOR,
    N7_CEIL,
    N7_ROUND,
    N7_RAD,
    N7_DEG,
    N7_MIN,
    N7_MAX,
    N7_THIS,
    N7_PLN,
    N7_RLN,
    N7_DATETIME,
    N7_TIME,
    N7_CLOCK,
    N7_WAIT,
    N7_FWAIT,
    N7_RND,
    N7_RANDOMIZE,
    N7_SYSTEM,
    N7_SPLIT,
    N7_LEFT,
    N7_RIGHT,
    N7_MID,
    N7_INSTR,
    N7_REPLACE,
    N7_LOWER,
    N7_UPPER,
    N7_CHR,
    N7_ASC,
    N7_KEY,
    N7_VAL,
    N7_CLEAR,
    N7_INSERT,
    N7_KEYOF,
    N7_SET,
    N7_LOAD,
    N7_SAVE,
    N7_CREATE,
    N7_OPEN,
    N7_OPENFILEDIALOG,
    N7_SAVEFILEDIALOG,
    N7_EXISTS,
    N7_DRAW,
    N7_WINDOW,
    N7_ACTIVE,
    N7_REDRAW,
    N7_SCREENW,
    N7_SCREENH,
    N7_MOUSE,
    N7_MOUSEX,
    N7_MOUSEY,
    N7_MOUSEDX,
    N7_MOUSEDY,
    N7_MOUSEBUTTON,
    N7_JOYX,
    N7_JOYY,
    N7_JOYBUTTON,
    N7_ZONE,
    N7_CREATEZONE,
    N7_ZONEX,
    N7_ZONEY,
    N7_ZONEW,
    N7_ZONEH,
    N7_INKEY,
    N7_KEYDOWN,
    N7_COLOR,
    N7_COLORI,
    N7_ADDITIVE,
    N7_CLIP,
    N7_PIXEL,
    N7_PIXELI,
    N7_LINE,
    N7_RECT,
    N7_ELLIPSE,
    N7_POLY,
    N7_VRASTER,
    N7_HRASTER,
    N7_CLS,
    N7_IMAGE,
    N7_FONT,
    N7_FILE,
    N7_OPENFILE,
    N7_CREATEFILE,
    N7_FREAD,
    N7_FREADC,
    N7_FRLN,
    N7_FILETELL,
    N7_FILESEEK,
    N7_SEEK,
    N7_PRIMARY,
    N7_WIDTH,
    N7_HEIGHT,
    N7_COLS,
    N7_ROWS,
    N7_CELLS,
    N7_COLORKEY,
    N7_GRID,
    N7_LOADIMAGE,
    N7_CREATEIMAGE,
    N7_LOADFONT,
    N7_FWIDTH,
    N7_FHEIGHT,
    N7_WRITE,
    N7_WLN,
    N7_JUSTIFICATION,
    N7_CENTER,
    N7_CARET,
    N7_CREATEFONT,
    N7_SCROLL,
    N7_CLIPBOARD,
    N7_SOUND,
    N7_LOADSOUND,
    N7_CREATESOUND,
    N7_MUSIC,
    N7_LOADMUSIC,
    N7_PLAY,
    N7_STOP,
    N7_VOLUME,
    N7_DOWNLOAD,
    N7_CONSOLE,
    N7_TRANSFORMED,
    N7_LOAD_FUNCTION,
    N7_CALL_FUNCTION,
    /* Constants. */
    N7_VERSION,
    N7_TRUE,
    N7_FALSE,
    N7_ON,
    N7_OFF,
    N7_TYPE_NUMBER,
    N7_TYPE_STRING,
    N7_TYPE_FUNCTION,
    N7_TYPE_TABLE,
    N7_TYPE_UNSET,
    N7_SEEK_SET,
    N7_SEEK_CUR,
    N7_SEEK_END,
    N7_PI,
    /* Key codes, constants. */
    N7_KEY_TAB,
    N7_KEY_RETURN,
    N7_KEY_SHIFT,
    N7_KEY_CONTROL,
    N7_KEY_MENU,
    N7_KEY_ESCAPE,
    N7_KEY_SPACE,
    N7_KEY_PAGE_UP,
    N7_KEY_PAGE_DOWN,
    N7_KEY_END,
    N7_KEY_HOME,
    N7_KEY_LEFT,
    N7_KEY_UP,
    N7_KEY_RIGHT,
    N7_KEY_DOWN,
    N7_KEY_INSERT,
    N7_KEY_DELETE,
    N7_KEY_0,
    N7_KEY_1,
    N7_KEY_2,
    N7_KEY_3,
    N7_KEY_4,
    N7_KEY_5,
    N7_KEY_6,
    N7_KEY_7,
    N7_KEY_8,
    N7_KEY_9,
    N7_KEY_A,
    N7_KEY_B,
    N7_KEY_C,
    N7_KEY_D,
    N7_KEY_E,
    N7_KEY_F,
    N7_KEY_G,
    N7_KEY_H,
    N7_KEY_I,
    N7_KEY_J,
    N7_KEY_K,
    N7_KEY_L,
    N7_KEY_M,
    N7_KEY_N,
    N7_KEY_O,
    N7_KEY_P,
    N7_KEY_Q,
    N7_KEY_R,
    N7_KEY_S,
    N7_KEY_T,
    N7_KEY_U,
    N7_KEY_V,
    N7_KEY_W,
    N7_KEY_X,
    N7_KEY_Y,
    N7_KEY_Z,
    N7_KEY_MULTIPLY,
    N7_KEY_ADD,
    N7_KEY_SEPARATOR,
    N7_KEY_SUBTRACT,
    N7_KEY_DIVIDE,
    N7_KEY_F1,
    N7_KEY_F2,
    N7_KEY_F3,
    N7_KEY_F4,
    N7_KEY_F5,
    N7_KEY_F6,
    N7_KEY_F7,
    N7_KEY_F8,
    N7_KEY_F9,
    N7_KEY_F10,
    N7_KEY_F11,
    N7_KEY_F12    
} Keyword;

/* Type of factor. */
typedef enum {
    FACTOR_UNKNOWN = 1,
    FACTOR_NAME,
    FACTOR_TABLE,
    FACTOR_ARRAY,
    FACTOR_VALUE,
    FACTOR_FUNCTION
} FactorType;

/* Return value of factor. */
typedef struct {
    FactorType type;
    void *data;
} FactorInfo;

/* Function parameter, linked in reverse order since that's how the arguments
   are popped from the stack. */
typedef struct _FunctionParameter FunctionParameter;
struct _FunctionParameter {
    char *name;
    FunctionParameter *next;
};
typedef struct _FunctionDefinition FunctionDefinition;

/* Function. */
struct _FunctionDefinition {
    int index;
    char *name;
    char anonymous;
    int paramCount;
    FunctionParameter *parameters;
    FunctionDefinition *parent;
    HashTable *functions;
};
FunctionDefinition *sRootFunction; /* Root of all functions. */
FunctionDefinition *sFunction; /* Current function. */
static int sFunctionIndex; 
static int sLocalScope;
static FunctionDefinition *GetFunction(const char *name);
static void DeleteFunctions();

/* Visible (global) identifiers. */
typedef struct {
    char readOnly;
} VisibleEntry;
HashTable *sVisible;
static void DeleteVisible();

/* Constant and visible used to be same thing, just with readOnly flag set or
   not. But added a separate list when implementing libraries, keeping the same
   structure. Visibles are local to files, while constants are truely global. */
HashTable *sConstants;
static void DeleteConstants();

/* A stack of tables used to make sure (not really) variables are assigned
   values before they're used. */
static HashTable *sIdentifierStack[IDENTIFIER_STACK_SIZE];
static int sIdentifierStackIndex;
static HashTable *sIdentifiers;

/* Block info, mainly used for popping things when a return statement is within
   loops that use the stack. */
typedef enum {
    BLOCK_TYPE_GENERIC = 0,
    BLOCK_TYPE_IF,
    BLOCK_TYPE_SELECT,
    BLOCK_TYPE_DO,
    BLOCK_TYPE_WHILE,
    BLOCK_TYPE_FOR,
    BLOCK_TYPE_FOREACH
} BlockType;
typedef struct {
    BlockType type;
    int localScope;
} BlockInfo;
static BlockInfo sBlockInfoStack[BLOCK_STACK_SIZE];
static int sBlockLevel;
static void IncBlockLevel(BlockType type);
static void DecBlockLevel();

/* Source env stack, when working with included files. */
typedef struct sSourceEnv SourceEnv;
struct sSourceEnv {
    FILE *file;
    char *filename;
    char *libName;
    int lineNumber;
    HashTable *visible;
    SourceEnv *next;
};
static SourceEnv *sSourceEnvStack;

/* Added this hack 2023-10-18, dare not touch the SourceEnv stuff (used for other
   stuff than keeping track of what files have already been included). */
typedef struct sIncludeInfo IncludeInfo;
struct sIncludeInfo {
    char *filename;
    IncludeInfo *next;
};
static IncludeInfo *sIncludeInfoList;
int AddIncludeInfo(const char *filename);
void ClearIncludeInfoList();

/* Table containing keywords. */
typedef struct {
    Keyword keyword;
    /* For constants. */
    char type;
    union {
        int i;
        double f;
        const char *s;
    } value; 
} KeywordEntry;
static HashTable *sKeywords;
static void CreateKeywords();
static void DeleteKeywords();
static const char *GetKeywordString(Keyword keyword);

/* Error string. */
static char sError[1024];

/* Tokens. */
typedef enum {
    N7_KEYWORD = 0, /* Value in sKeyword. */
    N7_STRING,  /* Value in sString. */
    N7_NUMBER,  /* Value in sNumber, string version in sNumberS. */
    N7_NAME,    /* Value in sName. */
    N7_CHAR,    /* Value in sChar. */
    N7_EOL,
    N7_EOF,
} Token;

static void Prescan();
static void GetNext();
static void EatNewLines();
static void ExpectChar(char c);
static int CouldGetChar(char c);
static void ExpectNewLine();
static int Declared(const char *name);
static int Statement();
static void Block();
static void VisibleDeclaration();
static void ConstantDeclaration();
static void End();
static void Gc();
static void Assert();
static void Include();
static void Asm();
static void If();
static void Select();
static void While();
static void Do();
static void For();
static void Foreach();
static void Break();
static FunctionDefinition *Function();
static void Return();
static void CallFunction(FunctionDefinition *fd);
static FactorInfo Expression();
static FactorInfo ParsePrecedenceLevelZero();
static void ConstExpression();
static void ErrorUnexpected();
static void Error(const char *msg);
/* Keep track of break label indexes in a stack. */
static int sBreakStack[64];
static int sBreakCount;
static void PushBreak(int labelIndex);
static void PopBreak();
/* Helper. */
static int CallSystemFunction(int sysFunction, int min, int max, int isFunc, int getNext);
static int CallCFunction(int isFunc, int getNext);

FILE *sSrcFile;
FILE *sDstFile;
static unsigned int sRuntimeFlags = 0;
static int sMemoryRequest = 0;
static char *sSrcFilename;
static char *sMainSrcFilename;
static char *sLibName;
static char sLibPath[_MAX_PATH] = "";
static char sUserLibPath[_MAX_PATH] = "";
static Token sNext;
static Keyword sKeyword;
static KeywordEntry *sKeywordEntry;
static char sString[ASM_STRING_MAX_CHARS];
static double sNumber;
static char sNumberS[N7_NUMBER_MAX_CHARS];
static char sName[ASM_VAR_MAX_CHARS];
static char sChar;
static int sLabelIndex;
static int sLineNumber;
static int sLastEOLWasReal;
static int sPrescan;
static int sAsm; 

/* For throwing and catching errors. */
static jmp_buf sErrorJmpBuf;

/*
 * CleanFilename
 * -------------
 */
static char *CleanFilename(const char *filename) {
    int i = 0;

    for (i = strlen(filename) - 1; i >= 0; i--) 
        if (filename[i] == '\\' || filename[i] == '/') break;

    return strdup(filename + i + 1);
}

/*
 * N7_Compile
 * ----------
 * Compile n7 file srcFilename to assembler file dstFilename.
 */
int N7_Compile(const char *srcFilename, const char *dstFilename) {
    int error = 0;
    int result = 0;
    sRuntimeFlags = 0;
    sRootFunction = 0;
    sFunctionIndex = 0;
    sKeywords = 0;
    sLabelIndex = 0;
    sBreakCount = 0;
    sBlockLevel = 0;
    sIdentifiers = 0;
    sIdentifierStackIndex = 0;
    sSourceEnvStack = 0;
    sIncludeInfoList = 0;
    sLibName = 0;
    
    sSrcFile = fopen(srcFilename, "r");
    if (!sSrcFile) {
        sprintf(sError, "Could not open file '%s' for reading", srcFilename);
        return N7_FAILURE;
    }
    
    sDstFile = fopen(dstFilename, "w");
    if (!sDstFile) {
        fclose(sSrcFile);
        sprintf(sError, "Could not open file '%s' for writing", dstFilename);
        return N7_FAILURE;
    }
    
    //sSrcFilename = strdup(srcFilename);
    sSrcFilename = CleanFilename(srcFilename);
    sMainSrcFilename = sSrcFilename;
    
    CreateKeywords();

    sLocalScope = 0;

    /* Create global scope. */
    sVisible = HT_Create(1);
    sConstants = HT_Create(1);
    
    /* Add "args" to visible. Renv will load it with the command line
       arguments. */
    VisibleEntry *args = (VisibleEntry *)malloc(sizeof(VisibleEntry));
    args->readOnly = 0;
    HT_Add(sVisible, "args", 0, args);
    
    strcpy(sError, "");

    /* Set up long jump to catch errors. */
    error = setjmp(sErrorJmpBuf);
    /* Initial state. */
    sAsm = 0;
    if (error == 0) {
        sIdentifiers = HT_Create(1);

        Prescan();
        ClearIncludeInfoList();
        rewind(sSrcFile);
        sLineNumber = 1;
        sFunctionIndex = 0;
        sFunction = sRootFunction;
        fprintf(sDstFile, "/file:%s\n", sSrcFilename);
        fprintf(sDstFile, "/line:%d\n", sLineNumber);
        GetNext();
        Block();
        if (sNext != N7_EOF) ErrorUnexpected();
        result = N7_SUCCESS;
    }
    /* Catch and report error. */
    else {
        result = N7_FAILURE;
    }
    
    /* Close files. */
    fclose(sSrcFile);
    fclose(sDstFile);
    free(sSrcFilename);

    /* Clean up */
    DeleteFunctions();
    DeleteKeywords();
    DeleteVisible();
    DeleteConstants();
    while (sIdentifierStackIndex-- > 0) {
        free(sIdentifierStack[sIdentifierStackIndex]);
        sIdentifierStack[sIdentifierStackIndex] = 0;
    }
    free(sIdentifiers);
    sIdentifiers = 0;
    while (sSourceEnvStack) {
        SourceEnv *next;
        
        fclose(sSourceEnvStack->file);
        free(sSourceEnvStack->filename);
        free(sSourceEnvStack->libName);
        sVisible = sSourceEnvStack->visible;
        DeleteVisible();
        next = sSourceEnvStack->next;
        free(sSourceEnvStack);
        sSourceEnvStack = next;
    }
    ClearIncludeInfoList();
    
    return result;
}

static void PrescanFile() {
    GetNext();
    
    while (sNext != N7_EOF) {
        /* Scan included file. */
        if (sNext == N7_KEYWORD && sKeyword == N7_INCLUDE) {
            SourceEnv *sourceEnv;
            FILE *file;
            char *fnLibFull;
            
            GetNext();
            if (sNext != N7_STRING) Error("Expected string");

            size_t maxLen = strlen(sUserLibPath) > strlen(sLibPath) ? strlen(sUserLibPath) : strlen(sLibPath); 
            fnLibFull = (char *)malloc(maxLen + strlen(sString) + 1);
            strcpy(fnLibFull, sUserLibPath);
            strcat(fnLibFull, sString);
            
            if (!(file = fopen(fnLibFull, "r"))) {                
                strcpy(fnLibFull, sLibPath);
                strcat(fnLibFull, sString);
                if (!(file = fopen(fnLibFull, "r"))) {
                    char tmp[ASM_STRING_MAX_CHARS + 35];
                    sprintf(tmp, "Could not open file '%s' for reading", sString);
                    Error(tmp);
                }
            }
            
            strcpy(fnLibFull, sString);
            
            /* 23-10-18 */
            /*int alreadyIncluded = 0;
            if (strcmp(sMainSrcFilename, fnLibFull)) {
                sourceEnv = sSourceEnvStack;
                while (sourceEnv) {
                    if (!strcmp(sourceEnv->filename, fnLibFull)) {
                        //printf("prescan, already included lib %s\n", fnLibFull);
                        alreadyIncluded = 1;
                        break;
                    }
                    sourceEnv = sourceEnv->next;
                }
            }
            else {
                //printf("prescan, already included main %s\n", sMainSrcFilename);
                alreadyIncluded = 1;
            }*/
            int alreadyIncluded = !AddIncludeInfo(fnLibFull);
            
            if (alreadyIncluded) {
                free(fnLibFull);
                fclose(file);
            }
            else {            
                sourceEnv = (SourceEnv *)malloc(sizeof(SourceEnv));
                sourceEnv->file = sSrcFile;
                sourceEnv->filename = sSrcFilename;
                sourceEnv->libName = sLibName;
                sourceEnv->lineNumber = sLineNumber;
                sourceEnv->visible = 0;
                sourceEnv->next = sSourceEnvStack;
                sSourceEnvStack = sourceEnv;
       
                sSrcFile = file;
                sSrcFilename = fnLibFull; // strdup(sString);
                sLibName = 0;
                sLineNumber = 1;
                
                PrescanFile();
                
                fclose(sSrcFile);
                free(fnLibFull); // free(sSrcFilename);
                
                sSrcFile = sSourceEnvStack->file;
                sSrcFilename = sSourceEnvStack->filename;
                sLibName = sSourceEnvStack->libName;
                sLineNumber = sSourceEnvStack->lineNumber;
        
                sSourceEnvStack = sSourceEnvStack->next;
                free(sourceEnv);
            }
            
            GetNext();
        }
        /* Function. */
        else if (sNext == N7_KEYWORD && sKeyword == N7_FUNCTION) {
            /* Function definition. */
            FunctionDefinition *fd = (FunctionDefinition *)malloc(sizeof(FunctionDefinition));
            /* Index. */
            fd->index = sFunctionIndex;
            /* Parent. */
            fd->parent = sFunction;
            /* Subfunctions. */
            fd->functions = HT_Create(1);
            
            GetNext();
            /* Static function? */
            if (sNext == N7_NAME) {
                fd->anonymous = 0;
                fd->name = strdup(sName);
                GetNext();
            }
            /* Anonymous. */
            else {
                fd->anonymous = 1;
                fd->name = (char *)malloc(sizeof(char)*12);
                sprintf(fd->name, "%d", sFunctionIndex);
            }
            sFunctionIndex++;
            /* Create function definition. */
            fd->paramCount = 0;
            fd->parameters = 0;
            
            /* Would it shadow any outer function? Not sure if that should be
               allowed ... But I think it should ... for now ... I think. */
            /*if (GetFunction(fd->name)) {
                char tmp[47 + ASM_VAR_MAX_CHARS];
                sprintf(tmp, "Function %s is already defined", fd->name);
                free(fd->name);
                free(fd);
                Error(tmp);
            }*/
            if (!HT_Add(sFunction->functions, fd->name, 0, fd)) {
                char tmp[30 + ASM_VAR_MAX_CHARS];
                sprintf(tmp, "Function %s is already defined", fd->name);
                free(fd->name);
                free(fd);
                Error(tmp);
            }
            
            /* Does the name collide with any of the parent's parameter names? */
            if (fd->parent) {
                FunctionParameter *p = fd->parent->parameters;
                while (p) {
                    if (strcmp(p->name, fd->name) == 0) {
                        char tmp[59 + ASM_VAR_MAX_CHARS];
                        sprintf(tmp, "Collision between parameter and function identifier %s", fd->name);
                        Error(tmp);
                    }
                    p = p->next;
                }
            }

            /* Gather parameters. */
            ExpectChar('(');
            if (sNext == N7_CHAR && sChar == ')') {
                GetNext();
            }
            else {
                do {
                    if (sNext == N7_NAME) {
                        FunctionParameter *p;
                        
                        /* Does the parameter name collide with any visible
                           function? */
                        if (GetFunction(sName)) {
                            char tmp[59 + ASM_VAR_MAX_CHARS];
                            sprintf(tmp, "Collision between parameter and function identifier %s", sName);
                            Error(tmp);
                        }                        
                        /* Does the parameter name collide with a previous
                           parameter name? */
                        p = fd->parameters;
                        while (p) {
                            if (strcmp(p->name, sName) == 0) {
                                char tmp[39 + ASM_VAR_MAX_CHARS*2];
                                sprintf(tmp, "Parameter name %s defined more than once", sName);
                                Error(tmp);
                            }
                            p = p->next;
                        }
                        
                        p = (FunctionParameter *)malloc(sizeof(FunctionParameter));
                        p->name = strdup(sName);
                        p->next = 0;
                        fd->paramCount++;
                        if (!fd->parameters) {
                            fd->parameters = p;
                        }
                        else {
                            p->next = fd->parameters;
                            fd->parameters = p;
                        }
                        GetNext();
                    }
                    else {
                        Error("Expected parameter name");
                    }
                } while (CouldGetChar(','));
                ExpectChar(')');
            }
            sFunction = fd;
        }
        else if (sNext == N7_KEYWORD && sKeyword == N7_ENDFUNC) {
            sFunction = sFunction->parent;
            if (sFunction == 0) ErrorUnexpected();
        }
        GetNext();
    }
    if (sFunction != sRootFunction) Error("Expected 'endfunc'");    
}

/*
 * Prescan
 * -------
 * To allow subroutine declarations before use, a prescan of the code has to be
 * made.
 */
static void Prescan() {
    sPrescan = 1;
    sLineNumber = 1;
    sFunctionIndex = 0;
    
    sRootFunction = (FunctionDefinition *)malloc(sizeof(FunctionDefinition));
    sRootFunction->index = -1;
    sRootFunction->parent = 0;
    sRootFunction->functions = HT_Create(1);
    sRootFunction->name = strdup("Program");
    sRootFunction->anonymous = 0;
    sRootFunction->paramCount = 0;
    sRootFunction->parameters = 0;
    sFunction = sRootFunction;
    
    PrescanFile();
    
    sPrescan = 0;
}

/*
 * GetFunction
 * -----------
 */
static FunctionDefinition *GetFunction(const char *name) {
    FunctionDefinition *f = sFunction;
    while (f) {
        FunctionDefinition *parent = f->parent;
        if ((f = (FunctionDefinition *)HT_Get(f->functions, name, 0))) return f;
        f = parent;
    }
    return 0;
}

/*
 * DeleteFunction
 * --------------
 */
static void DeleteFunction(void *data) {
    FunctionDefinition *f = (FunctionDefinition *)data;
    FunctionParameter *p;
    
    HT_Free(f->functions, DeleteFunction);
    
    free(f->name);
    p = f->parameters;
    while (p) {
        FunctionParameter *next = p->next;
        free(p->name);
        free(p);
        p = next;
    }
    free(f);
}

/*
 * DeleteFunctions
 * ---------------
 * Delete all function definitions recursively.
 */
static void DeleteFunctions() {
    if (sRootFunction) {
        FunctionParameter *p;

        HT_Free(sRootFunction->functions, DeleteFunction);
        free(sRootFunction->name);
        p = sRootFunction->parameters;
        while (p) {
            FunctionParameter *next = p->next;
            free(p->name);
            free(p);
            p = next;
        }
        free(sRootFunction);
        sRootFunction = 0;
    }
}

/*
 * N7_SetLibPath
 * -------------
 */
void N7_SetLibPath(const char *path) {
    strcpy(sLibPath, path);
}

/*
 * N7_SetUserLibPath
 * -----------------
 */
void N7_SetUserLibPath(const char *path) {
    strcpy(sUserLibPath, path);
}

/*
 * N7_GetRuntimeFlags
 * ------------------
 */
unsigned int N7_GetRuntimeFlags() {
    return sRuntimeFlags;
}

/*
 * N7_SetRuntimeFlags
 * ------------------
 */
void N7_SetRuntimeFlags(unsigned int flags) {
    sRuntimeFlags = flags;
}

/*
 * N7_MemoryRequest
 * ----------------
 */
int N7_MemoryRequest() {
    return sMemoryRequest;
}

/*
 * N7_Error
 * --------
 * Return error string.
 */
const char *N7_Error() {
    return sError;
}

/*
 * Error
 * -----
 * Set message and throw error.
 */
static void Error(const char *msg) {
    if (!msg) sprintf(sError, "%s:%d: error: Syntax error", sSrcFilename, sLineNumber);
    else sprintf(sError, "%s:%d: error: %s", sSrcFilename, sLineNumber, msg);
    longjmp(sErrorJmpBuf, 1);
}

/*
 * EndOfBlock
 * ----------
 * Returns 1 if next instruction ends a block.
 */
static int EndOfBlock() {
    return sNext == N7_EOF ||
           (sNext == N7_KEYWORD && (sKeyword == N7_ELSE ||
                                    sKeyword == N7_ELSEIF ||
                                    sKeyword == N7_ENDIF ||
                                    sKeyword == N7_CASE ||
                                    sKeyword == N7_DEFAULT ||
                                    sKeyword == N7_ENDSELECT ||
                                    sKeyword == N7_WEND ||
                                    sKeyword == N7_LOOP ||
                                    sKeyword == N7_UNTIL ||
                                    sKeyword == N7_NEXT ||
                                    sKeyword == N7_ENDFUNC));
}

/*
 * Statement
 * ---------
 * Return 1 until end of a block is reached.
 */
static int Statement() {
    int isFree = 0;

    while (sNext == N7_EOL) GetNext();
    
    if (EndOfBlock()) return 0;

    /* Hack, using assignment code for free <id>. */
    if (sNext == N7_KEYWORD && sKeyword == N7_FREE) {
        isFree = 1;
        GetNext();
    }
  
    /* Identifier for assignment or function call. */
    if (sNext == N7_NAME || (sNext == N7_KEYWORD && sKeyword == N7_THIS) || (sNext == N7_CHAR && sChar == '.')) {
        char name[ASM_VAR_MAX_CHARS];
        VisibleEntry *ve = 0;
        char baseName[ASM_VAR_MAX_CHARS];
        int isThis = 0;
        int last, didPush, indir;
        
        /* Special case. */
        if (sNext == N7_KEYWORD && sKeyword == N7_THIS) {
            if (sLocalScope == 0) ErrorUnexpected();
            strcpy(name, "this");
            strcpy(baseName, "this");
            isThis = 1;
            GetNext();
        }
        /* Treat '.' as 'this', skip GetNext. */
        else if (sNext == N7_CHAR && sChar == '.') {
            if (sLocalScope == 0) ErrorUnexpected();
            strcpy(name, "this");
            strcpy(baseName, "this");
            isThis = 1;
        }
        else {
            strcpy(name, sName);
            strcpy(baseName, name);
            GetNext();
        }

        /*GetNext();*/
        
        /* Push memory to stack. */
        fprintf(sDstFile, "%s\n", ASM_MPUSH);        
        
        /* Load program memory if it's a global variable and we're in a
           subroutine. */
        /* Constant? */
        if ((ve = (VisibleEntry *)HT_Get(sConstants, name, 0))) {
            if (sLibName) {
                fprintf(sDstFile, "%s\n", ASM_LOADPM);
            }
        }
        /* Visible. */
        else if ((ve = (VisibleEntry *)HT_Get(sVisible, name, 0))) {
            if (sLocalScope > 0) {
                fprintf(sDstFile, "%s\n", ASM_LOADPM);
                if (sLibName) fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sLibName);
            }
        }
        
        /* Type of previous part, 0 = name, 1 = index, 2 = function call. */
        last = 0;
        /* Using the memory of a function result requires that we push it to the
           stack (where it's safe) and load it from there. We use this flag to
           to pop when we're done with the memory. */
        didPush = 0;
        /* Indirection level. */
        indir = 0; 
        /* Load and loop until no more indirection exists. */
        while (sNext == N7_CHAR && (sChar == '.' || sChar == '[' || sChar == '(')) {
            FunctionDefinition *fd = 0;
            /* Just two ovious errors. */
            if (indir == 0) {
                if ((fd = GetFunction(baseName))) {
                    if (sChar != '(') {
                        char tmp[64 + ASM_STRING_MAX_CHARS];
                        sprintf(tmp, "'%s' is a static function", baseName);
                        Error(tmp);
                    }
                }
                else if (!Declared(baseName)) {
                    char tmp[64 + ASM_VAR_MAX_CHARS];
                    sprintf(tmp, "Undeclared identifier '%s'", baseName);
                    Error(tmp);
                }
            }

            indir++;
            /* Load previous variable into memory. */
            if (last == 0) {
                if (fd) {
                    /* This is kind of annoying, that calling a static function
                       requires a push that lasts during the call. If it's in a
                       register it will be overwritten. Ugh ... might go through
                       all this again later. */
                    fprintf(sDstFile, "%s @0 __%d:\n", ASM_MOVE, fd->index);
                    fprintf(sDstFile, "%s @0\n", ASM_PUSH);
                    fprintf(sDstFile, "%s\n", ASM_MLOADS);
                    didPush = 1;
                }
                else {
                    fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, name);
                    if (didPush) {
                        fprintf(sDstFile, "%s @1\n", ASM_POP);
                        didPush = 0;
                    }
                }
            }
            else if (last == 1) {
                fprintf(sDstFile, "%s @0\n", ASM_MLOAD);
                if (didPush) {
                    fprintf(sDstFile, "%s @1\n", ASM_POP);
                    didPush = 0;
                }
            }
            else {
                /* Pop before pushing the new function result to the stack.
                   Popping doesn't affect what's now on an invalid stack
                   index. */
                if (didPush) {
                    fprintf(sDstFile, "%s @1\n", ASM_POP);
                    didPush = 0;
                }
                fprintf(sDstFile, "%s @0\n", ASM_PUSH);
                fprintf(sDstFile, "%s\n", ASM_MLOADS);
                didPush = 1;
            }
            
            /* Identifier? */
            if (sChar == '.') {
                GetNext();
                if (sNext != N7_NAME) Error("Expected identifier");
                strcpy(name, sName);
                GetNext();
                last = 0;
            }
            /* Index? */
            else if (sChar == '[') {
                GetNext();
                fprintf(sDstFile, "%s\n", ASM_MSWAP);
                Expression();
                fprintf(sDstFile, "%s\n", ASM_MSWAP);
                ExpectChar(']');
                last = 1;
            }
            /* Function. */
            else {
                
                
                /* Can the variable be released in CallFunction, to avoid stack
                   growing? */
                CallFunction(fd);
                last = 2;
            }
        }

        /* Free variable. */
        if (isFree) {
            if (GetFunction(baseName) && indir == 0) {
                char tmp[64 + ASM_STRING_MAX_CHARS];
                sprintf(tmp, "'%s' is a static function", baseName);
                Error(tmp);
            }
            else if (ve && ve->readOnly) {
                char tmp[64 + ASM_STRING_MAX_CHARS];
                sprintf(tmp, "'%s' is a constant", baseName);
                Error(tmp);
            }

            if (last == 0) fprintf(sDstFile, "%s .%s\n", ASM_MDEL, name);
            else if (last == 1) fprintf(sDstFile, "%s @0\n", ASM_MDEL);
            
            /* Pop. */
            fprintf(sDstFile, "%s\n", ASM_MPOP);
            if (didPush) {
                fprintf(sDstFile, "%s @1\n", ASM_POP);
                didPush = 0;
            }
        }
        /* Assignment. */
        else if (sNext == N7_CHAR && sChar == '=') {
            if (indir == 0 && isThis) Error("Invalid assignment");

            /* Can't assign something to a function call. */
            if (last == 2) Error("Invalid assignment");

            if (GetFunction(baseName) && indir == 0) {
                char tmp[64 + ASM_STRING_MAX_CHARS];
                sprintf(tmp, "'%s' is a static function", baseName);
                Error(tmp);
            }
            else if (ve && ve->readOnly) {
                char tmp[64 + ASM_STRING_MAX_CHARS];
                sprintf(tmp, "'%s' is a constant", baseName);
                Error(tmp);
            }
            
            /* Add to identifiers. */
            HT_Add(sIdentifiers, baseName, 0, 0);

            /* Add field, load, swap back. */
#ifdef OPT_MALS
            if (last == 1) fprintf(sDstFile, "%s @0\n", ASM_OPT_MALS);
            else  fprintf(sDstFile, "%s .%s\n", ASM_OPT_MALS, name);
#else            
            if (last == 1) {
                fprintf(sDstFile, "%s @0\n", ASM_MADD);
                fprintf(sDstFile, "%s @0\n", ASM_MLOAD);
            }
            else {
                fprintf(sDstFile, "%s .%s\n", ASM_MADD, name);
                fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, name);
            }
            fprintf(sDstFile, "%s\n", ASM_MSWAP);
#endif

            GetNext();
            /* Allow new lines after =. */
            EatNewLines();
            
            /* Load an expression into register 0. */
            Expression();
            /* Swap back to the variable, set it's value to whatever is
               in register 0 and pop back to old memory. */
#ifdef OPT_MSSP
            fprintf(sDstFile, "%s @0\n", ASM_OPT_MSSP);
#else
            fprintf(sDstFile, "%s\n", ASM_MSWAP);
            fprintf(sDstFile, "%s @0\n", ASM_MSET);
            fprintf(sDstFile, "%s\n", ASM_MPOP);
#endif
            if (didPush) {
                fprintf(sDstFile, "%s @1\n", ASM_POP);
                didPush = 0;
            }
        }
        /* Function call, dangling expressions are not allowed. */
        else {
            if (last != 2) Error("Syntax error");
            
            if (didPush) {
                fprintf(sDstFile, "%s @1\n", ASM_POP);
                didPush = 0;
            }
            fprintf(sDstFile, "%s\n", ASM_MPOP);
        }
    }
    /* Set before, but now for releasing system stuff. */
    else if (isFree) {
        if (sNext == N7_KEYWORD) {
            switch (sKeyword) {
                case N7_KEY:   CallSystemFunction(SYS_TBL_FREE_KEY, 2, 2, 0, 1); break;
                /* To questionable how this should work. Renv would handle the
                   call if uncommented. */
                case N7_VAL:   CallSystemFunction(SYS_TBL_FREE_VALUE, 2, 2, 0, 1); break;
                case N7_FILE:  CallSystemFunction(SYS_FREE_FILE, 1, 1, 0, 1); break;
                case N7_IMAGE: CallSystemFunction(SYS_FREE_IMAGE, 1, 1, 0, 1); break;
                case N7_FONT:  CallSystemFunction(SYS_FREE_FONT, 1, 1, 0, 1); break;
                case N7_ZONE:  CallSystemFunction(SYS_FREE_ZONE, 1, 1, 0, 1); break;
                case N7_SOUND: CallSystemFunction(SYS_FREE_SOUND, 1, 1, 0, 1); break;
                case N7_MUSIC: CallSystemFunction(SYS_FREE_MUSIC, 1, 1, 0, 1); break;
                default:
                    Error("Syntax error");
            }
        }
        else {
            Error("Syntax error");
        }
    }
    /* Keywords. */
    else if (sNext == N7_KEYWORD) {
        int argc;
        
        switch (sKeyword) {
            case N7_END:       End(); break;
            case N7_GC:        Gc(); break;
            case N7_ASSERT:    Assert(); break;
            /* Block starters, because of one-liners and optional terminators
               they manage eol themselves. */
            case N7_INCLUDE:   Include(); return 1;
            case N7_ASM:       Asm(); return 1;
            case N7_IF:        If(); return 1;
            case N7_SELECT:    Select(); return 1;
            case N7_WHILE:     While(); return 1;
            case N7_DO:        Do(); return 1;
            case N7_FOR:       For(); return 1;
            case N7_FOREACH:   Foreach(); return 1;
            case N7_FUNCTION:  Function(); return 1;
            /* Jumpers. */
            case N7_BREAK:     Break(); break;
            case N7_RETURN:    Return(); break;
            /* Declarations. */
            case N7_VISIBLE:   VisibleDeclaration(); break;
            case N7_CONSTANT:  ConstantDeclaration(); break;
            /* System commands, pretty random order. */ 
            case N7_CALL_FUNCTION: CallCFunction(0, 1); break;
            case N7_PLN:       CallSystemFunction(SYS_PLN, 0, 1, 0, 1); break;
            case N7_SYSTEM:    CallSystemFunction(SYS_SYSTEM, 1, 1, 0, 1); break;
            case N7_WAIT:      CallSystemFunction(SYS_SLEEP, 1, 1, 0, 1); break;
            case N7_FWAIT:     CallSystemFunction(SYS_FRAME_SLEEP, 1, 1, 0, 1); break;
            case N7_RANDOMIZE: CallSystemFunction(SYS_RANDOMIZE, 1, 1, 0, 1); break;
            case N7_REDRAW:    CallSystemFunction(SYS_WIN_REDRAW, 0, 0, 0, 1); break;
            case N7_CLS:       CallSystemFunction(SYS_CLS, 0, 1, 0, 1); break;
            case N7_CENTER:    CallSystemFunction(SYS_CENTER, 0, 1, 0, 1); break;
            case N7_INSERT:    CallSystemFunction(SYS_TBL_INSERT, 3, 3, 0, 1); break;
            case N7_CLEAR:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_CLIP) {
                    GetNext();
                    if (sNext == N7_KEYWORD && sKeyword == N7_RECT) {
                        CallSystemFunction(SYS_CLEAR_IMAGE_CLIP_RECT, 0, 0, 0, 1);
                    }
                    else {
                        Error("Syntax error");
                    }
                }
                else {
                    CallSystemFunction(SYS_TBL_CLEAR, 1, 1, 0, 0);
                }
                break;
            case N7_WLN:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_FILE) {
                    CallSystemFunction(SYS_FILE_WRITE_LINE, 1, 2, 0, 1);
                }
                else {
                    CallSystemFunction(SYS_WRITE_LINE, 0, 1, 0, 0);
                }
                break;
            case N7_WRITE:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_FILE) {
                    CallSystemFunction(SYS_FILE_WRITE, 2, 4, 0, 1);
                }
                else {
                    CallSystemFunction(SYS_WRITE, 1, 1, 0, 0);
                }
                break;
            case N7_SET:
                GetNext();
                if (sNext == N7_KEYWORD) {
                    switch (sKeyword) {
                        case N7_WINDOW:    CallSystemFunction(SYS_SET_WINDOW, 3, 7, 0, 1); break;
                        case N7_REDRAW:    CallSystemFunction(SYS_SET_REDRAW, 1, 1, 0, 1); break;
                        case N7_MOUSE:     CallSystemFunction(SYS_SET_MOUSE, 1, 2, 0, 1); break;
                        case N7_PIXEL:     CallSystemFunction(SYS_SET_PIXEL, 2, 2, 0, 1); break;
                        case N7_CARET:     CallSystemFunction(SYS_SET_CARET, 2, 2, 0, 1); break;
                        case N7_FONT:      CallSystemFunction(SYS_SET_FONT, 1, 1, 0, 1); break;
                        case N7_CLIPBOARD: CallSystemFunction(SYS_SET_CLIPBOARD, 1, 1, 0, 1); break;
                        case N7_CONSOLE:   CallSystemFunction(SYS_CONSOLE, 1, 1, 0, 1); break;
                        case N7_COLOR:
                            if (CallSystemFunction(SYS_SET_COLOR, 1, 4, 0, 1) == 2) Error(0);
                            break;
                        case N7_COLORI:    CallSystemFunction(SYS_SET_COLOR_INT, 1, 1, 0, 1); break;
                        case N7_ADDITIVE:  CallSystemFunction(SYS_SET_ADDITIVE, 1, 1, 0, 1); break;
                        case N7_CLIP:
                            GetNext();
                            if (sNext == N7_KEYWORD && sKeyword == N7_RECT) {
                                CallSystemFunction(SYS_SET_IMAGE_CLIP_RECT, 4, 4, 0, 1);
                            }
                            else {
                                Error("Syntax error");
                            }
                            break;
                        case N7_IMAGE:
                            GetNext();
                            if (sNext == N7_KEYWORD && sKeyword == N7_COLORKEY) {
                                argc = CallSystemFunction(SYS_SET_IMAGE_COLOR_KEY, 4, 4, 0, 1);
                            }
                            else if (sNext == N7_KEYWORD && sKeyword == N7_GRID) {
                                CallSystemFunction(SYS_SET_IMAGE_GRID, 3, 3, 0, 1);
                            }
                            else {
                                CallSystemFunction(SYS_SET_IMAGE, 1, 2, 0, 0);
                            }
                            break;
                        case N7_JUSTIFICATION:
                            GetNext();
                            if (sNext == N7_KEYWORD && sKeyword == N7_LEFT) {
                                GetNext();
                                fprintf(sDstFile, "%s @0 1\n", ASM_MOVE);
                                fprintf(sDstFile, "%s @0\n", ASM_NEG);
                            }
                            else if (sNext == N7_KEYWORD && sKeyword == N7_RIGHT) {
                                GetNext();
                                fprintf(sDstFile, "%s @0 1\n", ASM_MOVE);
                            }
                            else if (sNext == N7_KEYWORD && sKeyword == N7_CENTER) {
                                GetNext();
                                fprintf(sDstFile, "%s @0 0\n", ASM_MOVE);
                            }
                            else {
                                Expression();
                            }
                            fprintf(sDstFile, "%s @0\n", ASM_PUSH);
                            fprintf(sDstFile, "%s %d 1\n", ASM_SYS, SYS_SET_JUSTIFICATION);
                            break;
                        case N7_MUSIC:
                            GetNext();
                            if (sNext == N7_KEYWORD && sKeyword == N7_VOLUME) {
                                CallSystemFunction(SYS_SET_MUSIC_VOLUME, 2, 2, 0, 1);
                            }
                            else {
                                Error(0);
                            }
                            break;
                        default:
                            Error("Syntax error");
                    }
                }
                else {
                    Error("Syntax error");
                }
                break;
            case N7_LOAD:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_IMAGE) {
                    argc = CallSystemFunction(SYS_LOAD_IMAGE_LEGACY, 2, 4, 0, 1);
                    if (!(argc == 2 || argc == 4)) ExpectChar(',');
                }
                else if (sNext == N7_KEYWORD && sKeyword == N7_FONT) {
                    argc = CallSystemFunction(SYS_LOAD_FONT_LEGACY, 2, 2, 0, 1);
                }
                else if (sNext == N7_KEYWORD && sKeyword == N7_SOUND) {
                    CallSystemFunction(SYS_LOAD_SOUND_LEGACY, 2, 2, 0, 1);
                }
                else if (sNext == N7_KEYWORD && sKeyword == N7_MUSIC) {
                    CallSystemFunction(SYS_LOAD_MUSIC_LEGACY, 2, 2, 0, 1);
                }
                else {
                    Error(0);
                }
                break;
            case N7_SAVE:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_IMAGE) {
                    argc = CallSystemFunction(SYS_SAVE_IMAGE, 2, 2, 0, 1);
                }
                else if (sNext == N7_KEYWORD && sKeyword == N7_FONT) {
                    argc = CallSystemFunction(SYS_SAVE_FONT, 2, 2, 0, 1);
                }
                else {
                    Error(0);
                }
                break;
            case N7_CREATE:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_FILE) {
                    CallSystemFunction(SYS_CREATE_FILE_LEGACY, 2, 3, 0, 1);
                }
                else if (sNext == N7_KEYWORD && sKeyword == N7_IMAGE) {
                    CallSystemFunction(SYS_CREATE_IMAGE_LEGACY, 3, 3, 0, 1);
                }
                else if (sNext == N7_KEYWORD && sKeyword == N7_SOUND) {
                    CallSystemFunction(SYS_CREATE_SOUND_LEGACY, 4, 4, 0, 1);
                }
                else if (sNext == N7_KEYWORD && sKeyword == N7_FONT) {
                    CallSystemFunction(SYS_CREATE_FONT_LEGACY, 3, 7, 0, 1);
                }
                else if (sNext == N7_KEYWORD && sKeyword == N7_ZONE) {
                    CallSystemFunction(SYS_CREATE_ZONE_LEGACY, 5, 5, 0, 1);
                }
                else {
                    Error("Syntax error");
                }
                break;
            case N7_OPEN:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_FILE) {
                    CallSystemFunction(SYS_OPEN_FILE_LEGACY, 2, 3, 0, 1);
                }
                else {
                    Error("Syntax error");
                }
                break;
            case N7_DRAW:
                GetNext();
                if (sNext == N7_KEYWORD) {
                    switch (sKeyword) {
                        case N7_PIXEL:   CallSystemFunction(SYS_DRAW_PIXEL, 2, 2, 0, 1); break;
                        case N7_LINE:
                            GetNext();
                            if (sNext == N7_KEYWORD && sKeyword == N7_TO) {
                                CallSystemFunction(SYS_DRAW_LINE, 2, 2, 0, 1);
                            }
                            else {
                                argc = CallSystemFunction(SYS_DRAW_LINE, 2, 4, 0, 0);
                                if (!(argc == 2 || argc == 4)) ExpectChar(',');
                            }
                            break;
                        case N7_RECT:    CallSystemFunction(SYS_DRAW_RECT, 4, 5, 0, 1); break;
                        case N7_ELLIPSE: CallSystemFunction(SYS_DRAW_ELLIPSE, 4, 5, 0, 1); break;
                        case N7_POLY:
                            GetNext();
                            if (sNext == N7_KEYWORD && sKeyword == N7_IMAGE) {
                                GetNext();
                                if (sNext == N7_KEYWORD && sKeyword == N7_TRANSFORMED) {
                                    CallSystemFunction(SYS_DRAW_POLYGON_IMAGE_TRANSFORMED, 9, 11, 0, 1);
                                }
                                else {
                                    CallSystemFunction(SYS_DRAW_POLYGON_IMAGE, 2, 4, 0, 0);
                                }
                            }
                            else if (sNext == N7_KEYWORD && sKeyword == N7_TRANSFORMED) {
                                CallSystemFunction(SYS_DRAW_POLYGON_TRANSFORMED, 8, 10, 0, 1);
                            }
                            else {
                                CallSystemFunction(SYS_DRAW_POLYGON, 1, 3, 0, 0);
                            }
                            break;
                        case N7_VRASTER: CallSystemFunction(SYS_DRAW_VRASTER, 8, 8, 0, 1); break;
                        case N7_HRASTER: CallSystemFunction(SYS_DRAW_HRASTER, 8, 8, 0, 1); break;
                        case N7_IMAGE:
                            GetNext();
                            if (sNext == N7_KEYWORD && sKeyword == N7_TRANSFORMED) {
                                argc = CallSystemFunction(SYS_DRAW_IMAGE_TRANSFORMED, 8, 12, 0, 1);
                                if (!(argc == 8 || argc == 9 || argc == 12)) ExpectChar(',');
                            }
                            else {
                                argc = CallSystemFunction(SYS_DRAW_IMAGE, 3, 7, 0, 0);
                                if (!(argc == 3 || argc == 4 || argc == 7)) ExpectChar(',');
                            }
                            break;
                        default:
                            Error("Syntax error");
                    }
                }
                else {
                    Error("Syntax error");
                }
                break;
            case N7_SCROLL:
                CallSystemFunction(SYS_SCROLL, 2, 2, 0, 1);
                break;
            case N7_PLAY:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_SOUND) CallSystemFunction(SYS_PLAY_SOUND, 1, 3, 0, 1);
                else if (sNext == N7_KEYWORD && sKeyword == N7_MUSIC) CallSystemFunction(SYS_PLAY_MUSIC, 1, 2, 0, 1);
                else Error("Syntax error");
                break;
            case N7_STOP:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_MUSIC) CallSystemFunction(SYS_STOP_MUSIC, 1, 1, 0, 1);
                else Error(0);
                break;
            case N7_FILE:
                GetNext();
                if (sNext == N7_KEYWORD && sKeyword == N7_SEEK) CallSystemFunction(SYS_FILE_SEEK, 2, 3, 0, 1);
                else Error(0);
                break;
            default:
                Error("Syntax error");
        }
    }
    else {
        Error("Syntax error");
    }

    ExpectNewLine();
    
    return 1;
}


/*
 * Block
 * -----
 * Perform instructions until a block ending keyword or end of file is
 * reached.
 */
static void Block() {
    while (Statement());
}

/*
 * IncBlockLevel
 * -------------
 */
static void IncBlockLevel(BlockType type) {
    sBlockInfoStack[sBlockLevel].type = type;
    sBlockInfoStack[sBlockLevel].localScope = sLocalScope;
    sBlockLevel++;
}

/*
 * DecBlockLevel
 * -------------
 */
static void DecBlockLevel() {
    sBlockLevel--;
}

/*
 * PushBreak
 * ---------
 * Push a break label index to stack.
 */
static void PushBreak(int labelIndex) {
    sBreakStack[sBreakCount++] = labelIndex;
}

/*
 * PopBreak
 * --------
 * Pop break label index from stack.
 */
static void PopBreak() {
    sBreakCount--;
}

/*
 * AddBreakLabel
 * -------------
 * Add a break label, "break_<label index>:".
 */
static void AddBreakLabel(int labelIndex) {
    fprintf(sDstFile, "break_%d:\n", labelIndex);
}

/*
 * Break
 * -----
 * Jump to break label on top of stack.
 */
static void Break() {
    GetNext();
    if (sBreakCount <= 0) Error("Unexpected 'break'");
    fprintf(sDstFile, "%s break_%d:\n", ASM_JMP, sBreakStack[sBreakCount - 1]);
}

/*
 * End
 * ---
 * end
 */
static void End() {
    GetNext();
    fprintf(sDstFile, "%s\n", ASM_END);
}

/*
 * Gc
 * --
 * gc
 */
static void Gc() {
    GetNext();
    fprintf(sDstFile, "%s\n", ASM_GC);
}

/*
 * Assert
 * ------
 * assert <expr>[, <msg>]
 */
static void Assert() {
    GetNext();
    Expression();
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);
    if (sNext == N7_CHAR && sChar == ',') {
        GetNext();
        Expression();
    }
    else {
        fprintf(sDstFile, "%s @0 \"Assertion failed\"\n", ASM_MOVE);
    }
    fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
    fprintf(sDstFile, "%s @0 @1\n", ASM_ASSERT);
}

/*
 * Include
 * -------
 * include (string)filename
 *
 * Include works like 'import' in n6. 'import' may be needed for another type of
 * libraries later on.
 */
static void Include() {
    SourceEnv *sourceEnv;
    char *libName;
    FILE *file;
    char *fnLibFull;
    size_t maxLen = strlen(sUserLibPath) > strlen(sLibPath) ? strlen(sUserLibPath) : strlen(sLibPath); 
    
    GetNext();
    
    if (sBlockLevel > 0) Error("Include can't be conditional");
    if (sNext != N7_STRING) Error("Expected string");
    
    fnLibFull = (char *)malloc(maxLen + strlen(sString) + 1);    
    
    strcpy(fnLibFull, sUserLibPath);
    strcat(fnLibFull, sString);
    if (!(file = fopen(fnLibFull, "r"))) {
        strcpy(fnLibFull, sLibPath);
        strcat(fnLibFull, sString);
        if (!(file = fopen(fnLibFull, "r"))) {
            char tmp[ASM_STRING_MAX_CHARS + 35];
            sprintf(tmp, "Could not open file '%s' for reading", sString);
            Error(tmp);
        }
    }
    
    strcpy(fnLibFull, sString);
    
    /* 23-10-18 */
    /*int alreadyIncluded = 0;
    if (strcmp(sMainSrcFilename, fnLibFull)) {
        sourceEnv = sSourceEnvStack;
        while (sourceEnv) {
            if (!strcmp(sourceEnv->filename, fnLibFull)) {
                alreadyIncluded = 1;
                break;
            }
            sourceEnv = sourceEnv->next;
        }
    }
    else {
        alreadyIncluded = 1;
    }*/
    int alreadyIncluded = !AddIncludeInfo(fnLibFull);
    
    if (alreadyIncluded) {
        free(fnLibFull);
        fclose(file);
        GetNext();
        ExpectNewLine();
        return;
    }
    
    
    /* Create library table name. */
    libName = (char *)malloc(sizeof(char)*(strlen(fnLibFull) + 3));
    libName[0] = '_';
    libName[1] = '\0';
    strcat(libName, fnLibFull);
    for (size_t i = 1; i < strlen(libName); i++) {
        if (!(isalpha(libName[i]) || isdigit(libName[i]))) libName[i] = '_';
    }
    
    /* Push current file info to stack. */
    sourceEnv = (SourceEnv *)malloc(sizeof(SourceEnv));
    sourceEnv->file = sSrcFile;
    sourceEnv->filename = sSrcFilename;
    sourceEnv->libName = sLibName;
    sourceEnv->lineNumber = sLineNumber;
    sourceEnv->visible = sVisible;
    sourceEnv->next = sSourceEnvStack;
    sSourceEnvStack = sourceEnv;
   
    /* Set up new file. */
    sSrcFile = file;
    sSrcFilename = fnLibFull;
    sLibName = libName;
    sLineNumber = 1;
    sVisible = HT_Create(1);
    
    sIdentifierStack[sIdentifierStackIndex++] = sIdentifiers;
    sIdentifiers = HT_Create(1);
    
    GetNext();
    
    /* Create and load library memory (part of program memory). */
    fprintf(sDstFile, "%s\n", ASM_MPUSH);
    fprintf(sDstFile, "%s\n", ASM_LOADPM);
    fprintf(sDstFile, "%s .%s\n", ASM_MADD, libName);
    fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, libName);
    fprintf(sDstFile, "%s @0\n", ASM_CTBL);
    fprintf(sDstFile, "%s @0\n", ASM_MSET);

    /* Metadata. */
    fprintf(sDstFile, "/file:%s\n", sSrcFilename);
    fprintf(sDstFile, "/line:%d\n", sLineNumber);
    
    /* Compile code. */
    Block();
    if (sNext != N7_EOF) ErrorUnexpected();
    
    /* Restore memory. */
    fprintf(sDstFile, "%s\n", ASM_MPOP);
    
    /* Free. */
    HT_Free(sIdentifiers, 0);
    sIdentifiers = sIdentifierStack[--sIdentifierStackIndex];
    fclose(sSrcFile);
    free(sSrcFilename);
    free(sLibName);
    DeleteVisible();
    
    /* Restore file. */
    sSrcFile = sSourceEnvStack->file;
    sSrcFilename = sSourceEnvStack->filename;
    sLibName = sSourceEnvStack->libName;
    sLineNumber = sSourceEnvStack->lineNumber;
    sVisible = sSourceEnvStack->visible;
    
    sSourceEnvStack = sSourceEnvStack->next;
    free(sourceEnv);
    
    fprintf(sDstFile, "/file:%s\n", sSrcFilename);
    fprintf(sDstFile, "/line:%d\n", sLineNumber);

    GetNext();
    ExpectNewLine();
}

/*
 * Asm
 * ---
 * Basicly paste everything found into asm.
 */
static void Asm() {
    sAsm = 1;
    GetNext();
    while (sNext == N7_STRING) {
        fprintf(sDstFile, "%s\n", sString);
        GetNext();
    }
    if (sNext != N7_KEYWORD && sKeyword == N7_ENDASM) {
        Error("Expected 'endasm'");
    }
    sAsm = 0;
    GetNext();
}

/*
 * If
 * --
 * An if <expr> {then], elseif <expr> [then] and else can be followed by a
 * single statement and a new line, or a new line and a block. If the last if,
 * elseif or else ended with a single statement, endif is not expected.
 */
static void If() {
    int endifIndex = sLabelIndex++;
    int lastWasBlock = 0;

    GetNext();
    IncBlockLevel(BLOCK_TYPE_IF);
    
    /* This loop got a bit dirty, I'll rewrite it ... later. */
    while (1) {
        /* if or elseif. */
        int nextIndex = sLabelIndex++;
        Expression();
        /*fprintf(sDstFile, "%s @0\n", ASM_EVAL);
        fprintf(sDstFile, "%s if_%d:\n", ASM_JMPF, nextIndex);*/
        fprintf(sDstFile, "%s @0 if_%d:\n", ASM_JMPEF, nextIndex);
        /* Uhm, then is allowed but not required. */
        if (sNext == N7_KEYWORD && sKeyword == N7_THEN) GetNext(); 
        if (sNext == N7_EOL) {
            Block();
            lastWasBlock = 1;
        }
        else {
            if (!Statement()) Error("Syntax error");
            lastWasBlock = 0;
        }
        fprintf(sDstFile, "%s endif_%d:\n", ASM_JMP, endifIndex);        
        fprintf(sDstFile, "if_%d:\n", nextIndex);
        if (sNext == N7_KEYWORD && sKeyword == N7_ELSEIF) {
            GetNext();
            continue;
        }
        /* else */
        if (sNext == N7_KEYWORD && sKeyword == N7_ELSE) {
            GetNext();
            if (sNext == N7_EOL) {
                Block();
                lastWasBlock = 1;
            }
            else {
                if (!Statement()) Error("Syntax error");
                lastWasBlock = 0;
            }
        }
        /* Expect endif only if previous if/elseif/else was a block. */
        if (lastWasBlock) {
            if (sNext == N7_KEYWORD && sKeyword == N7_ENDIF) {
                GetNext();
                ExpectNewLine();
                break;
            }
            else {
                Error("Expected 'endif'");
            }
        }
        else break;       
    };
    fprintf(sDstFile, "endif_%d:\n", endifIndex);
    
    DecBlockLevel();
}

/*
 * Select
 * ------
 * select [case] <expr>
 *   case <expr>[,<expr> ..]
 *     <block>
 *  [case <expr>[,<expr> ..]
 *     <block>
 *   ..]
 *  [default
 *     <block>]
 * endsel 
 *
 * If any of the expressions on a case line matches the selected expression, the
 * next block is executed, whereafter a jump is made to endsel. If no case
 * matches, the optional default block can be executed. Any block can be
 * replaced with a single statement on the same line as case/default.
 */
static void Select() {
    int selectIndex = sLabelIndex++;
    int caseIndex = 0;
    
    GetNext();
    IncBlockLevel(BLOCK_TYPE_SELECT);
    
    /* Optional. */
    if (sNext == N7_KEYWORD && sKeyword == N7_CASE) GetNext();
    Expression();
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);
    ExpectNewLine();
    while (!(sNext == N7_KEYWORD && sKeyword == N7_ENDSELECT)) {
        if (sNext == N7_KEYWORD && sKeyword == N7_CASE) {
            GetNext();
            do {
                Expression();
                fprintf(sDstFile, "%s @1\n", ASM_POP);
                fprintf(sDstFile, "%s @1 @0\n", ASM_ECMP);
                fprintf(sDstFile, "%s @1\n", ASM_PUSH);
                fprintf(sDstFile, "%s sel_%d_case_%d:\n", ASM_JMPT, selectIndex, caseIndex);
            } while (CouldGetChar(','));
            fprintf(sDstFile, "%s sel_%d_case_end_%d:\n", ASM_JMP, selectIndex, caseIndex);
            fprintf(sDstFile, "sel_%d_case_%d:\n", selectIndex, caseIndex);
            
            /* Block or single statement. */
            if (sNext == N7_EOL) Block();
            else if (!Statement()) Error("Syntax error");
      
            fprintf(sDstFile, "%s sel_%d_end:\n", ASM_JMP, selectIndex);
            fprintf(sDstFile, "sel_%d_case_end_%d:\n", selectIndex, caseIndex);
            caseIndex++;
        }
        else if (sNext == N7_KEYWORD && sKeyword == N7_DEFAULT) {
            GetNext();
            /* Block or single statement. */
            if (sNext == N7_EOL) Block();
            else if (!Statement()) Error("Syntax error");
            
            if (!(sNext == N7_KEYWORD && sKeyword == N7_ENDSELECT)) Error("Expected 'endsel'");
        }
        else {
            ErrorUnexpected();
        }
    }
    GetNext();
    ExpectNewLine();
    fprintf(sDstFile, "sel_%d_end:\n", selectIndex);
    fprintf(sDstFile, "%s @0\n", ASM_POP);
    
    DecBlockLevel();
}

/*
 * While
 * -----
 * while <expr> [then] can be followed by a single statement and a new line,
 * or a new line and a block. wend is only expected after a block.
 */
static void While() {
    int whileLabelIndex = sLabelIndex++;
    
    GetNext();
    IncBlockLevel(BLOCK_TYPE_WHILE);
    
    fprintf(sDstFile, "while_%d:\n", whileLabelIndex);
    Expression();
    /*fprintf(sDstFile, "%s @0\n", ASM_EVAL);
    fprintf(sDstFile, "%s while_%d_wend:\n", ASM_JMPF, whileLabelIndex);*/
    fprintf(sDstFile, "%s @0 while_%d_wend:\n", ASM_JMPEF, whileLabelIndex);
    /* Allow then, since it makes single statements easier to read. */
    if (sNext == N7_KEYWORD && sKeyword == N7_THEN) GetNext();
    /* Block. */
    PushBreak(whileLabelIndex);
    if (sNext == N7_EOL) {
        Block();
        if (sNext == N7_KEYWORD && sKeyword == N7_WEND) {
            GetNext();
            ExpectNewLine();
            fprintf(sDstFile, "%s while_%d:\n", ASM_JMP, whileLabelIndex);
        }
        else {
            Error("Expected 'wend'");
        }
    }
    /* Single statement. */
    else {
        if (!Statement()) Error("Syntax error");
        fprintf(sDstFile, "%s while_%d:\n", ASM_JMP, whileLabelIndex);
    }
    PopBreak();
    fprintf(sDstFile, "while_%d_wend:\n", whileLabelIndex);
    AddBreakLabel(whileLabelIndex);
    
    DecBlockLevel();
}

/*
 * Do
 * --
 * Do is always followed by a block that ends with loop or until <expr>.
 */
static void Do() {
    int doLabelIndex = sLabelIndex++;
    
    GetNext();
    IncBlockLevel(BLOCK_TYPE_DO);
    
    ExpectNewLine();
    fprintf(sDstFile, "do_%d:\n", doLabelIndex);
    PushBreak(doLabelIndex);
    Block();
    PopBreak();
    if (sNext == N7_KEYWORD && sKeyword == N7_LOOP) {
        GetNext();
        ExpectNewLine();
        fprintf(sDstFile, "%s do_%d:\n", ASM_JMP, doLabelIndex);
    }
    else if (sNext == N7_KEYWORD && sKeyword == N7_UNTIL) {
        GetNext();
        Expression();
        ExpectNewLine();
        /*fprintf(sDstFile, "%s @0\n", ASM_EVAL);
        fprintf(sDstFile, "%s do_%d:\n", ASM_JMPF, doLabelIndex);*/
        fprintf(sDstFile, "%s @0 do_%d:\n", ASM_JMPEF, doLabelIndex);
    }
    AddBreakLabel(doLabelIndex);
    
    DecBlockLevel();
}

/*
 * For
 * ---
 * for <id> = <expr> to <expr> [step <expr>]  Just as with while loops, a for
 * loop can be followed by a statement or a new line and a block, where next
 * marks the end of the block. The sign of the step value is always set based
 * on the start end destination expressions:
 *   for i = 6 to 2
 * , would set the step size to -1, and:
 *   for i = 2 to 6 step -2
 * , would set the step size to 2.
 */
static void For() {
    int forLabelIndex = sLabelIndex++;
    VisibleEntry *ve;
    
    GetNext();
    IncBlockLevel(BLOCK_TYPE_FOR);

    /* Identifier, currently only a simple identifier is allowed. */
    if (!(sNext == N7_NAME)) Error("Expected identifier");
    
    if (!(ve = (VisibleEntry *)HT_Get(sConstants, sName, 0))) ve = (VisibleEntry *)HT_Get(sVisible, sName, 0);
  
    if (GetFunction(sName)) {
        char tmp[64 + ASM_STRING_MAX_CHARS];
        sprintf(tmp, "'%s' is a static function", sName);
        Error(tmp);
    }
    if (ve && ve->readOnly) {
        char tmp[64 + ASM_STRING_MAX_CHARS];
        sprintf(tmp, "'%s' is a constant", sName);
        Error(tmp);
    }
    
    if (!ve) fprintf(sDstFile, "%s .%s\n", ASM_MADD, sName);
    
    /* fprintf(sDstFile, "%s .%s\n", ASM_MADD, sName); */
    fprintf(sDstFile, "%s\n", ASM_MPUSH);
    if (ve && sLocalScope > 0) {
        fprintf(sDstFile, "%s\n", ASM_LOADPM);
        if (sLibName) fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sLibName);
    }
    else {
        HT_Add(sIdentifiers, sName, 0, 0);
    }

    fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sName);
    fprintf(sDstFile, "%s\n", ASM_MSWAP);
    GetNext();
    
    /* = */
    ExpectChar('=');
   
    /* Start value. */
    Expression();
    fprintf(sDstFile, "%s\n", ASM_MSWAP);
    fprintf(sDstFile, "%s @0 @0\n", ASM_TONUM);
    fprintf(sDstFile, "%s @0\n", ASM_MSET);
    fprintf(sDstFile, "%s\n", ASM_MSWAP);
    
    /* to */
    if (!(sNext == N7_KEYWORD && sKeyword == N7_TO)) Error("Expected 'to'");
    GetNext();
    
    /* Destination value to stack. */
    Expression();
    fprintf(sDstFile, "%s @0 @0\n", ASM_TONUM);
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);
    
    /* Absolute step value to stack. */
    if (sNext == N7_KEYWORD && sKeyword == N7_STEP) {
        GetNext();
        Expression();
        fprintf(sDstFile, "%s @0\n", ASM_ABS);
    }
    else {
        fprintf(sDstFile, "%s @0 1\n", ASM_MOVE);
    }

    /* Make step negative if destination < start */
    fprintf(sDstFile, "%s\n", ASM_MSWAP);
    fprintf(sDstFile, "%s @1\n", ASM_MGET);
    fprintf(sDstFile, "%s\n", ASM_MSWAP);
    fprintf(sDstFile, "%s @2\n", ASM_POP);
    fprintf(sDstFile, "%s @2\n", ASM_PUSH);   
    fprintf(sDstFile, "%s @1 @2\n", ASM_LEQL);
    /*fprintf(sDstFile, "%s @1\n", ASM_EVAL);
    fprintf(sDstFile, "%s for_%d_step_not_neg:\n", ASM_JMPT, forLabelIndex);*/
    fprintf(sDstFile, "%s @1 for_%d_step_not_neg:\n", ASM_JMPET, forLabelIndex);
    fprintf(sDstFile, "%s @0\n", ASM_NEG);
    fprintf(sDstFile, "for_%d_step_not_neg:\n", forLabelIndex);
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);
    
    /* Block. */
    fprintf(sDstFile, "for_%d_body:\n", forLabelIndex);
    PushBreak(forLabelIndex);
    if (sNext == N7_EOL) {
        Block();
        /* Next. */
        if (!(sNext == N7_KEYWORD && sKeyword == N7_NEXT)) Error("Expected 'next'");
        GetNext();
        ExpectNewLine();
    }
    else {
        if (!Statement()) Error("Syntax error");  
    }
    PopBreak();
    
#ifdef OPT_FOR_STEP
#else
    /* Add step to identifier. */
    fprintf(sDstFile, "%s\n", ASM_MSWAP); 
    fprintf(sDstFile, "%s @0\n", ASM_MGET);
    fprintf(sDstFile, "%s @1\n", ASM_POP); /* Step value. */
    fprintf(sDstFile, "%s @2\n", ASM_POP); /* Destination value. */
    fprintf(sDstFile, "%s @2\n", ASM_PUSH);
    fprintf(sDstFile, "%s @1\n", ASM_PUSH);
    fprintf(sDstFile, "%s @0 @1\n", ASM_ADD); /* Add step value. */
    fprintf(sDstFile, "%s @0\n", ASM_MSET); /* Update identifier. */
    fprintf(sDstFile, "%s\n", ASM_MSWAP);

    /* Do different comparisons depending on the step sign. */
    fprintf(sDstFile, "%s @3 0\n", ASM_MOVE);
    fprintf(sDstFile, "%s @1 @3\n", ASM_LESS);
    fprintf(sDstFile, "%s @1 for_%d_neg_step:\n", ASM_JMPET, forLabelIndex);
    fprintf(sDstFile, "%s @0 @2\n", ASM_LEQL);
    fprintf(sDstFile, "%s for_%d_neg_step_end:\n", ASM_JMP, forLabelIndex);
    fprintf(sDstFile, "for_%d_neg_step:\n", forLabelIndex);
    fprintf(sDstFile, "%s @0 @2\n", ASM_GEQL);
    fprintf(sDstFile, "for_%d_neg_step_end:\n", forLabelIndex);  
#endif
    /* Done? */
    fprintf(sDstFile, "%s @0 for_%d_body:\n", ASM_JMPET, forLabelIndex);
    
    /* Break label. */
    AddBreakLabel(forLabelIndex);
    
    /* Pop step and destination values. */
    fprintf(sDstFile, "%s @0\n", ASM_POP);
    fprintf(sDstFile, "%s @0\n", ASM_POP);
    /* Put primary memory on stack, then pop it. */
    fprintf(sDstFile, "%s\n", ASM_MSWAP);
    fprintf(sDstFile, "%s\n", ASM_MPOP);
    
    DecBlockLevel();
}

/*
 * Foreach
 * -------
 */
static void Foreach() {
    int foreachLabelIndex = sLabelIndex++;
    char valueName[ASM_VAR_MAX_CHARS];
    char keyName[ASM_VAR_MAX_CHARS];
    VisibleEntry *veValue;
    VisibleEntry *veKey = 0;
    int withKey = 0;
    
    GetNext();
    IncBlockLevel(BLOCK_TYPE_FOREACH);
    
    if (sNext != N7_NAME) Error("Expected identifier");
    
    if (GetFunction(sName)) {
        char tmp[64 + ASM_STRING_MAX_CHARS];
        sprintf(tmp, "'%s' is a static function", sName);
        Error(tmp);
    }
    
    if (!(veValue = (VisibleEntry *)HT_Get(sConstants, sName, 0))) veValue = (VisibleEntry *)HT_Get(sVisible, sName, 0);    
    if (veValue && veValue->readOnly) {
        char tmp[64 + ASM_STRING_MAX_CHARS];
        sprintf(tmp, "'%s' is a constant", sName);
        Error(tmp);        
    }
    strcpy(valueName, sName);
    
    if (!veValue) fprintf(sDstFile, "%s .%s\n", ASM_MADD, sName);
    HT_Add(sIdentifiers, sName, 0, 0);
    GetNext();
    
    if (sNext == N7_CHAR && sChar == ',') {
        GetNext();
        EatNewLines();
        withKey = 1;
        strcpy(keyName, valueName);
        veKey = veValue;
        if (sNext != N7_NAME) Error("Expected identifier");
        if (strcmp(sName, keyName) == 0) Error("Key and value can't share identifier");

        if (GetFunction(sName)) {
            char tmp[64 + ASM_STRING_MAX_CHARS];
            sprintf(tmp, "'%s' is a static function", sName);
            Error(tmp);
        }

        if (!(veValue = (VisibleEntry *)HT_Get(sConstants, sName, 0))) veValue = (VisibleEntry *)HT_Get(sVisible, sName, 0);
        if (veValue && veValue->readOnly) {
            char tmp[64 + ASM_STRING_MAX_CHARS];
            sprintf(tmp, "'%s' is a constant", sName);
            Error(tmp);        
        }
        strcpy(valueName, sName);

        if (!veValue) fprintf(sDstFile, "%s .%s\n", ASM_MADD, sName);
        HT_Add(sIdentifiers, sName, 0, 0);
        GetNext();
    }
    
    if (!(sNext == N7_KEYWORD && sKeyword == N7_IN)) Error("Expected 'in'");
    GetNext();
    
    Expression();
    /* You should add instructions here to check if @0 is a table, and if not
       output an error, or let the runtime do it all. Problem is that MLOAD @0
       assumes that it should load the key/index if register is a string or
       number. Here we assume that the expression gives a table. */
    fprintf(sDstFile, "%s @1 @0\n", ASM_TYPE);
    fprintf(sDstFile, "%s @2 %d\n", ASM_MOVE, VAR_TBL);
    fprintf(sDstFile, "%s @1 @2\n", ASM_EQL);
    /* HACK! See BC_ILOAD in renv, the eval flag set here is used for setting
       the wrapper flag in the iterator! */
    /*fprintf(sDstFile, "%s @1\n", ASM_EVAL);
    fprintf(sDstFile, "%s foreach_%d_table:\n", ASM_JMPT, foreachLabelIndex);*/
    fprintf(sDstFile, "%s @1 foreach_%d_table:\n", ASM_JMPET, foreachLabelIndex);
    /* Value other than table, might be good, idk, wrap it up in a table. */
    fprintf(sDstFile, "%s @1\n", ASM_CTBL);
    fprintf(sDstFile, "%s\n", ASM_MPUSH);
    fprintf(sDstFile, "%s @1\n", ASM_MLOAD);
    fprintf(sDstFile, "%s .tbl_wrapper\n", ASM_MADD);
    fprintf(sDstFile, "%s .tbl_wrapper\n", ASM_MLOAD);
    fprintf(sDstFile, "%s @0\n", ASM_MSET);
    fprintf(sDstFile, "%s\n", ASM_MPOP);
    fprintf(sDstFile, "%s @0 @1\n", ASM_MOVE);
    fprintf(sDstFile, "%s @1\n", ASM_CLR);
    /* Table version, the real one. */
    fprintf(sDstFile, "foreach_%d_table:\n", foreachLabelIndex);
    fprintf(sDstFile, "%s\n", ASM_MPUSH);
    fprintf(sDstFile, "%s @0\n", ASM_MLOAD);
    fprintf(sDstFile, "%s\n", ASM_ILOAD);
    fprintf(sDstFile, "%s\n", ASM_MPOP);
    
    /* Loop start. */
    fprintf(sDstFile, "foreach_%d:\n", foreachLabelIndex);
    fprintf(sDstFile, "%s\n", ASM_IHAS);
    fprintf(sDstFile, "%s foreach_%d_end:\n", ASM_JMPF, foreachLabelIndex);
    
    /* Load variable from iterator. */
    fprintf(sDstFile, "%s @0\n", ASM_IVAL);
    fprintf(sDstFile, "%s\n", ASM_MPUSH);
    if (veValue && sLocalScope > 0) {
        fprintf(sDstFile, "%s\n", ASM_LOADPM);
        if (sLibName) fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sLibName);
    }
    fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, valueName);
    fprintf(sDstFile, "%s @0\n", ASM_MSET);
    fprintf(sDstFile, "%s\n", ASM_MPOP);
    /* Load key. */
    if (withKey) {
        fprintf(sDstFile, "%s @0\n", ASM_IKEY);
        fprintf(sDstFile, "%s\n", ASM_MPUSH);
        if (veKey && sLocalScope > 0) {
            fprintf(sDstFile, "%s\n", ASM_LOADPM);
            if (sLibName) fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sLibName);
        }
        fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, keyName);
        fprintf(sDstFile, "%s @0\n", ASM_MSET);
        fprintf(sDstFile, "%s\n", ASM_MPOP);
    }
    
    fprintf(sDstFile, "%s\n", ASM_IPUSH);
    PushBreak(foreachLabelIndex);
    if (sNext == N7_EOL) {
        Block();
        if (!(sNext == N7_KEYWORD && sKeyword == N7_NEXT)) Error("Expected 'next'");
        GetNext();
        ExpectNewLine();
    }
    else {
        if (!Statement()) Error("Syntax error");
    }
    PopBreak();
    fprintf(sDstFile, "%s\n", ASM_IPOP);
    fprintf(sDstFile, "%s\n", ASM_ISTEP);
    fprintf(sDstFile, "%s foreach_%d:\n", ASM_JMP, foreachLabelIndex);
    
    AddBreakLabel(foreachLabelIndex);
    fprintf(sDstFile, "%s\n", ASM_IPOP);
    
    fprintf(sDstFile, "foreach_%d_end:\n", foreachLabelIndex);
    fprintf(sDstFile, "%s\n", ASM_IDEL);
    
    DecBlockLevel();
}

/*
 * VisibleDeclaration
 * ------------------
 */
static void VisibleDeclaration() {
    GetNext();

    if (sLocalScope > 0) Error("Visible declarations can't be local");
    if (sBlockLevel > 0) Error("Visible declarations can't be conditional");

    if (!(sNext == N7_EOL || sNext == N7_EOF)) {
        do {
            if (sNext == N7_NAME) {
                if (GetFunction(sName)) {
                    char tmp[64 + ASM_VAR_MAX_CHARS];
                    sprintf(tmp, "'%s' is a static function", sName);
                    Error(tmp);
                }
                /*VisibleEntry *ve = HT_Get(sVisible, sName, 0);*/
                VisibleEntry *ve;
                if (!(ve = (VisibleEntry *)HT_Get(sConstants, sName, 0))) ve = (VisibleEntry *)HT_Get(sVisible, sName, 0);
                if (ve) {
                    char tmp[64 + ASM_VAR_MAX_CHARS];
                    if (ve->readOnly) sprintf(tmp, "'%s' has already been declared as constant", sName);
                    else sprintf(tmp, "'%s' has already been declared as visible", sName);
                    Error(tmp);
                }
                else {
                    char name[ASM_VAR_MAX_CHARS];
                    VisibleEntry *ve = (VisibleEntry *)malloc(sizeof(VisibleEntry));
                    ve->readOnly = 0;
                    HT_Add(sVisible, sName, 0, ve);
                    strcpy(name, sName);
                    GetNext();
                    fprintf(sDstFile, "%s\n", ASM_MPUSH);
                    fprintf(sDstFile, "%s .%s\n", ASM_MADD, name);
                    HT_Add(sIdentifiers, name, 0, 0);
                    /* Assignment? */
                    if (sNext == N7_CHAR && sChar == '=') {
                        GetNext();
                        fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, name);
                        fprintf(sDstFile, "%s\n", ASM_MSWAP);
                        Expression();
                        fprintf(sDstFile, "%s\n", ASM_MSWAP);
                        fprintf(sDstFile, "%s @0\n", ASM_MSET);                    
                    }
                    fprintf(sDstFile, "%s\n", ASM_MPOP);
                }
            }
            else {
                Error("Expected identifier");
            }
        } while (CouldGetChar(','));
    }
}

/*
 * ConstantDeclaration
 * ------------------
 */
static void ConstantDeclaration() {
    GetNext();

    if (sLocalScope > 0) Error("Constant declarations can't be local");
    if (sBlockLevel > 0) Error("Constant declarations can't be conditional");

    if (!(sNext == N7_EOL || sNext == N7_EOF)) {
        do {
            if (sNext == N7_NAME) {
                if (GetFunction(sName)) {
                    char tmp[64 + ASM_VAR_MAX_CHARS];
                    sprintf(tmp, "'%s' is a static function", sName);
                    Error(tmp);
                }
                /*VisibleEntry *ve = HT_Get(sVisible, sName, 0);*/
                VisibleEntry *ve;
                if (!(ve = (VisibleEntry *)HT_Get(sConstants, sName, 0))) ve = (VisibleEntry *)HT_Get(sVisible, sName, 0); 
                if (ve) {
                    char tmp[64 + ASM_VAR_MAX_CHARS];
                    if (ve->readOnly) sprintf(tmp, "'%s' has already been declared as constant", sName);
                    else sprintf(tmp, "'%s' has already been declared as visible", sName);
                    Error(tmp);
                }
                else {
                    char name[ASM_VAR_MAX_CHARS];
                    VisibleEntry *ve = (VisibleEntry *)malloc(sizeof(VisibleEntry));
                    ve->readOnly = 1;
                    HT_Add(sConstants, sName, 0, ve);
                    strcpy(name, sName);
                    GetNext();
                    fprintf(sDstFile, "%s\n", ASM_MPUSH);
                    if (sLibName) fprintf(sDstFile, "%s\n", ASM_LOADPM);
                    fprintf(sDstFile, "%s .%s\n", ASM_MADD, name);
                    ExpectChar('=');
                    HT_Add(sIdentifiers, name, 0, 0);
                    fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, name);
                    fprintf(sDstFile, "%s\n", ASM_MSWAP);
                    ConstExpression();
                    fprintf(sDstFile, "%s\n", ASM_MSWAP);
                    fprintf(sDstFile, "%s @0\n", ASM_MSET);                    
                    fprintf(sDstFile, "%s\n", ASM_MPOP);
                }
            }
            else {
                Error("Expected identifier");
            }
        } while (CouldGetChar(','));
    }
}

/*
 * GetFunctionDefinition
 * ---------------------
 */
static FunctionDefinition *GetFunctionDefinition(const char *name) {
    FunctionDefinition *f = 0;
    
    if (!(f = (FunctionDefinition *)HT_Get(sFunction->functions, name, 0))) {
        char tmp[64 + ASM_VAR_MAX_CHARS];
        sprintf(tmp, "Could not find '%s' definition", name);
        Error(tmp);
    }
    
    return f;
}

/*
 * Function
 * --------
 */
static FunctionDefinition *Function() {
    FunctionDefinition *f;
    FunctionParameter *p;
    int functionIndex = sFunctionIndex;
    char key[ASM_VAR_MAX_CHARS];

    GetNext();
   
    /* Static function? */
    if (sNext == N7_NAME) {
        /* This first one could be checked in prescan. */
        if (sBlockLevel > 0) Error("Static function definitions can't be conditional");
        strcpy(key, sName);
        GetNext();
    }
    /* Anonymous. */
    else if (sNext == N7_CHAR && sChar == '(') {
        sprintf(key, "%d", functionIndex);
    }
    /* Shouldn't happen, caught by prescan. */
    else {
        Error("Syntax error");
    }
    
    sFunctionIndex++;

    /* The function code is inserted where the function is declared. This may be
       anywhere in the program, so let's just jump over it. */
    fprintf(sDstFile, "%s __%d_end:\n", ASM_JMP, functionIndex);
    
    /* This is the label we jump to during a function call. */
    fprintf(sDstFile, "__%d:\n", functionIndex);
    /* Use the name to get the function definition generated during prescan. */

    f = GetFunctionDefinition(key);

    /* Generate runtime error if the numbers mismatch. */
#ifdef OPT_PVAL
    if (f->anonymous) fprintf(sDstFile, "%s %d \"Anonymous function\"\n", ASM_OPT_PVAL, f->paramCount);
    else fprintf(sDstFile, "%s %d \"'%s'\"\n", ASM_OPT_PVAL, f->paramCount, f->name);
#else
    /* Pop number of parameters that was passed when calling. */
    fprintf(sDstFile, "%s @0\n", ASM_POP);
    fprintf(sDstFile, "%s @1 %d\n", ASM_MOVE, f->paramCount);
    fprintf(sDstFile, "%s @0 @1\n", ASM_ECMP);
    fprintf(sDstFile, "%s __%d_ok:\n", ASM_JMPT, functionIndex);
    if (f->paramCount == 0) {
        if (f->anonymous) fprintf(sDstFile, "%s @0 \"Anonymous function expected no arguments\"\n", ASM_MOVE);
        else fprintf(sDstFile, "%s @0 \"'%s' expected no arguments\"\n", ASM_MOVE, f->name);
    }
    else if (f->paramCount == 1) {
        if (f->anonymous) fprintf(sDstFile, "%s @0 \"Anonymous function expected %d argument\"\n", ASM_MOVE, f->paramCount);
        else fprintf(sDstFile, "%s @0 \"'%s' expected %d argument\"\n", ASM_MOVE, f->name, f->paramCount);
    }
    else {
        if (f->anonymous) fprintf(sDstFile, "%s @0 \"Anonymous function expected %d arguments\"\n", ASM_MOVE, f->paramCount);
        else fprintf(sDstFile, "%s @0 \"'%s' expected %d arguments\"\n", ASM_MOVE, f->name, f->paramCount);
    }
    fprintf(sDstFile, "%s @0\n", ASM_RTE);
    fprintf(sDstFile, "__%d_ok:\n", functionIndex);
#endif    
    ExpectChar('(');
    /* Push and load local memory created by CALL. */
    fprintf(sDstFile, "%s\n", ASM_MPUSH);
    fprintf(sDstFile, "%s\n", ASM_LOCAL);

    /* Push current identifiers to the stack and create a new table for 
       this function. */
    sIdentifierStack[sIdentifierStackIndex++] = sIdentifiers;
    sIdentifiers = HT_Create(1);

    /* Load parameters. */
    p = f->parameters;
    while (p) {
        /* Add parameter name to identifiers. */
        HT_Add(sIdentifiers, p->name, 0, 0);
        
        GetNext();
        if (sNext == N7_CHAR && sChar == ',') {
            GetNext();
            EatNewLines();
        }
        
#ifdef OPT_LOADPARAM
        fprintf(sDstFile, "%s .%s\n", ASM_OPT_LOADPARAM, p->name);
#else
        /* Create and load. */
        fprintf(sDstFile, "%s .%s\n", ASM_MADD, p->name);
        fprintf(sDstFile, "%s\n", ASM_MPUSH);
        fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, p->name);
        /* Pop value from stack and set. */
        fprintf(sDstFile, "%s @0\n", ASM_POP);
        fprintf(sDstFile, "%s @0\n", ASM_MSET);
        fprintf(sDstFile, "%s\n", ASM_MPOP);
#endif
        p = p->next;
    }        
    ExpectChar(')');
    
    /* This. */
    HT_Add(sIdentifiers, "this", 0, 0);
#ifdef OPT_LOADPARAM
    fprintf(sDstFile, "%s .this\n", ASM_OPT_LOADPARAM);
#else
    fprintf(sDstFile, "%s .this\n", ASM_MADD);
    fprintf(sDstFile, "%s\n", ASM_MPUSH);
    fprintf(sDstFile, "%s .this\n", ASM_MLOAD);
    fprintf(sDstFile, "%s @0\n", ASM_POP);
    fprintf(sDstFile, "%s @0\n", ASM_MSET);
    fprintf(sDstFile, "%s\n", ASM_MPOP);
#endif    
    sLocalScope++;

    sFunction = f;
    Block();
    sFunction = f->parent;
    if (!(sNext == N7_KEYWORD && sKeyword == N7_ENDFUNC)) Error("Expected 'endfunc'");
    GetNext();
    if (!f->anonymous) ExpectNewLine();
    sLocalScope--;
    HT_Free(sIdentifiers, 0);
    sIdentifiers = sIdentifierStack[--sIdentifierStackIndex];

    /* Pop local memory, RET will free it. */
    fprintf(sDstFile, "%s\n", ASM_MPOP);
    fprintf(sDstFile, "%s @0\n", ASM_CLR); /* 210714. */
    fprintf(sDstFile, "%s\n", ASM_RET);
    
    fprintf(sDstFile, "__%d_end:\n", functionIndex);
    
    return f;
}

/*
 * Return
 * ------
 * Return from subroutine.
 */
static void Return() {
    if (sLocalScope > 0) {
        GetNext();
        /* Any return value? */
        if (sNext == N7_EOL) fprintf(sDstFile, "%s @0\n", ASM_CLR);
        else Expression();
        
        /* "House keeping!" */
        for (int i = sBlockLevel - 1; i >= 0 && sBlockInfoStack[i].localScope == sLocalScope; i--) {
            switch (sBlockInfoStack[i].type) {
                case BLOCK_TYPE_SELECT:
                    fprintf(sDstFile, "%s @1\n", ASM_POP);
                    break;
                case BLOCK_TYPE_FOR:
                    fprintf(sDstFile, "%s @1\n", ASM_POP);
                    fprintf(sDstFile, "%s @1\n", ASM_POP);
                    fprintf(sDstFile, "%s\n", ASM_MSWAP);
                    fprintf(sDstFile, "%s\n", ASM_MPOP);
                    break;
                case BLOCK_TYPE_FOREACH:
                    fprintf(sDstFile, "%s\n", ASM_IPOP);
                    fprintf(sDstFile, "%s\n", ASM_IDEL);
                    break;
                default:
                    break;
            }
        }
        
        /* Pop local memory, RET will free it. */
        fprintf(sDstFile, "%s\n", ASM_MPOP);
        fprintf(sDstFile, "%s\n", ASM_RET);
    }
    else {
        ErrorUnexpected();
    }
}

/*
 * CallFunction
 * ------------
 */
static void CallFunction(FunctionDefinition *fd) {
    int argCount = 0;
 
    /* This. */
    fprintf(sDstFile, "%s @0\n", ASM_LPTBL);
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);

    ExpectChar('(');
    /* The memory loaded right now should contain a variable with a label. */
    /*if (!((isProc && (sNext == N7_EOL || sNext == N7_EOF)) || (!isProc && sNext == N7_CHAR && sChar == ')'))) {*/
    if (!(sNext == N7_CHAR && sChar == ')')) {
        do {
            fprintf(sDstFile, "%s\n", ASM_MSWAP);
            Expression();
            fprintf(sDstFile, "%s\n", ASM_MSWAP);
            fprintf(sDstFile, "%s @0\n", ASM_PUSH);
            argCount++;
        } while (CouldGetChar(','));
    }
    ExpectChar(')');
    
    if (fd && argCount != fd->paramCount) {
        if (fd->paramCount == 0) {
            char tmp[64 + ASM_VAR_MAX_CHARS];
            sprintf(tmp, "'%s' expects no arguments but gets %d", fd->name, argCount);
            Error(tmp);
        }
        else if (fd->paramCount == 1) {
            char tmp[64 + ASM_VAR_MAX_CHARS];
            sprintf(tmp, "'%s' expects %d argument but gets %d", fd->name, fd->paramCount, argCount);
            Error(tmp);
        }
        else {
            char tmp[32 + ASM_VAR_MAX_CHARS];
            sprintf(tmp, "'%s' expects %d arguments but gets %d", fd->name, fd->paramCount, argCount);
            Error(tmp);
        }
    }
    
    fprintf(sDstFile, "%s @0 %d\n", ASM_MOVE, argCount);
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);
    /* The memory is still the loaded variable, so get and pop. */
    /*fprintf(sDstFile, "%s @0\n", ASM_MGET);
    fprintf(sDstFile, "%s\n", ASM_MPOP);*/
    /* No, the variable shouldn't be popped here, replaced with two mswaps,
       mpop happens outside. */
    fprintf(sDstFile, "%s @0\n", ASM_MGET);
    fprintf(sDstFile, "%s\n", ASM_MSWAP);
    /* This will wander away but return after this instruction with a value in
       @0. */
    fprintf(sDstFile, "%s @0\n", ASM_CALL);
    fprintf(sDstFile, "%s\n", ASM_MSWAP);
}


/*
 * LoadSystemFunctionParams
 * ------------------------
 * Helper.
 */
static int LoadSystemFunctionParams(int min, int max, int isFunction) {
    int argc = 0;
    
    if (isFunction) {
        ExpectChar('(');
        if (sNext == N7_CHAR && sChar == ')') {
            if (min != 0) Error("Expected expression");
            GetNext();
            return 0;
        }
    }
    else if (sNext == N7_EOL || sNext == N7_EOF) {
        if (min != 0) Error("Expected expression");
        return 0;
    }
    
    if (max > 0) {
        do {
            Expression();
            fprintf(sDstFile, "%s @0\n", ASM_PUSH);
            argc++;
        } while (argc < max && CouldGetChar(','));
    }
    if (argc < min) ExpectChar(',');
    if (isFunction) ExpectChar(')');
    
    return argc;
}

/*
 * CallSystemFunction
 * ------------------
 * Helper.
 */
static int CallSystemFunction(int sysFunction, int min, int max, int isFunction, int getNext) {
    int count;
    
    if (getNext) GetNext();
    count = LoadSystemFunctionParams(min, max, isFunction);
    fprintf(sDstFile, "%s %d %d\n", ASM_SYS, sysFunction, count);
    
    return count;
}

/*
 * CallCFunction
 * -------------
 * Helper.
 */
static int CallCFunction(int isFunction, int getNext) {
    int count;
    if (getNext) GetNext();
    count = LoadSystemFunctionParams(1, 64, isFunction);
    fprintf(sDstFile, "%s %d\n", ASM_FCALL, count);
    
    return count;
}

/*
 * EatWhite
 * --------
 * Eat white space and increase line number.
 */
static void EatWhite() {
    int c = fgetc(sSrcFile);
    while (c == ' ' || c == '\t' || c == '\r' || c == '\'') {
        if (c == '\'') {
            c = fgetc(sSrcFile);
            while (!(c == '\n' || c == EOF)) c = fgetc(sSrcFile);
            ungetc(c, sSrcFile);
        }
        c = fgetc(sSrcFile);
    }
    ungetc(c, sSrcFile); 
}

static void EatNewLines() {
    while (sNext == N7_EOL) GetNext();
}

/*
 * GetNext
 * -------
 * Get next token.
 */
static void GetNext() {
    int c;
    
    /* Can't increase line number until a new token is actually loaded. */
    if (sNext == N7_EOL && sLastEOLWasReal) {
        sLineNumber++;
        /* Add line number tag. */
        if (!sPrescan && !sAsm) fprintf(sDstFile, "/line:%d\n", sLineNumber);
    }
    
    EatWhite();
    
    c = fgetc(sSrcFile);
    
    /* Single line comment. */
    if (c == '\'') {
        c = fgetc(sSrcFile);
        while (!(c ==  '\n' || c == EOF)) c = fgetc(sSrcFile);
    }
    
    /* Hack. */
    if (sAsm) {
        int i = 0;
        while (!(c == '\n' || c == EOF || i >= ASM_STRING_MAX_CHARS - 1)) {
            sString[i++] = c;
            c = fgetc(sSrcFile);
        }
        sString[i] = '\0';
        if (i >= ASM_STRING_MAX_CHARS) {
            Error("Assembler line too long");
        }
        else if (c == EOF) {
            Error("End of file in assembler line");
        }
        /*fprintf(sDstFile, "/line:%d\n", sLineNumber++);*/
        sLineNumber++;
        if (strncmp(sString, "endasm", 6) == 0) {
            KeywordEntry *ke = (KeywordEntry *)HT_Get(sKeywords, "endasm", 0);
            sNext = N7_KEYWORD;
            sKeyword = ke->keyword;
            sKeywordEntry = ke;
        }
        else {
            sNext = N7_STRING;
        }
        return;
    }
    
    /* Runtime flag. */
    if (c == '#') {
        char flag[64];
        int i = 0;
        c = fgetc(sSrcFile);
        while (!(c == '\n' || c == EOF)) {
            if (i < 63) flag[i++] = tolower(c);
            c = fgetc(sSrcFile);
        }
        flag[i] = '\0';
        if (strcmp(flag, "win32") == 0) {
            sRuntimeFlags |= N7_WIN32_FLAG;
        }
        else if (strcmp(flag, "dbg") == 0) {
            sRuntimeFlags |= N7_DBG_FLAG;
        }
        else if (strncmp(flag, "mem", 3) == 0) {
            if (strlen(flag) > 3) {
                sMemoryRequest = atoi(flag + 3);
                sMemoryRequest = sMemoryRequest > 0 ? sMemoryRequest : 0;
            }
        }
    }
    
    if (c == EOF) {
        sNext = N7_EOF;
    }
    else if (c == '\n') {
        sNext = N7_EOL;
        /* Need to store this flag for correct line numbers. */
        sLastEOLWasReal = 1;
    }
    else if (c == ';') {
        sNext = N7_EOL;
        sLastEOLWasReal = 0;
    }
    /* Might keep _ for proc as in n6, so a variable name can't start with that
       character. */
    else if (isalpha(c)) { // || c == '_') {
        char str[ASM_VAR_MAX_CHARS];
        char *s = str;
        KeywordEntry *ke;
        do {
            *s = c;
            s++;
            if (s - str >= ASM_VAR_MAX_CHARS) Error("Name too long");
            c = fgetc(sSrcFile);
        } while (isalpha(c) || isdigit(c) || c == '_');
        ungetc(c, sSrcFile);
        *s = '\0';
        ke = (KeywordEntry *)HT_Get(sKeywords, str, 0);
        if (ke) {
            sNext = N7_KEYWORD;
            sKeyword = ke->keyword;
            sKeywordEntry = ke;
        }
        else {
            sNext = N7_NAME;
            strcpy(sName, str);
        }
    }
    else if (isdigit(c)) {
        char str[N7_NUMBER_MAX_CHARS];
        char *s = str;
        int decimal = 0;
        do {
            if (s - str < N7_NUMBER_MAX_CHARS - 1) {
                *s = c;
                s++;
            }
            c = fgetc(sSrcFile);
            if (c == '.') decimal++;
        } while (isdigit(c) || (c == '.' && decimal == 1));
        ungetc(c, sSrcFile);
        *s = '\0';
        sNext = N7_NUMBER;
        sNumber = atof(str);
        strcpy(sNumberS, str);
    }
    else if (c == '\"') {
        int i = 0;
        c = fgetc(sSrcFile);
        while (!(c == '\"' || c == '\n' || c == EOF || i >= ASM_STRING_MAX_CHARS - 1)) {
            sString[i++] = c;
            c = fgetc(sSrcFile);
        }
        sString[i] = '\0';
        if (c == '\"') {
            sNext = N7_STRING;
        }
        else if (c == '\n') {
            Error("End of line in string constant");
        }
        else if (c == EOF) {
            Error("End of file in string constant");
        }
        else {
            Error("String constant too long");
        }
    }
    else {
        sNext = N7_CHAR;
        sChar = c;
    }
}

/*
 * ExpectChar
 * ----------
 * Continue if next token is char c, else throw an error.
 */
static void ExpectChar(char c) {
    if (sNext == N7_CHAR && sChar == c) {
        GetNext();
        if (c == ',' || c == '(' ) EatNewLines();
    }
    else {
        char tmp[13];
        sprintf(tmp, "Expected '%c'", c);
        Error(tmp);
    }
}

/*
 * CouldGetChar
 * ------------
 * Eat char c and return 1 if possible, else return 0. 
 */
static int CouldGetChar(char c) {
    if (sNext == N7_CHAR && sChar == c) {
        GetNext();
        if (c == ',' || c == '(' ) EatNewLines();
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * ExpectNewLine
 * -------------
 * Expect end of line or end of file.
 */
static void ExpectNewLine() {
    if (sNext == N7_EOL) {
        while (sNext == N7_EOL) GetNext();
    }
    else if (sNext != N7_EOF) {
        /*Error("Expected new line");*/
        ErrorUnexpected();
    }
}

/*
 * Declared
 * --------
 * Returns 1 if variable has been declared, else 0.
 */
static int Declared(const char *name) {
    /* A variable is added to the current scope's identifier table when it's
       assigned a value. If we're not in the main program scope we also need to
       check the table of global (visible) variables. */
    return HT_Exists(sIdentifiers, name, 0) || HT_Exists(sVisible, name, 0) || HT_Exists(sConstants, name, 0);
}


/*
 * ErrorUnexpected
 * ---------------
 * Report anything next as unexpected.
 */
static void ErrorUnexpected() {
    if (sNext == N7_EOF) {
        Error("Unexpected end of file");
    }
    else if (sNext == N7_EOL) {
        Error("Unexpected end of line");
    }
    else if (sNext == N7_CHAR) {
        char error[25];
        sprintf(error, "Unexpected character '%c'", sChar);
        Error(error);
    }
    else if (sNext == N7_STRING) {
        char error[21 + ASM_STRING_MAX_CHARS];
        sprintf(error, "Unexpected string '%s'", sString); 
        Error(error);
    }
    else if (sNext == N7_NUMBER) {
        char error[512]; /* Uhm, idk ... */
        sprintf(error, "Unexpected number '%s'", sNumberS);
        Error(error);
    }
    else if (sNext == N7_NAME) {
        char error[25 + ASM_VAR_MAX_CHARS];
        sprintf(error, "Unexpected identifier '%s'", sName);
        Error(error);
    }
    else if (sNext == N7_KEYWORD) {
        char error[128];
        sprintf(error, "Unexpected '%s'", GetKeywordString(sKeyword)); 
        Error(error);
    }
}

/*
 * PeekForChar
 * -----------
 * A sad must, as I see it, for TableFactor. Sorry.
 */
static int PeekForChar(char wanted) {
    fpos_t pos;
    fgetpos(sSrcFile, &pos);
    int c = fgetc(sSrcFile);
    while (c == ' ' || c == '\t') c = fgetc(sSrcFile);
    fsetpos(sSrcFile, &pos);
    return c == wanted;
}

/*
 * TableFactor
 * -----------
 * A table can be constructed from a list of autoindexed values:
 *   table = [42, 15, "foo", 27]
 * or from a list of identifier and value pairs (<id>:<value>):
 *   pos = [x: 9, y = 3, z = 15]
 * Allow new lines here.
 */
static FactorType TableFactor() {
    FactorType type;
    
    ExpectChar('[');
    EatNewLines();
    fprintf(sDstFile, "%s @0\n", ASM_CTBL);
    /* List of <id>:<value> pairs. */
    if (sNext == N7_NAME && PeekForChar(':')) {
        do {
            char name[ASM_VAR_MAX_CHARS];
            /* Identifier. */
            EatNewLines();
            if (sNext != N7_NAME) Error("Expected identifier");
            strcpy(name, sName);
            GetNext();
            EatNewLines();
            ExpectChar(':');
            EatNewLines();
            fprintf(sDstFile, "%s @0\n", ASM_PUSH);        

            /* Value. */
            Expression();
            EatNewLines();
            fprintf(sDstFile, "%s @1\n", ASM_POP);
            fprintf(sDstFile, "%s\n", ASM_MPUSH);
            fprintf(sDstFile, "%s @1\n", ASM_MLOAD);
            fprintf(sDstFile, "%s .%s\n", ASM_MADD, name);
            fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, name);
            fprintf(sDstFile, "%s @0\n", ASM_MSET);
            fprintf(sDstFile, "%s\n", ASM_MPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_MOVE);
        } while (CouldGetChar(','));
        type = FACTOR_TABLE;
    }
    /* List of autoindexed values. */
    else {
        int index = 0;
        if (!(sNext == N7_CHAR && sChar == ']')) {
            do {
                EatNewLines();
                
                /* Value. */
                fprintf(sDstFile, "%s @0\n", ASM_PUSH);        
                Expression();
                fprintf(sDstFile, "%s @1\n", ASM_POP);
                fprintf(sDstFile, "%s\n", ASM_MPUSH);
                fprintf(sDstFile, "%s @1\n", ASM_MLOAD);
                /* Index. */
                fprintf(sDstFile, "%s %d\n", ASM_MADD, index);
                fprintf(sDstFile, "%s %d\n", ASM_MLOAD, index);
                fprintf(sDstFile, "%s @0\n", ASM_MSET);
                fprintf(sDstFile, "%s\n", ASM_MPOP);
                fprintf(sDstFile, "%s @0 @1\n", ASM_MOVE);
                
                index++;
                EatNewLines();
            } while (CouldGetChar(','));
        }
        type = FACTOR_ARRAY;
    }
    EatNewLines();
    
    ExpectChar(']');
    return type;
}

/*
 * DimRec
 * ------
 * Allocate indexes for an array, possibly multidimensional. If fill is set,
 * fill the array with copies of a variable that is assumed to be on top of the
 * stack.
 *    Note that this function generates code for the entire initialization
 * rather than call some vm or system function. That is, the n7 line:
 *   foo = fill([x: 0, y: 0], 10, 3)
 * , generates code with the same result as:
 *   filler = [x: 0, y: 0]
 *   foo = []
 *   for i = 0 to 9
 *     foo[i] = []
 *     for j = 0 to 2
 *       foo[i][j] = copy(filler)
 *     next
 *   next 
 * It could probably be done with a lot fewer instructions :)
 */
static void DimRec(int fill) {
    int labelIndex = sLabelIndex++;    
    int last;
   
    /* Create new table in @0 and push it to the stack. */
    fprintf(sDstFile, "%s @0\n", ASM_CTBL);
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);
    
    /* Size of the dimension. */
    Expression();
    
    /* Flag if we're dealing with the last dimension. */
    last = !(sNext == N7_CHAR && sChar == ',');
    
     /* Move destination (dimension size) to @2, and put initial index (0) in
        @1. */
    fprintf(sDstFile, "%s @2 @0\n", ASM_MOVE);
    fprintf(sDstFile, "%s @1 0\n", ASM_MOVE); /* Initial index, 0, to @1. */
    
    /* Start of loop, compare index with size. */
    fprintf(sDstFile, "dim_%d_start:\n", labelIndex);
    fprintf(sDstFile, "%s @0 @1\n", ASM_MOVE);
    fprintf(sDstFile, "%s @0 @2\n", ASM_GEQL);
    /* Jump to end if done. */
    /*fprintf(sDstFile, "%s @0\n", ASM_EVAL);
    fprintf(sDstFile, "%s dim_%d_end:\n", ASM_JMPT, labelIndex);*/
    fprintf(sDstFile, "%s @0 dim_%d_end:\n", ASM_JMPET, labelIndex);
    
    /* Put the table we created in @0 and add the index as a subvariable. */
    fprintf(sDstFile, "%s @0\n", ASM_POP);
    fprintf(sDstFile, "%s\n", ASM_MPUSH);
    fprintf(sDstFile, "%s @0\n", ASM_MLOAD);
    fprintf(sDstFile, "%s @1\n", ASM_MADD);
    /* If this is the last dimension and we're filling, copy the filler to the
       new subvariable. */
    if (last && fill) {
        /* Get the filler, make a copy and push the original back to the
           stack and set the subvariable to the copy. */
        fprintf(sDstFile, "%s @4\n", ASM_POP);
        fprintf(sDstFile, "%s @3 @4\n", ASM_CPY);
        fprintf(sDstFile, "%s @4\n", ASM_PUSH);
        fprintf(sDstFile, "%s @1\n", ASM_MLOAD);
        fprintf(sDstFile, "%s @3\n", ASM_MSET);
    }
    fprintf(sDstFile, "%s\n", ASM_MPOP);
    /* Push this table back on the stack. */
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);

    /* More dimensions? */
    if (!last) {
        GetNext();
        /* If we're filling, we have to pass on the filler. */
        if (fill) {
            /* The filler is not on top of the stack, so we need two pops to
               get it into @4. */
            fprintf(sDstFile, "%s @3\n", ASM_POP); /* The table we're building */
            fprintf(sDstFile, "%s @4\n", ASM_POP); /* The filler. */
            /* Push the filler and table back on stack. */
            fprintf(sDstFile, "%s @4\n", ASM_PUSH);
            fprintf(sDstFile, "%s @3\n", ASM_PUSH);
        }
        /* Push current index and destination. */
        fprintf(sDstFile, "%s @1\n", ASM_PUSH);
        fprintf(sDstFile, "%s @2\n", ASM_PUSH);
        if (fill) fprintf(sDstFile, "%s @4\n", ASM_PUSH); /* filler to stack. */
        /* Puts a new table in @0. */
        DimRec(fill);
        /* Just pop filler into oblivion, original is still on stack since
           before. */
        if (fill) fprintf(sDstFile, "%s @4\n", ASM_POP);
        /* Pop destination and index. */
        fprintf(sDstFile, "%s @2\n", ASM_POP);
        fprintf(sDstFile, "%s @1\n", ASM_POP);
        
        /* Pop the table we created to @3 and put the new table (@0) at the
           right index (@1).*/
        fprintf(sDstFile, "%s @3\n", ASM_POP);
        fprintf(sDstFile, "%s\n", ASM_MPUSH);
        fprintf(sDstFile, "%s @3\n", ASM_MLOAD);
        fprintf(sDstFile, "%s @1\n", ASM_MLOAD);
        fprintf(sDstFile, "%s @0\n", ASM_MSET);
        fprintf(sDstFile, "%s\n", ASM_MPOP);
        fprintf(sDstFile, "%s @3\n", ASM_PUSH);
    }
    
    /* Increase @1 by 1 (need another ADD version ...) and loop. */
    fprintf(sDstFile, "%s @3 1\n", ASM_MOVE);
    fprintf(sDstFile, "%s @1 @3\n", ASM_ADD);
    fprintf(sDstFile, "%s dim_%d_start:\n", ASM_JMP, labelIndex);
    fprintf(sDstFile, "dim_%d_end:\n", labelIndex);
    fprintf(sDstFile, "%s @0\n", ASM_POP);
}

/*
 * Dim
 * ---
 * Allocate indexes for an array, possibly multidimensional.
 */
static void Dim() {
    GetNext();
    ExpectChar('(');
    DimRec(0);
    ExpectChar(')');
}

/*
 * Fill
 * ----
 * Allocate indexes for an array, possibly multidimensional, and fill it with
 * copies of the variable passed as first argument.
 */
static void Fill() {
    GetNext();
    ExpectChar('(');
    Expression();
    ExpectChar(',');
    fprintf(sDstFile, "%s @0\n", ASM_PUSH);
    DimRec(1);
    fprintf(sDstFile, "%s @1\n", ASM_POP);
    ExpectChar(')');
    /* Might be some garbage tables here, let gc deal with them. @1 will soon
       be cleared anyway, but @4 isn't used that much. */
    fprintf(sDstFile, "%s @1\n", ASM_CLR);
    fprintf(sDstFile, "%s @4\n", ASM_CLR);
}

/*
 * AsmFunction
 * -----------
 * Helper.
 */
void AsmFunction(const char *instruction, int argc, int expected) {
    GetNext();
    ExpectChar('(');
    Expression();
    if (argc == 2) {
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        ExpectChar(',');
        Expression();
        fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
        fprintf(sDstFile, "%s @0 @1\n", instruction);
    }
    else {
        if (expected == 2) fprintf(sDstFile, "%s @0 @0\n", instruction);
        else fprintf(sDstFile, "%s @0\n", instruction);
    }
    ExpectChar(')');
}    

/*
 * Factor
 * ------
 * Factor.
 */
static FactorInfo Factor() {
    FactorInfo result;
    
    result.data = 0;
    
    /* Unary + and -. */
    if (sNext == N7_CHAR && sChar == '+') {
        GetNext();
        EatNewLines();
        result = ParsePrecedenceLevelZero();
        /* This does jack shit, but should actually cause a runtime exception
           if the factor is not numeric, let's say:
              x = +dim(3, 3)
           Or maybe just disable unary +, treat it as a compile time error? */
    }
    else if (sNext == N7_CHAR && sChar == '-') {
        GetNext();
        EatNewLines();
        result = ParsePrecedenceLevelZero();
        fprintf(sDstFile, "%s @0\n", ASM_NEG);
        /* I don't remember, why the heck did I remove the line below? */
        result.type = FACTOR_VALUE;
    }
    /* Numeric constant. */
    else if (sNext == N7_NUMBER) {
        fprintf(sDstFile, "%s @0 %s\n", ASM_MOVE, sNumberS);
        GetNext();
        result.type = FACTOR_VALUE;
    }
    /* String contstant. */
    else if (sNext == N7_STRING) {
        fprintf(sDstFile, "%s @0 \"%s\"\n", ASM_MOVE, sString);
        GetNext();
        result.type = FACTOR_VALUE;
    }
    /* (<expr>) */
    else if (sNext == N7_CHAR && sChar == '(') {
        GetNext();
        result = Expression();
        ExpectChar(')');
    }
    /* Variable. */
    else if (sNext == N7_NAME) {
        FunctionDefinition *fd = GetFunction(sName);
        if (fd) {
            fprintf(sDstFile, "%s @0 __%d:\n", ASM_MOVE, fd->index);
            GetNext();
            result.type = FACTOR_FUNCTION;
            result.data = fd;
        }
        else {
            fprintf(sDstFile, "%s\n", ASM_MPUSH);
            if (!Declared(sName)) {
                char tmp[30 + ASM_VAR_MAX_CHARS];
                sprintf(tmp, "Undeclared identifier '%s'", sName);
                Error(tmp);
            }
            /*if (sLocalScope > 0 && HT_Exists(sVisible, sName, 0)) {
                fprintf(sDstFile, "%s\n", ASM_LOADPM);
                if (sLibName) fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sLibName);
            }*/
            /*if (sLocalScope > 0) {
                if (HT_Exists(sConstants, sName, 0)) {
                    fprintf(sDstFile, "%s\n", ASM_LOADPM);
                }
                else if (HT_Exists(sVisible, sName, 0)) {
                    fprintf(sDstFile, "%s\n", ASM_LOADPM);
                    if (sLibName) fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sLibName);
                }
            }*/
            if (HT_Exists(sConstants, sName, 0)) {
                fprintf(sDstFile, "%s\n", ASM_LOADPM);
            }
            else if (sLocalScope > 0) {
                if (HT_Exists(sVisible, sName, 0)) {
                    fprintf(sDstFile, "%s\n", ASM_LOADPM);
                    if (sLibName) fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sLibName);
                }
            }
            
            fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sName);
            GetNext();
            result.type = FACTOR_NAME;
        }
    }    
    /* Table construct. */
    else if (sNext == N7_CHAR && sChar == '[') {
        result.type = TableFactor();
    }
    /* Function definition. */
    else if (sNext == N7_KEYWORD && sKeyword == N7_FUNCTION) {
        FunctionDefinition *fd = Function();
        if (!fd->anonymous) Error("Unexpected identifier for non-static function");
        fprintf(sDstFile, "%s @0 __%d:\n", ASM_MOVE, fd->index);
        result.type = FACTOR_FUNCTION;
        result.data = fd;
    }
    /* Constant. */
    else if (sNext == N7_KEYWORD && sKeywordEntry->type) {
        if (sKeywordEntry->type == 1) fprintf(sDstFile, "%s @0 %d\n", ASM_MOVE, sKeywordEntry->value.i);
        else if (sKeywordEntry->type == 2) fprintf(sDstFile, "%s @0 %.12f\n", ASM_MOVE, sKeywordEntry->value.f);
        else if (sKeywordEntry->type == 3) fprintf(sDstFile, "%s @0 \"%s\"\n", ASM_MOVE, sKeywordEntry->value.s);
        else fprintf(sDstFile, "%s @0\n", ASM_CLR);
        GetNext();
        result.type = FACTOR_VALUE;
    }
    else if (sNext == N7_KEYWORD) {
        int argc;
        
        result.type = FACTOR_VALUE;
        switch (sKeyword) {
            /* Table (array) allocation. */
            case N7_DIM:  Dim(); result.type = FACTOR_TABLE; break;
            case N7_FILL: Fill(); result.type = FACTOR_TABLE; break;
            /* Instruction level functions. */
            /*case N7_TOSTRING:  AsmFunction(ASM_TOSTR, 1, 2); break;*/
            case N7_TOSTRING:  CallSystemFunction(SYS_STR, 1, 3, 1, 1); break;
            case N7_TONUMBER:  AsmFunction(ASM_TONUM, 1, 2); break;
            case N7_TOINTEGER: AsmFunction(ASM_TOINT, 1, 2); break;
            case N7_SIZEOF:    AsmFunction(ASM_SIZE, 1, 2); break;
            case N7_LEN:       AsmFunction(ASM_LEN, 1, 2); break;
            /* Oops, new implementation of the copy instruction can't handle
               source and destination registers being the same. */
            /*case N7_COPY:      AsmFunction(ASM_CPY, 1, 2); result.type = FACTOR_UNKNOWN; break;*/
            case N7_COPY:
                GetNext();
                Expression();
                fprintf(sDstFile, "%s @1 @0\n", ASM_MOVE);
                fprintf(sDstFile, "%s @0 @1\n", ASM_CPY);
                result.type = FACTOR_UNKNOWN;
                break;
            case N7_TYPEOF:         AsmFunction(ASM_TYPE, 1, 2); break;
            case N7_ABS:            AsmFunction(ASM_ABS, 1, 1); break;
            case N7_SGN:            AsmFunction(ASM_SGN, 1, 1); break;
            case N7_COS:            AsmFunction(ASM_COS, 1, 1); break;
            case N7_SIN:            AsmFunction(ASM_SIN, 1, 1); break;
            case N7_TAN:            AsmFunction(ASM_TAN, 1, 1); break;
            case N7_ACOS:           AsmFunction(ASM_ACOS, 1, 1); break;
            case N7_ASIN:           AsmFunction(ASM_ASIN, 1, 1); break;
            case N7_ATAN:           AsmFunction(ASM_ATAN, 1, 1); break;
            case N7_ATAN2:          AsmFunction(ASM_ATAN2, 2, 2); break;
            case N7_SQR:            AsmFunction(ASM_SQR, 1, 1); break;
            case N7_POW:            AsmFunction(ASM_POW, 2, 2); break;
            case N7_FLOOR:          AsmFunction(ASM_FLOOR, 1, 1); break;
            case N7_CEIL:           AsmFunction(ASM_CEIL, 1, 1); break;
            case N7_ROUND:          AsmFunction(ASM_ROUND, 1, 1); break;
            case N7_RAD:            AsmFunction(ASM_RAD, 1, 1); break;
            case N7_DEG:            AsmFunction(ASM_DEG, 1, 1); break;
            case N7_MIN:            AsmFunction(ASM_MIN, 2, 2); break;
            case N7_MAX:            AsmFunction(ASM_MAX, 2, 2); break;
            case N7_LOAD_FUNCTION:  AsmFunction(ASM_FLOAD, 1, 1); break;
            /* System commands. */
            case N7_RLN:         CallSystemFunction(SYS_READ_LINE, 0, 2, 1, 1); break;
            case N7_DATETIME:    CallSystemFunction(SYS_DATE_TIME, 0, 1, 1, 1); result.type = FACTOR_TABLE; break;
            case N7_TIME:        CallSystemFunction(SYS_TIME, 0, 6, 1, 1); break;
            case N7_CLOCK:       CallSystemFunction(SYS_CLOCK, 0, 0, 1, 1); break;
            case N7_RND:         CallSystemFunction(SYS_RND, 0, 2, 1, 1); break;
            case N7_SYSTEM:      CallSystemFunction(SYS_CAPTURE, 1, 1, 1, 1); break;
            case N7_SPLIT:       CallSystemFunction(SYS_SPLIT_STR, 2, 2, 1, 1); result.type = FACTOR_ARRAY; break;
            case N7_LEFT:        CallSystemFunction(SYS_LEFT_STR, 2, 2, 1, 1); break;
            case N7_RIGHT:       CallSystemFunction(SYS_RIGHT_STR, 2, 2, 1, 1); break;
            case N7_MID:         CallSystemFunction(SYS_MID_STR, 2, 3, 1, 1); break;
            case N7_INSTR:       CallSystemFunction(SYS_IN_STR, 2, 3, 1, 1); break;
            case N7_REPLACE:     CallSystemFunction(SYS_REPLACE_STR, 3, 4, 1, 1); break;
            case N7_LOWER:       CallSystemFunction(SYS_LOWER_STR, 1, 1, 1, 1); break;
            case N7_UPPER:       CallSystemFunction(SYS_UPPER_STR, 1, 1, 1, 1); break;
            case N7_CHR:         CallSystemFunction(SYS_CHR, 1, 1, 1, 1); break;
            case N7_ASC:         CallSystemFunction(SYS_ASC, 1, 1, 1, 1); break;
            case N7_KEY:         CallSystemFunction(SYS_TBL_HAS_KEY, 2, 2, 1, 1); break;
            case N7_VAL:         CallSystemFunction(SYS_TBL_HAS_VALUE, 2, 2, 1, 1); break;
            case N7_KEYOF:       CallSystemFunction(SYS_TBL_KEY_OF, 2, 3, 1, 1); break;
            case N7_FILE:        CallSystemFunction(SYS_FILE_EXISTS, 1, 1, 1, 1); break;
            case N7_OPENFILE:    CallSystemFunction(SYS_OPEN_FILE, 1, 2, 1, 1); break;
            case N7_CREATEFILE:  CallSystemFunction(SYS_CREATE_FILE, 1, 2, 1, 1); break;
            case N7_OPENFILEDIALOG: CallSystemFunction(SYS_OPEN_FILE_DIALOG, 0, 1, 1, 1); break;
            case N7_SAVEFILEDIALOG: CallSystemFunction(SYS_SAVE_FILE_DIALOG, 0, 1, 1, 1); break;
            case N7_EXISTS:      CallSystemFunction(SYS_CHECK_FILE_EXISTS, 1, 1, 1, 1); break;
            case N7_FREAD:       CallSystemFunction(SYS_FILE_READ, 1, 3, 1, 1); break;
            case N7_FREADC:      CallSystemFunction(SYS_FILE_READ_CHAR, 1, 1, 1, 1); break;
            case N7_FRLN:        CallSystemFunction(SYS_FILE_READ_LINE, 1, 1, 1, 1); break;
            case N7_FILETELL:    CallSystemFunction(SYS_FILE_TELL, 1, 1, 1, 1); break;
            case N7_FILESEEK:    CallSystemFunction(SYS_FILE_SEEK, 2, 3, 1, 1); break;
            case N7_ACTIVE:      CallSystemFunction(SYS_WIN_ACTIVE, 0, 0, 1, 1); break;
            case N7_WINDOW:      CallSystemFunction(SYS_WIN_EXISTS, 1, 1, 1, 1); break;
            case N7_SCREENW:     CallSystemFunction(SYS_SCREEN_W, 0, 0, 1, 1); break;
            case N7_SCREENH:     CallSystemFunction(SYS_SCREEN_H, 0, 0, 1, 1); break;
            case N7_MOUSEX:      CallSystemFunction(SYS_MOUSE_X, 0, 0, 1, 1); break;
            case N7_MOUSEY:      CallSystemFunction(SYS_MOUSE_Y, 0, 0, 1, 1); break;
            case N7_MOUSEDX:     CallSystemFunction(SYS_MOUSE_DX, 0, 0, 1, 1); break;
            case N7_MOUSEDY:     CallSystemFunction(SYS_MOUSE_DY, 0, 0, 1, 1); break;
            case N7_MOUSEBUTTON: CallSystemFunction(SYS_MOUSE_DOWN, 1, 2, 1, 1); break;
            case N7_JOYX:        CallSystemFunction(SYS_JOY_X, 0, 0, 1, 1); break;
            case N7_JOYY:        CallSystemFunction(SYS_JOY_Y, 0, 0, 1, 1); break;
            case N7_JOYBUTTON:   CallSystemFunction(SYS_JOY_BUTTON, 0, 2, 1, 1); break;
            case N7_CREATEZONE:  CallSystemFunction(SYS_CREATE_ZONE, 4, 4, 1, 1); break;
            case N7_ZONE:        CallSystemFunction(SYS_ZONE, 0, 2, 1, 1); break;
            case N7_ZONEX:       CallSystemFunction(SYS_ZONE_X, 1, 1, 1, 1); break;
            case N7_ZONEY:       CallSystemFunction(SYS_ZONE_Y, 1, 1, 1, 1); break;
            case N7_ZONEW:       CallSystemFunction(SYS_ZONE_W, 1, 1, 1, 1); break;
            case N7_ZONEH:       CallSystemFunction(SYS_ZONE_H, 1, 1, 1, 1); break;
            case N7_INKEY:       CallSystemFunction(SYS_INKEY, 0, 0, 1, 1); break;
            case N7_KEYDOWN:     CallSystemFunction(SYS_KEY_DOWN, 1, 2, 1, 1); break;
            case N7_FWAIT:       CallSystemFunction(SYS_FRAME_SLEEP, 1, 1, 1, 1); break;
            case N7_IMAGE:       CallSystemFunction(SYS_IMAGE_EXISTS, 1, 1, 1, 1); break;
            case N7_WIDTH:       CallSystemFunction(SYS_IMAGE_WIDTH, 0, 1, 1, 1); break;
            case N7_HEIGHT:      CallSystemFunction(SYS_IMAGE_HEIGHT, 0, 1, 1, 1); break;
            case N7_COLS:        CallSystemFunction(SYS_IMAGE_COLS, 0, 1, 1, 1); break;
            case N7_ROWS:        CallSystemFunction(SYS_IMAGE_ROWS, 0, 1, 1, 1); break;
            case N7_CELLS:       CallSystemFunction(SYS_IMAGE_CELLS, 0, 1, 1, 1); break;
            case N7_PIXEL:       CallSystemFunction(SYS_GET_PIXEL, 2, 3, 1, 1); result.type = FACTOR_ARRAY; break;
            case N7_PIXELI:      CallSystemFunction(SYS_GET_PIXEL_INT, 2, 3, 1, 1); break;
            case N7_CREATEIMAGE: CallSystemFunction(SYS_CREATE_IMAGE, 2, 2, 1, 1); break;
            case N7_CREATEFONT:  CallSystemFunction(SYS_CREATE_FONT, 2, 6, 1, 1); break;
            case N7_FONT:        CallSystemFunction(SYS_FONT_EXISTS, 1, 1, 1, 1); break;
            case N7_FWIDTH:      CallSystemFunction(SYS_FONT_WIDTH, 1, 2, 1, 1); break;
            case N7_FHEIGHT:     CallSystemFunction(SYS_FONT_HEIGHT, 0, 1, 1, 1); break;
            case N7_CLIPBOARD:   CallSystemFunction(SYS_GET_CLIPBOARD, 0, 0, 1, 1); break;
            case N7_SOUND:       CallSystemFunction(SYS_SOUND_EXISTS, 1, 1, 1, 1); break;
            case N7_MUSIC:       CallSystemFunction(SYS_MUSIC_EXISTS, 1, 1, 1, 1); break;
            case N7_LOADSOUND:   CallSystemFunction(SYS_LOAD_SOUND, 1, 1, 1, 1); break;
            case N7_CREATESOUND: CallSystemFunction(SYS_CREATE_SOUND, 3, 3, 1, 1); break;
            case N7_LOADMUSIC:   CallSystemFunction(SYS_LOAD_MUSIC, 1, 1, 1, 1); break;
            case N7_LOADFONT:    CallSystemFunction(SYS_LOAD_FONT, 1, 1, 1, 1); break;
            case N7_DOWNLOAD:    CallSystemFunction(SYS_DOWNLOAD, 2, 2, 1, 1); break;
            case N7_LOADIMAGE:
                argc = CallSystemFunction(SYS_LOAD_IMAGE, 1, 3, 1, 1);
                if (!(argc == 1 || argc == 3)) ExpectChar(',');
                break;
            /* This, very special. */
            case N7_THIS:
                if (sLocalScope == 0) ErrorUnexpected();
                fprintf(sDstFile, "%s\n", ASM_MPUSH);
                fprintf(sDstFile, "%s .this\n", ASM_MLOAD);
                GetNext();
                result.type = FACTOR_NAME;
                break;
            /* Call C function. */
            case N7_CALL_FUNCTION:  CallCFunction(1, 1); break;
            default:
                ErrorUnexpected();
        }
    }
    /* Short for 'this', skip 'GetNext'. */
    else if (sNext == N7_CHAR && sChar == '.') {
        if (sLocalScope == 0) ErrorUnexpected();
        fprintf(sDstFile, "%s\n", ASM_MPUSH);
        fprintf(sDstFile, "%s .this\n", ASM_MLOAD);
        result.type = FACTOR_NAME;
    }
    /* |<expr>|, same as abs(<expr>). */
    else if (sNext == N7_CHAR && sChar == '|') {
        GetNext();
        Expression();
        ExpectChar('|');
        fprintf(sDstFile, "%s @0\n", ASM_ABS);
        result.type = FACTOR_VALUE;
    }
    else {
        Error("Expected expression");
    }
    
    return result;
}

/*
 * ParsePrecedenceLevelZero
 * ------------------------
 * Handle ., [ and (. This may look overly complex, but that's because I'm
 * chaining the operators instead of pushing and popping even more.
 */
static FactorInfo ParsePrecedenceLevelZero() {
    FactorInfo fi = Factor();
    int lastWasLoad = fi.type == FACTOR_NAME;

    if (sNext == N7_CHAR && (sChar == '.' || sChar == '[' || sChar == '(')) {
        /* Catch some obvious errors at compile time. */
        if (fi.type == FACTOR_VALUE) ErrorUnexpected();
        if (fi.type == FACTOR_TABLE && (sChar == '(' || sChar == '[')) ErrorUnexpected();
        if (fi.type == FACTOR_ARRAY && (sChar == '(' || sChar == '.')) ErrorUnexpected();
        if (fi.type == FACTOR_FUNCTION && sChar != '(') ErrorUnexpected();
        
        if (!lastWasLoad) fprintf(sDstFile, "%s\n", ASM_MPUSH);
        while (sNext == N7_CHAR && (sChar == '.' || sChar == '[' || sChar == '(')) {
            if (!lastWasLoad) {
                fprintf(sDstFile, "%s @0\n", ASM_PUSH);
                fprintf(sDstFile, "%s\n", ASM_MLOADS);
            }
            if (sChar == '.') {
                GetNext();
                if (sNext != N7_NAME) Error("Expected identifier");
                fprintf(sDstFile, "%s .%s\n", ASM_MLOAD, sName);
                if (!lastWasLoad) {
                    fprintf(sDstFile, "%s @1\n", ASM_POP);
                }
                GetNext();
                lastWasLoad = 1;
            }
            else if (sChar == '[') {
                GetNext();
                fprintf(sDstFile, "%s\n", ASM_MSWAP);
                Expression();
                ExpectChar(']');
                fprintf(sDstFile, "%s\n", ASM_MSWAP);
                fprintf(sDstFile, "%s @0\n", ASM_MLOAD);
                if (!lastWasLoad) {
                    fprintf(sDstFile, "%s @1\n", ASM_POP);
                }
                lastWasLoad = 1;
            }
            else {
                CallFunction((FunctionDefinition *)fi.data);
                fi.data = 0;
                if (!lastWasLoad) {
                    fprintf(sDstFile, "%s @1\n", ASM_POP);
                }
                lastWasLoad = 0;
            }
        }
        if (lastWasLoad) fprintf(sDstFile, "%s @0\n", ASM_MGET);
        fprintf(sDstFile, "%s\n", ASM_MPOP);
        fi.type = FACTOR_UNKNOWN;
    }
    else if (lastWasLoad) {
        fprintf(sDstFile, "%s @0\n", ASM_MGET);
        fprintf(sDstFile, "%s\n", ASM_MPOP);
        fi.type = FACTOR_UNKNOWN;
    }
    
    return fi;
}

static FactorInfo ParsePrecedenceLevelOne() {
    FactorInfo result = ParsePrecedenceLevelZero();
    
    while (sNext == N7_CHAR && sChar == '^') {
        result.type = FACTOR_VALUE;
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        GetNext();
        EatNewLines();
        ParsePrecedenceLevelZero();
        fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
        fprintf(sDstFile, "%s @0 @1\n", ASM_POW);
    }
    
    return result;
}

/*
 * ParsePrecedenceLevelOne
 * -----------------------
 * Handle *, / and %.
 */
static FactorInfo ParsePrecedenceLevelTwo() {   
    FactorInfo result = ParsePrecedenceLevelOne();
    
    while (sNext == N7_CHAR && (sChar == '*' || sChar == '/' || sChar == '%')) {
        result.type = FACTOR_VALUE;
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        if (sChar == '*') {
            GetNext();
            EatNewLines();
            ParsePrecedenceLevelOne();
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_MUL);*/
            fprintf(sDstFile, "%s\n", ASM_SPMUL);
        }
        else if (sChar == '/') {
            GetNext();
            EatNewLines();
            ParsePrecedenceLevelOne();
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_DIV);*/
            fprintf(sDstFile, "%s\n", ASM_SPDIV);
        }
        else if (sChar == '%') {
            GetNext();
            EatNewLines();
            ParsePrecedenceLevelOne();
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_MOD);*/
            fprintf(sDstFile, "%s\n", ASM_SPMOD);
        }
    }
    return result;
}

/*
 * ParsePrecedenceLevelTwo
 * -----------------------
 * Handle + and -.
 */
static FactorInfo ParsePrecedenceLevelThree() {
    FactorInfo result = ParsePrecedenceLevelTwo();
    while (sNext == N7_CHAR && (sChar == '+' || sChar == '-')) {
        result.type = FACTOR_VALUE;
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        if (sChar == '+') {
            GetNext();
            EatNewLines();
            ParsePrecedenceLevelTwo();
            /* Have to swap with stack, since strings have to be added in right order. */
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_ADD);*/
            fprintf(sDstFile, "%s\n", ASM_SPADD); 
        }
        else if (sChar == '-') {
            GetNext();
            EatNewLines();
            ParsePrecedenceLevelTwo();
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_SUB);*/
            fprintf(sDstFile, "%s\n", ASM_SPSUB);
        }
    }
    return result;
}

/*
 * ParsePrecedenceLevelThree
 * -------------------------
 * Handle =, >, >=, <, <= and <>.
 */
static FactorInfo ParsePrecedenceLevelFour() {
    int invert = 0;
    FactorInfo result;
    /* In n6, not had the lowest precedence and applied to entire expressions.
       Not quite sure if I */
    if (sNext == N7_KEYWORD && sKeyword == N7_NOT) {
        GetNext();
        invert = 1;
    }
    
    result = ParsePrecedenceLevelThree();
    while (sNext == N7_CHAR && (sChar == '=' || sChar == '>' || sChar == '<')) {
        result.type = FACTOR_VALUE;
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        if (sChar == '=') {
            GetNext();
            EatNewLines();
            ParsePrecedenceLevelThree();
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_EQL);*/
            fprintf(sDstFile, "%s\n", ASM_SPEQL);
        }
        else if (sChar == '>') {
            GetNext();
            if (sNext == N7_CHAR && sChar == '=') {
                GetNext();
                EatNewLines();
                ParsePrecedenceLevelThree();
                /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
                fprintf(sDstFile, "%s @0 @1\n", ASM_GEQL);*/
                fprintf(sDstFile, "%s\n", ASM_SPGEQL);
            }
            else {
                EatNewLines();
                ParsePrecedenceLevelThree();
                /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
                fprintf(sDstFile, "%s @0 @1\n", ASM_GRE);*/
                fprintf(sDstFile, "%s\n", ASM_SPGRE);
            }
        }
        else if (sChar == '<') {
            GetNext();
            if (sNext == N7_CHAR && sChar == '=') {
                GetNext();
                EatNewLines();
                ParsePrecedenceLevelThree();
                /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
                fprintf(sDstFile, "%s @0 @1\n", ASM_LEQL);*/
                fprintf(sDstFile, "%s\n", ASM_SPLEQL);
            }
            else if (sNext == N7_CHAR && sChar == '>') {
                GetNext();
                EatNewLines();
                ParsePrecedenceLevelThree();
                /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
                fprintf(sDstFile, "%s @0 @1\n", ASM_NEQL);*/
                fprintf(sDstFile, "%s\n", ASM_SPNEQL);
            }
            else {
                EatNewLines();
                ParsePrecedenceLevelThree();
                /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
                fprintf(sDstFile, "%s @0 @1\n", ASM_LESS);*/
                fprintf(sDstFile, "%s\n", ASM_SPLESS);
            }
        }
    }
    
    if (invert) {
        fprintf(sDstFile, "%s @0\n", ASM_NOT);
        result.type = FACTOR_VALUE;
    }
    return result;
}

/*
 * ParsePrecedenceLevelFour
 * ------------------------
 * Handle locical and.
 */
static FactorInfo ParsePrecedenceLevelFive() {
    int scLabelIndex = -1;
    
    FactorInfo result = ParsePrecedenceLevelFour();
    while (sNext == N7_KEYWORD && sKeyword == N7_AND) {
        result.type = FACTOR_VALUE;
        GetNext();
        EatNewLines();
        
        /* Short circuit evaluation. */
        if (scLabelIndex < 0) scLabelIndex = sLabelIndex++;
        /*fprintf(sDstFile, "%s @0\n", ASM_EVAL);
        fprintf(sDstFile, "%s and_%d_sc:\n", ASM_JMPF, scLabelIndex);*/
        fprintf(sDstFile, "%s @0 and_%d_sc:\n", ASM_JMPEF, scLabelIndex);
        
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        ParsePrecedenceLevelFour();
        /*fprintf(sDstFile, "%s @1\n", ASM_POP);
        fprintf(sDstFile, "%s @0 @1\n", ASM_AND);*/
        fprintf(sDstFile, "%s\n", ASM_PAND);
    }
    if (scLabelIndex >= 0) fprintf(sDstFile, "and_%d_sc:\n", scLabelIndex);
    return result;
}

/*
 * ParsePrecedenceLevelFive
 * ------------------------
 * Handle logical or.
 */
static FactorInfo ParsePrecedenceLevelSix() {
    int scLabelIndex = -1;    
    FactorInfo result = ParsePrecedenceLevelFive();
    while (sNext == N7_KEYWORD && sKeyword == N7_OR) {
        result.type = FACTOR_VALUE;
        GetNext();
        EatNewLines();
        
        /* Short circuit evaluation. */
        if (scLabelIndex < 0) scLabelIndex = sLabelIndex++;
        /*fprintf(sDstFile, "%s @0\n", ASM_EVAL);
        fprintf(sDstFile, "%s or_%d_sc:\n", ASM_JMPT, scLabelIndex);*/
        fprintf(sDstFile, "%s @0 or_%d_sc:\n", ASM_JMPET, scLabelIndex);
        
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        ParsePrecedenceLevelFive();
        /*fprintf(sDstFile, "%s @1\n", ASM_POP);
        fprintf(sDstFile, "%s @0 @1\n", ASM_OR);*/
        fprintf(sDstFile, "%s\n", ASM_POR);
    }
    if (scLabelIndex >= 0) fprintf(sDstFile, "or_%d_sc:\n", scLabelIndex);
    return result;
}

/*
 * Expresssion
 * -----------
 * Expression.
 */
static FactorInfo Expression() {
    /* In n6, the not operator was handled here, but it's been moved up. */
    return ParsePrecedenceLevelSix();
}

/*
 * ConstFactor
 * -----------
 * Constant factor, numbers and strings. Maybe you should allow other constants,
 * both built in and user defined?
 */
static void ConstFactor() {
    /* Unary + and -. */
    if (sNext == N7_CHAR && sChar == '+') {
        GetNext();
        EatNewLines();
        ConstFactor();
    }
    else if (sNext == N7_CHAR && sChar == '-') {
        GetNext();
        EatNewLines();
        ConstFactor();
        fprintf(sDstFile, "%s @0\n", ASM_NEG);
    }
    /* (<expr>) */
    else if (sNext == N7_CHAR && sChar == '(') {
        GetNext();
        ConstExpression();
        ExpectChar(')');
    }
    /* Numeric constant. */
    else if (sNext == N7_NUMBER) {
        fprintf(sDstFile, "%s @0 %s\n", ASM_MOVE, sNumberS);
        GetNext();
    }
    /* String contstant. */
    else if (sNext == N7_STRING) {
        fprintf(sDstFile, "%s @0 \"%s\"\n", ASM_MOVE, sString);
        GetNext();
    }
    /* |<expr>|, same as abs(<expr>). */
    else if (sNext == N7_CHAR && sChar == '|') {
        GetNext();
        ConstExpression();
        ExpectChar('|');
        fprintf(sDstFile, "%s @0\n", ASM_ABS);
    }
    
    else {
        Error("Invalid constant expression");
    }
}

/*
 * ParseConstPrecedenceLevelOne
 * ----------------------------
 * Handle *, / and %.
 */
static void ParseConstPrecedenceLevelOne() {
    ConstFactor();
    while (sNext == N7_CHAR && (sChar == '*' || sChar == '/' || sChar == '%')) {
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        if (sChar == '*') {
            GetNext();
            EatNewLines();
            ConstFactor();
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_MUL);*/
            fprintf(sDstFile, "%s\n", ASM_SPMUL);
        }
        else if (sChar == '/') {
            GetNext();
            EatNewLines();
            ConstFactor();
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_DIV);*/
            fprintf(sDstFile, "%s\n", ASM_SPDIV);
        }
        else if (sChar == '%') {
            GetNext();
            EatNewLines();
            ConstFactor();
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_MOD);*/
            fprintf(sDstFile, "%s\n", ASM_SPMOD);
        }
    }
}

/*
 * ParseConstPrecedenceLevelTwo
 * ----------------------------
 * Handle + and -.
 */
static void ParseConstPrecedenceLevelTwo() {
    ParseConstPrecedenceLevelOne();
    while (sNext == N7_CHAR && (sChar == '+' || sChar == '-')) {
        fprintf(sDstFile, "%s @0\n", ASM_PUSH);
        if (sChar == '+') {
            GetNext();
            EatNewLines();
            ParseConstPrecedenceLevelOne();
            /* Have to swap with stack, since strings have to be added in right order. */
            /*fprintf(sDstFile, "%s @0\n", ASM_SWAP);
            fprintf(sDstFile, "%s @1\n", ASM_POP);*/
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_ADD);*/
            fprintf(sDstFile, "%s\n", ASM_SPADD);
        }
        else if (sChar == '-') {
            GetNext();
            EatNewLines();
            ParseConstPrecedenceLevelOne();
            /*fprintf(sDstFile, "%s @0\n", ASM_SWAP);
            fprintf(sDstFile, "%s @1\n", ASM_POP);*/
            /*fprintf(sDstFile, "%s @0 @1\n", ASM_SPOP);
            fprintf(sDstFile, "%s @0 @1\n", ASM_SUB);*/
            fprintf(sDstFile, "%s\n", ASM_SPSUB);
        }
    }
}

/*
 * ConstExpression
 * ---------------
 * Only used when declaring constants, handles numeric and string
 * expressions. */
static void ConstExpression() {
    ParseConstPrecedenceLevelTwo();
}

/*
 * AddKeyword
 * ----------
 * Add keyword to table.
 */
static void AddKeyword(const char *key, Keyword keyword) {
    KeywordEntry *ke = (KeywordEntry *)malloc(sizeof(KeywordEntry));
    ke->keyword = keyword;
    ke->type = 0;
    HT_Add(sKeywords, key, 0, ke);
}

static void AddConstIntKeyword(const char *key, Keyword keyword, int value) {
    KeywordEntry *ke = (KeywordEntry *)malloc(sizeof(KeywordEntry));
    ke->keyword = keyword;
    ke->type = 1;
    ke->value.i = value;
    HT_Add(sKeywords, key, 0, ke);
}

static void AddConstFloatKeyword(const char *key, Keyword keyword, double value) {
    KeywordEntry *ke = (KeywordEntry *)malloc(sizeof(KeywordEntry));
    ke->keyword = keyword;
    ke->type = 2;
    ke->value.f = value;
    HT_Add(sKeywords, key, 0, ke);
}

static void AddConstStringKeyword(const char *key, Keyword keyword, const char *value) {
    KeywordEntry *ke = (KeywordEntry *)malloc(sizeof(KeywordEntry));
    ke->keyword = keyword;
    ke->type = 3;
    ke->value.s = value;
    HT_Add(sKeywords, key, 0, ke);
}

static void AddConstNullKeyword(const char *key, Keyword keyword) {
    KeywordEntry *ke = (KeywordEntry *)malloc(sizeof(KeywordEntry));
    ke->keyword = keyword;
    ke->type = 4;
    HT_Add(sKeywords, key, 0, ke);
}

/*
 * CreateKeywords
 * --------------
 * Put all keys in a table.
 */
static void CreateKeywords() {
    sKeywords = HT_Create(1);
    AddKeyword("end", N7_END);
    AddKeyword("gc", N7_GC);
    AddKeyword("assert", N7_ASSERT);
    AddKeyword("include", N7_INCLUDE);
    AddKeyword("asm", N7_ASM);
    AddKeyword("endasm", N7_ENDASM);
    AddKeyword("and", N7_AND);
    AddKeyword("or", N7_OR);
    AddKeyword("xor", N7_XOR);
    AddKeyword("not", N7_NOT);
    AddKeyword("if", N7_IF);
    AddKeyword("then", N7_THEN);
    AddKeyword("else", N7_ELSE);
    AddKeyword("elseif", N7_ELSEIF);
    AddKeyword("endif", N7_ENDIF);
    AddKeyword("select", N7_SELECT);
    AddKeyword("case", N7_CASE);
    AddKeyword("default", N7_DEFAULT);
    AddKeyword("endsel", N7_ENDSELECT);
    AddKeyword("while", N7_WHILE);
    AddKeyword("wend", N7_WEND);
    AddKeyword("do", N7_DO);
    AddKeyword("loop", N7_LOOP);
    AddKeyword("until", N7_UNTIL);
    AddKeyword("for", N7_FOR);
    AddKeyword("to", N7_TO);
    AddKeyword("step", N7_STEP);
    AddKeyword("next", N7_NEXT);
    AddKeyword("foreach", N7_FOREACH);
    AddKeyword("in", N7_IN);
    AddKeyword("break", N7_BREAK);
    AddKeyword("visible", N7_VISIBLE);
    AddKeyword("constant", N7_CONSTANT);
    /* Instruction level functions. */
    AddKeyword("str", N7_TOSTRING);
    AddKeyword("float", N7_TONUMBER);
    AddKeyword("int", N7_TOINTEGER);
    AddKeyword("sizeof", N7_SIZEOF);
    AddKeyword("len", N7_LEN);
    AddKeyword("free", N7_FREE);
    AddKeyword("dim", N7_DIM);
    AddKeyword("fill", N7_FILL);
    AddKeyword("copy", N7_COPY);
    AddKeyword("function", N7_FUNCTION);
    AddKeyword("endfunc", N7_ENDFUNC);
    AddKeyword("return", N7_RETURN);
    AddKeyword("typeof", N7_TYPEOF);
    AddKeyword("abs", N7_ABS);
    AddKeyword("sgn", N7_SGN);
    AddKeyword("cos", N7_COS);
    AddKeyword("sin", N7_SIN);
    AddKeyword("tan", N7_TAN);
    AddKeyword("acos", N7_ACOS);
    AddKeyword("asin", N7_ASIN);
    AddKeyword("atan", N7_ATAN);
    AddKeyword("atan2", N7_ATAN2);
    AddKeyword("sqr", N7_SQR);
    AddKeyword("pow", N7_POW);
    AddKeyword("floor", N7_FLOOR);
    AddKeyword("ceil", N7_CEIL);
    AddKeyword("round", N7_ROUND);
    AddKeyword("rad", N7_RAD);
    AddKeyword("deg", N7_DEG);
    AddKeyword("min", N7_MIN);
    AddKeyword("max", N7_MAX);
    /* Specials. */
    AddKeyword("this", N7_THIS);
    /* System commands. */
    AddKeyword("pln", N7_PLN);
    AddKeyword("rln", N7_RLN);
    AddKeyword("datetime", N7_DATETIME);
    AddKeyword("time", N7_TIME);
    AddKeyword("clock", N7_CLOCK);
    AddKeyword("wait", N7_WAIT);
    AddKeyword("fwait", N7_FWAIT);
    AddKeyword("rnd", N7_RND);
    AddKeyword("randomize", N7_RANDOMIZE);
    AddKeyword("system", N7_SYSTEM);
    AddKeyword("split", N7_SPLIT);
    AddKeyword("left", N7_LEFT);
    AddKeyword("right", N7_RIGHT);
    AddKeyword("mid", N7_MID);
    AddKeyword("instr", N7_INSTR);
    AddKeyword("replace", N7_REPLACE);
    AddKeyword("lower", N7_LOWER);
    AddKeyword("upper", N7_UPPER);
    AddKeyword("chr", N7_CHR);
    AddKeyword("asc", N7_ASC);
    AddKeyword("key", N7_KEY);
    AddKeyword("val", N7_VAL);
    AddKeyword("clear", N7_CLEAR);
    AddKeyword("insert", N7_INSERT);
    AddKeyword("keyof", N7_KEYOF);
    
    AddKeyword("set", N7_SET);
    AddKeyword("load", N7_LOAD);
    AddKeyword("save", N7_SAVE);
    AddKeyword("create", N7_CREATE);
    AddKeyword("open", N7_OPEN);
    
    AddKeyword("draw", N7_DRAW);
    AddKeyword("window", N7_WINDOW);
    AddKeyword("active", N7_ACTIVE);
    AddKeyword("redraw", N7_REDRAW);
    AddKeyword("screenw", N7_SCREENW);
    AddKeyword("screenh", N7_SCREENH);
    AddKeyword("mouse", N7_MOUSE);
    AddKeyword("mousex", N7_MOUSEX);
    AddKeyword("mousey", N7_MOUSEY);
    AddKeyword("mouserelx", N7_MOUSEDX);
    AddKeyword("mouserely", N7_MOUSEDY);
    AddKeyword("mousebutton", N7_MOUSEBUTTON);
    AddKeyword("joyx", N7_JOYX);
    AddKeyword("joyy", N7_JOYY);
    AddKeyword("joybutton", N7_JOYBUTTON);
    AddKeyword("zone", N7_ZONE);
    AddKeyword("createzone", N7_CREATEZONE);
    AddKeyword("zonex", N7_ZONEX);
    AddKeyword("zoney", N7_ZONEY);
    AddKeyword("zonew", N7_ZONEW);
    AddKeyword("zoneh", N7_ZONEH);
    AddKeyword("inkey", N7_INKEY);
    AddKeyword("keydown", N7_KEYDOWN);
    AddKeyword("color", N7_COLOR);
    AddKeyword("colori", N7_COLORI);
    AddKeyword("additive", N7_ADDITIVE);
    AddKeyword("clip", N7_CLIP);
    AddKeyword("pixel", N7_PIXEL);
    AddKeyword("pixeli", N7_PIXELI);
    AddKeyword("line", N7_LINE);
    AddKeyword("rect", N7_RECT);
    AddKeyword("ellipse", N7_ELLIPSE);
    AddKeyword("poly", N7_POLY);
    AddKeyword("vraster", N7_VRASTER);
    AddKeyword("hraster", N7_HRASTER);
    AddKeyword("cls", N7_CLS);
    AddKeyword("image", N7_IMAGE);
    AddKeyword("font", N7_FONT);
    AddKeyword("file", N7_FILE);
    AddKeyword("openfile", N7_OPENFILE);
    AddKeyword("createfile", N7_CREATEFILE);
    AddKeyword("openfiledialog", N7_OPENFILEDIALOG);
    AddKeyword("savefiledialog", N7_SAVEFILEDIALOG);
    AddKeyword("exists", N7_EXISTS);
    AddKeyword("fread", N7_FREAD);
    AddKeyword("freadc", N7_FREADC);
    AddKeyword("frln", N7_FRLN);
    AddKeyword("filetell", N7_FILETELL);
    AddKeyword("fileseek", N7_FILESEEK);
    AddKeyword("seek", N7_SEEK);

    AddKeyword("width", N7_WIDTH);
    AddKeyword("height", N7_HEIGHT);
    AddKeyword("cols", N7_COLS);
    AddKeyword("rows", N7_ROWS);
    AddKeyword("cels", N7_CELLS);
    AddKeyword("colorkey", N7_COLORKEY);
    AddKeyword("grid", N7_GRID);
    AddKeyword("loadimage", N7_LOADIMAGE);
    AddKeyword("createimage", N7_CREATEIMAGE);
    AddKeyword("loadfont", N7_LOADFONT);
    AddKeyword("fwidth", N7_FWIDTH);
    AddKeyword("fheight", N7_FHEIGHT);
    AddKeyword("write", N7_WRITE);
    AddKeyword("wln", N7_WLN);
    AddKeyword("justification", N7_JUSTIFICATION);
    AddKeyword("center", N7_CENTER);
    AddKeyword("caret", N7_CARET);
    AddKeyword("createfont", N7_CREATEFONT);
    AddKeyword("scroll", N7_SCROLL);
    AddKeyword("clipboard", N7_CLIPBOARD);
    AddKeyword("download", N7_DOWNLOAD);
    AddKeyword("console", N7_CONSOLE);
    AddKeyword("xform", N7_TRANSFORMED);
    
    AddKeyword("sound", N7_SOUND);
    AddKeyword("loadsound", N7_LOADSOUND);
    AddKeyword("createsound", N7_CREATESOUND);
    AddKeyword("music", N7_MUSIC);
    AddKeyword("loadmusic", N7_LOADMUSIC);
    AddKeyword("play", N7_PLAY);
    AddKeyword("stop", N7_STOP);
    AddKeyword("volume", N7_VOLUME);
    
    /* External C functions. */
    AddKeyword("LOAD_FUNCTION", N7_LOAD_FUNCTION);
    AddKeyword("CALL", N7_CALL_FUNCTION);
    
    /* Constants. */
    AddConstStringKeyword("VERSION", N7_VERSION, N7_VERSION_STRING);
    AddConstNullKeyword("unset", N7_UNSET);
    AddConstIntKeyword("true", N7_TRUE, 1);
    AddConstIntKeyword("false", N7_FALSE, 0);
    AddConstIntKeyword("on", N7_ON, 1);
    AddConstIntKeyword("off", N7_OFF, 0);
    AddConstIntKeyword("TYPE_NUMBER", N7_TYPE_NUMBER, VAR_NUM);
    AddConstIntKeyword("TYPE_STRING", N7_TYPE_STRING, VAR_STR);
    AddConstIntKeyword("TYPE_FUNCTION", N7_TYPE_FUNCTION, VAR_LBL);
    AddConstIntKeyword("TYPE_TABLE", N7_TYPE_TABLE, VAR_TBL);
    AddConstIntKeyword("TYPE_UNSET", N7_TYPE_UNSET, VAR_UNSET);
    AddConstIntKeyword("primary", N7_PRIMARY, SYS_PRIMARY_IMAGE);
    AddConstIntKeyword("SEEK_SET", N7_SEEK_SET, 0);
    AddConstIntKeyword("SEEK_CUR", N7_SEEK_CUR, 1);
    AddConstIntKeyword("SEEK_END", N7_SEEK_END, 2);
    AddConstFloatKeyword("PI", N7_PI, 3.141592653589);
    AddConstIntKeyword("KEY_TAB", N7_KEY_TAB, KC_TAB);
    AddConstIntKeyword("KEY_RETURN", N7_KEY_RETURN, KC_RETURN);
    AddConstIntKeyword("KEY_SHIFT", N7_KEY_SHIFT, KC_SHIFT);
    AddConstIntKeyword("KEY_CONTROL", N7_KEY_CONTROL, KC_CONTROL);
    AddConstIntKeyword("KEY_MENU", N7_KEY_MENU, KC_MENU);
    AddConstIntKeyword("KEY_ESCAPE", N7_KEY_ESCAPE, KC_ESCAPE);
    AddConstIntKeyword("KEY_SPACE", N7_KEY_SPACE, KC_SPACE);
    AddConstIntKeyword("KEY_PAGE_UP", N7_KEY_PAGE_UP, KC_PAGE_UP);
    AddConstIntKeyword("KEY_PAGE_DOWN", N7_KEY_PAGE_DOWN, KC_PAGE_DOWN);
    AddConstIntKeyword("KEY_END", N7_KEY_END, KC_END);
    AddConstIntKeyword("KEY_HOME", N7_KEY_HOME, KC_HOME);
    AddConstIntKeyword("KEY_LEFT", N7_KEY_LEFT, KC_LEFT);
    AddConstIntKeyword("KEY_UP", N7_KEY_UP, KC_UP);
    AddConstIntKeyword("KEY_RIGHT", N7_KEY_RIGHT, KC_RIGHT);
    AddConstIntKeyword("KEY_DOWN", N7_KEY_DOWN, KC_DOWN);
    AddConstIntKeyword("KEY_INSERT", N7_KEY_INSERT, KC_INSERT);
    AddConstIntKeyword("KEY_DELETE", N7_KEY_DELETE, KC_DELETE);
    AddConstIntKeyword("KEY_0", N7_KEY_0, KC_0);
    AddConstIntKeyword("KEY_1", N7_KEY_1, KC_1);
    AddConstIntKeyword("KEY_2", N7_KEY_2, KC_2);
    AddConstIntKeyword("KEY_3", N7_KEY_3, KC_3);
    AddConstIntKeyword("KEY_4", N7_KEY_4, KC_4);
    AddConstIntKeyword("KEY_5", N7_KEY_5, KC_5);
    AddConstIntKeyword("KEY_6", N7_KEY_6, KC_6);
    AddConstIntKeyword("KEY_7", N7_KEY_7, KC_7);
    AddConstIntKeyword("KEY_8", N7_KEY_8, KC_8);
    AddConstIntKeyword("KEY_9", N7_KEY_9, KC_9);
    AddConstIntKeyword("KEY_A", N7_KEY_A, KC_A);
    AddConstIntKeyword("KEY_B", N7_KEY_B, KC_B);
    AddConstIntKeyword("KEY_C", N7_KEY_C, KC_C);
    AddConstIntKeyword("KEY_D", N7_KEY_D, KC_D);
    AddConstIntKeyword("KEY_E", N7_KEY_E, KC_E);
    AddConstIntKeyword("KEY_F", N7_KEY_F, KC_F);
    AddConstIntKeyword("KEY_G", N7_KEY_G, KC_G);
    AddConstIntKeyword("KEY_H", N7_KEY_H, KC_H);
    AddConstIntKeyword("KEY_I", N7_KEY_I, KC_I);
    AddConstIntKeyword("KEY_J", N7_KEY_J, KC_J);
    AddConstIntKeyword("KEY_K", N7_KEY_K, KC_K);
    AddConstIntKeyword("KEY_L", N7_KEY_L, KC_L);
    AddConstIntKeyword("KEY_M", N7_KEY_M, KC_M);
    AddConstIntKeyword("KEY_N", N7_KEY_N, KC_N);
    AddConstIntKeyword("KEY_O", N7_KEY_O, KC_O);
    AddConstIntKeyword("KEY_P", N7_KEY_P, KC_P);
    AddConstIntKeyword("KEY_Q", N7_KEY_Q, KC_Q);
    AddConstIntKeyword("KEY_R", N7_KEY_R, KC_R);
    AddConstIntKeyword("KEY_S", N7_KEY_S, KC_S);
    AddConstIntKeyword("KEY_T", N7_KEY_T, KC_T);
    AddConstIntKeyword("KEY_U", N7_KEY_U, KC_U);
    AddConstIntKeyword("KEY_V", N7_KEY_V, KC_V);
    AddConstIntKeyword("KEY_W", N7_KEY_W, KC_W);
    AddConstIntKeyword("KEY_X", N7_KEY_X, KC_X);
    AddConstIntKeyword("KEY_Y", N7_KEY_Y, KC_Y);
    AddConstIntKeyword("KEY_Z", N7_KEY_Z, KC_Z);
    AddConstIntKeyword("KEY_MULTIPLY", N7_KEY_MULTIPLY, KC_MULTIPLY);
    AddConstIntKeyword("KEY_ADD", N7_KEY_ADD, KC_ADD);
    AddConstIntKeyword("KEY_SEPARATOR", N7_KEY_SEPARATOR, KC_SEPARATOR);
    AddConstIntKeyword("KEY_SUBTRACT", N7_KEY_SUBTRACT, KC_SUBTRACT);
    AddConstIntKeyword("KEY_DIVIDE", N7_KEY_DIVIDE, KC_DIVIDE);
    AddConstIntKeyword("KEY_F1", N7_KEY_F1, KC_F1);
    AddConstIntKeyword("KEY_F2", N7_KEY_F2, KC_F2);
    AddConstIntKeyword("KEY_F3", N7_KEY_F3, KC_F3);
    AddConstIntKeyword("KEY_F4", N7_KEY_F4, KC_F4);
    AddConstIntKeyword("KEY_F5", N7_KEY_F5, KC_F5);
    AddConstIntKeyword("KEY_F6", N7_KEY_F6, KC_F6);
    AddConstIntKeyword("KEY_F7", N7_KEY_F7, KC_F7);
    AddConstIntKeyword("KEY_F8", N7_KEY_F8, KC_F8);
    AddConstIntKeyword("KEY_F9", N7_KEY_F9, KC_F9);
    AddConstIntKeyword("KEY_F10", N7_KEY_F10, KC_F10);
    AddConstIntKeyword("KEY_F11", N7_KEY_F11, KC_F11);
    AddConstIntKeyword("KEY_F12", N7_KEY_F12, KC_F12);
}

static Keyword sKeywordSearch;
static char sFoundKeyword[64];

static void SearchKeyword(char *skey, int ikey, void *data, void *userData) {
    KeywordEntry *ke = (KeywordEntry *)data;
    if (ke->keyword == sKeywordSearch) {
        strcpy(sFoundKeyword, skey);
    }
}

static const char *GetKeywordString(Keyword keyword) {
    if (!sKeywords) return "";
    
    sKeywordSearch = keyword;
    strcpy(sFoundKeyword, "");
    HT_ApplyKeyFunction(sKeywords, SearchKeyword, 0);
    return sFoundKeyword;
}

/*
 * DeleteKeywordEntry
 * ------------------
 * Delete keyword entry.
 */
static void DeleteKeywordEntry(void *data) {
    KeywordEntry *ke = (KeywordEntry *)data;
    free(ke);
}

/*
 * DeleteKeywords
 * --------------
 * Delete keywords.
 */
static void DeleteKeywords() {
    if (sKeywords) {
        HT_Free(sKeywords, DeleteKeywordEntry);
        sKeywords = 0;
    }
}

/*
 * DeleteVisibleEntry
 * ------------------
 */
static void DeleteVisibleEntry(void *data) {
    VisibleEntry *vb = (VisibleEntry *)data;
    free(vb);
}

/*
 * DeleteVisible
 * -------------
 */
static void DeleteVisible() {
    if (sVisible) {
        HT_Free(sVisible, DeleteVisibleEntry);
        sVisible = 0;
    }
}

/*
 * DeleteConstants
 * ---------------
 */
static void DeleteConstants() {
    if (sConstants) {
        HT_Free(sConstants, DeleteVisibleEntry);
        sConstants = 0;
    }
}

/*
 * AddIncludeInfo
 * --------------
 */
int AddIncludeInfo(const char *filename) {
    IncludeInfo *info;
    
    if (strcmp(filename, sMainSrcFilename) == 0) return 0;
    
    info = sIncludeInfoList;
    while (info) {
        if (strcmp(info->filename, filename) == 0) {
            return 0;
        }
        info = info->next;
    }
    
    info = (IncludeInfo *)malloc(sizeof(IncludeInfo));
    info->filename = strdup(filename);
    info->next = sIncludeInfoList;
    sIncludeInfoList = info;
    
    return 1;
}

/*
 * ClearIncludeInfoList
 * --------------------
 */
void ClearIncludeInfoList() {
    while (sIncludeInfoList) {
        IncludeInfo *next = sIncludeInfoList->next;
        free(sIncludeInfoList->filename);
        free(sIncludeInfoList);
        sIncludeInfoList = next;
    }
}
