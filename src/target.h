/**
 * @file target.h
 * @brief تجريد الهدف (Target Abstraction) — v0.3.2.8.1
 *
 * الهدف من هذا الملف هو فصل افتراضات:
 * - نظام التشغيل/صيغة الكائنات (PE/COFF مقابل ELF)
 * - اتفاقية الاستدعاء (Windows x64 مقابل SystemV AMD64)
 * - تخطيط البيانات (أحجام ومحاذاة)
 *
 * عن بقية الخلفية (isel/regalloc/emit) حتى نتمكن من دعم أهداف متعددة.
 */

#ifndef BAA_TARGET_H
#define BAA_TARGET_H

#include <stdbool.h>

#include "ir_data_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// أنواع الهدف (Target Kinds)
// ============================================================================

typedef enum {
    BAA_TARGET_X86_64_WINDOWS = 0,
    BAA_TARGET_X86_64_LINUX   = 1,
} BaaTargetKind;

typedef enum {
    BAA_OBJFORMAT_COFF = 0,
    BAA_OBJFORMAT_ELF  = 1,
} BaaObjectFormat;

// ============================================================================
// اتفاقية الاستدعاء (Calling Convention)
// ============================================================================

/**
 * @struct BaaCallingConv
 * @brief وصف اتفاقية الاستدعاء للهدف.
 */
typedef struct BaaCallingConv {
    int int_arg_reg_count;            // عدد سجلات معاملات الأعداد الصحيحة
    int int_arg_phys_regs[8];         // PhysReg values (from regalloc.h)

    int ret_phys_reg;                 // PhysReg (عادةً RAX)

    unsigned int callee_saved_mask;   // bitmask over PhysReg
    unsigned int caller_saved_mask;   // bitmask over PhysReg

    int shadow_space_bytes;           // Windows: 32, SysV: 0
    bool home_reg_args_on_call;       // Windows varargs: true, SysV: false
    bool sysv_set_al_zero_on_call;    // SysV varargs rule: true
} BaaCallingConv;

// ============================================================================
// وصف الهدف (Target Descriptor)
// ============================================================================

/**
 * @struct BaaTarget
 * @brief وصف هدف محدد (OS + ABI + object format).
 */
typedef struct BaaTarget {
    BaaTargetKind kind;
    const char* name;                 // short name: x86_64-windows, x86_64-linux
    const char* triple;               // future: full triple

    BaaObjectFormat obj_format;
    const IRDataLayout* data_layout;
    const BaaCallingConv* cc;

    const char* default_exe_ext;      // ".exe" on Windows, "" on Linux
} BaaTarget;

// ============================================================================
// أهداف مدمجة (Built-in Targets)
// ============================================================================

const BaaTarget* baa_target_builtin_windows_x86_64(void);
const BaaTarget* baa_target_builtin_linux_x86_64(void);

/**
 * @brief الهدف الافتراضي حسب نظام بناء المترجم (host).
 */
const BaaTarget* baa_target_host_default(void);

/**
 * @brief تحليل نص الهدف.
 *
 * يدعم:
 * - "x86_64-windows"
 * - "x86_64-linux"
 */
const BaaTarget* baa_target_parse(const char* s);

#ifdef __cplusplus
}
#endif

#endif // BAA_TARGET_H
