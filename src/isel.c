/**
 * @file isel.c
 * @brief اختيار التعليمات - تحويل IR إلى تعليمات الآلة (v0.3.2.1)
 *
 * يحوّل كل تعليمة IR إلى تعليمة آلة أو أكثر لمعمارية x86-64.
 * يدعم:
 * - تضمين القيم الفورية (immediates) في التعليمات مباشرة
 * - اختيار أنماط التعليمات المثلى لكل عملية IR
 * - التعامل مع اتفاقية استدعاء Windows x64 ABI
 * - القسمة باستخدام CQO + IDIV
 * - المقارنات باستخدام CMP + SETcc + MOVZX
 */

#include "isel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// بناء المعاملات (Operand Construction)
// ============================================================================

MachineOperand mach_op_none(void) {
    MachineOperand op = {0};
    op.kind = MACH_OP_NONE;
    op.size_bits = 0;
    return op;
}

MachineOperand mach_op_vreg(int vreg, int bits) {
    MachineOperand op = {0};
    op.kind = MACH_OP_VREG;
    op.size_bits = bits;
    op.data.vreg = vreg;
    return op;
}

MachineOperand mach_op_imm(int64_t imm, int bits) {
    MachineOperand op = {0};
    op.kind = MACH_OP_IMM;
    op.size_bits = bits;
    op.data.imm = imm;
    return op;
}

MachineOperand mach_op_mem(int base_vreg, int32_t offset, int bits) {
    MachineOperand op = {0};
    op.kind = MACH_OP_MEM;
    op.size_bits = bits;
    op.data.mem.base_vreg = base_vreg;
    op.data.mem.offset = offset;
    return op;
}

MachineOperand mach_op_label(int label_id) {
    MachineOperand op = {0};
    op.kind = MACH_OP_LABEL;
    op.size_bits = 0;
    op.data.label_id = label_id;
    return op;
}

MachineOperand mach_op_global(const char* name) {
    MachineOperand op = {0};
    op.kind = MACH_OP_GLOBAL;
    op.size_bits = 64;
    op.data.name = name ? strdup(name) : NULL;
    return op;
}

MachineOperand mach_op_func(const char* name) {
    MachineOperand op = {0};
    op.kind = MACH_OP_FUNC;
    op.size_bits = 64;
    op.data.name = name ? strdup(name) : NULL;
    return op;
}

// ============================================================================
// بناء التعليمات (Instruction Construction)
// ============================================================================

MachineInst* mach_inst_new(MachineOp op, MachineOperand dst,
                           MachineOperand src1, MachineOperand src2) {
    MachineInst* inst = calloc(1, sizeof(MachineInst));
    if (!inst) return NULL;
    inst->op = op;
    inst->dst = dst;
    inst->src1 = src1;
    inst->src2 = src2;
    inst->ir_reg = -1;
    inst->comment = NULL;
    inst->prev = NULL;
    inst->next = NULL;
    return inst;
}

/**
 * @brief تحرير اسم المعامل إذا كان مخصصاً ديناميكياً.
 */
static void mach_operand_cleanup(MachineOperand* op) {
    if (!op) return;
    if ((op->kind == MACH_OP_GLOBAL || op->kind == MACH_OP_FUNC) && op->data.name) {
        free(op->data.name);
        op->data.name = NULL;
    }
}

void mach_inst_free(MachineInst* inst) {
    if (!inst) return;
    mach_operand_cleanup(&inst->dst);
    mach_operand_cleanup(&inst->src1);
    mach_operand_cleanup(&inst->src2);
    // لا نحرر comment لأنه عادة سلسلة ثابتة
    free(inst);
}

// ============================================================================
// بناء الكتل (Block Construction)
// ============================================================================

MachineBlock* mach_block_new(const char* label, int id) {
    MachineBlock* block = calloc(1, sizeof(MachineBlock));
    if (!block) return NULL;
    block->label = label ? strdup(label) : NULL;
    block->id = id;
    block->first = NULL;
    block->last = NULL;
    block->inst_count = 0;
    block->succ_count = 0;
    block->succs[0] = NULL;
    block->succs[1] = NULL;
    block->next = NULL;
    return block;
}

void mach_block_append(MachineBlock* block, MachineInst* inst) {
    if (!block || !inst) return;
    inst->prev = block->last;
    inst->next = NULL;
    if (block->last) {
        block->last->next = inst;
    } else {
        block->first = inst;
    }
    block->last = inst;
    block->inst_count++;
}

void mach_block_free(MachineBlock* block) {
    if (!block) return;
    // تحرير جميع التعليمات
    MachineInst* inst = block->first;
    while (inst) {
        MachineInst* next = inst->next;
        mach_inst_free(inst);
        inst = next;
    }
    free(block->label);
    free(block);
}

// ============================================================================
// بناء الدوال (Function Construction)
// ============================================================================

