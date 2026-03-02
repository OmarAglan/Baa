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

#include "emit.h"
#include "target.h"
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

/**
 * @brief طباعة تعليق إلى ملف التجميع عند تفعيل --asm-comments.
 */
static void emit_comment(FILE* out, const char* fmt, ...)
{
    if (!g_emit_asm_comments || !out || !fmt) return;

    fprintf(out, "    # ");
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fprintf(out, "\n");
}

static const BaaCallingConv* emit_cc_or_default(void)
{
    if (g_emit_cc)
        return g_emit_cc;
    return baa_target_builtin_windows_x86_64()->cc;
}

// إعلان مسبق لأن بعض دوال حماية المكدس تستخدمه
static int emit_shadow_bytes(void);

static bool emit_stack_protector_enabled_for_func(MachineFunc* func)
{
    if (!func) return false;
    if (!g_emit_target || g_emit_target->obj_format != BAA_OBJFORMAT_ELF) return false;

    if (g_emit_opts.stack_protector == BAA_STACKPROT_ALL) return true;
    if (g_emit_opts.stack_protector == BAA_STACKPROT_ON) return func->stack_size > 0;
    return false;
}

static int emit_stack_protector_size(MachineFunc* func)
{
    return emit_stack_protector_enabled_for_func(func) ? 8 : 0;
}

static int emit_stack_protector_offset(MachineFunc* func)
{
    // نضع الكناري مباشرةً بعد مساحة locals/shadow وقبل حفظ callee-saved.
    int local_size = func ? func->stack_size : 0;
    int shadow = emit_shadow_bytes();
    return -(local_size + shadow + 8);
}

static void emit_stack_protector_prologue(MachineFunc* func, FILE* out)
{
    if (!emit_stack_protector_enabled_for_func(func) || !out) return;

    // نستخدم r11 كمؤقت حتى لا نخرب سجلات معاملات ABI عند دخول الدالة.
    int off = emit_stack_protector_offset(func);
    fprintf(out, "    movq %%fs:40, %%r11\n");
    fprintf(out, "    movq %%r11, %d(%%rbp)\n", off);
}

static void emit_stack_protector_epilogue(MachineFunc* func, FILE* out, int ok_label_id)
{
    if (!emit_stack_protector_enabled_for_func(func) || !out) return;

    int off = emit_stack_protector_offset(func);

    // نستخدم r10/r11 لتفادي تخريب RAX (قيمة الإرجاع) وسجلات المعاملات.
    fprintf(out, "    movq %d(%%rbp), %%r11\n", off);
    fprintf(out, "    movq %%fs:40, %%r10\n");
    fprintf(out, "    xorq %%r10, %%r11\n");
    fprintf(out, "    je .L__sp_ok_%d_%d\n", g_emit_current_func_uid, ok_label_id);
    fprintf(out, "    call __stack_chk_fail\n");
    fprintf(out, "    .byte 0x0f, 0x0b\n");
    fprintf(out, ".L__sp_ok_%d_%d:\n", g_emit_current_func_uid, ok_label_id);
}

static int emit_shadow_bytes(void)
{
    const BaaCallingConv* cc = emit_cc_or_default();
    return cc ? cc->shadow_space_bytes : 32;
}

static int emit_stack_align_bytes(void)
{
    const BaaCallingConv* cc = emit_cc_or_default();
    return (cc && cc->stack_align_bytes > 0) ? cc->stack_align_bytes : 16;
}

static void emit_rodata_section(FILE* out)
{
    if (!out) return;
    if (g_emit_target && g_emit_target->obj_format == BAA_OBJFORMAT_ELF)
    {
        fprintf(out, ".section .rodata\n");
    }
    else
    {
        fprintf(out, ".section .rdata,\"dr\"\n");
    }
}

static bool emit_reg_is_callee_saved(PhysReg r)
{
    const BaaCallingConv* cc = emit_cc_or_default();
    if (!cc) return false;
    if (r < 0 || r >= PHYS_REG_COUNT) return false;
    return (cc->callee_saved_mask & (1u << (unsigned)r)) != 0u;
}

typedef struct {
    const char** files;
    int file_count;
    int file_cap;

    int last_file_id;
    int last_line;
    int last_col;

    const char* last_dbg_name;
} EmitDebugState;

static EmitDebugState g_emit_dbg = {0};

static void emit_debug_reset(void) {
    if (g_emit_dbg.files) {
        free(g_emit_dbg.files);
        g_emit_dbg.files = NULL;
    }
    g_emit_dbg.file_count = 0;
    g_emit_dbg.file_cap = 0;
    g_emit_dbg.last_file_id = -1;
    g_emit_dbg.last_line = -1;
    g_emit_dbg.last_col = -1;
    g_emit_dbg.last_dbg_name = NULL;
}

static void emit_debug_escape_path(FILE* out, const char* s) {
    if (!out || !s) return;

    for (const char* p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\' || c == '"') {
            fputc('\\', out);
            fputc((int)c, out);
        } else if (c == '\n') {
            fputs("\\n", out);
        } else if (c == '\r') {
            fputs("\\r", out);
        } else if (c == '\t') {
            fputs("\\t", out);
        } else {
            fputc((int)c, out);
        }
    }
}

static int emit_debug_get_file_id(FILE* out, const char* filename) {
    if (!out || !filename) return -1;

    for (int i = 0; i < g_emit_dbg.file_count; i++) {
        const char* cur = g_emit_dbg.files[i];
        if (cur && strcmp(cur, filename) == 0) {
            return i + 1; // .file ids are 1-based
        }
    }

    if (g_emit_dbg.file_count >= g_emit_dbg.file_cap) {
        int new_cap = (g_emit_dbg.file_cap == 0) ? 8 : g_emit_dbg.file_cap * 2;
        const char** new_arr = (const char**)realloc(g_emit_dbg.files, (size_t)new_cap * sizeof(const char*));
        if (!new_arr) return -1;
        g_emit_dbg.files = new_arr;
        g_emit_dbg.file_cap = new_cap;
    }

    int id = g_emit_dbg.file_count + 1;
    g_emit_dbg.files[g_emit_dbg.file_count++] = filename;

    // إصدار .file مرة واحدة لكل ملف
    fprintf(out, "    .file %d \"", id);
    emit_debug_escape_path(out, filename);
    fprintf(out, "\"\n");

    return id;
}

static void emit_debug_loc(FILE* out, const MachineInst* inst) {
    if (!g_emit_debug_info) return;
    if (!out || !inst) return;
    if (!inst->src_file || inst->src_line <= 0) return;

    int file_id = emit_debug_get_file_id(out, inst->src_file);
    if (file_id <= 0) return;

    int line = inst->src_line;
    int col = inst->src_col;
    if (col <= 0) col = 1;

    if (file_id != g_emit_dbg.last_file_id ||
        line != g_emit_dbg.last_line ||
        col != g_emit_dbg.last_col) {
        fprintf(out, "    .loc %d %d %d\n", file_id, line, col);
        g_emit_dbg.last_file_id = file_id;
        g_emit_dbg.last_line = line;
        g_emit_dbg.last_col = col;
    }

    if (inst->dbg_name) {
        int changed = 1;
        if (g_emit_dbg.last_dbg_name && strcmp(g_emit_dbg.last_dbg_name, inst->dbg_name) == 0) {
            changed = 0;
        }
        if (changed) {
            fprintf(out, "    # متغير: %s\n", inst->dbg_name);
            g_emit_dbg.last_dbg_name = inst->dbg_name;
        }
    }
}

