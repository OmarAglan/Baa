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