MachineFunc* mach_func_new(const char* name) {
    MachineFunc* func = calloc(1, sizeof(MachineFunc));
    if (!func) return NULL;
    func->name = name ? strdup(name) : NULL;
    func->is_prototype = false;
    func->entry = NULL;
    func->blocks = NULL;
    func->block_count = 0;
    func->next_vreg = 0;
    func->stack_size = 0;
    func->param_count = 0;
    func->next = NULL;
    return func;
}

int mach_func_alloc_vreg(MachineFunc* func) {
    if (!func) return -1;
    return func->next_vreg++;
}

void mach_func_add_block(MachineFunc* func, MachineBlock* block) {
    if (!func || !block) return;
    // إضافة إلى نهاية القائمة المترابطة
    if (!func->blocks) {
        func->blocks = block;
        func->entry = block;
    } else {
        MachineBlock* cur = func->blocks;
        while (cur->next) cur = cur->next;
        cur->next = block;
    }
    func->block_count++;
}

void mach_func_free(MachineFunc* func) {
    if (!func) return;
    MachineBlock* block = func->blocks;
    while (block) {
        MachineBlock* next = block->next;
        mach_block_free(block);
        block = next;
    }
    free(func->name);
    free(func);
}

// ============================================================================
// بناء الوحدة (Module Construction)
// ============================================================================

MachineModule* mach_module_new(const char* name) {
    MachineModule* module = calloc(1, sizeof(MachineModule));
    if (!module) return NULL;
    module->name = name ? strdup(name) : NULL;
    module->funcs = NULL;
    module->func_count = 0;
    module->globals = NULL;
    module->global_count = 0;
    module->strings = NULL;
    module->string_count = 0;
    return module;
}

void mach_module_add_func(MachineModule* module, MachineFunc* func) {
    if (!module || !func) return;
    if (!module->funcs) {
        module->funcs = func;
    } else {
        MachineFunc* cur = module->funcs;
        while (cur->next) cur = cur->next;
        cur->next = func;
    }
    module->func_count++;
}

void mach_module_free(MachineModule* module) {
    if (!module) return;
    MachineFunc* func = module->funcs;
    while (func) {
        MachineFunc* next = func->next;
        mach_func_free(func);
        func = next;
    }
    // لا نحرر globals و strings لأنها مملوكة من وحدة IR
    free(module->name);
    free(module);
}

// ============================================================================
// سياق اختيار التعليمات (ISel Context)
// ============================================================================

/**
 * @struct ISelCtx
 * @brief سياق داخلي لعملية اختيار التعليمات.
 */
typedef struct {
    MachineModule* mmod;        // الوحدة الآلية الناتجة
    MachineFunc* mfunc;         // الدالة الآلية الحالية
    MachineBlock* mblock;       // الكتلة الآلية الحالية

    IRModule* ir_module;        // وحدة IR المصدر
    IRFunc* ir_func;            // دالة IR الحالية
    IRBlock* ir_block;          // كتلة IR الحالية
} ISelCtx;

// ============================================================================
// دوال مساعدة لاختيار التعليمات
// ============================================================================

/**
 * @brief تحديد حجم البتات لنوع IR.
 */
static int isel_type_bits(IRType* type) {
    if (!type) return 64;
    switch (type->kind) {
        case IR_TYPE_I1:    return 8;   // نستخدم بايت كامل للمنطقي
        case IR_TYPE_I8:    return 8;
        case IR_TYPE_I16:   return 16;
        case IR_TYPE_I32:   return 32;
        case IR_TYPE_I64:   return 64;
        case IR_TYPE_PTR:   return 64;
        default:            return 64;
    }
}

/**
 * @brief تحويل قيمة IR إلى معامل آلة.
 *
 * إذا كانت القيمة ثابتاً، يتم تضمينها كقيمة فورية.
 * إذا كانت سجلاً، يتم استخدام سجل افتراضي.
 */
static MachineOperand isel_lower_value(ISelCtx* ctx, IRValue* val) {
    if (!val) return mach_op_none();

    int bits = isel_type_bits(val->type);

    switch (val->kind) {
        case IR_VAL_CONST_INT:
            // تضمين القيمة الفورية مباشرة
            return mach_op_imm(val->data.const_int, bits);

        case IR_VAL_REG:
            return mach_op_vreg(val->data.reg_num, bits);

        case IR_VAL_GLOBAL:
            return mach_op_global(val->data.global_name);

        case IR_VAL_FUNC:
            return mach_op_func(val->data.global_name);

        case IR_VAL_BLOCK:
            if (val->data.block)
                return mach_op_label(val->data.block->id);
            return mach_op_none();

        case IR_VAL_CONST_STR:
            // النصوص تُعالج كمعاملات عامة مع معرف خاص
            {
                char label_buf[64];
                snprintf(label_buf, sizeof(label_buf), ".Lstr_%d", val->data.const_str.id);
                return mach_op_global(label_buf);
            }

        default:
            return mach_op_none();
    }
}

/**
 * @brief إصدار تعليمة آلة في الكتلة الحالية.
 */