// ============================================================================
// دوال مساعدة للحصول على اسم السجل (Register Name Helpers)
// ============================================================================

/**
 * @brief الحصول على اسم السجل حسب الحجم بالبتات.
 * @param reg رقم السجل الفيزيائي (PhysReg).
 * @param bits حجم المعامل (8, 32, 64).
 * @return سلسلة اسم السجل بصيغة AT&T.
 */
static const char* reg_name_for_bits(int reg, int bits) {
    if (reg < 0 || reg >= PHYS_REG_COUNT) return "%rax"; // احتياطي
    switch (bits) {
        case 8:  return reg8_names[reg];
        case 16: return reg16_names[reg];
        case 32: return reg32_names[reg];
        default: return reg64_names[reg];  // 64-بت افتراضي
    }
}

/**
 * @brief الحصول على لاحقة حجم التعليمة (q, l, w, b).
 * @param bits حجم المعامل بالبتات.
 * @return حرف اللاحقة.
 */
static char size_suffix(int bits) {
    switch (bits) {
        case 8:  return 'b';
        case 16: return 'w';
        case 32: return 'l';
        default: return 'q';  // 64-بت افتراضي
    }
}

// ============================================================================
// إصدار المعاملات (Operand Emission)
// ============================================================================

/**
 * @brief كتابة معامل آلة بصيغة AT&T إلى ملف الخرج.
 *
 * الصيغ المدعومة:
 * - سجل: %rax, %ecx, %al, ...
 * - فوري: $123
 * - ذاكرة: offset(%rbp), (%rax), offset(%rax)
 * - تسمية: .LBB_N
 * - عام: name(%rip)
 * - دالة: name
 *
 * @param op المعامل.
 * @param out ملف الخرج.
 */
static void emit_operand(MachineOperand* op, FILE* out) {
    if (!op || !out) return;

    switch (op->kind) {
        case MACH_OP_NONE:
            break;

        case MACH_OP_VREG:
            // بعد تخصيص السجلات، الرقم هو سجل فيزيائي
            {
                int reg = op->data.vreg;
                if (reg >= 0 && reg < PHYS_REG_COUNT) {
                    int bits = op->size_bits;
                    if (bits <= 0) bits = 64;
                    fprintf(out, "%s", reg_name_for_bits(reg, bits));
                } else {
                    // سجل غير محلول (لا ينبغي أن يحدث بعد regalloc)
                    fprintf(out, "%%v%d", reg);
                }
            }
            break;

        case MACH_OP_IMM:
            fprintf(out, "$%lld", (long long)op->data.imm);
            break;

        case MACH_OP_MEM:
            // صيغة AT&T: offset(%base)
            {
                int base = op->data.mem.base_vreg;
                int32_t offset = op->data.mem.offset;
                const char* base_name = "%rbp"; // افتراضي

                if (base >= 0 && base < PHYS_REG_COUNT) {
                    base_name = reg64_names[base];
                }

                if (offset != 0) {
                    fprintf(out, "%d(%s)", offset, base_name);
                } else {
                    fprintf(out, "(%s)", base_name);
                }
            }
            break;

        case MACH_OP_LABEL:
            fprintf(out, ".LBB_%d_%d", g_emit_current_func_uid, op->data.label_id);
            break;

        case MACH_OP_GLOBAL:
            // متغير عام: name(%rip) لعنونة RIP-relative
            if (op->data.name) {
                // التحقق من أنه تسمية نص (.Lstr_N) أو اسم متغير
                if (strncmp(op->data.name, ".Lstr_", 6) == 0) {
                    fprintf(out, "%s(%%rip)", op->data.name);
                } else {
                    fprintf(out, "%s(%%rip)", op->data.name);
                }
            }
            break;

        case MACH_OP_FUNC:
            // مرجع دالة: اسم الدالة مباشرة
            if (op->data.name) {
                // تحويل الرئيسية → main
                if (strcmp(op->data.name, "الرئيسية") == 0) {
                    fprintf(out, "main");
                }
                // تحويل اطبع → printf
                else if (strcmp(op->data.name, "اطبع") == 0 ||
                         strcmp(op->data.name, "اطبع_صحيح") == 0) {
                    fprintf(out, "printf");
                }
                // تحويل اقرأ → scanf
                else if (strcmp(op->data.name, "اقرأ") == 0 ||
                         strcmp(op->data.name, "اقرأ_صحيح") == 0) {
                    fprintf(out, "scanf");
                }
                else {
                    fprintf(out, "%s", op->data.name);
                }
            }
            break;

        case MACH_OP_XMM:
            fprintf(out, "%%xmm%d", op->data.xmm);
            break;
    }
}

// ============================================================================
// إصدار مقدمة وخاتمة الدالة (Function Prologue/Epilogue)
// ============================================================================

/**
 * @brief إصدار مقدمة الدالة (Prologue).
 *
 * تشمل:
 * 1. حفظ مؤشر الإطار القديم (push %rbp)
 * 2. إعداد مؤشر الإطار الجديد (mov %rsp, %rbp)
 * 3. حجز مساحة المكدس المحلي (sub $N, %rsp)
 * 4. حفظ السجلات المحفوظة (callee-saved) المستخدمة
 *
 * @param func الدالة.
 * @param out ملف الخرج.
 * @param callee_saved_used مصفوفة السجلات المحفوظة المستخدمة.
 * @param callee_count عدد السجلات المحفوظة لحفظها.
 */
