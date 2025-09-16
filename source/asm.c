/*
 * asm.c
 * -----
 * N7 assembler compiler. Compiles n7a text files to binary files that can be
 * executed by the vm (renv). 
 *
 * By: Marcus 2021
 */
 
#include "stdlib.h"
#include "stdio.h"
#include "ctype.h"
#include "string.h"
#include "math.h"

#include "asm.h"
#include "bytecodes.h"
#include "hash_table.h"
#include "renv.h"

/* Token types. */
#define ASM_NONE   0
#define ASM_REG    1
#define ASM_NUM    2
#define ASM_INT    3
#define ASM_STR    4
#define ASM_LBL    5
#define ASM_CMD    6
#define ASM_EOL    7
#define ASM_EOF    8
#define ASM_ERR    9
#define ASM_IGNORE 10

#define IS_PARAM(x) ((x) >= ASM_REG && (x) <= ASM_LBL)

#define INSTRUCTIONS_GROW_SIZE 1024
#define LINE_NUMBERS_GROW_SIZE 512
#define FILENAMES_GROW_SIZE    64

/*
 * InstructionDefinition
 * ---------------------
 * Each "assembler" command may translate into many different instructions
 * depending on the parameter types. For example, the command "mload" may be
 * used to load memory from a string, a number or the content of a register.
 *   "Why doesn't n7 just generate the right instruction in the first place?"
 * you might ask. And actually, I'm not quite sure. I believe that future
 * inline "assembler" commands was the original reason for doing it this way,
 * not forcing the programmer to remember if to use mload_s, mload_n or
 * mload_r.
 */ 
typedef struct _InstructionDefinition InstructionDefinition;
struct _InstructionDefinition {
    unsigned short instruction;   /* Bytecode for the instruction. */
    char lparam;                  /* Left parameter type. */
    char rparam;                  /* Right parameter type. */
    const char *name;             /* Name, for text output. */
    InstructionDefinition *next;  /* Next instructiondefinition for command. */
};
static HashTable *sInstructionDefinitions;
static InstructionDefinition *sInstructionDefinitionsA[BC_COUNT];

static void CreateInstructionDefinitions();
static void FreeInstructionDefinitions();

static FILE *sFile;

/* Token data. */
static int sNext;
/*unsigned char sCommand;*/
InstructionDefinition *sCommand; /* For ASM_CMD. */
static double sNumber;
static int sRegister;
static int sString;
static int sLabel;
static char sLabelName[64];
static char sError[256];

/* Instructions. */
Instruction *sInstructions;
int sInstructionsCapacity;
int sInstructionCount;
static void AddInstruction(Instruction instruction);

/* String constants. */
typedef struct {
    int index;
} StringEntry;
HashTable *sStrings;
char **sStringTable;
int sStringCount;
static int AddString(const char *str);
static const char *GetString(int index);
static void StringEntryToTable(char *skey, int ikey, void *data, void *userData);
static void DeleteStringEntry(void *data);

/* Labels are gathered and translated to instruction indexes for jumps. */
typedef struct {
    int index;
    int instruction;
    int originalInstruction; /* Added for correct subtractions during optimization stuff. */
} LabelEntry;
HashTable *sLabels;
int sLabelId;
static int AddLabel(const char *lbl);
static int FindLabel(void *data, void *userData);
static const char *GetLabel(int instruction);
static void LinkLabel(char *skey, int ikey, void *data, void *userData);
static void DeleteLabelEntry(void *data);

/* Meta data, for generating more valuable runtime errors. */
static LineNumberMetadata *sLineNumbers;
static int sLineNumbersCapacity;
static int sLineNumberCount;
static void AddLineNumberMetadata(int lineNumber);

static FilenameMetadata *sFilenames;
static int sFilenamesCapacity;
static int sFilenameCount;
static void AddFilenameMetadata(const char *filename);

static void OptimizeBytecode();
static void CleanUp();
static int GetNext(int wantEOL);

/*
 * ASM_Compile
 * -----------
 * Compile assembler file srcFilename to binary file dstFilename.
 */
