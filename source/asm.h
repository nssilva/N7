/*
 * asm.h
 * -----
 * N7 assembler compiler.
 *
 * By: Marcus 2021
 */
 
#ifndef __N7_ASM_H__
#define __N7_ASM_H__
  
/* ASM_Compile return codes. */
#define ASM_SUCCESS 0
#define ASM_FAILURE 1
 
#define ASM_VAR_MAX_CHARS 64
#define ASM_STRING_MAX_CHARS 512
 
/* Commands, each command may translate into different instructions depending on
   the parameters. */
#define ASM_NOP     "nop"
#define ASM_END     "end"
#define ASM_ASSERT  "assert"
#define ASM_MDUMP   "mdump"
#define ASM_RDUMP   "rdump"
#define ASM_SDUMP   "sdump"
#define ASM_MADD    "madd"
#define ASM_MLOAD   "mload"
#define ASM_MLOADS  "mloads"
#define ASM_MSET    "mset"
#define ASM_LPTBL   "lptbl"
#define ASM_MGET    "mget"
#define ASM_MPUSH   "mpush"      
#define ASM_MPOP    "mpop"
#define ASM_MSWAP   "mswap"
#define ASM_MOVE    "move"
#define ASM_JMP     "jmp"
#define ASM_EVAL    "eval"
#define ASM_JMPT    "jmpt"
#define ASM_JMPF    "jmpf"
#define ASM_JMPET   "jmpet"
#define ASM_JMPEF   "jmpef"
#define ASM_PUSH    "push"
#define ASM_POP     "pop"
#define ASM_SWAP    "swap"
#define ASM_SPOP    "spop"  /* SWAP WITH STACK and POP, optimization. */
#define ASM_SPADD   "spadd" /* SWAP WITH STACK, POP and ADD etc, further optimization. Always @0 @1, so ... skip parameters? */
#define ASM_SPSUB   "spsub"
#define ASM_SPMUL   "spmul"
#define ASM_SPDIV   "spdiv"
#define ASM_SPMOD   "spmod"
#define ASM_SPEQL   "speql"
#define ASM_SPLESS  "spless"
#define ASM_SPGRE   "spgre"
#define ASM_SPLEQL  "spleql"
#define ASM_SPGEQL  "spgeql"
#define ASM_SPNEQL  "spneql"
#define ASM_OR      "or"
#define ASM_POR     "por"  /* OR with popped. */
#define ASM_AND     "and"
#define ASM_PAND    "pand" /* AND with popped. */
#define ASM_NOT     "not"
#define ASM_EQL     "eql"
#define ASM_LESS    "less"
#define ASM_GRE     "gre"
#define ASM_LEQL    "leql"
#define ASM_GEQL    "geql"
#define ASM_NEQL    "neql"
#define ASM_ADD     "add"
#define ASM_SUB     "sub"
#define ASM_MUL     "mul"
#define ASM_DIV     "div"
#define ASM_MOD     "mod"
#define ASM_NEG     "neg"
#define ASM_CTBL    "ctbl"
#define ASM_TOSTR   "str"
#define ASM_TONUM   "num"
#define ASM_TOINT   "int"
#define ASM_ABS     "abs"
#define ASM_MDEL    "mdel"
#define ASM_GC      "gc"
#define ASM_LGC     "lgc"
#define ASM_ULGC    "ulgc"
#define ASM_CPY     "cpy"
#define ASM_ECMP    "ecmp"
#define ASM_RTE     "rte"
#define ASM_CLR     "clr"
#define ASM_CALL    "call"
#define ASM_RET     "ret"
#define ASM_LOCAL   "local"
#define ASM_MCLR    "mclr"
#define ASM_LOADPM  "loadpm"
#define ASM_SIZE    "size"
#define ASM_LEN     "len"
#define ASM_ILOAD   "iload"
#define ASM_IHAS    "ihas"
#define ASM_IVAL    "ival"
#define ASM_IKEY    "ikey"
#define ASM_IPUSH   "ipush"
#define ASM_IPOP    "ipop"
#define ASM_ISTEP   "istep"
#define ASM_IDEL    "idel"
#define ASM_COS     "cos"
#define ASM_SIN     "sin"
#define ASM_TAN     "tan"
#define ASM_ACOS    "acos"
#define ASM_ASIN    "asin"
#define ASM_ATAN    "atan"
#define ASM_ATAN2   "atan2"
#define ASM_SQR     "sqr"
#define ASM_LOG     "log"
#define ASM_SGN     "sgn"
#define ASM_POW     "pow"
#define ASM_FLOOR   "floor"
#define ASM_CEIL    "ceil"
#define ASM_ROUND   "round"
#define ASM_RAD     "rad"
#define ASM_DEG     "deg"
#define ASM_MIN     "min"
#define ASM_MAX     "max"

#define ASM_TYPE    "type"
#define ASM_SYS     "sys"

#define ASM_FLOAD   "fload"
#define ASM_FCALL   "fcall"

#define ASM_OPT_MALS           "opt_mals"           /* Optimization: Memory Add, Load and Swap. */
#define ASM_OPT_MSSP           "opt_mssp"           /* Optimization: Memory Swap, Set and Pop. */
#define ASM_OPT_LOADPARAM      "opt_loadparam"      /* Optimization: Memory Add, Push, Load, Stack Pop, Memory Set, Pop. */
#define ASM_OPT_LOADSINGLEVAR  "opt_loadsinglevar"  /* Optimization: Load single variable from current memory. */
#define ASM_OPT_LOADSINGLEVARG "opt_loadsinglevarg" /* Optimization: Load single variable from global (program) memory. */
#define ASM_OPT_PVAL           "opt_pval"           /* Optimization: Validates function parameter count. */

int ASM_Compile(const char *srcFilename, const char *dstFilename, int optimize);
const char *ASM_Error();
 
#endif