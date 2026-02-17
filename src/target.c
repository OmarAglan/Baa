/**
 * @file target.c
 * @brief تنفيذ تجريد الهدف (Target Abstraction) — v0.3.2.8.1
 */

#include "target.h"

#include "regalloc.h" // PhysReg

#include <string.h>

// ============================================================================
// أدوات مساعدة
// ============================================================================

static unsigned int phys_mask(PhysReg r)
{
    if (r < 0 || r >= PHYS_REG_COUNT)
        return 0u;
    return 1u << (unsigned)r;
}

// ============================================================================
// Calling Conventions
// ============================================================================

// Windows x64 ABI
static const BaaCallingConv CC_WIN_X64 = {
    .int_arg_reg_count = 4,
    .int_arg_phys_regs = {PHYS_RCX, PHYS_RDX, PHYS_R8, PHYS_R9, 0, 0, 0, 0},
    .ret_phys_reg = PHYS_RAX,

    // Windows x64: RBX, RBP, RDI, RSI, R12-R15 callee-saved
    .callee_saved_mask = 0u,
    .caller_saved_mask = 0u,
    .shadow_space_bytes = 32,
    .home_reg_args_on_call = true,
    .sysv_set_al_zero_on_call = false,
};

// SystemV AMD64 ABI
static const BaaCallingConv CC_SYSV_AMD64 = {
    .int_arg_reg_count = 6,
    .int_arg_phys_regs = {PHYS_RDI, PHYS_RSI, PHYS_RDX, PHYS_RCX, PHYS_R8, PHYS_R9, 0, 0},
    .ret_phys_reg = PHYS_RAX,
    // SysV: RBX, RBP, R12-R15 callee-saved
    .callee_saved_mask = 0u,
    .caller_saved_mask = 0u,
    .shadow_space_bytes = 0,
    .home_reg_args_on_call = false,
    .sysv_set_al_zero_on_call = true,
};

// نُهيئ الأقنعة عند التحميل مرة واحدة عبر قيم ثابتة.
static BaaCallingConv init_cc_masks(BaaCallingConv cc_in)
{
    // ملاحظة: لا نضع RSP ضمن أي قناع.
    unsigned int callee = 0u;
    unsigned int caller = 0u;

    if (cc_in.shadow_space_bytes == 32)
    {
        // Windows x64
        callee |= phys_mask(PHYS_RBX);
        callee |= phys_mask(PHYS_RBP);
        callee |= phys_mask(PHYS_RDI);
        callee |= phys_mask(PHYS_RSI);
        callee |= phys_mask(PHYS_R12);
        callee |= phys_mask(PHYS_R13);
        callee |= phys_mask(PHYS_R14);
        callee |= phys_mask(PHYS_R15);

        // caller-saved: RAX, RCX, RDX, R8-R11
        caller |= phys_mask(PHYS_RAX);
        caller |= phys_mask(PHYS_RCX);
        caller |= phys_mask(PHYS_RDX);
        caller |= phys_mask(PHYS_R8);
        caller |= phys_mask(PHYS_R9);
        caller |= phys_mask(PHYS_R10);
        caller |= phys_mask(PHYS_R11);
    }
    else
    {
        // SysV AMD64
        callee |= phys_mask(PHYS_RBX);
        callee |= phys_mask(PHYS_RBP);
        callee |= phys_mask(PHYS_R12);
        callee |= phys_mask(PHYS_R13);
        callee |= phys_mask(PHYS_R14);
        callee |= phys_mask(PHYS_R15);

        // caller-saved: RAX, RCX, RDX, RSI, RDI, R8-R11
        caller |= phys_mask(PHYS_RAX);
        caller |= phys_mask(PHYS_RCX);
        caller |= phys_mask(PHYS_RDX);
        caller |= phys_mask(PHYS_RSI);
        caller |= phys_mask(PHYS_RDI);
        caller |= phys_mask(PHYS_R8);
        caller |= phys_mask(PHYS_R9);
        caller |= phys_mask(PHYS_R10);
        caller |= phys_mask(PHYS_R11);
    }

    cc_in.callee_saved_mask = callee;
    cc_in.caller_saved_mask = caller;
    return cc_in;
}

static const BaaCallingConv* cc_win_x64(void)
{
    static BaaCallingConv cc;
    static int inited = 0;
    if (!inited)
    {
        cc = init_cc_masks(CC_WIN_X64);
        inited = 1;
    }
    return &cc;
}

static const BaaCallingConv* cc_sysv_amd64(void)
{
    static BaaCallingConv cc;
    static int inited = 0;
    if (!inited)
    {
        cc = init_cc_masks(CC_SYSV_AMD64);
        inited = 1;
    }
    return &cc;
}

// ============================================================================
// Built-in Targets
// ============================================================================

static const BaaTarget TARGET_WIN_X64 = {
    .kind = BAA_TARGET_X86_64_WINDOWS,
    .name = "x86_64-windows",
    .triple = "x86_64-w64-mingw32",
    .obj_format = BAA_OBJFORMAT_COFF,
    .data_layout = &IR_DATA_LAYOUT_WIN_X64,
    .cc = NULL, // سيملأ عبر دالة getter
    .default_exe_ext = ".exe",
};

static const BaaTarget TARGET_LINUX_X64 = {
    .kind = BAA_TARGET_X86_64_LINUX,
    .name = "x86_64-linux",
    .triple = "x86_64-unknown-linux-gnu",
    .obj_format = BAA_OBJFORMAT_ELF,
    .data_layout = &IR_DATA_LAYOUT_WIN_X64, // نفس التخطيط على x86-64 (مؤقتاً)
    .cc = NULL, // سيملأ عبر دالة getter
    .default_exe_ext = "",
};

const BaaTarget* baa_target_builtin_windows_x86_64(void)
{
    static BaaTarget t;
    static int inited = 0;
    if (!inited)
    {
        t = TARGET_WIN_X64;
        t.cc = cc_win_x64();
        inited = 1;
    }
    return &t;
}

const BaaTarget* baa_target_builtin_linux_x86_64(void)
{
    static BaaTarget t;
    static int inited = 0;
    if (!inited)
    {
        t = TARGET_LINUX_X64;
        t.cc = cc_sysv_amd64();
        inited = 1;
    }
    return &t;
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