int ASM_Compile(const char *srcFilename, const char *dstFilename, int optimize) {
    sFile = fopen(srcFilename, "r");
    FILE *out;
    
    strcpy(sError, "");
    
    if (sFile == 0) {
        sprintf(sError, "Error: Could not open file %s", srcFilename);
        return ASM_FAILURE;
    }
    
    CreateInstructionDefinitions();
      
    sStrings = HT_Create(1);
    sStringCount = 0;
    sStringTable = 0;
    
    sLabels = HT_Create(1);
    sLabelId = 1;
    
    sInstructions = 0;
    sInstructionsCapacity = 0;
    sInstructionCount = 0;

    sLineNumbers = 0;
    sLineNumbersCapacity = 0;
    sLineNumberCount = 0;
    
    sFilenames = 0;
    sFilenamesCapacity = 0;
    sFilenameCount = 0;
    
    sNext = GetNext(0);
 
    while (!(sNext == ASM_ERR || sNext == ASM_EOF)) {
        if (sNext == ASM_CMD) {
            Instruction instruction;
            InstructionDefinition *idef = sCommand;
            char lparam = ASM_NONE, rparam = ASM_NONE;
            const char *name = idef->name;
            
            sNext = GetNext(1);
            if (IS_PARAM(sNext)) {
                lparam = sNext;
                if (sNext == ASM_REG) instruction.lparam.i = sRegister;
                else if (sNext == ASM_NUM) instruction.lparam.d = sNumber;
                else if (sNext == ASM_STR) instruction.lparam.i = sString;
                else instruction.lparam.i = -sLabel;
                sNext = GetNext(1);
                if (IS_PARAM(sNext)) {
                    rparam = sNext;
                    if (sNext == ASM_REG) instruction.rparam.i = sRegister;
                    else if (sNext == ASM_NUM) instruction.rparam.d = sNumber;
                    else if (sNext == ASM_STR) instruction.rparam.i = sString;
                    else instruction.rparam.i = -sLabel;
                    sNext = GetNext(1);
                }
            }
            
            do {
                if ((idef->lparam == lparam || (idef->lparam == ASM_INT && lparam == ASM_NUM)) &&
                        (idef->rparam == rparam || (idef->rparam == ASM_INT && rparam == ASM_NUM))) {
                    break;
                }
                idef = idef->next;
            } while (idef);
            
            if (idef) {
                instruction.cmd = idef->instruction;
                if (idef->lparam == ASM_INT) instruction.lparam.i = (int)instruction.lparam.d;
                if (idef->rparam == ASM_INT) instruction.rparam.i = (int)instruction.rparam.d;
                if (idef->instruction == BC_CALL_R) instruction.rparam.i = 1;
                /*printf("add instruction: %s %d %d\n", name, lparam, rparam);*/
                AddInstruction(instruction);
                if (sNext == ASM_EOL) sNext = GetNext(0);
            }
            else {
                sprintf(sError, "Error: %s, invalid parameters", name);
                sNext = ASM_ERR;
            }
        }
        else if (sNext == ASM_LBL) {
            LabelEntry *lbl = (LabelEntry *)HT_Get(sLabels, sLabelName, 0);
            lbl->instruction = sInstructionCount;
            lbl->originalInstruction = sInstructionCount;
            sNext = GetNext(0);
        }
        else if (sNext == ASM_IGNORE) {
            sNext = GetNext(0);
        }
        else {
            strcpy(sError, "Error: Expected command");
            sNext = ASM_ERR;
            break;
        }
    }

    fclose(sFile);
    
    /* Error? */
    if (sNext == ASM_ERR) {
        CleanUp();
        return ASM_FAILURE;
    }
 
    /* Add BC_END instruction, since I always forget it. */
    Instruction end;
    end.cmd = BC_END;
    AddInstruction(end);

    /* Optimize bytecode. */
    if (optimize) OptimizeBytecode();
    
    /* Link instructions to all jumps and detect unset labels. */
    HT_ApplyKeyFunction(sLabels, LinkLabel, 0);
    if (sNext == ASM_ERR) {
        CleanUp();
        return ASM_FAILURE;
    }
    
    /* Output optimized asm. This is pretty slow. We're building an asm version
       of the generated bytecode. I only do this to see optimization results. */
    sFile = fopen(srcFilename, "w");
    if (sFile) {
        int lnIndex = 0;
        int fnIndex = 0;
        for (int i = 0; i < sInstructionCount; i++) {
            InstructionDefinition *idef;
            HashEntry *he;

            /* Filename metadata. */
            while (fnIndex < sFilenameCount && sFilenames[fnIndex].instructionIndex <= i) {
                fprintf(sFile, "/file:%s\n", sFilenames[fnIndex].filename);
                fnIndex++;
            }

            /* Line number metadata. */
            while (lnIndex < sLineNumberCount && sLineNumbers[lnIndex].instructionIndex <= i) {
                fprintf(sFile, "/line:%d\n", sLineNumbers[lnIndex].lineNumber);
                lnIndex++;
            }

            /* Label. */
            if ((he = HT_FindEntry(sLabels, FindLabel, &i))) fprintf(sFile, "%s:\n", he->skey);
            idef = sInstructionDefinitionsA[sInstructions[i].cmd];
            fprintf(sFile, "%s", idef->name);
            if (idef->lparam == ASM_REG) fprintf(sFile, " @%d", sInstructions[i].lparam.i);
            else if (idef->lparam == ASM_NUM) {
                if (sInstructions[i].lparam.d == trunc(sInstructions[i].lparam.d)) fprintf(sFile, " %d", (int)sInstructions[i].lparam.d);
                else fprintf(sFile, " %f", sInstructions[i].lparam.d);
            }
            else if (idef->lparam == ASM_INT) fprintf(sFile, " %d", sInstructions[i].lparam.i);
            else if (idef->lparam == ASM_STR) fprintf(sFile, " \"%s\"", GetString(sInstructions[i].lparam.i));
            else if (idef->lparam == ASM_LBL) fprintf(sFile, " %s:", GetLabel(sInstructions[i].lparam.i));
            if (idef->rparam == ASM_REG) fprintf(sFile, " @%d", sInstructions[i].rparam.i);
            else if (idef->rparam == ASM_NUM) {
                if (sInstructions[i].rparam.d == trunc(sInstructions[i].rparam.d)) fprintf(sFile, " %d", (int)sInstructions[i].rparam.d);
                else fprintf(sFile, " %f", sInstructions[i].rparam.d);
            }
            else if (idef->rparam == ASM_INT) fprintf(sFile, " %d", sInstructions[i].rparam.i);
            else if (idef->rparam == ASM_STR) fprintf(sFile, " \"%s\"", GetString(sInstructions[i].rparam.i));
            else if (idef->rparam == ASM_LBL) fprintf(sFile, " %s:", GetLabel(sInstructions[i].rparam.i));
            fprintf(sFile, "\n");
        }
        fclose(sFile);
    }
   
    /* Write to binary file. */
    out = fopen(dstFilename, "wb");
    
    /* Write line number metadata. */
    fwrite(&sLineNumberCount, sizeof(int), 1, out);
    if (sLineNumberCount > 0) fwrite(sLineNumbers, sizeof(LineNumberMetadata), sLineNumberCount, out);
    
    /* Write filename metadata. */
    fwrite(&sFilenameCount, sizeof(int), 1, out);
    if (sFilenameCount > 0) {
        for (int i = 0; i < sFilenameCount; i++) {
            int len = strlen(sFilenames[i].filename);
            fwrite(&sFilenames[i].instructionIndex, sizeof(int), 1, out);
            fwrite(&len, sizeof(int), 1, out);
            fwrite(sFilenames[i].filename, sizeof(char), strlen(sFilenames[i].filename), out);
            free(sFilenames[i].filename);
        }
    }
    
    /* Copy strings from hash table to array. */
    sStringTable = (char **)malloc(sizeof(char *)*sStringCount);
    HT_ApplyKeyFunction(sStrings, StringEntryToTable, 0);
 
    /* Write strings. */
    fwrite(&sStringCount, sizeof(int), 1, out);
    for (int i = 0; i < sStringCount; i++) {
        int len = strlen(sStringTable[i]);
        fwrite(&len, sizeof(int), 1, out);
        fwrite(sStringTable[i], sizeof(char), len, out);
    }
    
    /* Write instructions. */
    fwrite(&sInstructionCount, sizeof(int), 1, out);
    if (sInstructionCount) fwrite(sInstructions, sizeof(Instruction), sInstructionCount, out);
    
    fclose(out);

    CleanUp();

    return ASM_SUCCESS;
}


