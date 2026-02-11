/**
 * @file emit.c
 * @brief إصدار كود التجميع - تحويل تمثيل الآلة إلى نص تجميع AT&T (v0.3.2.3)
 *
 * يحوّل وحدة آلية (MachineModule) بعد تخصيص السجلات إلى ملف تجميع
 * x86-64 بصيغة AT&T متوافق مع GAS (GNU Assembler) على Windows.
 *
 * الخرج يتضمن:
 * - قسم .rdata: صيغ الطباعة (fmt_int, fmt_str, fmt_scan_int)
 * - قسم .data: المتغيرات العامة
 * - قسم .text: الدوال مع prologue/epilogue
 * - جدول النصوص: سلاسل نصية ثابتة (.Lstr_N)
 *
 * اتفاقية Windows x64 ABI:
 * - المعاملات: RCX, RDX, R8, R9 (أول 4)، ثم المكدس
 * - القيمة المرجعة: RAX
 * - Shadow space: 32 بايت قبل كل استدعاء
 * - السجلات المحفوظة (callee-saved): RBX, RBP, RDI, RSI, R12-R15
 * - محاذاة المكدس: 16 بايت
 */

#include "emit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
 * @brief أسماء السجلات 8-بت بصيغة AT&T (الجزء السفلي).
 */
static const char* reg8_names[PHYS_REG_COUNT] = {
    "%al",  "%cl",  "%dl",  "%bl",  "%spl", "%bpl",
    "%sil", "%dil", "%r8b",  "%r9b",  "%r10b", "%r11b",
    "%r12b", "%r13b", "%r14b", "%r15b"
};

/**
 * @brief معرفات داخلية لتفادي تعارض تسميات الكتل بين الدوال.
 *
 * في GAS، التسميات المحلية مثل .LBB_0 تكون على مستوى الملف كله،
 * لذا قد تتعارض بين دوال مختلفة إذا كانت أرقام الكتل تبدأ من الصفر
 * داخل كل دالة. الحل: إضافة بادئة رقمية لكل دالة أثناء الإصدار.
 */
static int g_emit_next_func_uid = 0;
static int g_emit_current_func_uid = 0;

