/**
 * @file emit.c
 * @brief إصدار كود التجميع - تحويل تمثيل الآلة إلى نص تجميع AT&T (v0.3.2.3)
 *
 * يحوّل وحدة آلية (MachineModule) بعد تخصيص السجلات إلى ملف تجميع
 * x86-64 بصيغة AT&T متوافق مع GAS (GNU Assembler).
 *
 * الخرج يتضمن:
 * - قسم rodata: صيغ الطباعة (COFF: .rdata / ELF: .rodata)
 * - قسم .data: المتغيرات العامة
 * - قسم .text: الدوال مع prologue/epilogue
 * - جدول النصوص: سلاسل نصية ثابتة (.Lstr_N)
 *
 * الهدف/اتفاقية الاستدعاء (v0.3.2.8+):
 * - Windows x64: shadow space (32) و RCX/RDX/R8/R9
 * - SystemV AMD64 (Linux): لا shadow space و RDI/RSI/RDX/RCX/R8/R9
 */

#include "backend_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

// ============================================================================
// أسماء السجلات بصيغة AT&T (Register Names - AT&T Syntax)
// ============================================================================

/**
 * @brief أسماء السجلات 64-بت بصيغة AT&T.
 *
 * مرتبة حسب ترقيم PhysReg (0=RAX, 1=RCX, ..., 15=R15).
 */
static const char* reg64_names[PHYS_REG_COUNT] = {
    "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp",
    "%rsi", "%rdi", "%r8",  "%r9",  "%r10", "%r11",
    "%r12", "%r13", "%r14", "%r15"
};

/**
 * @brief أسماء السجلات 32-بت بصيغة AT&T.
 */
static const char* reg32_names[PHYS_REG_COUNT] = {
    "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp",
    "%esi", "%edi", "%r8d",  "%r9d",  "%r10d", "%r11d",
    "%r12d", "%r13d", "%r14d", "%r15d"
};

/**
 * @brief أسماء السجلات 16-بت بصيغة AT&T.
 */
static const char* reg16_names[PHYS_REG_COUNT] = {
    "%ax",  "%cx",  "%dx",  "%bx",  "%sp",  "%bp",
    "%si",  "%di",  "%r8w", "%r9w", "%r10w", "%r11w",
    "%r12w", "%r13w", "%r14w", "%r15w"
};

/**
 * @brief أسماء السجلات 8-بت بصيغة AT&T (الجزء السفلي).
 */
static const char* reg8_names[PHYS_REG_COUNT] = {
    "%al",  "%cl",  "%dl",  "%bl",  "%spl", "%bpl",
    "%sil", "%dil", "%r8b",  "%r9b",  "%r10b", "%r11b",
    "%r12b", "%r13b", "%r14b", "%r15b"
};

/**
 * @brief التحقق إن كانت القيمة الفورية تصلح كـ imm32 (مع تمديد الإشارة) في x86-64.
 *
 * معظم تعليمات x86-64 التي تقبل immediate بعرض 64-بت تقبل فعلياً imm32 فقط
 * ويتم تمديد الإشارة. لذا القيم التي لا تتسع في int32 تحتاج materialization
 * إلى سجل وسيط (مثل movabs).
 */
static bool imm_fits_imm32(int64_t imm)
{
    return (imm >= (int64_t)INT32_MIN) && (imm <= (int64_t)INT32_MAX);
}

static MachineOperand emit_scratch_r11(int bits)
{
    MachineOperand tmp = (MachineOperand){0};
    tmp.kind = MACH_OP_VREG;
    tmp.size_bits = (bits > 0) ? bits : 64;
    tmp.data.vreg = PHYS_R11;
    return tmp;
}

/**
 * @brief معرفات داخلية لتفادي تعارض تسميات الكتل بين الدوال.
 *
 * في GAS، التسميات المحلية مثل .LBB_0 تكون على مستوى الملف كله،
 * لذا قد تتعارض بين دوال مختلفة إذا كانت أرقام الكتل تبدأ من الصفر
 * داخل كل دالة. الحل: إضافة بادئة رقمية لكل دالة أثناء الإصدار.
 */
static int g_emit_next_func_uid = 0;
static int g_emit_current_func_uid = 0;
static int g_emit_sp_seq = 0;

// هل إصدار معلومات الديبغ مفعل؟
static bool g_emit_debug_info = false;
// هل إصدار تعليقات تجميع وصفية مفعل؟
static bool g_emit_asm_comments = false;

// الهدف واتفاقية الاستدعاء الحالية (تُملأ عبر emit_module_ex)
static const BaaTarget* g_emit_target = NULL;
static const BaaCallingConv* g_emit_cc = NULL;
static BaaCodegenOptions g_emit_opts;

#include "emit_support.c"
#include "emit_frame.c"

void emit_inst(MachineInst* inst, MachineFunc* func, FILE* out) {
    if (!inst || !out) return;
    (void)func;

    switch (inst->op) {
#include "emit_inst_body_a.inc"
#include "emit_inst_body_b.inc"
#include "emit_inst_body_c.inc"
    }
}

#include "emit_data_section.c"
#include "emit_module_driver.c"