static void ModifyLabel(char *skey, int ikey, void *data, void *userData) {
    LabelEntry *le = (LabelEntry *)data;
    int *info = (int*)userData;
    if (le->instruction >= 0) {
        if (le->originalInstruction >= info[0]) le->instruction -= info[1]; 
    }
}


static void CorrectLineNumbers(LineNumberMetadata *dst, LineNumberMetadata *src, int count, int instructionIndex, int sub) {
    for (int i = 0; i < count; i++) {
        if (src[i].instructionIndex > instructionIndex) dst[i].instructionIndex -= sub;
    }
}

static void CorrectFilenames(FilenameMetadata *dst, FilenameMetadata *src, int count, int instructionIndex, int sub) {
    for (int i = 0; i < count; i++) {
        if (src[i].instructionIndex > instructionIndex) dst[i].instructionIndex -= sub;
    }
}

/*
 * OptimizeBytecode
 * ----------------
 */
static void OptimizeBytecode() {
    int readIndex = 0;
    int writeIndex = 0;
    LineNumberMetadata *newLineNumbers = (LineNumberMetadata *)malloc(sizeof(LineNumberMetadata)*sLineNumberCount);
    FilenameMetadata *newFilenames = (FilenameMetadata *)malloc(sizeof(FilenameMetadata)*sFilenameCount);
    memcpy(newLineNumbers, sLineNumbers, sizeof(LineNumberMetadata)*sLineNumberCount);
    memcpy(newFilenames, sFilenames, sizeof(FilenameMetadata)*sFilenameCount);
   
    for (readIndex = 0; readIndex < sInstructionCount; readIndex++) {
        int opt = 0;
        /* loading the value of a single global variable into register.
           NOTE: I believe a PUSH after this is a common case, pushing the
           content of a single variable to stack. x = v + 4, would load v
           as below and then push. */
        if (readIndex >= 4) {
            if (sInstructions[readIndex - 4].cmd == BC_MPUSH &&
                    sInstructions[readIndex - 3].cmd == BC_MLOAD &&
                    sInstructions[readIndex - 2].cmd == BC_MLOAD_S &&
                    sInstructions[readIndex - 1].cmd == BC_MGET_R &&
                    sInstructions[readIndex].cmd == BC_MPOP) {
                Instruction i;
                writeIndex -= 4;
                i.cmd = BC_OPT_LOADSINGLEVARG_R_S;
                i.lparam.i = sInstructions[readIndex - 1].lparam.i;
                i.rparam.i = sInstructions[readIndex - 2].lparam.i;
                sInstructions[writeIndex++] = i;
                opt = 4;
            }
        }
        if (!opt && readIndex >= 3) {
            /* Loading the value of a single variable into register.
               NOTE: Same as above. */
            if (sInstructions[readIndex - 3].cmd == BC_MPUSH &&
                    sInstructions[readIndex - 2].cmd == BC_MLOAD_S &&
                    sInstructions[readIndex - 1].cmd == BC_MGET_R &&
                    sInstructions[readIndex].cmd == BC_MPOP) {
                Instruction i;
                writeIndex -= 3;
                i.cmd = BC_OPT_LOADSINGLEVAR_R_S;
                i.lparam.i = sInstructions[readIndex - 1].lparam.i;
                i.rparam.i = sInstructions[readIndex - 2].lparam.i;
                sInstructions[writeIndex++] = i;
                opt = 3;
            }
        }
        if (!opt && readIndex >= 1) {
            /* Constants to stack.
               WARNING: This assumes that the constant is not NEEDED in the
               register, which it never is in n7 generated code. */
            if (sInstructions[readIndex - 1].cmd == BC_MOVE_R_N &&
                    sInstructions[readIndex].cmd == BC_PUSH_R) {
                Instruction i;
                writeIndex -= 1;
                i.cmd = BC_PUSH_N;
                i.lparam.d = sInstructions[readIndex - 1].rparam.d;
                sInstructions[writeIndex++] = i;
                opt = 1;
            }
            else if (sInstructions[readIndex - 1].cmd == BC_MOVE_R_S &&
                    sInstructions[readIndex].cmd == BC_PUSH_R) {
                Instruction i;
                writeIndex -= 1;
                i.cmd = BC_PUSH_S;
                i.lparam.i = sInstructions[readIndex - 1].rparam.i;
                sInstructions[writeIndex++] = i;
                opt = 1;
            }
            else if (sInstructions[readIndex - 1].cmd == BC_MOVE_R_L &&
                    sInstructions[readIndex].cmd == BC_PUSH_R) {
                Instruction i;
                writeIndex -= 1;
                i.cmd = BC_PUSH_L;
                i.lparam.i = sInstructions[readIndex - 1].rparam.i;
                sInstructions[writeIndex++] = i;
                opt = 1;
            }
        }
        if (opt) {
            int info[2];
            info[0] = readIndex;
            info[1] = opt;
            HT_ApplyKeyFunction(sLabels, ModifyLabel, info);
            CorrectLineNumbers(newLineNumbers, sLineNumbers, sLineNumberCount, readIndex, opt);
            CorrectFilenames(newFilenames, sFilenames, sFilenameCount, readIndex, opt);
        }
        else {
            /* Just some minors. */
            if (sInstructions[readIndex].cmd == BC_STR_R_R && sInstructions[readIndex].lparam.i == sInstructions[readIndex].rparam.i) {
                sInstructions[writeIndex] = sInstructions[readIndex];
                sInstructions[writeIndex++].cmd = BC_STR_R;
            }
            else if (sInstructions[readIndex].cmd == BC_NUM_R_R && sInstructions[readIndex].lparam.i == sInstructions[readIndex].rparam.i) {
                sInstructions[writeIndex] = sInstructions[readIndex];
                sInstructions[writeIndex++].cmd = BC_NUM_R;
            }
            else if (sInstructions[readIndex].cmd == BC_INT_R_R && sInstructions[readIndex].lparam.i == sInstructions[readIndex].rparam.i) {
                sInstructions[writeIndex] = sInstructions[readIndex];
                sInstructions[writeIndex++].cmd = BC_INT_R;
            }
            else {
                sInstructions[writeIndex++] = sInstructions[readIndex];
            }
        }        
    }
    sInstructionCount = writeIndex;
    free(sLineNumbers);
    free(sFilenames);
    sLineNumbers = newLineNumbers;
    sFilenames = newFilenames;
}