static MachineInst* isel_emit(ISelCtx* ctx, MachineOp op,
                               MachineOperand dst, MachineOperand src1,
                               MachineOperand src2) {
    MachineInst* inst = mach_inst_new(op, dst, src1, src2);
    if (!inst) return NULL;
    mach_block_append(ctx->mblock, inst);
    return inst;
}

/**
 * @brief إصدار تعليمة آلة مع تعليق.
 */
static MachineInst* isel_emit_comment(ISelCtx* ctx, MachineOp op,
                                       MachineOperand dst, MachineOperand src1,
                                       MachineOperand src2, const char* comment) {
    MachineInst* inst = isel_emit(ctx, op, dst, src1, src2);
    if (inst) inst->comment = comment;
    return inst;
}

// ============================================================================
// اختيار التعليمات لكل عملية IR
// ============================================================================

/**
 * @brief خفض عملية حسابية ثنائية (جمع، طرح، ضرب).
 *
 * النمط: %dst = op type lhs, rhs
 * →  mov vreg_dst, lhs
 *    op  vreg_dst, rhs
 *
 * تحسين: إذا كان أحد المعاملين قيمة فورية وكان نفس سجل الوجهة
 * يمكننا استخدامها مباشرة.
 */
static void isel_lower_binop(ISelCtx* ctx, IRInst* inst, MachineOp mop) {
    if (!inst || inst->operand_count < 2) return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // نقل القيمة اليسرى إلى سجل الوجهة
    isel_emit(ctx, MACH_MOV, dst, lhs, mach_op_none());

    // تنفيذ العملية
    MachineInst* mi = isel_emit(ctx, mop, dst, dst, rhs);
    if (mi) mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية القسمة والباقي.
 *
 * القسمة في x86-64 تستخدم idiv:
 *   cqo          ; توسيع الإشارة RAX → RDX:RAX
 *   idiv divisor ; RAX = حاصل القسمة، RDX = الباقي
 *
 * نمثل ذلك بسجلات افتراضية ونترك تخصيص السجلات لاحقاً.
 *
 * النمط: %dst = قسم type lhs, rhs
 * →  mov vreg_dst, lhs
 *    cqo
 *    idiv rhs
 *    (الناتج في vreg_dst لحاصل القسمة أو vreg مؤقت للباقي)
 */
static void isel_lower_div(ISelCtx* ctx, IRInst* inst, bool is_mod) {
    if (!inst || inst->operand_count < 2) return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // إذا كان القاسم قيمة فورية، ننقله إلى سجل مؤقت
    // لأن idiv لا يقبل قيمة فورية مباشرة
    MachineOperand divisor = rhs;
    if (rhs.kind == MACH_OP_IMM) {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, bits);
        isel_emit(ctx, MACH_MOV, tmp_op, rhs, mach_op_none());
        divisor = tmp_op;
    }

    // نقل المقسوم إلى سجل الوجهة (سيصبح RAX لاحقاً)
    isel_emit_comment(ctx, MACH_MOV, dst, lhs, mach_op_none(),
                      is_mod ? "// باقي: تحضير المقسوم" : "// قسمة: تحضير المقسوم");

    // توسيع الإشارة
    isel_emit(ctx, MACH_CQO, mach_op_none(), dst, mach_op_none());

    // القسمة
    MachineInst* mi = isel_emit(ctx, MACH_IDIV, dst, divisor, mach_op_none());
    if (mi) {
        mi->ir_reg = inst->dest;
        mi->comment = is_mod ? "// باقي القسمة في RDX" : "// حاصل القسمة في RAX";
    }

    // ملاحظة: لعملية الباقي (MOD)، الناتج الفعلي يكون في RDX
    // هذا سيُعالج في مرحلة تخصيص السجلات (v0.3.2.2)
}

/**
 * @brief خفض عملية السالب (negation).
 *
 * النمط: %dst = سالب type operand
 * →  mov vreg_dst, operand
 *    neg vreg_dst
 */
