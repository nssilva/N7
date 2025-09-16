/*
 * bytecodes.h
 * -----------
 * Bytecode instructions.
 *
 * By: Marcus 2021
 */

#ifndef __BYTECODES_H__
#define __BYTECODES_H__

/* The order of the instructions is pretty random. */

enum {
    BC_NOP = 0,
    BC_END,
    BC_ASSERT_R_R,
    BC_RTE_R,
    
    BC_MDUMP,
    BC_RDUMP,
    BC_SDUMP,

    BC_MADD_S,
    BC_MADD_N,
    BC_MADD_R,
    
    /* Optimization for assignments: MADD + MLOAD + MSWAP */
    BC_OPT_MALS_S,
    BC_OPT_MALS_N,
    BC_OPT_MALS_R,

    BC_MLOAD_S,
    BC_MLOAD_N,
    BC_MLOAD_R,
    BC_MLOAD,   /* Loads program memory. */
    BC_MLOADS,  /* Loads memory from variable on top of stack. */

    BC_MSET_S,
    BC_MSET_N,
    BC_MSET_L,
    BC_MSET_R,
    BC_MCLR,
    
    /* Optimization for assigments: MSWAP + MSET + MPOP */
    BC_OPT_MSSP_R,

    BC_MGET_R,

    BC_MPUSH,
    BC_MPOP,
    BC_MSWAP,

    BC_CLR_R,
    BC_MOVE_R_S,
    BC_MOVE_R_N,
    BC_MOVE_R_L,
    BC_MOVE_R_R,

    BC_JMP_L,
    BC_EVAL_R,   /* Evaluate register. */
    BC_ECMP_R_R, /* Compare registers. */
    BC_JMPT_L,   /* Jump if true. */
    BC_JMPF_L,   /* Jump if false. */
    
    BC_JMPET_R_L, /* Evaluate register, jump if true. */
    BC_JMPEF_R_L, /* Evaluate register, jump if false. */

    BC_PUSH_R,
    BC_PUSH_N,
    BC_PUSH_S,
    BC_PUSH_L,
    BC_POP_R,
    BC_SWAP_R,
    BC_SPOP_R_R,
    
    BC_OR_R_R,
    BC_AND_R_R,

    /* OR, AND with popped value and @0. */
    BC_POR,
    BC_PAND,
    
    BC_EQL_R_R,
    BC_LESS_R_R,
    BC_GRE_R_R,
    BC_LEQL_R_R,
    BC_GEQL_R_R,
    BC_NEQL_R_R,

    /* Optimization commands, stack swap, pop and op, always @0. */
    BC_SPEQL,
    BC_SPLESS,
    BC_SPGRE,
    BC_SPLEQL,
    BC_SPGEQL,
    BC_SPNEQL,

    BC_ADD_R_R,
    BC_SUB_R_R,
    BC_MUL_R_R,
    BC_DIV_R_R,
    BC_MOD_R_R,

    /* Optimization commands, stack swap, pop and op, always @0. */
    BC_SPADD,
    BC_SPSUB,
    BC_SPMUL,
    BC_SPDIV,
    BC_SPMOD,

    BC_NEG_R,

    BC_CTBL_R,
    BC_LPTBL_R,

    BC_STR_R_R,
    BC_STR_R,
    BC_NUM_R_R,
    BC_NUM_R,
    BC_INT_R_R,
    BC_INT_R,
    BC_SIZE_R_R,
    BC_LEN_R_R,

    BC_NOT_R,

    BC_MDEL_S,
    BC_MDEL_N,
    BC_MDEL_R,

    BC_LGC,
    BC_ULGC,
    BC_GC,

    BC_CPY_R_R,

    BC_CALL_R,   /* 210707, modified to create memory outside of aloctable. */
    BC_RET,      /*         modified to release the memory. */
    BC_LOCAL,    /*         added to load the memory. */
    BC_OPT_PVAL, /* Optimization: parameter count check with possible rte. */
    
    BC_ILOAD,
    BC_IHAS,
    BC_IVAL_R,
    BC_IKEY_R,
    BC_IPUSH,
    BC_IPOP,
    BC_ISTEP,
    BC_IDEL,
    
    /* Result is always put in the same/left register. */
    BC_ABS_R,
    BC_COS_R,
    BC_SIN_R,
    BC_TAN_R,
    BC_ACOS_R,
    BC_ASIN_R,
    BC_ATAN_R,
    BC_ATAN2_R_R,
    BC_LOG_R,
    BC_SGN_R,
    BC_SQR_R,
    BC_POW_R_R,
    BC_FLOOR_R,
    BC_CEIL_R,
    BC_ROUND_R,
    BC_RAD_R,
    BC_DEG_R,
    BC_MIN_R_R,
    BC_MAX_R_R,
    
    BC_TYPE_R_R,
    
    /* Call system function id, parameter count. */
    BC_SYS_N_N,
    
    /* Load index for external C function with its name (string) in R. */
    BC_FLOAD_R,
    /* Call external C function by its index. */
    BC_FCALL_N,
    
    /* Post processing optimizations. */
    BC_OPT_LOADSINGLEVAR_R_S,
    BC_OPT_LOADSINGLEVARG_R_S,
    BC_OPT_LOADPARAM_S,
    
    BC_COUNT
};

#endif