/*
 * CleanUp
 * -------
 * Clean up.
 */
static void CleanUp() {
    FreeInstructionDefinitions();
    
    if (sLineNumbers) {
        free(sLineNumbers);
        sLineNumbers = 0;
    }
    if (sFilenames) {
        free(sFilenames);
        sFilenames = 0;
    }
    if (sStringTable) {
        free(sStringTable);
        sStringTable = 0;
    }
    if (sStrings) {
        HT_Free(sStrings, DeleteStringEntry);
        sStrings = 0;
    }
    if (sLabels) {
        HT_Free(sLabels, DeleteLabelEntry);
        sLabels = 0;
    }
    if (sInstructions) {
        free(sInstructions);
        sInstructions = 0;
    }
}

/*
 * ASM_Error
 * ---------
 * Return error message.
 */
const char *ASM_Error() {
    return sError;
}

/*
 * StringEntryToTable
 * ------------------
 * Convert strings in hash table to table.
 */
static void StringEntryToTable(char *skey, int ikey, void *data, void *userData) {
    StringEntry *se = (StringEntry *)data;
    sStringTable[se->index] = skey;
}

/*
 * DeleteStringEntry
 * -----------------
 * Delete string entry.
 */
static void DeleteStringEntry(void *data) {
    StringEntry *se = (StringEntry *)data;
    free(se);
}

/*
 * LinkLabel
 * ---------
 * Link label indexes to instructions and check for undefined labels.
 */
