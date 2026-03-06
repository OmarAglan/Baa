/**
 * @file target.c
 * @brief تنفيذ تجريد الهدف (Target Abstraction) — v0.3.2.8.1
 */

#include "backend_internal.h"

#include <string.h>

#define PR(r) (1u << (unsigned)(r))

// ============================================================================
// Calling Conventions
// ============================================================================

// Windows x64 ABI
static const BaaCallingConv CC_WIN_X64 = {
    .int_arg_reg_count = 4,
    .int_arg_phys_regs = {PHYS_RCX, PHYS_RDX, PHYS_R8, PHYS_R9, 0, 0, 0, 0},
    .ret_phys_reg = PHYS_RAX,

    .callee_saved_mask =
        PR(PHYS_RBX) | PR(PHYS_RBP) | PR(PHYS_RDI) | PR(PHYS_RSI) |
        PR(PHYS_R12) | PR(PHYS_R13) | PR(PHYS_R14) | PR(PHYS_R15),
    .caller_saved_mask =
        PR(PHYS_RAX) | PR(PHYS_RCX) | PR(PHYS_RDX) |
        PR(PHYS_R8) | PR(PHYS_R9) | PR(PHYS_R10) | PR(PHYS_R11),

    .stack_align_bytes = 16,
    .abi_arg_vreg0 = -10,
    .abi_ret_vreg = -2,
    .shadow_space_bytes = 32,
    .home_reg_args_on_call = true,
    .sysv_set_al_zero_on_call = false,
};

// SystemV AMD64 ABI
static const BaaCallingConv CC_SYSV_AMD64 = {
    .int_arg_reg_count = 6,
    .int_arg_phys_regs = {PHYS_RDI, PHYS_RSI, PHYS_RDX, PHYS_RCX, PHYS_R8, PHYS_R9, 0, 0},
    .ret_phys_reg = PHYS_RAX,

    .callee_saved_mask =
        PR(PHYS_RBX) | PR(PHYS_RBP) |
        PR(PHYS_R12) | PR(PHYS_R13) | PR(PHYS_R14) | PR(PHYS_R15),
    .caller_saved_mask =
        PR(PHYS_RAX) | PR(PHYS_RCX) | PR(PHYS_RDX) |
        PR(PHYS_RSI) | PR(PHYS_RDI) |
        PR(PHYS_R8) | PR(PHYS_R9) | PR(PHYS_R10) | PR(PHYS_R11),

    .stack_align_bytes = 16,
    .abi_arg_vreg0 = -10,
    .abi_ret_vreg = -2,
    .shadow_space_bytes = 0,
    .home_reg_args_on_call = false,
    .sysv_set_al_zero_on_call = true,
};

// ============================================================================
// Built-in Targets
// ============================================================================

static const BaaTarget TARGET_WIN_X64 = {
    .kind = BAA_TARGET_X86_64_WINDOWS,
    .name = "x86_64-windows",
    .triple = "x86_64-w64-mingw32",
    .obj_format = BAA_OBJFORMAT_COFF,
    .data_layout = &IR_DATA_LAYOUT_WIN_X64,
    .cc = &CC_WIN_X64,
    .default_exe_ext = ".exe",
};

static const BaaTarget TARGET_LINUX_X64 = {
    .kind = BAA_TARGET_X86_64_LINUX,
    .name = "x86_64-linux",
    .triple = "x86_64-unknown-linux-gnu",
    .obj_format = BAA_OBJFORMAT_ELF,
    .data_layout = &IR_DATA_LAYOUT_WIN_X64, // نفس التخطيط على x86-64 (مؤقتاً)
    .cc = &CC_SYSV_AMD64,
    .default_exe_ext = "",
};

const BaaTarget* baa_target_builtin_windows_x86_64(void)
{
    return &TARGET_WIN_X64;
}

const BaaTarget* baa_target_builtin_linux_x86_64(void)
{
    return &TARGET_LINUX_X64;
}

const BaaTarget* baa_target_host_default(void)
{
#ifdef _WIN32
    return baa_target_builtin_windows_x86_64();
#else
    return baa_target_builtin_linux_x86_64();
#endif
}

const BaaTarget* baa_target_parse(const char* s)
{
    if (!s || !s[0])
        return NULL;

    if (strcmp(s, "x86_64-windows") == 0 || strcmp(s, "x86_64-win") == 0)
        return baa_target_builtin_windows_x86_64();

    if (strcmp(s, "x86_64-linux") == 0 || strcmp(s, "x86_64-elf") == 0)
        return baa_target_builtin_linux_x86_64();

    return NULL;
}
