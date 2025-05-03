#include "x64.h"

const struct x64_arg X64_RAX = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RAX};
const struct x64_arg X64_RBX = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RBX};
const struct x64_arg X64_RCX = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RCX};
const struct x64_arg X64_RDX = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RDX};
const struct x64_arg X64_RSI = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RSI};
const struct x64_arg X64_RDI = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RDI};
const struct x64_arg X64_RSP = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RSP};
const struct x64_arg X64_RBP = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_RBP};
const struct x64_arg X64_R8 = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_R8};
const struct x64_arg X64_R9 = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_R9};
const struct x64_arg X64_R10 = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_R10};
const struct x64_arg X64_R11 = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_R11};
const struct x64_arg X64_R12 = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_R12};
const struct x64_arg X64_R13 = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_R13};
const struct x64_arg X64_R14 = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_R14};
const struct x64_arg X64_R15 = {.tag = X64_ARG_REG, .val.reg.reg = X64_REG_R15};

const struct x64_arg X64_REGS[] = {
    X64_RAX,
    X64_RBX,
    X64_RCX,
    X64_RDX,
    X64_RSI,
    X64_RDI,
    X64_RSP,
    X64_RBP,
    X64_R8,
    X64_R9,
    X64_R10,
    X64_R11,
    X64_R12,
    X64_R13,
    X64_R14,
    X64_R15,
};