static void LinkLabel(char *skey, int ikey, void *data, void *userData) {
    LabelEntry *le = (LabelEntry *)data;
    if (le->instruction < 0) {
        /* Reusing ... */
        if (sNext != ASM_ERR) {
            sNext = ASM_ERR;
            sprintf(sError, "Error: Missing label %s", skey);
        }
    }
    else {
        /* Set lparam for jumps to the instruction. */
        for (int i = 0; i < sInstructionCount; i++) {
            unsigned short cmd = sInstructions[i].cmd;
            if (cmd == BC_JMP_L || cmd == BC_JMPT_L || cmd == BC_JMPF_L ||
                cmd == BC_MSET_L || cmd == BC_PUSH_L) {
                if (sInstructions[i].lparam.i < 0 && le->index == -sInstructions[i].lparam.i) {
                    sInstructions[i].lparam.i = le->instruction;
                }
            }
            else if (cmd == BC_MOVE_R_L || cmd == BC_JMPET_R_L || cmd == BC_JMPEF_R_L) {
                if (sInstructions[i].rparam.i < 0 && le->index == -sInstructions[i].rparam.i) {
                   sInstructions[i].rparam.i = le->instruction;
                }
            }
        }
    }
}

/*
 * DeleteLabelEntry
 * ----------------
 * Delete label entry.
 */
void DeleteLabelEntry(void *data) {
    LabelEntry *le = (LabelEntry *)data;
    free(le);
}

/*
 * AddInstruction
 * --------------
 * Add instruction.
 */
void AddInstruction(Instruction instruction) {
    if (!sInstructions) {
        sInstructions = (Instruction *)malloc(sizeof(Instruction)*INSTRUCTIONS_GROW_SIZE);
        sInstructionsCapacity = INSTRUCTIONS_GROW_SIZE;
    }
    if (sInstructionCount >= sInstructionsCapacity) {
        sInstructionsCapacity += INSTRUCTIONS_GROW_SIZE;
        sInstructions = (Instruction *)realloc(sInstructions, sizeof(Instruction)*sInstructionsCapacity);
    }
    sInstructions[sInstructionCount++] = instruction;
}

/*
 * AddInstructionDefinition
 * ------------------------
 */
static void AddInstructionDefinition(const char *command, unsigned short instruction, char lparam, char rparam, const char *name) {
    InstructionDefinition *idef = (InstructionDefinition *)malloc(sizeof(InstructionDefinition));
    InstructionDefinition *list;
    
    idef->instruction = instruction;
    idef->lparam = lparam;
    idef->rparam = rparam;
    idef->name = name ? name : command;
    idef->next = 0;

    list = (InstructionDefinition *)HT_Get(sInstructionDefinitions, command, 0);
    if (!list) HT_Add(sInstructionDefinitions, command, 0, idef);
    else {
        while (list->next) list = list->next;
        list->next = idef;
    }
    
    sInstructionDefinitionsA[instruction] = idef;
}

/*
 * CreateInstructionDefinitions
 * ----------------------------
 * Define all valid instructions as combinations of commands and parameters.
 */