static void isel_lower_neg(ISelCtx* ctx, IRInst* inst) {
    if (!inst || inst->operand_count < 1) return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

    isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
    MachineInst* mi = isel_emit(ctx, MACH_NEG, dst, dst, mach_op_none());
    if (mi) mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية تخصيص المكدس (alloca).
 *
 * النمط: %dst = حجز type
 * →  lea vreg_dst, [rbp - offset]
 *
 * نحسب الإزاحة بناءً على حجم النوع ونضيفها لحجم المكدس.
 */
static void isel_lower_alloca(ISelCtx* ctx, IRInst* inst) {
    if (!inst) return;

    int bits = isel_type_bits(inst->type);
    int alloc_size = (bits + 7) / 8;   // حجم التخصيص بالبايت
    if (alloc_size < 8) alloc_size = 8; // محاذاة 8 بايت كحد أدنى

    ctx->mfunc->stack_size += alloc_size;

    MachineOperand dst = mach_op_vreg(inst->dest, 64); // المؤشر دائماً 64 بت
    MachineOperand mem = mach_op_mem(-1, -(int32_t)ctx->mfunc->stack_size, 64);
    // base_vreg = -1 يعني RBP (سيُحل في مرحلة إصدار الكود)

    MachineInst* mi = isel_emit(ctx, MACH_LEA, dst, mem, mach_op_none());
    if (mi) {
        mi->ir_reg = inst->dest;
        mi->comment = "// حجز مكان في المكدس";
    }
}

/**
 * @brief خفض عملية التحميل (load).
 *
 * النمط: %dst = حمل type, ptr
 * →  mov vreg_dst, [ptr]
 */
static void isel_lower_load(ISelCtx* ctx, IRInst* inst) {
    if (!inst || inst->operand_count < 1) return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand ptr = isel_lower_value(ctx, inst->operands[0]);

    // إذا كان المعامل سجلاً، نولد load من الذاكرة
    if (ptr.kind == MACH_OP_VREG) {
        MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
        MachineInst* mi = isel_emit(ctx, MACH_LOAD, dst, mem, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
    } else if (ptr.kind == MACH_OP_GLOBAL) {
        // تحميل من متغير عام
        MachineInst* mi = isel_emit(ctx, MACH_LOAD, dst, ptr, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
    } else {
        // حالة احتياطية: نقل مباشر
        MachineInst* mi = isel_emit(ctx, MACH_MOV, dst, ptr, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
    }
}

/**
 * @brief خفض عملية التخزين (store).
 *
 * النمط: خزن value, ptr
 * →  mov [ptr], value
 */
static void isel_lower_store(ISelCtx* ctx, IRInst* inst) {
    if (!inst || inst->operand_count < 2) return;

    MachineOperand val = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand ptr = isel_lower_value(ctx, inst->operands[1]);
    int bits = 64;
    if (inst->operands[0] && inst->operands[0]->type)
        bits = isel_type_bits(inst->operands[0]->type);

    // إذا كانت القيمة فورية، نحتاج لنقلها لسجل أولاً
    // (لا يمكن تخزين فوري مباشرة في بعض الحالات)
    MachineOperand store_val = val;
    if (val.kind == MACH_OP_IMM && ptr.kind == MACH_OP_VREG) {
        // يمكن لـ x86-64 تخزين فوري مباشرة في الذاكرة
        MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
        isel_emit(ctx, MACH_STORE, mem, store_val, mach_op_none());
        return;
    }

    if (ptr.kind == MACH_OP_VREG) {
        MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
        isel_emit(ctx, MACH_STORE, mem, store_val, mach_op_none());
    } else if (ptr.kind == MACH_OP_GLOBAL) {
        isel_emit(ctx, MACH_STORE, ptr, store_val, mach_op_none());
    } else {
        // حالة احتياطية
        isel_emit(ctx, MACH_STORE, ptr, store_val, mach_op_none());
    }
}

/**
 * @brief خفض عملية المقارنة.
 *
 * النمط: %dst = قارن pred type lhs, rhs
 * →  cmp lhs, rhs
 *    setCC tmp8
 *    movzx vreg_dst, tmp8
 *
 * تحسين: إذا كان المعامل الأيمن قيمة فورية، نستخدمها مباشرة مع cmp.
 */
static void isel_lower_cmp(ISelCtx* ctx, IRInst* inst) {
    if (!inst || inst->operand_count < 2) return;

    int bits = isel_type_bits(inst->operands[0] ? inst->operands[0]->type : NULL);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // إذا كان الطرف الأيسر قيمة فورية، ننقله لسجل مؤقت
    // (cmp لا يقبل فوري كمعامل أول)
    if (lhs.kind == MACH_OP_IMM) {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, bits);
        isel_emit(ctx, MACH_MOV, tmp_op, lhs, mach_op_none());
        lhs = tmp_op;
    }

    // تنفيذ المقارنة
    isel_emit(ctx, MACH_CMP, mach_op_none(), lhs, rhs);

    // اختيار تعليمة setCC المناسبة
    MachineOp setcc;
    switch (inst->cmp_pred) {
        case IR_CMP_EQ: setcc = MACH_SETE;  break;
        case IR_CMP_NE: setcc = MACH_SETNE; break;
        case IR_CMP_GT: setcc = MACH_SETG;  break;
        case IR_CMP_LT: setcc = MACH_SETL;  break;
        case IR_CMP_GE: setcc = MACH_SETGE; break;
        case IR_CMP_LE: setcc = MACH_SETLE; break;
        default:        setcc = MACH_SETE;  break;
    }

    // setCC يكتب بايت واحد
    MachineOperand dst8 = mach_op_vreg(inst->dest, 8);
    isel_emit(ctx, setcc, dst8, mach_op_none(), mach_op_none());

    // توسيع بالأصفار إلى حجم كامل
    MachineOperand dst64 = mach_op_vreg(inst->dest, 64);
    MachineInst* mi = isel_emit(ctx, MACH_MOVZX, dst64, dst8, mach_op_none());
    if (mi) mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عمليات منطقية (AND, OR, NOT).
 */
static void isel_lower_logical(ISelCtx* ctx, IRInst* inst) {
    if (!inst) return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);

    if (inst->op == IR_OP_NOT) {
        // نفي: not dst
        if (inst->operand_count < 1) return;
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);
        isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
        MachineInst* mi = isel_emit(ctx, MACH_NOT, dst, dst, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
    } else {
        // AND أو OR
        if (inst->operand_count < 2) return;
        MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
        MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);
        MachineOp mop = (inst->op == IR_OP_AND) ? MACH_AND : MACH_OR;
        isel_emit(ctx, MACH_MOV, dst, lhs, mach_op_none());
        MachineInst* mi = isel_emit(ctx, mop, dst, dst, rhs);
        if (mi) mi->ir_reg = inst->dest;
    }
}

/**
 * @brief خفض عملية القفز غير المشروط.
 *
 * النمط: قفز target
 * →  jmp label
 */
static void isel_lower_br(ISelCtx* ctx, IRInst* inst) {
    if (!inst || inst->operand_count < 1) return;

    MachineOperand target = isel_lower_value(ctx, inst->operands[0]);
    isel_emit(ctx, MACH_JMP, target, mach_op_none(), mach_op_none());
}

/**
 * @brief خفض عملية القفز المشروط.
 *
 * النمط: قفز_شرط cond, if_true, if_false
 * →  test cond, cond    (أو cmp cond, 0)
 *    jne  if_true
 *    jmp  if_false
 */
static void isel_lower_br_cond(ISelCtx* ctx, IRInst* inst) {
    if (!inst || inst->operand_count < 3) return;

    MachineOperand cond = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand if_true = isel_lower_value(ctx, inst->operands[1]);
    MachineOperand if_false = isel_lower_value(ctx, inst->operands[2]);

    // إذا كان الشرط قيمة فورية، ننقله لسجل
    if (cond.kind == MACH_OP_IMM) {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_MOV, tmp_op, cond, mach_op_none());
        cond = tmp_op;
    }

    // فحص الشرط
    isel_emit(ctx, MACH_TEST, mach_op_none(), cond, cond);

    // قفز إلى الكتلة الصحيحة
    isel_emit(ctx, MACH_JNE, if_true, mach_op_none(), mach_op_none());
    isel_emit(ctx, MACH_JMP, if_false, mach_op_none(), mach_op_none());
}

/**
 * @brief خفض عملية الرجوع.
 *
 * النمط: رجوع value
 * →  mov vreg_ret, value   (إذا كانت هناك قيمة)
 *    ret
 */
static void isel_lower_ret(ISelCtx* ctx, IRInst* inst) {
    if (!inst) return;

    if (inst->operand_count > 0 && inst->operands[0]) {
        MachineOperand val = isel_lower_value(ctx, inst->operands[0]);
        // نضع القيمة المرجعة في سجل خاص (سيصبح RAX في تخصيص السجلات)
        // نستخدم vreg -2 كمؤشر لسجل القيمة المرجعة
        MachineOperand ret_reg = mach_op_vreg(-2, isel_type_bits(inst->operands[0]->type));
        isel_emit_comment(ctx, MACH_MOV, ret_reg, val, mach_op_none(),
                          "// قيمة الإرجاع → RAX");
    }

    isel_emit(ctx, MACH_RET, mach_op_none(), mach_op_none(), mach_op_none());
}

/**
 * @brief خفض عملية الاستدعاء.
 *
 * اتفاقية Windows x64 ABI:
 * - المعاملات الأربعة الأولى: RCX, RDX, R8, R9
 * - المعاملات الإضافية: على المكدس
 * - القيمة المرجعة: RAX
 * - Shadow space: 32 بايت
 *
 * النمط: %dst = نداء ret_type @target(args...)
 * →  mov arg_reg1, arg1     (أول 4 معاملات في سجلات)
 *    push argN              (المعاملات الإضافية)
 *    call target
 *    mov vreg_dst, ret_reg  (إذا كانت هناك قيمة مرجعة)
 */
static void isel_lower_call(ISelCtx* ctx, IRInst* inst) {
    if (!inst) return;

    // تحضير المعاملات (نضعها في سجلات مرقمة خاصة)
    // سجلات ABI ستُخصص لاحقاً في v0.3.2.2
    for (int i = 0; i < inst->call_arg_count && i < 4; i++) {
        MachineOperand arg = isel_lower_value(ctx, inst->call_args[i]);
        int bits = 64;
        if (inst->call_args[i] && inst->call_args[i]->type)
            bits = isel_type_bits(inst->call_args[i]->type);

        // سجل المعامل: نستخدم أرقام سالبة خاصة (-10, -11, -12, -13)
        // لتمثيل RCX, RDX, R8, R9 (سيُحل لاحقاً)
        MachineOperand param_reg = mach_op_vreg(-(10 + i), bits);
        isel_emit(ctx, MACH_MOV, param_reg, arg, mach_op_none());
    }

    // المعاملات الإضافية على المكدس (من اليمين لليسار)
    for (int i = inst->call_arg_count - 1; i >= 4; i--) {
        MachineOperand arg = isel_lower_value(ctx, inst->call_args[i]);
        isel_emit(ctx, MACH_PUSH, mach_op_none(), arg, mach_op_none());
    }

    // استدعاء الدالة
    MachineOperand target = mach_op_func(inst->call_target);
    isel_emit(ctx, MACH_CALL, mach_op_none(), target, mach_op_none());

    // نقل القيمة المرجعة (إذا وجدت)
    if (inst->dest >= 0 && inst->type && inst->type->kind != IR_TYPE_VOID) {
        int bits = isel_type_bits(inst->type);
        MachineOperand dst = mach_op_vreg(inst->dest, bits);
        MachineOperand ret_reg = mach_op_vreg(-2, bits); // RAX
        MachineInst* mi = isel_emit_comment(ctx, MACH_MOV, dst, ret_reg, mach_op_none(),
                                             "// القيمة المرجعة من RAX");
        if (mi) mi->ir_reg = inst->dest;
    }
}

/**
 * @brief خفض عملية النسخ (copy).
 *
 * النمط: %dst = نسخ type src
 * →  mov vreg_dst, src
 */
static void isel_lower_copy(ISelCtx* ctx, IRInst* inst) {
    if (!inst || inst->operand_count < 1) return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

    MachineInst* mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
    if (mi) mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عقدة فاي (phi).
 *
 * عقد فاي تُحل عادةً أثناء خروج SSA أو تخصيص السجلات.
 * حالياً، نولد تعليمات mov مشروحة كتعليقات.
 *
 * ملاحظة: في التنفيذ الكامل، يجب إدراج النسخ في الكتل السابقة.
 */
static void isel_lower_phi(ISelCtx* ctx, IRInst* inst) {
    if (!inst) return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);

    // نولد تعليمة NOP كعنصر نائب لعقدة فاي
    // سيتم حل هذا في مرحلة خروج SSA (v0.3.2.2+)
    MachineInst* mi = isel_emit_comment(ctx, MACH_NOP, dst, mach_op_none(), mach_op_none(),
                                         "// فاي - سيُحل في تخصيص السجلات");
    if (mi) mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية التحويل (cast).
 *
 * النمط: %dst = تحويل from_type to to_type value
 * →  mov/movzx/movsx vreg_dst, value
 */
static void isel_lower_cast(ISelCtx* ctx, IRInst* inst) {
    if (!inst || inst->operand_count < 1) return;

    int dst_bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, dst_bits);
    MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

    int src_bits = src.size_bits;

    if (src_bits < dst_bits && src_bits > 0) {
        // توسيع بالأصفار
        MachineInst* mi = isel_emit(ctx, MACH_MOVZX, dst, src, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
    } else {
        // نسخ مباشر (نفس الحجم أو تقليص)
        MachineInst* mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
    }
}

// ============================================================================
// خفض تعليمة IR واحدة
// ============================================================================

/**
 * @brief خفض تعليمة IR واحدة إلى تعليمات آلة.
 *
 * هذه هي الدالة المحورية لاختيار التعليمات. تقوم بتوزيع
 * كل عملية IR إلى دالة الخفض المناسبة.
 */
static void isel_lower_inst(ISelCtx* ctx, IRInst* inst) {
    if (!inst) return;

    switch (inst->op) {
        // عمليات حسابية
        case IR_OP_ADD:
            isel_lower_binop(ctx, inst, MACH_ADD);
            break;
        case IR_OP_SUB:
            isel_lower_binop(ctx, inst, MACH_SUB);
            break;
        case IR_OP_MUL:
            isel_lower_binop(ctx, inst, MACH_IMUL);
            break;
        case IR_OP_DIV:
            isel_lower_div(ctx, inst, false);
            break;
        case IR_OP_MOD:
            isel_lower_div(ctx, inst, true);
            break;
        case IR_OP_NEG:
            isel_lower_neg(ctx, inst);
            break;

        // عمليات الذاكرة
        case IR_OP_ALLOCA:
            isel_lower_alloca(ctx, inst);
            break;
        case IR_OP_LOAD:
            isel_lower_load(ctx, inst);
            break;
        case IR_OP_STORE:
            isel_lower_store(ctx, inst);
            break;

        // عملية المقارنة
        case IR_OP_CMP:
            isel_lower_cmp(ctx, inst);
            break;

        // عمليات منطقية
        case IR_OP_AND:
        case IR_OP_OR:
        case IR_OP_NOT:
            isel_lower_logical(ctx, inst);
            break;

        // عمليات التحكم
        case IR_OP_BR:
            isel_lower_br(ctx, inst);
            break;
        case IR_OP_BR_COND:
            isel_lower_br_cond(ctx, inst);
            break;
        case IR_OP_RET:
            isel_lower_ret(ctx, inst);
            break;
        case IR_OP_CALL:
            isel_lower_call(ctx, inst);
            break;

        // عمليات SSA
        case IR_OP_PHI:
            isel_lower_phi(ctx, inst);
            break;
        case IR_OP_COPY:
            isel_lower_copy(ctx, inst);
            break;

        // تحويل الأنواع
        case IR_OP_CAST:
            isel_lower_cast(ctx, inst);
            break;

        // لا عملية
        case IR_OP_NOP:
            isel_emit(ctx, MACH_NOP, mach_op_none(), mach_op_none(), mach_op_none());
            break;

        default:
            // عملية غير معروفة - نولد تعليق
            isel_emit_comment(ctx, MACH_NOP, mach_op_none(), mach_op_none(), mach_op_none(),
                              "// عملية IR غير مدعومة");
            break;
    }
}

// ============================================================================
// خفض كتلة IR
// ============================================================================

/**
 * @brief خفض كتلة IR إلى كتلة آلية.
 */
static MachineBlock* isel_lower_block(ISelCtx* ctx, IRBlock* ir_block) {
    if (!ir_block) return NULL;

    MachineBlock* mblock = mach_block_new(ir_block->label, ir_block->id);
    if (!mblock) return NULL;

    ctx->ir_block = ir_block;
    ctx->mblock = mblock;

    // إصدار تسمية الكتلة
    MachineInst* label = mach_inst_new(MACH_LABEL, mach_op_label(ir_block->id),
                                        mach_op_none(), mach_op_none());
    if (label) {
        label->comment = ir_block->label;
        mach_block_append(mblock, label);
    }

    // خفض كل تعليمة في الكتلة
    for (IRInst* inst = ir_block->first; inst; inst = inst->next) {
        isel_lower_inst(ctx, inst);
    }

    // نسخ معلومات الخلفاء
    mblock->succ_count = ir_block->succ_count;
    // (سيتم ربط الخلفاء بالكتل الآلية لاحقاً)

    return mblock;
}

// ============================================================================
// خفض دالة IR
// ============================================================================

/**
 * @brief خفض دالة IR إلى دالة آلية.
 */
static MachineFunc* isel_lower_func(ISelCtx* ctx, IRFunc* ir_func) {
    if (!ir_func) return NULL;

    MachineFunc* mfunc = mach_func_new(ir_func->name);
    if (!mfunc) return NULL;

    mfunc->is_prototype = ir_func->is_prototype;
    mfunc->param_count = ir_func->param_count;

    // نبدأ عداد السجلات الافتراضية من حيث توقف IR
    mfunc->next_vreg = ir_func->next_reg;

    ctx->ir_func = ir_func;
    ctx->mfunc = mfunc;

    // تخطي النماذج الأولية
    if (ir_func->is_prototype) return mfunc;

    // خفض كل كتلة
    for (IRBlock* block = ir_func->blocks; block; block = block->next) {
        MachineBlock* mblock = isel_lower_block(ctx, block);
        if (mblock) {
            mach_func_add_block(mfunc, mblock);
        }
    }

    return mfunc;
}

// ============================================================================
// نقطة الدخول الرئيسية لاختيار التعليمات
// ============================================================================

MachineModule* isel_run(IRModule* ir_module) {
    if (!ir_module) return NULL;

    MachineModule* mmod = mach_module_new(ir_module->name);
    if (!mmod) return NULL;

    // نسخ مراجع المتغيرات العامة وجدول النصوص
    mmod->globals = ir_module->globals;
    mmod->global_count = ir_module->global_count;
    mmod->strings = ir_module->strings;
    mmod->string_count = ir_module->string_count;

    ISelCtx ctx = {0};
    ctx.mmod = mmod;
    ctx.ir_module = ir_module;

    // خفض كل دالة
    for (IRFunc* func = ir_module->funcs; func; func = func->next) {
        MachineFunc* mfunc = isel_lower_func(&ctx, func);
        if (mfunc) {
            mach_module_add_func(mmod, mfunc);
        }
    }

    return mmod;
}

// ============================================================================
// طباعة تمثيل الآلة (للتنقيح)
// ============================================================================

const char* mach_op_to_string(MachineOp op) {
    switch (op) {
        case MACH_ADD:    return "add";
        case MACH_SUB:    return "sub";
        case MACH_IMUL:   return "imul";
        case MACH_IDIV:   return "idiv";
        case MACH_NEG:    return "neg";
        case MACH_CQO:    return "cqo";
        case MACH_MOV:    return "mov";
        case MACH_LEA:    return "lea";
        case MACH_LOAD:   return "load";
        case MACH_STORE:  return "store";
        case MACH_CMP:    return "cmp";
        case MACH_TEST:   return "test";
        case MACH_SETE:   return "sete";
        case MACH_SETNE:  return "setne";
        case MACH_SETG:   return "setg";
        case MACH_SETL:   return "setl";
        case MACH_SETGE:  return "setge";
        case MACH_SETLE:  return "setle";
        case MACH_MOVZX:  return "movzx";
        case MACH_AND:    return "and";
        case MACH_OR:     return "or";
        case MACH_NOT:    return "not";
        case MACH_XOR:    return "xor";
        case MACH_JMP:    return "jmp";
        case MACH_JE:     return "je";
        case MACH_JNE:    return "jne";
        case MACH_CALL:   return "call";
        case MACH_RET:    return "ret";
        case MACH_PUSH:   return "push";
        case MACH_POP:    return "pop";
        case MACH_NOP:    return "nop";
        case MACH_LABEL:  return "label";
        case MACH_COMMENT:return "comment";
        default:          return "???";
    }
}

void mach_operand_print(MachineOperand* op, FILE* out) {
    if (!op || !out) return;

    switch (op->kind) {
        case MACH_OP_NONE:
            break;
        case MACH_OP_VREG:
            if (op->data.vreg == -2) {
                fprintf(out, "%%ret");
            } else if (op->data.vreg <= -10 && op->data.vreg >= -13) {
                // سجلات معاملات ABI
                const char* abi_regs[] = {"%%rcx", "%%rdx", "%%r8", "%%r9"};
                int idx = -(op->data.vreg + 10);
                fprintf(out, "%s", abi_regs[idx]);
            } else {
                fprintf(out, "%%v%d", op->data.vreg);
            }
            break;
        case MACH_OP_IMM:
            fprintf(out, "$%lld", (long long)op->data.imm);
            break;
        case MACH_OP_MEM:
            if (op->data.mem.base_vreg == -1) {
                fprintf(out, "%d(%%rbp)", op->data.mem.offset);
            } else {
                if (op->data.mem.offset != 0)
                    fprintf(out, "%d(%%v%d)", op->data.mem.offset, op->data.mem.base_vreg);
                else
                    fprintf(out, "(%%v%d)", op->data.mem.base_vreg);
            }
            break;
        case MACH_OP_LABEL:
            fprintf(out, ".L%d", op->data.label_id);
            break;
        case MACH_OP_GLOBAL:
            fprintf(out, "%s", op->data.name ? op->data.name : "???");
            break;
        case MACH_OP_FUNC:
            fprintf(out, "@%s", op->data.name ? op->data.name : "???");
            break;
    }
}

void mach_inst_print(MachineInst* inst, FILE* out) {
    if (!inst || !out) return;

    if (inst->op == MACH_LABEL) {
        // طباعة تسمية
        fprintf(out, ".L%d:", inst->dst.data.label_id);
        if (inst->comment) fprintf(out, "  # %s", inst->comment);
        fprintf(out, "\n");
        return;
    }

    if (inst->op == MACH_COMMENT) {
        if (inst->comment) fprintf(out, "    # %s\n", inst->comment);
        return;
    }

    fprintf(out, "    %-8s", mach_op_to_string(inst->op));

    // طباعة المعاملات
    int has_dst = (inst->dst.kind != MACH_OP_NONE);
    int has_src1 = (inst->src1.kind != MACH_OP_NONE);
    int has_src2 = (inst->src2.kind != MACH_OP_NONE);

    if (has_dst) {
        fprintf(out, " ");
        mach_operand_print(&inst->dst, out);
    }
    if (has_src1) {
        fprintf(out, "%s", has_dst ? ", " : " ");
        mach_operand_print(&inst->src1, out);
    }
    if (has_src2) {
        fprintf(out, ", ");
        mach_operand_print(&inst->src2, out);
    }

    if (inst->comment) fprintf(out, "  # %s", inst->comment);
    fprintf(out, "\n");
}

void mach_block_print(MachineBlock* block, FILE* out) {
    if (!block || !out) return;

    for (MachineInst* inst = block->first; inst; inst = inst->next) {
        mach_inst_print(inst, out);
    }
}

void mach_func_print(MachineFunc* func, FILE* out) {
    if (!func || !out) return;

    if (func->is_prototype) {
        fprintf(out, "# prototype: %s\n", func->name ? func->name : "???");
        return;
    }

    fprintf(out, "# function: %s (stack=%d, vregs=%d)\n",
            func->name ? func->name : "???",
            func->stack_size, func->next_vreg);

    for (MachineBlock* block = func->blocks; block; block = block->next) {
        mach_block_print(block, out);
    }
    fprintf(out, "\n");
}

void mach_module_print(MachineModule* module, FILE* out) {
    if (!module || !out) return;

    fprintf(out, "# ============================================\n");
    fprintf(out, "# Machine IR: %s\n", module->name ? module->name : "???");
    fprintf(out, "# Functions: %d\n", module->func_count);
    fprintf(out, "# ============================================\n\n");

    for (MachineFunc* func = module->funcs; func; func = func->next) {
        mach_func_print(func, out);
    }
}