// هل إصدار معلومات الديبغ مفعل؟
static bool g_emit_debug_info = false;

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
    // حفظ إطار المكدس
    fprintf(out, "    push %%rbp\n");
    fprintf(out, "    mov %%rsp, %%rbp\n");

    // حساب حجم المكدس الإجمالي:
    // حجم محلي (func->stack_size) + shadow space (32) + حفظ callee-saved
    // مع محاذاة 16 بايت
    int local_size = func->stack_size;
    int callee_save_size = callee_count * 8;

    // إجمالي الحجم المطلوب (بعد push rbp)
    // المكدس بعد push rbp: RSP ناقص 8 (لـ rbp المحفوظ)
    // نحتاج: local + shadow(32) + callee_save + محاذاة
    int total_frame = local_size + 32 + callee_save_size;

    // محاذاة إلى 16 بايت
    // بعد push rbp، RSP = aligned - 8
    // نحتاج sub ليجعل RSP محاذى لـ 16
    if (total_frame % 16 != 0) {
        total_frame = ((total_frame / 16) + 1) * 16;
    }

    if (total_frame > 0) {
        fprintf(out, "    sub $%d, %%rsp\n", total_frame);
    }

    // حفظ السجلات المحفوظة (callee-saved)
    for (int i = 0; i < callee_count; i++) {
        int save_offset = -(local_size + 32 + (i + 1) * 8);
        fprintf(out, "    mov %s, %d(%%rbp)\n",
                reg64_names[callee_regs[i]], save_offset);
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
    int local_size = func->stack_size;

    // استعادة السجلات المحفوظة (callee-saved) بترتيب عكسي
    for (int i = callee_count - 1; i >= 0; i--) {
        int save_offset = -(local_size + 32 + (i + 1) * 8);
        fprintf(out, "    mov %d(%%rbp), %s\n",
                save_offset, reg64_names[callee_regs[i]]);
    }

    // استعادة المكدس والرجوع
    fprintf(out, "    leave\n");
    fprintf(out, "    ret\n");
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
            fprintf(out, "    addq ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SUB:
            fprintf(out, "    subq ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_IMUL:
            fprintf(out, "    imulq ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_NEG:
            fprintf(out, "    negq ");
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

        // ================================================================
        // تحميل العنوان الفعّال (LEA)
        // ================================================================
        case MACH_LEA:
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
            // تحميل من [ptr] أو من متغير عام
            if (inst->src1.kind == MACH_OP_MEM) {
                fprintf(out, "    movq ");
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else if (inst->src1.kind == MACH_OP_GLOBAL) {
                fprintf(out, "    movq ");
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                // حالة احتياطية: mov من سجل (لا ينبغي أن تحدث عادةً)
                fprintf(out, "    movq ");
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        // ================================================================
        // تخزين في الذاكرة (STORE)
        // ================================================================
        case MACH_STORE:
            // تخزين: mov src1, [dst]
            if (inst->dst.kind == MACH_OP_MEM) {
                fprintf(out, "    movq ");
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else if (inst->dst.kind == MACH_OP_GLOBAL) {
                fprintf(out, "    movq ");
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    movq ");
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        // ================================================================
        // عمليات المقارنة (Comparison)
        // ================================================================
        case MACH_CMP:
            // AT&T: cmp src2, src1 (يقارن src1 مع src2)
            fprintf(out, "    cmp%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_TEST:
            // AT&T: test src2, src1
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

        // ================================================================
        // توسيع بالأصفار (MOVZX)
        // ================================================================
        case MACH_MOVZX:
            // movzbq src8, dst64
            fprintf(out, "    movzbq ");
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            // الوجهة دائماً 64-بت لـ movzbq
            {
                MachineOperand dst64 = inst->dst;
                dst64.size_bits = 64;
                emit_operand(&dst64, out);
            }
            fprintf(out, "\n");
            break;

        // ================================================================
        // عمليات منطقية (Logical)
        // ================================================================
        case MACH_AND:
            fprintf(out, "    andq ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_OR:
            fprintf(out, "    orq ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_NOT:
            fprintf(out, "    notq ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_XOR:
            fprintf(out, "    xorq ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
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

        // ================================================================
        // استدعاء دالة (CALL)
        // ================================================================
        case MACH_CALL:
            // حجز shadow space قبل الاستدعاء (Windows x64 ABI)
            // ملاحظة مهمة: لبعض الدوال المتغيرة (مثل printf/scanf) تعتمد المكتبة على "home space"
            // لقراءة المعاملات، لذا نكتب RCX/RDX/R8/R9 إلى هذا الحيز قبل call.
            fprintf(out, "    sub $32, %%rsp\n");
            fprintf(out, "    movq %%rcx, 0(%%rsp)\n");
            fprintf(out, "    movq %%rdx, 8(%%rsp)\n");
            fprintf(out, "    movq %%r8, 16(%%rsp)\n");
            fprintf(out, "    movq %%r9, 24(%%rsp)\n");
            fprintf(out, "    call ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            fprintf(out, "    add $32, %%rsp\n");
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
    fprintf(out, ".section .rdata,\"dr\"\n");
    fprintf(out, "fmt_int: .asciz \"%%d\\n\"\n");
    fprintf(out, "fmt_str: .asciz \"%%s\\n\"\n");
    fprintf(out, "fmt_scan_int: .asciz \"%%d\"\n");
}

/**
 * @brief إصدار قسم المتغيرات العامة.
 */
static void emit_data_section(MachineModule* module, FILE* out) {
    if (!module || module->global_count == 0) return;

    fprintf(out, "\n.data\n");

    for (IRGlobal* g = module->globals; g; g = g->next) {
        if (!g->name) continue;

        // إصدار القيمة الابتدائية
        if (g->init) {
            if (g->init->kind == IR_VAL_CONST_INT) {
                fprintf(out, "%s: .quad %lld\n", g->name,
                        (long long)g->init->data.const_int);
            } else if (g->init->kind == IR_VAL_CONST_STR) {
                // نص: تخزين مؤشر لسلسلة في جدول النصوص
                fprintf(out, "%s: .quad .Lstr_%d\n", g->name,
                        g->init->data.const_str.id);
            } else {
                fprintf(out, "%s: .quad 0\n", g->name);
            }
        } else {
            fprintf(out, "%s: .quad 0\n", g->name);
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

    fprintf(out, "\n.section .rdata,\"dr\"\n");

    for (IRStringEntry* s = module->strings; s; s = s->next) {
        if (!s->content) continue;

        fprintf(out, ".Lstr_%d: .asciz \"", s->id);
        emit_gas_escaped_string(out, s->content);
        fprintf(out, "\"\n");
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
    // RBX, RSI, RDI, R12-R15 (لا نحفظ RBP لأنه يُحفظ في المقدمة)
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
        if (used[candidates[i]]) {
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
                if (inst->op == MACH_RET) has_ret = true;
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

bool emit_module(MachineModule* module, FILE* out, bool debug_info) {
    if (!module || !out) return false;

    g_emit_debug_info = debug_info ? true : false;
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

    emit_debug_reset();

    return true;
}