static void CreateInstructionDefinitions() {
    sInstructionDefinitions = HT_Create(128);

    /* Du kan plocka bort ASM_VAR och låta både . och " generera ASM_STR. */
    /* VÄNTA VÄNTA! Vissa instructioner vill ha asm_num, MEN de vill ha dem
       castade till int och placerade i param.i istf param.d. Så du måste
       alltså lägga till ASM_INT, som då ALDRIG skapas av parsern. En
       instruktion som förväntar sig en asm_int accepterar alltså asm_num
       men castar och placerar i param.i. Du måste dock gå igenom alla jäkla
       instruktioner och kolla upp detta. Leta efter (int)sNumber */
    AddInstructionDefinition(ASM_NOP, BC_NOP, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_END, BC_END, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MDUMP, BC_MDUMP, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_RDUMP, BC_RDUMP, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SDUMP, BC_SDUMP, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MADD, BC_MADD_S, ASM_STR, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MADD, BC_MADD_N, ASM_INT, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MADD, BC_MADD_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_OPT_MALS, BC_OPT_MALS_S, ASM_STR, ASM_NONE, 0);
    AddInstructionDefinition(ASM_OPT_MALS, BC_OPT_MALS_N, ASM_INT, ASM_NONE, 0);
    AddInstructionDefinition(ASM_OPT_MALS, BC_OPT_MALS_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MLOAD, BC_MLOAD, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_LOADPM, BC_MLOAD, ASM_NONE, ASM_NONE, 0); /* Don't ask. */
    AddInstructionDefinition(ASM_MLOAD, BC_MLOAD_S, ASM_STR, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MLOAD, BC_MLOAD_N, ASM_INT, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MLOAD, BC_MLOAD_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MLOADS, BC_MLOADS, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MSET, BC_MSET_S, ASM_STR, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MSET, BC_MSET_N, ASM_NUM, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MSET, BC_MSET_L, ASM_LBL, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MSET, BC_MSET_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_OPT_MSSP, BC_OPT_MSSP_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_LPTBL, BC_LPTBL_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MCLR, BC_MCLR, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MGET, BC_MGET_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MPUSH, BC_MPUSH, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MPOP, BC_MPOP, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MSWAP, BC_MSWAP, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_CLR, BC_CLR_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MOVE, BC_MOVE_R_S, ASM_REG, ASM_STR, 0);
    AddInstructionDefinition(ASM_MOVE, BC_MOVE_R_N, ASM_REG, ASM_NUM, 0);
    AddInstructionDefinition(ASM_MOVE, BC_MOVE_R_L, ASM_REG, ASM_LBL, 0);
    AddInstructionDefinition(ASM_MOVE, BC_MOVE_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_JMP, BC_JMP_L, ASM_LBL, ASM_NONE, 0);
    AddInstructionDefinition(ASM_EVAL, BC_EVAL_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ECMP, BC_ECMP_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_JMPT, BC_JMPT_L, ASM_LBL, ASM_NONE, 0);
    AddInstructionDefinition(ASM_JMPF, BC_JMPF_L, ASM_LBL, ASM_NONE, 0);
    AddInstructionDefinition(ASM_JMPET, BC_JMPET_R_L, ASM_REG, ASM_LBL, 0);
    AddInstructionDefinition(ASM_JMPEF, BC_JMPEF_R_L, ASM_REG, ASM_LBL, 0);
    AddInstructionDefinition(ASM_PUSH, BC_PUSH_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_PUSH, BC_PUSH_N, ASM_NUM, ASM_NONE, 0);
    AddInstructionDefinition(ASM_PUSH, BC_PUSH_S, ASM_STR, ASM_NONE, 0);
    AddInstructionDefinition(ASM_PUSH, BC_PUSH_L, ASM_LBL, ASM_NONE, 0);
    AddInstructionDefinition(ASM_POP, BC_POP_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SWAP, BC_SWAP_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPOP, BC_SPOP_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_OR, BC_OR_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_AND, BC_AND_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_POR, BC_POR, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_PAND, BC_PAND, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_EQL, BC_EQL_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_LESS, BC_LESS_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_GRE, BC_GRE_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_LEQL, BC_LEQL_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_GEQL, BC_GEQL_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_NEQL, BC_NEQL_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_SPEQL, BC_SPEQL, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPLESS, BC_SPLESS, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPGRE, BC_SPGRE, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPLEQL, BC_SPLEQL, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPGEQL, BC_SPGEQL, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPNEQL, BC_SPNEQL, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ADD, BC_ADD_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_SUB, BC_SUB_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_MUL, BC_MUL_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_DIV, BC_DIV_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_MOD, BC_MOD_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_SPADD, BC_SPADD, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPSUB, BC_SPSUB, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPMUL, BC_SPMUL, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPDIV, BC_SPDIV, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SPMOD, BC_SPMOD, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_NEG, BC_NEG_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_CTBL, BC_CTBL_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_TOSTR, BC_STR_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_TOSTR, BC_STR_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_TONUM, BC_NUM_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_TONUM, BC_NUM_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_TOINT, BC_INT_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_TOINT, BC_INT_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SIZE, BC_SIZE_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_LEN, BC_LEN_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_NOT, BC_NOT_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MDEL, BC_MDEL_S, ASM_STR, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MDEL, BC_MDEL_N, ASM_INT, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MDEL, BC_MDEL_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_LGC, BC_LGC, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ULGC, BC_ULGC, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_GC, BC_GC, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_CPY, BC_CPY_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_ASSERT, BC_ASSERT_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_RTE, BC_RTE_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_CALL, BC_CALL_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_RET, BC_RET, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_LOCAL, BC_LOCAL, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ILOAD, BC_ILOAD, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_IHAS, BC_IHAS, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_IVAL, BC_IVAL_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_IKEY, BC_IKEY_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_IPUSH, BC_IPUSH, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_IPOP, BC_IPOP, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ISTEP, BC_ISTEP, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_IDEL, BC_IDEL, ASM_NONE, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ABS, BC_ABS_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_COS, BC_COS_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SIN, BC_SIN_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_TAN, BC_TAN_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ACOS, BC_ACOS_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ASIN, BC_ASIN_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ATAN, BC_ATAN_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ATAN2, BC_ATAN2_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_SQR, BC_SQR_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_LOG, BC_LOG_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_SGN, BC_SGN_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_POW, BC_POW_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_FLOOR, BC_FLOOR_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_CEIL, BC_CEIL_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_ROUND, BC_ROUND_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_RAD, BC_RAD_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_DEG, BC_DEG_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_MIN, BC_MIN_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_MAX, BC_MAX_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_TYPE, BC_TYPE_R_R, ASM_REG, ASM_REG, 0);
    AddInstructionDefinition(ASM_SYS, BC_SYS_N_N, ASM_INT, ASM_INT, 0);
    AddInstructionDefinition(ASM_FLOAD, BC_FLOAD_R, ASM_REG, ASM_NONE, 0);
    AddInstructionDefinition(ASM_FCALL, BC_FCALL_N, ASM_INT, ASM_NONE, 0);
    AddInstructionDefinition(ASM_OPT_LOADSINGLEVAR, BC_OPT_LOADSINGLEVAR_R_S, ASM_REG, ASM_STR, 0);
    AddInstructionDefinition(ASM_OPT_LOADSINGLEVARG, BC_OPT_LOADSINGLEVARG_R_S, ASM_REG, ASM_STR, 0);
    AddInstructionDefinition(ASM_OPT_LOADPARAM, BC_OPT_LOADPARAM_S, ASM_STR, ASM_NONE, 0);
    AddInstructionDefinition(ASM_OPT_PVAL, BC_OPT_PVAL, ASM_INT, ASM_STR, 0);
}