static void emit_prologue(MachineFunc* func, FILE* out,
                           PhysReg* callee_regs, int callee_count) {
    emit_comment(out, "بداية prologue");
    // حفظ إطار المكدس
    fprintf(out, "    push %%rbp\n");
    fprintf(out, "    mov %%rsp, %%rbp\n");

    // حساب حجم المكدس الإجمالي:
    // حجم محلي (func->stack_size) + shadow space (حسب الهدف) + حفظ callee-saved
    // مع محاذاة 16 بايت
    int local_size = func->stack_size;
    int callee_save_size = callee_count * 8;

    // إجمالي الحجم المطلوب (بعد push rbp)
    // المكدس بعد push rbp: RSP ناقص 8 (لـ rbp المحفوظ)
    int shadow = emit_shadow_bytes();
    int canary_size = emit_stack_protector_size(func);
    int total_frame = local_size + shadow + canary_size + callee_save_size;

    // محاذاة إلى قيمة الهدف (عادة 16 بايت)
    // بعد push rbp، RSP = aligned - 8
    // نحتاج sub ليجعل RSP محاذى عند نقاط الاستدعاء.
    int align = emit_stack_align_bytes();
    if (align > 0 && (total_frame % align) != 0) {
        total_frame = ((total_frame / align) + 1) * align;
    }

    if (total_frame > 0) {
        fprintf(out, "    sub $%d, %%rsp\n", total_frame);
        emit_comment(out, "حجز إطار مكدس بحجم %d بايت", total_frame);
    }

    // تهيئة كناري حماية المكدس (إن كان مفعلاً)
    emit_stack_protector_prologue(func, out);

    // حفظ السجلات المحفوظة (callee-saved)
    for (int i = 0; i < callee_count; i++) {
        int save_offset = -(local_size + shadow + canary_size + (i + 1) * 8);
        fprintf(out, "    mov %s, %d(%%rbp)\n",
                reg64_names[callee_regs[i]], save_offset);
        emit_comment(out, "حفظ سجل callee-saved: %s", reg64_names[callee_regs[i]]);
    }
}

/**
 * @brief إصدار خاتمة الدالة (Epilogue).
 *
 * تشمل:
 * 1. استعادة السجلات المحفوظة (callee-saved)
 * 2. استعادة مؤشر الإطار (leave)
 * 3. الرجوع (ret)
 *
 * ملاحظة: يتم إصدار الخاتمة قبل كل تعليمة ret في الدالة.
 */
static void emit_epilogue(MachineFunc* func, FILE* out,
                           PhysReg* callee_regs, int callee_count) {
    emit_comment(out, "بداية epilogue");
    int local_size = func->stack_size;
    int shadow = emit_shadow_bytes();
    int canary_size = emit_stack_protector_size(func);
    // تحقق كناري حماية المكدس قبل تفكيك الإطار
    emit_stack_protector_epilogue(func, out, g_emit_sp_seq++);

    // استعادة السجلات المحفوظة (callee-saved) بترتيب عكسي
    for (int i = callee_count - 1; i >= 0; i--) {
        int save_offset = -(local_size + shadow + canary_size + (i + 1) * 8);
        fprintf(out, "    mov %d(%%rbp), %s\n",
                save_offset, reg64_names[callee_regs[i]]);
        emit_comment(out, "استرجاع سجل callee-saved: %s", reg64_names[callee_regs[i]]);
    }

    // استعادة المكدس والرجوع
    fprintf(out, "    leave\n");
    fprintf(out, "    ret\n");
    emit_comment(out, "نهاية epilogue");
}

/**
 * @brief إصدار نداء_ذيلي (Tail Call) كقفز بعد تفكيك إطار الدالة.
 *
 * الفكرة:
 * - نفك إطار الدالة الحالية (استعادة callee-saved ثم leave)
 * - نقل/تحضير المعاملات يتم مسبقاً في ISel
 * - نقفز إلى الدالة الهدف بدون push عنوان رجوع جديد
 *
 * بهذه الطريقة، عندما تُنفّذ الدالة الهدف `ret` ستعود مباشرةً
 * إلى مستدعي الدالة الحالية (إعادة استخدام إطار المكدس).
 */
static void emit_tailjmp(MachineFunc* func, MachineInst* inst, FILE* out,
                         PhysReg* callee_regs, int callee_count) {
    if (!func || !inst || !out) return;

    int local_size = func->stack_size;
    int shadow = emit_shadow_bytes();
    int canary_size = emit_stack_protector_size(func);
    // تحقق كناري حماية المكدس قبل الخروج من الدالة (حتى في النداء_الذيلي)
    emit_stack_protector_epilogue(func, out, g_emit_sp_seq++);

    // استعادة السجلات المحفوظة (callee-saved) بترتيب عكسي
    for (int i = callee_count - 1; i >= 0; i--) {
        int save_offset = -(local_size + shadow + canary_size + (i + 1) * 8);
        fprintf(out, "    mov %d(%%rbp), %s\n",
                save_offset, reg64_names[callee_regs[i]]);
    }

    // تفكيك إطار الدالة: بعد leave يصبح RSP على عنوان الرجوع
    fprintf(out, "    leave\n");

    fprintf(out, "    jmp ");
    emit_operand(&inst->src1, out);
    fprintf(out, "\n");
}

// ============================================================================
// إصدار التعليمات (Instruction Emission)
// ============================================================================

/**
 * @brief تحديد لاحقة الحجم لتعليمة بناءً على معاملاتها.
 */
static char infer_suffix(MachineInst* inst) {
    // نستخدم أول معامل يحدد الحجم (dst ثم src1 ثم src2)
    if (inst->dst.size_bits > 0) return size_suffix(inst->dst.size_bits);
    if (inst->src1.size_bits > 0) return size_suffix(inst->src1.size_bits);
    if (inst->src2.size_bits > 0) return size_suffix(inst->src2.size_bits);
    return 'q'; // افتراضي 64-بت
}

