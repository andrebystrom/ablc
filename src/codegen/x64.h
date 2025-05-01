#ifndef X64_H
#define X64_H

#include "../data/abc_arr.h"
#include "../data/abc_pool.h"
#include "ir.h"

#define X64_VAR_SIZE 8
#define X64_STACK_PARAM_OFFSET 24
#define X64_FORMAT_STR_LABEL "format_string"

enum x64_reg {
    X64_REG_RAX, // retval
    X64_REG_RBX,
    X64_REG_RCX, // ARG 4
    X64_REG_RDX, // ARG 3
    X64_REG_RSI, // ARG 2
    X64_REG_RDI, // ARG 1
    X64_REG_RSP, // stack pointer, callee saved
    X64_REG_RBP, // base pointer callee saved
    X64_REG_R8, // ARG 5
    X64_REG_R9, // ARG 6
    X64_REG_R10,
    X64_REG_R11,
    X64_REG_R12, // callee saved
    X64_REG_R13, // callee saved
    X64_REG_R14, // callee saved
    X64_REG_R15, // callee saved
};

enum x64_cc {
    X64_CC_E,
    X64_CC_NE,
    X64_CC_L,
    X64_CC_LE,
    X64_CC_G,
    X64_CC_GE,
};

enum x64_arg_tag {
    X64_ARG_STR, // used as intermediate
    X64_ARG_REG,
    X64_ARG_IMM,
    X64_ARG_DEREF,
};

struct x64_arg_str {
    char *str;
};

struct x64_arg_reg {
    enum x64_reg reg;
};

struct x64_arg_imm {
    long imm;
};

struct x64_arg_deref {
    long offset;
    enum x64_reg reg;
};

struct x64_arg {
    enum x64_arg_tag tag;
    union {
        struct x64_arg_str str;
        struct x64_arg_reg reg;
        struct x64_arg_imm imm;
        struct x64_arg_deref deref;
    } val;
};

enum x64_instr_tag {
    X64_INSTR_ADDQ,
    X64_INSTR_SUBQ,
    X64_INSTR_IMULQ,
    X64_INSTR_IDIVQ,

    X64_INSTR_MOVQ,
    X64_INSTR_PUSHQ,
    X64_INSTR_POPQ,
    X64_INSTR_LEAQ,

    X64_INSTR_NEGQ, // arithmetic negate
    X64_INSTR_NOTQ, // logical negate
    X64_INSTR_JMP,
    X64_INSTR_CMPQ,
    X64_INSTR_JMPCC,

    X64_INSTR_CALLQ,
    X64_INSTR_RETQ,
    X64_INSTR_LEAVEQ,
};

struct x64_instr_addq {
    struct x64_arg src;
    struct x64_arg dst;
};

struct x64_instr_subq {
    struct x64_arg src;
    struct x64_arg dst;
};

struct x64_instr_imulq {
    struct x64_arg mul;
};

struct x64_instr_idivq {
    struct x64_arg div;
};

struct x64_instr_movq {
    struct x64_arg src;
    struct x64_arg dst;
};

struct x64_instr_pushq {
    struct x64_arg src;
};

struct x64_instr_popq {
    struct x64_arg dest;
};

struct x64_instr_leaq {
    char *label; // relative to rip
    struct x64_arg dest;
};

struct x64_instr_negq {
    struct x64_arg dest;
};

struct x64_instr_notq {
    struct x64_arg dest;
};

struct x64_instr_jmp {
    char *label;
};

struct x64_instr_cmpq {
    struct x64_arg left;
    struct x64_arg right;
};

struct x64_instr_jmpcc {
    char *label;
    enum x64_cc code;
};

struct x64_instr_callq {
    char *label;
};

struct x64_instr_retq {
    int dummy;
};

struct x64_instr_leaveq {
    int dummy;
};

struct x64_instr {
    enum x64_instr_tag tag;
    union {
        struct x64_instr_addq add;
        struct x64_instr_subq sub;
        struct x64_instr_imulq imul;
        struct x64_instr_idivq idiv;

        struct x64_instr_movq mov;
        struct x64_instr_pushq push;
        struct x64_instr_popq pop;
        struct x64_instr_leaq leaq;

        struct x64_instr_negq neg;
        struct x64_instr_notq not;
        struct x64_instr_jmp jmp;
        struct x64_instr_cmpq cmp;
        struct x64_instr_jmpcc jmpcc;

        struct x64_instr_callq callq;
        struct x64_instr_retq ret;
        struct x64_instr_leaveq leave;
    } val;
};

struct x64_block {
    char *label;
    struct abc_arr x64_instrs; // x64_instr
};

struct x64_fun {
    char *label;
    struct abc_arr x64_blocks; // x64_block
};

struct x64_program {
    struct abc_arr x64_funs; // x64_fun
};

/* TRANSLATION */

// used for variables and parameters to a function
struct x64_var_spec {
    char *label;
    int offset; // rbp offset, positive indicates that it is an argument passed through the stack
};

struct x64_translator {
    struct abc_pool *pool;
    struct x64_fun *curr_fun;
    struct x64_block *curr_block;
    struct abc_arr var_specs; // x64_var_spec
    int num_stacked;
};

void x64_translator_init(struct x64_translator *t);
void x64_translator_destroy(struct x64_translator *t);

struct x64_program x64_translate(struct x64_translator *t, struct ir_program *program);

void x64_program_print(struct x64_program *prog, FILE *f);

#endif // X64_H