/*
 * DeleteInstructionDefinition
 * ---------------------------
 * Callback.
 */
void DeleteInstructionDefinition(void *data) {
    InstructionDefinition *idef = (InstructionDefinition *)data;
    
    do {
        InstructionDefinition *next = idef->next;
        free(idef);
        idef = next;
    } while (idef);
}

/*
 * FreeInstructionDefinitions
 * --------------------------
 */
static void FreeInstructionDefinitions() {
    HT_Free(sInstructionDefinitions, DeleteInstructionDefinition);
}

/*
 * IsWhitespace
 * ------------
 * Return 1 if character is whitespace, else 0.
 */
int IsWhitespace(int c, int wantEOL) {
    return c == ' ' || c == '\t' || (c == '\n' && !wantEOL);
}

/*
 * GetNext
 * -------
 * Load next token and return its type.
 */
/* Need to rewrite this .. I think ... maybe not. */
int GetNext(int wantEOL) {
    int c = fgetc(sFile);
    while (IsWhitespace(c, wantEOL)) c = fgetc(sFile);
    
    /* Metadata. */
    if (c == '/') {
        char tag[8];
        int i = 0;
        c = fgetc(sFile);
        while (!(c == ':' || i > 6)) {
            tag[i++] = c;
            c = fgetc(sFile);
        }
        tag[i] = '\0';
        if (strcmp(tag, "line") == 0) {
            char line[9];
            int lineNumber;
            i = 0;
            c = fgetc(sFile);
            while (!(c == '\n' || c == EOF || i > 7)) {
                line[i++] = c;
                c = fgetc(sFile);
            }
            if (i > 7) {
                sprintf(sError, "%s, bad metadata", tag);
                return ASM_ERR;
            }
            else {
                line[i] = '\0';
                lineNumber = atoi(line);
                AddLineNumberMetadata(lineNumber);
            }
            return ASM_IGNORE;
        }
        else if (strcmp(tag, "file") == 0) {
            char line[512];
            i = 0;
            c = fgetc(sFile);
            while (!(c == '\n' || c == EOF)) {
                line[i++] = c;
                c = fgetc(sFile);
            }
            line[i] = '\0';
            AddFilenameMetadata(line);
            return ASM_IGNORE;
        }
        else {
            sprintf(sError, "Invalid metadata tag, %s", tag);
            return ASM_ERR;
        }
        
    }
    /* Register. */
    if (c == '@') {
        c = fgetc(sFile);
        if (isdigit(c)) {
            sRegister = c - 48;
            return ASM_REG;
        }
        else {
            strcpy(sError, "Error: Invalid register");
            return ASM_ERR;
        }
    }
    /* Table entry, variable. */
    else if (c == '.') {
        char varName[ASM_VAR_MAX_CHARS];
        char *s = varName;
        c = fgetc(sFile);        
        while (isalpha(c) || isdigit(c) || c == '_') {
            *s = (char)c;
            s++;
            c = fgetc(sFile);
        }
        ungetc(c, sFile);
        if (s == varName) {
            strcpy(sError, "Error: Invalid variable name");
            return ASM_ERR;
        }
        else {
            *s = '\0';
            /*sVariable = AddString(varName);
            return ASM_VAR;*/
            sString = AddString(varName);
            return ASM_STR;
        }
    }
    /* Number. */
    else if (isdigit(c)) {
        char num[64];
        char *s = num;
        int decimal = 0;
        while (isdigit(c) || (c == '.' && decimal++ == 0)) {
            *s = c;
            s++;
            c = fgetc(sFile);
        }
        ungetc(c, sFile);
        *s = '\0';
        sNumber = atof(num);
        
        return ASM_NUM;
    }
    /* String. */
    else if (c == '\"') {
        char txt[ASM_STRING_MAX_CHARS];
        char *s = txt;
        c = fgetc(sFile);
        while (c != '\"') {
            *s = (char)c;
            s++;
            c = fgetc(sFile);
        }
        *s = '\0';
        sString = AddString(txt);
        return ASM_STR;
    }
    /* Command or label. */
    else if (isalpha(c) || c == '_') {
        char cmd[64];
        char *s = cmd;
        while (isalpha(c) || isdigit(c) || c == '_') {
            *s = c;
            s++;
            c = fgetc(sFile);
        }
        *s = '\0';
        /* Label. */
        if (c == ':') {
            sLabel = AddLabel(cmd);
            strcpy(sLabelName, cmd);
            return ASM_LBL;
        }
        /* Command. */
        else {
            ungetc(c, sFile);
            if ((sCommand = (InstructionDefinition *)HT_Get(sInstructionDefinitions, cmd, 0))) {
                return ASM_CMD;
            }
            else {
                sprintf(sError, "Error: %s, unknown command", cmd);
                return ASM_ERR;
            }
        }
    }
    else if (c == '\n') {
        return ASM_EOL;
    }
    else if (c == EOF) {
        return ASM_EOF;
    }
    else {
        sprintf(sError, "Error: Unexpected character %c", c);
        return ASM_ERR;
    }
}