void emit_inst(MachineInst* inst, MachineFunc* func, FILE* out) {
    if (!inst || !out) return;
    (void)func;

    switch (inst->op) {
        // ================================================================
        // تسمية كتلة (Block Label)
        // ================================================================
        case MACH_LABEL:
            emit_comment(out, "بداية كتلة: %d", inst->dst.data.label_id);
            fprintf(out, ".LBB_%d_%d:\n", g_emit_current_func_uid, inst->dst.data.label_id);
            break;

        // ================================================================
        // تعليق (Comment)
        // ================================================================
        case MACH_COMMENT:
            if (inst->comment) {
                fprintf(out, "    # %s\n", inst->comment);
            }
            break;

        // ================================================================
        // لا عملية (NOP)
        // ================================================================
        case MACH_NOP:
            // تجاهل تعليمات NOP (عناصر نائبة لـ phi وغيرها)
            break;

        // ================================================================
        // عمليات نقل البيانات (Data Movement)
        // ================================================================
        case MACH_MOV:
            // AT&T: mov src, dst
            if (inst->dst.kind == MACH_OP_NONE) break;
            if (inst->src1.kind == MACH_OP_NONE) break;

            {
                int bits = inst->dst.size_bits;
                if (bits <= 0) bits = inst->src1.size_bits;
                if (bits <= 0) bits = 64;

                // دعم imm64 الحقيقي: movabs للتحميل، وسجل وسيط عند التخزين للذاكرة.
                if (inst->src1.kind == MACH_OP_IMM && bits == 64 && !imm_fits_imm32(inst->src1.data.imm)) {
                    MachineOperand imm = inst->src1;
                    imm.size_bits = 64;

                    if (inst->dst.kind == MACH_OP_VREG) {
                        fprintf(out, "    movabsq ");
                        emit_operand(&imm, out);
                        fprintf(out, ", ");
                        emit_operand(&inst->dst, out);
                        fprintf(out, "\n");
                        break;
                    }

                    if (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL) {
                        MachineOperand tmp = emit_scratch_r11(64);

                        fprintf(out, "    movabsq ");
                        emit_operand(&imm, out);
                        fprintf(out, ", ");
                        emit_operand(&tmp, out);
                        fprintf(out, "\n");

                        fprintf(out, "    movq ");
                        emit_operand(&tmp, out);
                        fprintf(out, ", ");
                        emit_operand(&inst->dst, out);
                        fprintf(out, "\n");
                        break;
                    }
                }
            }

            // معالجة حالة ذاكرة ← ذاكرة (غير صالحة مباشرة في x86-64)
            // نستخدم %rax كسجل مؤقت لأنه محجوز عادةً ولا نخصصه للفترات العادية.
            if (inst->dst.kind == MACH_OP_MEM && inst->src1.kind == MACH_OP_MEM) {
                int bits = inst->dst.size_bits;
                if (bits <= 0) bits = inst->src1.size_bits;
                if (bits <= 0) bits = 64;

                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", infer_suffix(inst));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    mov%c ", infer_suffix(inst));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
                break;
            }

            // التحقق من عدم وجود mov بين نفس السجل
            if (inst->dst.kind == MACH_OP_VREG &&
                inst->src1.kind == MACH_OP_VREG &&
                inst->dst.data.vreg == inst->src1.data.vreg) {
                // mov %reg, %reg - لا حاجة لإصداره
                break;
            }

            fprintf(out, "    mov%c ", infer_suffix(inst));
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // عمليات حسابية (Arithmetic)
        // ================================================================
        case MACH_ADD:
            // AT&T: add src, dst (dst = dst + src)
            // src2 هو المعامل الفعلي للإضافة
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    addq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    add%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        case MACH_SUB:
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    subq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    sub%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        case MACH_IMUL:
            fprintf(out, "    imul%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SHL:
            // AT&T: shl $imm, dst
            fprintf(out, "    shl%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SHR:
            // AT&T: shr $imm|%cl, dst
            fprintf(out, "    shr%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SAR:
            // AT&T: sar $imm|%cl, dst
            fprintf(out, "    sar%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_NEG:
            fprintf(out, "    neg%c ", infer_suffix(inst));
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // عمليات القسمة (Division)
        // ================================================================
        case MACH_CQO:
            fprintf(out, "    cqo\n");
            break;

        case MACH_IDIV:
            // AT&T: idiv src (يقسم RDX:RAX على src)
            fprintf(out, "    idivq ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_DIV:
            // AT&T: div src (يقسم RDX:RAX على src بدون إشارة)
            fprintf(out, "    divq ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // عمليات العشري (SSE2 f64)
        // ================================================================
        case MACH_ADDSD:
            fprintf(out, "    addsd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SUBSD:
            fprintf(out, "    subsd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_MULSD:
            fprintf(out, "    mulsd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_DIVSD:
            fprintf(out, "    divsd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_UCOMISD:
            // AT&T: ucomisd src, dst
            fprintf(out, "    ucomisd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_XORPD:
            fprintf(out, "    xorpd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_CVTSI2SD:
            // AT&T: cvtsi2sd src(int), dst(xmm)
            fprintf(out, "    cvtsi2sd ");
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_CVTTSD2SI:
            // AT&T: cvttsd2si src(xmm), dst(int)
            fprintf(out, "    cvttsd2si ");
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // تحميل العنوان الفعّال (LEA)
        // ================================================================
        case MACH_LEA:
            // AT&T: lea src, dst  (dst يجب أن يكون سجلاً)
            // إذا كانت الوجهة ذاكرة بسبب التسريب، نستخدم %rax مؤقتاً ثم نخزن.
            if (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL) {
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = 64;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    leaq ");
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    movq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
                break;
            }

            fprintf(out, "    leaq ");
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // تحميل من الذاكرة (LOAD)
        // ================================================================
        case MACH_LOAD:
        {
            // LOAD هو mov من مصدر إلى وجهة. قد تتحول الوجهة إلى ذاكرة بعد التسريب.
            bool src_mem = (inst->src1.kind == MACH_OP_MEM || inst->src1.kind == MACH_OP_GLOBAL);
            bool dst_mem = (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL);

            int bits = inst->dst.size_bits;
            if (bits <= 0) bits = inst->src1.size_bits;
            if (bits <= 0) bits = 64;

            if (src_mem && dst_mem)
            {
                // تجنب mem->mem: استخدم %rax كمؤقت (RAX غير مخصص لفترات عادية).
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            else
            {
                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
        }
            break;

        // ================================================================
        // تخزين في الذاكرة (STORE)
        // ================================================================
        case MACH_STORE:
        {
            // STORE هو mov src1 -> dst. بعد التسريب قد يصبح src1 ذاكرة.
            bool src_mem = (inst->src1.kind == MACH_OP_MEM || inst->src1.kind == MACH_OP_GLOBAL);
            bool dst_mem = (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL);

            int bits = inst->src1.size_bits;
            if (bits <= 0) bits = inst->dst.size_bits;
            if (bits <= 0) bits = 64;

            if (dst_mem && inst->src1.kind == MACH_OP_IMM && bits == 64 && !imm_fits_imm32(inst->src1.data.imm)) {
                MachineOperand imm = inst->src1;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);

                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    movq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
                break;
            }

            if (src_mem && dst_mem)
            {
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            else
            {
                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
        }
            break;

        // ================================================================
        // عمليات المقارنة (Comparison)
        // ================================================================
        case MACH_CMP:
            // AT&T: cmp src2, src1 (يقارن src1 مع src2)
            // cmp لا يدعم ذاكرة-إلى-ذاكرة. إذا كان الطرفان ذاكرة/رمزاً عالمياً، استخدم %rax مؤقتاً.
            if (inst->src2.kind == MACH_OP_IMM && inst->src1.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);

                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    cmpq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->src1, out);
                fprintf(out, "\n");
                break;
            }

            if ((inst->src1.kind == MACH_OP_MEM || inst->src1.kind == MACH_OP_GLOBAL) &&
                (inst->src2.kind == MACH_OP_MEM || inst->src2.kind == MACH_OP_GLOBAL)) {
                int bits = inst->src1.size_bits;
                if (bits <= 0) bits = inst->src2.size_bits;
                if (bits <= 0) bits = 64;

                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    cmp%c ", infer_suffix(inst));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->src1, out);
                fprintf(out, "\n");
                break;
            }

            fprintf(out, "    cmp%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_TEST:
            // AT&T: test src2, src1
            // test لا يدعم ذاكرة-إلى-ذاكرة.
            if ((inst->src1.kind == MACH_OP_MEM || inst->src1.kind == MACH_OP_GLOBAL) &&
                (inst->src2.kind == MACH_OP_MEM || inst->src2.kind == MACH_OP_GLOBAL)) {
                int bits = inst->src1.size_bits;
                if (bits <= 0) bits = inst->src2.size_bits;
                if (bits <= 0) bits = 64;

                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    test%c ", infer_suffix(inst));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->src1, out);
                fprintf(out, "\n");
                break;
            }

            fprintf(out, "    test%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // تعليمات SETcc (تعيين بايت حسب الشرط)
        // ================================================================
        case MACH_SETE:
            fprintf(out, "    sete ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETNE:
            fprintf(out, "    setne ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETG:
            fprintf(out, "    setg ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETL:
            fprintf(out, "    setl ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETGE:
            fprintf(out, "    setge ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETLE:
            fprintf(out, "    setle ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETA:
            fprintf(out, "    seta ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETB:
            fprintf(out, "    setb ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETAE:
            fprintf(out, "    setae ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETBE:
            fprintf(out, "    setbe ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETP:
            fprintf(out, "    setp ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETNP:
            fprintf(out, "    setnp ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // توسيع بالأصفار (MOVZX)
        // ================================================================
        case MACH_MOVZX:
        {
            int sb = inst->src1.size_bits;
            int db = inst->dst.size_bits;
            if (sb <= 0) sb = 8;
            if (sb == 1) sb = 8;
            if (db <= 0) db = 64;

            bool dst_mem = (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL);

            // إذا كانت الوجهة ذاكرة، لا يمكن لـ MOVZX الكتابة إليها مباشرة.
            // نُوسّع إلى سجل مؤقت ثم نخزّن.
            if (dst_mem)
            {
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = db;
                tmp.data.vreg = PHYS_RAX;

                // استعمل المسار العادي لكن بوجهة tmp.
                MachineOperand saved_dst = inst->dst;
                MachineOperand dst2 = tmp;
                MachineOperand src2 = inst->src1;
                src2.size_bits = sb;
                dst2.size_bits = db;

                // 32->64: movl إلى %eax يصفّر %rax.
                if (sb == 32 && db == 64)
                {
                    MachineOperand s32 = src2;
                    MachineOperand d32 = dst2;
                    s32.size_bits = 32;
                    d32.size_bits = 32;
                    fprintf(out, "    movl ");
                    emit_operand(&s32, out);
                    fprintf(out, ", ");
                    emit_operand(&d32, out);
                    fprintf(out, "\n");
                }
                else
                {
                    const char* mnem = NULL;
                    if (sb == 8 && db == 16) mnem = "movzbw";
                    else if (sb == 8 && db == 32) mnem = "movzbl";
                    else if (sb == 8 && db == 64) mnem = "movzbq";
                    else if (sb == 16 && db == 32) mnem = "movzwl";
                    else if (sb == 16 && db == 64) mnem = "movzwq";
                    else if (sb == 32 && db == 32) mnem = "movl";
                    else if (sb == 16 && db == 16) mnem = "movw";
                    else if (sb == 8 && db == 8) mnem = "movb";

                    if (!mnem)
                    {
                        fprintf(out, "    mov%c ", size_suffix(db));
                        MachineOperand ss = inst->src1;
                        MachineOperand dd = dst2;
                        ss.size_bits = db;
                        dd.size_bits = db;
                        emit_operand(&ss, out);
                        fprintf(out, ", ");
                        emit_operand(&dd, out);
                        fprintf(out, "\n");
                    }
                    else
                    {
                        fprintf(out, "    %s ", mnem);
                        emit_operand(&src2, out);
                        fprintf(out, ", ");
                        emit_operand(&dst2, out);
                        fprintf(out, "\n");
                    }
                }

                // store tmp -> dst
                fprintf(out, "    mov%c ", size_suffix(db));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&saved_dst, out);
                fprintf(out, "\n");
                break;
            }

            // 32->64: movl يصفّر تلقائياً عند الكتابة إلى سجل 32 بت.
            if (sb == 32 && db == 64)
            {
                MachineOperand src = inst->src1;
                MachineOperand dst = inst->dst;
                src.size_bits = 32;
                dst.size_bits = 32;
                fprintf(out, "    movl ");
                emit_operand(&src, out);
                fprintf(out, ", ");
                emit_operand(&dst, out);
                fprintf(out, "\n");
                break;
            }

            const char* mnem = NULL;
            if (sb == 8 && db == 16) mnem = "movzbw";
            else if (sb == 8 && db == 32) mnem = "movzbl";
            else if (sb == 8 && db == 64) mnem = "movzbq";
            else if (sb == 16 && db == 32) mnem = "movzwl";
            else if (sb == 16 && db == 64) mnem = "movzwq";
            else if (sb == 32 && db == 32) mnem = "movl";
            else if (sb == 16 && db == 16) mnem = "movw";
            else if (sb == 8 && db == 8) mnem = "movb";

            if (!mnem)
            {
                // احتياطي: استخدم mov حسب حجم الوجهة.
                fprintf(out, "    mov%c ", size_suffix(db));
                MachineOperand src = inst->src1;
                MachineOperand dst = inst->dst;
                src.size_bits = db;
                dst.size_bits = db;
                emit_operand(&src, out);
                fprintf(out, ", ");
                emit_operand(&dst, out);
                fprintf(out, "\n");
                break;
            }

            MachineOperand src = inst->src1;
            MachineOperand dst = inst->dst;
            src.size_bits = sb;
            dst.size_bits = db;

            fprintf(out, "    %s ", mnem);
            emit_operand(&src, out);
            fprintf(out, ", ");
            emit_operand(&dst, out);
            fprintf(out, "\n");
        }
            break;

        // ================================================================
        // توسيع بالإشارة (MOVSX)
        // ================================================================
        case MACH_MOVSX:
        {
            int sb = inst->src1.size_bits;
            int db = inst->dst.size_bits;
            if (sb <= 0) sb = 8;
            if (sb == 1) sb = 8;
            if (db <= 0) db = 64;

            bool dst_mem = (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL);
            if (dst_mem)
            {
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = db;
                tmp.data.vreg = PHYS_RAX;

                MachineOperand saved_dst = inst->dst;
                MachineOperand dst2 = tmp;
                MachineOperand src2 = inst->src1;
                src2.size_bits = sb;
                dst2.size_bits = db;

                const char* mnem = NULL;
                if (sb == 8 && db == 16) mnem = "movsbw";
                else if (sb == 8 && db == 32) mnem = "movsbl";
                else if (sb == 8 && db == 64) mnem = "movsbq";
                else if (sb == 16 && db == 32) mnem = "movswl";
                else if (sb == 16 && db == 64) mnem = "movswq";
                else if (sb == 32 && db == 64) mnem = "movslq";
                else if (sb == 32 && db == 32) mnem = "movl";
                else if (sb == 16 && db == 16) mnem = "movw";
                else if (sb == 8 && db == 8) mnem = "movb";

                if (!mnem)
                {
                    fprintf(out, "    mov%c ", size_suffix(db));
                    MachineOperand ss = inst->src1;
                    MachineOperand dd = dst2;
                    ss.size_bits = db;
                    dd.size_bits = db;
                    emit_operand(&ss, out);
                    fprintf(out, ", ");
                    emit_operand(&dd, out);
                    fprintf(out, "\n");
                }
                else
                {
                    fprintf(out, "    %s ", mnem);
                    emit_operand(&src2, out);
                    fprintf(out, ", ");
                    emit_operand(&dst2, out);
                    fprintf(out, "\n");
                }

                fprintf(out, "    mov%c ", size_suffix(db));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&saved_dst, out);
                fprintf(out, "\n");
                break;
            }

            const char* mnem = NULL;
            if (sb == 8 && db == 16) mnem = "movsbw";
            else if (sb == 8 && db == 32) mnem = "movsbl";
            else if (sb == 8 && db == 64) mnem = "movsbq";
            else if (sb == 16 && db == 32) mnem = "movswl";
            else if (sb == 16 && db == 64) mnem = "movswq";
            else if (sb == 32 && db == 64) mnem = "movslq";
            else if (sb == 32 && db == 32) mnem = "movl";
            else if (sb == 16 && db == 16) mnem = "movw";
            else if (sb == 8 && db == 8) mnem = "movb";

            if (!mnem)
            {
                fprintf(out, "    mov%c ", size_suffix(db));
                MachineOperand src = inst->src1;
                MachineOperand dst = inst->dst;
                src.size_bits = db;
                dst.size_bits = db;
                emit_operand(&src, out);
                fprintf(out, ", ");
                emit_operand(&dst, out);
                fprintf(out, "\n");
                break;
            }

            MachineOperand src = inst->src1;
            MachineOperand dst = inst->dst;
            src.size_bits = sb;
            dst.size_bits = db;

            fprintf(out, "    %s ", mnem);
            emit_operand(&src, out);
            fprintf(out, ", ");
            emit_operand(&dst, out);
            fprintf(out, "\n");
        }
            break;

        // ================================================================
        // عمليات منطقية (Logical)
        // ================================================================
        case MACH_AND:
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    andq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    and%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        case MACH_OR:
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    orq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    or%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        case MACH_NOT:
            fprintf(out, "    not%c ", infer_suffix(inst));
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_XOR:
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    xorq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    xor%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        // ================================================================
        // عمليات التحكم (Control Flow)
        // ================================================================
        case MACH_JMP:
            fprintf(out, "    jmp ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_JE:
            fprintf(out, "    je ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_JNE:
            fprintf(out, "    jne ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_INLINE_ASM:
            if (inst->comment && inst->comment[0]) {
                fprintf(out, "    %s\n", inst->comment);
            }
            break;

        // ================================================================
        // استدعاء دالة (CALL)
        // ================================================================
        case MACH_CALL:
        {
            const BaaCallingConv* cc = emit_cc_or_default();

            // SystemV AMD64 varargs rule: AL = number of XMM regs used (here 0).
            // (نطبقها بشكل محافظ على كل النداءات على ELF)
            if (cc && cc->sysv_set_al_zero_on_call)
            {
                int al = inst->sysv_al;
                if (al < 0) al = 0;
                if (al == 0) {
                    fprintf(out, "    xorl %%eax, %%eax\n");
                } else {
                    fprintf(out, "    movl $%d, %%eax\n", al);
                }
            }

            // ملاحظة (v0.3.2.8.5): إدارة إطار النداء (shadow/stack args) أصبحت في ISel.
            fprintf(out, "    call ");
            // النداء غير المباشر في AT&T يتطلب نجمة: call *%reg أو call *mem.
            if (inst->src1.kind != MACH_OP_FUNC) {
                fputc('*', out);
            }
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
        }
            break;

        // ================================================================
        // رجوع (RET) - يتم إصدار الخاتمة عند هذه النقطة
        // ================================================================
        case MACH_RET:
            // الخاتمة تُصدر في emit_func() عند الرجوع
            // هنا نضع علامة فقط
            fprintf(out, "    # ret placeholder\n");
            break;

        // ================================================================
        // عمليات المكدس (Stack)
        // ================================================================
        case MACH_PUSH:
            fprintf(out, "    pushq ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_POP:
            fprintf(out, "    popq ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        default:
            fprintf(out, "    # عملية غير معالجة: %s\n", mach_op_to_string(inst->op));
            break;
    }
}

// ============================================================================
// إصدار قسم البيانات (Data Section Emission)
// ============================================================================

/**
 * @brief إصدار قسم البيانات الثابتة (صيغ الطباعة والقراءة).
 */
static void emit_rdata_section(FILE* out) {
    emit_rodata_section(out);
    fprintf(out, "fmt_int: .asciz \"%%lld\\n\"\n");
    fprintf(out, "fmt_str: .asciz \"%%s\\n\"\n");
    fprintf(out, "fmt_scan_int: .asciz \"%%lld\"\n");
}

/**
 * @brief إصدار قسم المتغيرات العامة.
 */
static const char* emit_data_dir_for_type(IRType* t, int* out_size_bytes) {
    if (out_size_bytes) *out_size_bytes = 8;
    if (!t) return ".quad";

    switch (t->kind) {
        case IR_TYPE_I1:
        case IR_TYPE_I8:
        case IR_TYPE_U8:
            if (out_size_bytes) *out_size_bytes = 1;
            return ".byte";
        case IR_TYPE_I16:
        case IR_TYPE_U16:
            if (out_size_bytes) *out_size_bytes = 2;
            return ".word";
        case IR_TYPE_I32:
        case IR_TYPE_U32:
            if (out_size_bytes) *out_size_bytes = 4;
            return ".long";
        case IR_TYPE_I64:
        case IR_TYPE_U64:
        case IR_TYPE_CHAR:
        case IR_TYPE_F64:
        case IR_TYPE_PTR:
        default:
            if (out_size_bytes) *out_size_bytes = 8;
            return ".quad";
    }
}

static void emit_data_section(MachineModule* module, FILE* out) {
    if (!module || module->global_count == 0) return;

    fprintf(out, "\n.data\n");

    for (IRGlobal* g = module->globals; g; g = g->next) {
        if (!g->name) continue;

        if (g->is_internal) {
            if (g_emit_target && g_emit_target->obj_format == BAA_OBJFORMAT_ELF) {
                fprintf(out, ".local %s\n", g->name);
            }
        } else {
            fprintf(out, ".globl %s\n", g->name);
        }

        // مصفوفة عامة
        if (g->type && g->type->kind == IR_TYPE_ARRAY) {
            IRType* elem_t = g->type->data.array.element;
            int count = g->type->data.array.count;
            int elem_size = 8;
            const char* dir = emit_data_dir_for_type(elem_t, &elem_size);
            if (count < 0) count = 0;

            // إن كانت التهيئة كلها أصفار، نستخدم .zero لتقليل حجم النص.
            int all_zero = 1;
            if (g->init_elems) {
                for (int i = 0; i < g->init_elem_count; i++) {
                    IRValue* v = g->init_elems[i];
                    if (!v) continue;
                    if (v->kind == IR_VAL_CONST_INT) {
                        if (v->data.const_int != 0) { all_zero = 0; break; }
                    } else if (v->kind == IR_VAL_CONST_STR || v->kind == IR_VAL_BAA_STR) {
                        all_zero = 0;
                        break;
                    } else if (v->kind == IR_VAL_FUNC) {
                        all_zero = 0;
                        break;
                    } else {
                        all_zero = 0;
                        break;
                    }
                }
            }

            if (all_zero) {
                int total = count * elem_size;
                if (total < 0) total = 0;
                fprintf(out, "%s: .zero %d\n", g->name, total);
            } else {
                fprintf(out, "%s:\n", g->name);
                for (int i = 0; i < count; i++) {
                    if (g->init_elems && i < g->init_elem_count && g->init_elems[i]) {
                        IRValue* iv = g->init_elems[i];
                        if (iv->kind == IR_VAL_CONST_INT) {
                            fprintf(out, "    %s %lld\n", dir, (long long)iv->data.const_int);
                            continue;
                        }
                        if (iv->kind == IR_VAL_CONST_STR && elem_t && elem_t->kind == IR_TYPE_PTR) {
                            fprintf(out, "    .quad .Lstr_%d\n", iv->data.const_str.id);
                            continue;
                        }
                        if (iv->kind == IR_VAL_BAA_STR && elem_t && elem_t->kind == IR_TYPE_PTR) {
                            fprintf(out, "    .quad .Lbs_%d\n", iv->data.const_str.id);
                            continue;
                        }
                        if (iv->kind == IR_VAL_FUNC && elem_t && elem_t->kind == IR_TYPE_FUNC) {
                            MachineOperand mop = {0};
                            mop.kind = MACH_OP_FUNC;
                            mop.data.name = iv->data.global_name;
                            fprintf(out, "    .quad ");
                            emit_operand(&mop, out);
                            fprintf(out, "\n");
                            continue;
                        }
                    }
                    fprintf(out, "    %s 0\n", dir);
                }
            }

            continue;
        }

        // إصدار القيمة الابتدائية
        if (g->init) {
            int sz = 8;
            const char* dir = emit_data_dir_for_type(g->type, &sz);

            if (g->init->kind == IR_VAL_CONST_INT) {
                fprintf(out, "%s: %s %lld\n", g->name, dir,
                        (long long)g->init->data.const_int);
            } else if (g->init->kind == IR_VAL_CONST_STR) {
                // C-String: تخزين مؤشر لسلسلة في جدول النصوص
                fprintf(out, "%s: .quad .Lstr_%d\n", g->name,
                        g->init->data.const_str.id);
            } else if (g->init->kind == IR_VAL_BAA_STR) {
                // نص باء: تخزين مؤشر لسلسلة حرف[] في جدول نصوص باء
                fprintf(out, "%s: .quad .Lbs_%d\n", g->name,
                        g->init->data.const_str.id);
            } else if (g->init->kind == IR_VAL_FUNC) {
                // مؤشر دالة: تخزين عنوان الدالة كـ .quad <symbol>
                MachineOperand mop = {0};
                mop.kind = MACH_OP_FUNC;
                mop.data.name = g->init->data.global_name;
                fprintf(out, "%s: .quad ", g->name);
                emit_operand(&mop, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "%s: %s 0\n", g->name, dir);
            }
        } else {
            int sz = 8;
            const char* dir = emit_data_dir_for_type(g->type, &sz);
            fprintf(out, "%s: %s 0\n", g->name, dir);
        }
    }
}

/**
 * @brief تهريب سلسلة نصية لتكون صالحة داخل ".asciz" في GAS.
 *
 * GAS يدعم تهريب C داخل السلاسل النصية (مثل \n و \t).
 * نحن نحتاج تهريب المحارف التي قد تكسر ملف التجميع:
 * - علامة الاقتباس "
 * - الشرطة العكسية \
 * - أسطر جديدة/تبويب/عودة عربة
 * - المحارف غير القابلة للطباعة (نستخدم \xHH)
 */
static void emit_gas_escaped_string(FILE* out, const char* s) {
    if (!out || !s) return;

    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '\"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\t': fputs("\\t", out); break;
            case '\r': fputs("\\r", out); break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    fprintf(out, "\\x%02X", (unsigned)c);
                } else {
                    fputc((int)c, out);
                }
                break;
        }
    }
}

/**
 * @brief إصدار جدول النصوص (String Table).
 */
static void emit_string_table(MachineModule* module, FILE* out) {
    if (!module || module->string_count == 0) return;

    fprintf(out, "\n");
    emit_rodata_section(out);

    for (IRStringEntry* s = module->strings; s; s = s->next) {
        if (!s->content) continue;

        fprintf(out, ".Lstr_%d: .asciz \"", s->id);
        emit_gas_escaped_string(out, s->content);
        fprintf(out, "\"\n");
    }
}

static void emit_baa_string_table(MachineModule* module, FILE* out) {
    if (!module || module->baa_string_count == 0) return;

    fprintf(out, "\n");
    emit_rodata_section(out);

    for (IRBaaStringEntry* s = module->baa_strings; s; s = s->next) {
        if (!s->content) continue;

        // محاذاة 8 بايت لأن العناصر .quad
        fprintf(out, "    .p2align 3\n");
        fprintf(out, ".Lbs_%d:\n", s->id);

        const unsigned char* p = (const unsigned char*)s->content;
        while (*p) {
            unsigned char b0 = p[0];
            int len = 0;
            if ((b0 & 0x80u) == 0x00u) len = 1;
            else if ((b0 & 0xE0u) == 0xC0u) len = 2;
            else if ((b0 & 0xF0u) == 0xE0u) len = 3;
            else if ((b0 & 0xF8u) == 0xF0u) len = 4;
            else len = 0;

            unsigned char bytes[4] = {0, 0, 0, 0};

            bool ok = true;
            if (len <= 0) {
                ok = false;
            } else {
                for (int i = 0; i < len; i++) {
                    unsigned char bi = p[i];
                    if (bi == 0) { ok = false; break; }
                    if (i > 0 && ((bi & 0xC0u) != 0x80u)) { ok = false; break; }
                    bytes[i] = bi;
                }
            }

            if (!ok) {
                // U+FFFD (EF BF BD)
                bytes[0] = 0xEF;
                bytes[1] = 0xBF;
                bytes[2] = 0xBD;
                bytes[3] = 0x00;
                len = 3;
                // تقدم بايتاً واحداً لتجنب تعليق الحلقة
                p++;
            } else {
                p += (size_t)len;
            }

            uint64_t bytes_field = (uint64_t)bytes[0] |
                                   ((uint64_t)bytes[1] << 8) |
                                   ((uint64_t)bytes[2] << 16) |
                                   ((uint64_t)bytes[3] << 24);
            uint64_t packed = bytes_field | ((uint64_t)(unsigned)len << 32);

            fprintf(out, "    .quad %llu\n", (unsigned long long)packed);
        }

        // النهاية: حرف بطول 0
        fprintf(out, "    .quad 0\n");
    }
}

// ============================================================================
// إصدار دالة كاملة (Function Emission)
// ============================================================================

/**
 * @brief جمع السجلات المحفوظة (callee-saved) المستخدمة في الدالة.
 *
 * يمشي على جميع تعليمات الدالة ويكشف أي سجلات callee-saved
 * استُخدمت كوجهة (dst) حتى نحفظها في المقدمة ونستعيدها في الخاتمة.
 */
static int collect_callee_saved(MachineFunc* func, PhysReg* regs_out) {
    // السجلات المحفوظة التي قد نحتاج لحفظها
    // RBX, RSI, RDI, R12-R15 (نرشحها حسب الهدف؛ لا نحفظ RBP لأنه يُحفظ في المقدمة)
    static const PhysReg candidates[] = {
        PHYS_RBX, PHYS_RSI, PHYS_RDI,
        PHYS_R12, PHYS_R13, PHYS_R14, PHYS_R15
    };
    static const int candidate_count = 7;

    bool used[PHYS_REG_COUNT] = {false};

    // المشي على جميع التعليمات
    for (MachineBlock* block = func->blocks; block; block = block->next) {
        for (MachineInst* inst = block->first; inst; inst = inst->next) {
            // التحقق من الوجهة
            if (inst->dst.kind == MACH_OP_VREG) {
                int r = inst->dst.data.vreg;
                if (r >= 0 && r < PHYS_REG_COUNT) used[r] = true;
            }
            // التحقق من المصادر
            if (inst->src1.kind == MACH_OP_VREG) {
                int r = inst->src1.data.vreg;
                if (r >= 0 && r < PHYS_REG_COUNT) used[r] = true;
            }
            if (inst->src2.kind == MACH_OP_VREG) {
                int r = inst->src2.data.vreg;
                if (r >= 0 && r < PHYS_REG_COUNT) used[r] = true;
            }
        }
    }

    int count = 0;
    for (int i = 0; i < candidate_count; i++) {
        if (used[candidates[i]] && emit_reg_is_callee_saved(candidates[i])) {
            regs_out[count++] = candidates[i];
        }
    }

    return count;
}

bool emit_func(MachineFunc* func, FILE* out) {
    if (!func || !out) return false;

    // تخطي النماذج الأولية
    if (func->is_prototype) return true;

    // تعيين بادئة فريدة لتسميات الكتل داخل هذه الدالة
    g_emit_current_func_uid = g_emit_next_func_uid++;
    g_emit_sp_seq = 0;

    // تحديد اسم الدالة (تحويل الرئيسية → main)
    const char* func_name = func->name;
    bool is_main = false;
    if (func_name && strcmp(func_name, "الرئيسية") == 0) {
        func_name = "main";
        is_main = true;
    }

    // إصدار تعريف الرمز العام
    fprintf(out, "\n.globl %s\n", func_name);
    fprintf(out, "%s:\n", func_name);
    emit_comment(out, "دالة: %s", func_name);

    // جمع السجلات المحفوظة المستخدمة
    PhysReg callee_regs[16];
    int callee_count = collect_callee_saved(func, callee_regs);

    // إصدار المقدمة
    emit_prologue(func, out, callee_regs, callee_count);

    // إصدار تعليمات الجسم
    for (MachineBlock* block = func->blocks; block; block = block->next) {
        for (MachineInst* inst = block->first; inst; inst = inst->next) {
            if (inst->op != MACH_LABEL && inst->op != MACH_COMMENT) {
                emit_debug_loc(out, inst);
            }
            if (inst->op == MACH_RET) {
                // عند الرجوع: إصدار قيمة الإرجاع الافتراضية إذا كانت main
                // ثم إصدار الخاتمة
                emit_epilogue(func, out, callee_regs, callee_count);
            } else if (inst->op == MACH_TAILJMP) {
                // نداء_ذيلي: تفكيك الإطار ثم قفز إلى الهدف
                emit_tailjmp(func, inst, out, callee_regs, callee_count);
            } else {
                emit_inst(inst, func, out);
            }
        }
    }

    // إذا لم تنتهِ الدالة بـ ret صريح، نضيف خاتمة افتراضية
    // (للسلامة: إرجاع 0 من main)
    {
        bool has_ret = false;
        for (MachineBlock* block = func->blocks; block; block = block->next) {
            for (MachineInst* inst = block->first; inst; inst = inst->next) {
                if (inst->op == MACH_RET || inst->op == MACH_TAILJMP) has_ret = true;
            }
        }
        if (!has_ret) {
            if (is_main) {
                fprintf(out, "    mov $0, %%rax\n");
            }
            emit_epilogue(func, out, callee_regs, callee_count);
        }
    }

    return true;
}

// ============================================================================
// إصدار وحدة كاملة (Module Emission)
// ============================================================================

bool emit_module_ex2(MachineModule* module, FILE* out, bool debug_info,
                     const BaaTarget* target, BaaCodegenOptions opts) {
    if (!module || !out) return false;

    g_emit_target = target ? target : baa_target_builtin_windows_x86_64();
    g_emit_cc = (g_emit_target && g_emit_target->cc) ? g_emit_target->cc : baa_target_builtin_windows_x86_64()->cc;
    g_emit_opts = opts;

    g_emit_debug_info = debug_info ? true : false;
    g_emit_asm_comments = opts.asm_comments ? true : false;
    emit_debug_reset();

    // إعادة تعيين بادئات الدوال لضمان حتمية أسماء التسميات داخل كل ملف .s
    g_emit_next_func_uid = 0;
    g_emit_current_func_uid = 0;

    // 1. قسم البيانات الثابتة (صيغ الطباعة)
    emit_rdata_section(out);

    // 2. قسم البيانات العامة (المتغيرات)
    emit_data_section(module, out);

    // 3. قسم النصوص (الدوال)
    fprintf(out, "\n.text\n");

    for (MachineFunc* func = module->funcs; func; func = func->next) {
        if (!emit_func(func, out)) {
            return false;
        }
    }

    // 4. جدول النصوص
    emit_string_table(module, out);
    emit_baa_string_table(module, out);

    // 5. وسم "لا مكدس تنفيذي" على ELF لتفادي تحذيرات ld
    if (g_emit_target && g_emit_target->obj_format == BAA_OBJFORMAT_ELF)
    {
        fprintf(out, "\n.section .note.GNU-stack,\"\",@progbits\n");
    }

    emit_debug_reset();

    return true;
}

bool emit_module_ex(MachineModule* module, FILE* out, bool debug_info, const BaaTarget* target) {
    return emit_module_ex2(module, out, debug_info, target, baa_codegen_options_default());
}

bool emit_module(MachineModule* module, FILE* out, bool debug_info) {
    return emit_module_ex(module, out, debug_info, baa_target_builtin_windows_x86_64());
}