/*
 * AddString
 * ---------
 * Add string str to hash table if it's not already present and return its
 * index.
 */
int AddString(const char *str) {
    StringEntry *entry = (StringEntry *)HT_Get(sStrings, str, 0);
    if (entry) return entry->index;
    
    entry = (StringEntry *)malloc(sizeof(StringEntry));
    entry->index = sStringCount++;
    HT_Add(sStrings, str, 0, entry);
    
    return entry->index;
}

/*
 * FindString
 * ----------
 * Callback.
 */
static int FindString(void *data, void *userData) {
    StringEntry *se = (StringEntry *)data;
    int index = *((int *)userData);
    return se->index == index;
}

/*
 * GetString
 * ---------
 * Returns string from index.
 */
static const char *GetString(int index) {
    HashEntry *he = HT_FindEntry(sStrings, FindString, &index);
    if (he) return he->skey;
    else return "<unknown string>";
}

/* 
 * AddLabel
 * --------
 * Add label lbl to hash table if it's not already present and return its
 * index.
 */
int AddLabel(const char *lbl) {
    LabelEntry *entry = (LabelEntry *)HT_Get(sLabels, lbl, 0);
    if (entry) return entry->index;
    
    entry = (LabelEntry *)malloc(sizeof(LabelEntry));
    entry->index = sLabelId++;
    entry->instruction = -1;
    HT_Add(sLabels, lbl, 0, entry);
   
    return entry->index;
}

/*
 * FindLabel
 * ---------
 * Callback.
 */
static int FindLabel(void *data, void *userData) {
    LabelEntry *le = (LabelEntry *)data;
    int instruction = *((int *)userData);
    return le->instruction == instruction;
}

/*
 * GetLabel
 * --------
 * Returns label from index.
 */
static const char *GetLabel(int instruction) {
    HashEntry *he = HT_FindEntry(sLabels, FindLabel, &instruction);
    if (he) return he->skey;
    else {
        return "<unknown label>";
    }
}


void AddLineNumberMetadata(int lineNumber) {
    /*LineNumberMetadata lnmd;
    lnmd.instructionIndex = sInstructionCount;
    lnmd.lineNumber = lineNumber;
    if (!sLineNumbers) {
        sLineNumbers = (LineNumberMetadata *)malloc(sizeof(LineNumberMetadata)*LINE_NUMBERS_GROW_SIZE);
        sLineNumbersCapacity = LINE_NUMBERS_GROW_SIZE;
    }
    if (sLineNumberCount >= sLineNumbersCapacity) {
        sLineNumbersCapacity += LINE_NUMBERS_GROW_SIZE;
        sLineNumbers = (LineNumberMetadata *)realloc(sLineNumbers, sizeof(LineNumberMetadata)*sLineNumbersCapacity);
    }
    sLineNumbers[sLineNumberCount++] = lnmd;*/
    if (sLineNumberCount > 0 && sLineNumbers[sLineNumberCount - 1].instructionIndex == sInstructionCount) {
        sLineNumbers[sLineNumberCount - 1].lineNumber = lineNumber;
    }
    else {
        LineNumberMetadata lnmd;
        lnmd.instructionIndex = sInstructionCount;
        lnmd.lineNumber = lineNumber;
        if (!sLineNumbers) {
            sLineNumbers = (LineNumberMetadata *)malloc(sizeof(LineNumberMetadata)*LINE_NUMBERS_GROW_SIZE);
            sLineNumbersCapacity = LINE_NUMBERS_GROW_SIZE;
        }
        if (sLineNumberCount >= sLineNumbersCapacity) {
            sLineNumbersCapacity += LINE_NUMBERS_GROW_SIZE;
            sLineNumbers = (LineNumberMetadata *)realloc(sLineNumbers, sizeof(LineNumberMetadata)*sLineNumbersCapacity);
        }
        sLineNumbers[sLineNumberCount++] = lnmd;
    }
}

void AddFilenameMetadata(const char *filename) {
    FilenameMetadata fnmd;
    fnmd.instructionIndex = sInstructionCount;
    fnmd.filename = strdup(filename);
    if (!sFilenames) {
        sFilenames = (FilenameMetadata *)malloc(sizeof(FilenameMetadata)*FILENAMES_GROW_SIZE);
        sFilenamesCapacity = FILENAMES_GROW_SIZE;
    }
    if (sFilenameCount >= sFilenamesCapacity) {
        sFilenamesCapacity += FILENAMES_GROW_SIZE;
        sFilenames = (FilenameMetadata *)realloc(sFilenames, sizeof(FilenameMetadata)*sFilenamesCapacity);
    }
    sFilenames[sFilenameCount++] = fnmd;
}
