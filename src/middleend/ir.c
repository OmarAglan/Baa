/**
 * @file ir.c
 * @brief تنفيذ IR (نواة باء).
 * @version 0.3.0.1
 * 
 * تنفيذ التمثيل الوسيط (IR) العربي-أولاً.
 * يوفر دوال مساعدة لبناء الـ IR وطباعته وإدارة الذاكرة.
 */

#include "middleend_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// سياق الوحدة الحالية (للساحة)
// ============================================================================

static IRModule* g_ir_current_module = NULL;

void ir_module_set_current(IRModule* module) {
    g_ir_current_module = module;
}

IRModule* ir_module_get_current(void) {
    return g_ir_current_module;
}

static IRArena* ir_cur_arena(void) {
    if (!g_ir_current_module) {
        return NULL;
    }
    return &g_ir_current_module->arena;
}

static void* ir_alloc(size_t size, size_t align) {
    IRArena* a = ir_cur_arena();
    if (!a) {
        fprintf(stderr, "خطأ: محاولة تخصيص IR بدون وحدة حالية (Arena غير مهيئة)\n");
        return NULL;
    }
    return ir_arena_alloc(a, size, align);
}

static char* ir_strdup(const char* s) {
    IRArena* a = ir_cur_arena();
    if (!a) {
        fprintf(stderr, "خطأ: محاولة نسخ نص IR بدون وحدة حالية (Arena غير مهيئة)\n");
        return NULL;
    }
    return ir_arena_strdup(a, s);
}

// ============================================================================
// أنواع IR المعرّفة مسبقاً
// ============================================================================

static IRType ir_type_void_instance   = { .kind = IR_TYPE_VOID };
static IRType ir_type_i1_instance     = { .kind = IR_TYPE_I1 };
static IRType ir_type_i8_instance     = { .kind = IR_TYPE_I8 };
static IRType ir_type_i16_instance    = { .kind = IR_TYPE_I16 };
static IRType ir_type_i32_instance    = { .kind = IR_TYPE_I32 };
static IRType ir_type_i64_instance    = { .kind = IR_TYPE_I64 };
static IRType ir_type_u8_instance     = { .kind = IR_TYPE_U8 };
static IRType ir_type_u16_instance    = { .kind = IR_TYPE_U16 };
static IRType ir_type_u32_instance    = { .kind = IR_TYPE_U32 };
static IRType ir_type_u64_instance    = { .kind = IR_TYPE_U64 };
static IRType ir_type_char_instance   = { .kind = IR_TYPE_CHAR };
static IRType ir_type_f64_instance    = { .kind = IR_TYPE_F64 };

// مؤشرات عامة للأنواع الأساسية
IRType* IR_TYPE_VOID_T  = &ir_type_void_instance;
IRType* IR_TYPE_I1_T    = &ir_type_i1_instance;
IRType* IR_TYPE_I8_T    = &ir_type_i8_instance;
IRType* IR_TYPE_I16_T   = &ir_type_i16_instance;
IRType* IR_TYPE_I32_T   = &ir_type_i32_instance;
IRType* IR_TYPE_I64_T   = &ir_type_i64_instance;
IRType* IR_TYPE_U8_T    = &ir_type_u8_instance;
IRType* IR_TYPE_U16_T   = &ir_type_u16_instance;
IRType* IR_TYPE_U32_T   = &ir_type_u32_instance;
IRType* IR_TYPE_U64_T   = &ir_type_u64_instance;
IRType* IR_TYPE_CHAR_T  = &ir_type_char_instance;
IRType* IR_TYPE_F64_T   = &ir_type_f64_instance;

// ============================================================================
// تحويل الأرقام إلى أرقام هندية عربية
// ============================================================================

/**
 * @brief الأرقام الهندية العربية (٠١٢٣٤٥٦٧٨٩)
 * @note كل رقم يستهلك بايتين في UTF-8.
 */
static const char* arabic_digits[] = {
    "٠", "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩"
};

/**
 * @brief تحويل عدد صحيح إلى أرقام هندية عربية.
 * @param n العدد.
 * @param buf مخزن الخرج (يفضل أن يكون واسعاً بما يكفي).
 * @return مؤشر إلى buf.
 */
char* int_to_arabic_numerals(int n, char* buf) {
    if (n == 0) {
        strcpy(buf, "٠");
        return buf;
    }
    
    int is_negative = 0;
    if (n < 0) {
        is_negative = 1;
        n = -n;
    }
    
    // بناء الأرقام بترتيب عكسي
    char temp[64];
    int pos = 0;
    
    while (n > 0) {
        int digit = n % 10;
        // نسخ الرقم (بايتان UTF-8)
        temp[pos++] = arabic_digits[digit][0];
        temp[pos++] = arabic_digits[digit][1];
        n /= 10;
    }
    
    // عكس الترتيب إلى buf
    int buf_pos = 0;
    if (is_negative) {
        buf[buf_pos++] = '-';
    }
    
    // الأرقام بايتان لكل رقم، نعكس على شكل أزواج
    for (int i = pos - 2; i >= 0; i -= 2) {
        buf[buf_pos++] = temp[i];
        buf[buf_pos++] = temp[i + 1];
    }
    buf[buf_pos] = '\0';
    
    return buf;
}

// ============================================================================
// تحويل أسماء الـ IR
// ============================================================================

/**
 * @brief تحويل opcode إلى اسم عربي.
 */
const char* ir_op_to_arabic(IROp op) {
    switch (op) {
        // حسابي
        case IR_OP_ADD:     return "جمع";
        case IR_OP_SUB:     return "طرح";
        case IR_OP_MUL:     return "ضرب";
        case IR_OP_DIV:     return "قسم";
        case IR_OP_MOD:     return "باقي";
        case IR_OP_NEG:     return "سالب";
        
        // ذاكرة
        case IR_OP_ALLOCA:  return "حجز";
        case IR_OP_LOAD:    return "حمل";
        case IR_OP_STORE:   return "خزن";
        case IR_OP_PTR_OFFSET: return "إزاحة_مؤشر";
        
        // مقارنة
        case IR_OP_CMP:     return "قارن";
        
        // منطقي/بتّي
        case IR_OP_AND:     return "و";
        case IR_OP_OR:      return "أو";
        case IR_OP_XOR:     return "أو_حصري";
        case IR_OP_NOT:     return "نفي";
        case IR_OP_SHL:     return "ازاحة_يسار";
        case IR_OP_SHR:     return "ازاحة_يمين";
        
        // تدفق التحكم
        case IR_OP_BR:      return "قفز";
        case IR_OP_BR_COND: return "قفز_شرط";
        case IR_OP_RET:     return "رجوع";
        case IR_OP_CALL:    return "نداء";
        
        // SSA
        case IR_OP_PHI:     return "فاي";
        case IR_OP_COPY:    return "نسخ";
        
        // تحويل نوع
        case IR_OP_CAST:    return "تحويل";
        
        // متفرقات
        case IR_OP_NOP:     return "لاعمل";
        
        default:            return "مجهول";
    }
}

/**
 * @brief تحويل opcode إلى اسم إنجليزي (للتنقيح/التوافق).
 */
const char* ir_op_to_english(IROp op) {
    switch (op) {
        case IR_OP_ADD:     return "add";
        case IR_OP_SUB:     return "sub";
        case IR_OP_MUL:     return "mul";
        case IR_OP_DIV:     return "div";
        case IR_OP_MOD:     return "mod";
        case IR_OP_NEG:     return "neg";
        case IR_OP_ALLOCA:  return "alloca";
        case IR_OP_LOAD:    return "load";
        case IR_OP_STORE:   return "store";
        case IR_OP_PTR_OFFSET: return "ptr.offset";
        case IR_OP_CMP:     return "cmp";
        case IR_OP_AND:     return "and";
        case IR_OP_OR:      return "or";
        case IR_OP_XOR:     return "xor";
        case IR_OP_NOT:     return "not";
        case IR_OP_SHL:     return "shl";
        case IR_OP_SHR:     return "shr";
        case IR_OP_BR:      return "br";
        case IR_OP_BR_COND: return "br.cond";
        case IR_OP_RET:     return "ret";
        case IR_OP_CALL:    return "call";
        case IR_OP_PHI:     return "phi";
        case IR_OP_COPY:    return "copy";
        case IR_OP_CAST:    return "cast";
        case IR_OP_NOP:     return "nop";
        default:            return "unknown";
    }
}

/**
 * @brief تحويل شرط المقارنة إلى اسم عربي.
 */
const char* ir_cmp_pred_to_arabic(IRCmpPred pred) {
    switch (pred) {
        case IR_CMP_EQ:  return "يساوي";
        case IR_CMP_NE:  return "لا_يساوي";
        case IR_CMP_GT:  return "أكبر";
        case IR_CMP_LT:  return "أصغر";
        case IR_CMP_GE:  return "أكبر_أو_يساوي";
        case IR_CMP_LE:  return "أصغر_أو_يساوي";
        case IR_CMP_UGT: return "أكبر";
        case IR_CMP_ULT: return "أصغر";
        case IR_CMP_UGE: return "أكبر_أو_يساوي";
        case IR_CMP_ULE: return "أصغر_أو_يساوي";
        default:         return "مجهول";
    }
}

/**
 * Convert comparison predicate to English name
 */
const char* ir_cmp_pred_to_english(IRCmpPred pred) {
    switch (pred) {
        case IR_CMP_EQ:  return "eq";
        case IR_CMP_NE:  return "ne";
        case IR_CMP_GT:  return "sgt";
        case IR_CMP_LT:  return "slt";
        case IR_CMP_GE:  return "sge";
        case IR_CMP_LE:  return "sle";
        case IR_CMP_UGT: return "ugt";
        case IR_CMP_ULT: return "ult";
        case IR_CMP_UGE: return "uge";
        case IR_CMP_ULE: return "ule";
        default:         return "unknown";
    }
}

/**
 * Convert IR type to Arabic representation
 */
const char* ir_type_to_arabic(IRType* type) {
    if (!type) return "فراغ";
    
    switch (type->kind) {
        case IR_TYPE_VOID:   return "فراغ";     // Void
        case IR_TYPE_I1:     return "ص١";       // 1-bit (bool)
        case IR_TYPE_I8:     return "ص٨";       // 8-bit
        case IR_TYPE_I16:    return "ص١٦";      // 16-bit
        case IR_TYPE_I32:    return "ص٣٢";      // 32-bit
        case IR_TYPE_I64:    return "ص٦٤";      // 64-bit
        case IR_TYPE_U8:     return "ط٨";       // u8
        case IR_TYPE_U16:    return "ط١٦";      // u16
        case IR_TYPE_U32:    return "ط٣٢";      // u32
        case IR_TYPE_U64:    return "ط٦٤";      // u64
        case IR_TYPE_CHAR:   return "حرف";      // Baa char
        case IR_TYPE_F64:    return "ع٦٤";      // 64-bit float
        case IR_TYPE_PTR:    return "مؤشر";     // Pointer
        case IR_TYPE_ARRAY:  return "مصفوفة";   // Array
        case IR_TYPE_FUNC:   return "دالة";     // Function
        default:             return "مجهول";    // Unknown
    }
}

/**
 * Convert IR type to English representation
 */
const char* ir_type_to_english(IRType* type) {
    if (!type) return "void";
    
    switch (type->kind) {
        case IR_TYPE_VOID:   return "void";
        case IR_TYPE_I1:     return "i1";
        case IR_TYPE_I8:     return "i8";
        case IR_TYPE_I16:    return "i16";
        case IR_TYPE_I32:    return "i32";
        case IR_TYPE_I64:    return "i64";
        case IR_TYPE_U8:     return "u8";
        case IR_TYPE_U16:    return "u16";
        case IR_TYPE_U32:    return "u32";
        case IR_TYPE_U64:    return "u64";
        case IR_TYPE_CHAR:   return "char";
        case IR_TYPE_F64:    return "f64";
        case IR_TYPE_PTR:    return "ptr";
        case IR_TYPE_ARRAY:  return "array";
        case IR_TYPE_FUNC:   return "func";
        default:             return "unknown";
    }
}

// ============================================================================
// Type Construction
// ============================================================================

/**
 * Create a pointer type
 */
IRType* ir_type_ptr(IRType* pointee) {
    if (g_ir_current_module && pointee == IR_TYPE_I8_T && g_ir_current_module->cached_i8_ptr_type) {
        return g_ir_current_module->cached_i8_ptr_type;
    }

    IRType* type = (IRType*)ir_alloc(sizeof(IRType), _Alignof(IRType));
    if (!type) {
        fprintf(stderr, "خطأ: فشل تخصيص نوع مؤشر\n");
        return NULL;
    }
    type->kind = IR_TYPE_PTR;
    type->data.pointee = pointee;

    if (g_ir_current_module && pointee == IR_TYPE_I8_T && !g_ir_current_module->cached_i8_ptr_type) {
        g_ir_current_module->cached_i8_ptr_type = type;
    }
    return type;
}

/**
 * Create an array type
 */
IRType* ir_type_array(IRType* element, int count) {
    IRType* type = (IRType*)ir_alloc(sizeof(IRType), _Alignof(IRType));
    if (!type) {
        fprintf(stderr, "خطأ: فشل تخصيص نوع مصفوفة\n");
        return NULL;
    }
    type->kind = IR_TYPE_ARRAY;
    type->data.array.element = element;
    type->data.array.count = count;
    return type;
}

/**
 * Create a function type
 */
IRType* ir_type_func(IRType* ret, IRType** params, int param_count) {
    IRType* type = (IRType*)ir_alloc(sizeof(IRType), _Alignof(IRType));
    if (!type) {
        fprintf(stderr, "خطأ: فشل تخصيص نوع دالة\n");
        return NULL;
    }
    type->kind = IR_TYPE_FUNC;
    type->data.func.ret = ret;
    if (params && param_count > 0) {
        IRType** arr = (IRType**)ir_alloc((size_t)param_count * sizeof(IRType*), _Alignof(IRType*));
        if (!arr) return NULL;
        for (int i = 0; i < param_count; i++) arr[i] = params[i];
        type->data.func.params = arr;
    } else {
        type->data.func.params = NULL;
    }
    type->data.func.param_count = param_count;
    return type;
}

/**
 * Free a dynamically allocated type
 */
void ir_type_free(IRType* type) {
    (void)type;
    // تُدار الذاكرة عبر الساحة (Arena): لا تحرير فردي للأنواع.
}

/**
 * Check if two types are equal
 */
int ir_types_equal(IRType* a, IRType* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    
    switch (a->kind) {
        case IR_TYPE_PTR:
            return ir_types_equal(a->data.pointee, b->data.pointee);
        case IR_TYPE_ARRAY:
            return a->data.array.count == b->data.array.count &&
                   ir_types_equal(a->data.array.element, b->data.array.element);
        case IR_TYPE_FUNC:
            if (a->data.func.param_count != b->data.func.param_count) return 0;
            if (!ir_types_equal(a->data.func.ret, b->data.func.ret)) return 0;
            for (int i = 0; i < a->data.func.param_count; i++) {
                if (!ir_types_equal(a->data.func.params[i], b->data.func.params[i])) {
                    return 0;
                }
            }
            return 1;
        default:
            return 1;  // Simple types match by kind
    }
}

/**
 * Get the size in bits for a type
 */
int ir_type_bits(IRType* type) {
    if (!type) return 0;
    switch (type->kind) {
        case IR_TYPE_VOID:  return 0;
        case IR_TYPE_I1:    return 1;
        case IR_TYPE_I8:    return 8;
        case IR_TYPE_I16:   return 16;
        case IR_TYPE_I32:   return 32;
        case IR_TYPE_I64:   return 64;
        case IR_TYPE_U8:    return 8;
        case IR_TYPE_U16:   return 16;
        case IR_TYPE_U32:   return 32;
        case IR_TYPE_U64:   return 64;
        case IR_TYPE_CHAR:  return 64;
        case IR_TYPE_F64:   return 64;
        case IR_TYPE_PTR:   return 64;  // 64-bit pointers
        case IR_TYPE_ARRAY:
            return type->data.array.count * ir_type_bits(type->data.array.element);
        case IR_TYPE_FUNC:  return 64;  // Function pointers
        default:            return 0;
    }
}

// ============================================================================
// IR Value Construction
// ============================================================================

/**
 * Create a register value
 */
IRValue* ir_value_reg(int reg_num, IRType* type) {
    IRValue* val = (IRValue*)ir_alloc(sizeof(IRValue), _Alignof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_REG;
    val->type = type;
    val->data.reg_num = reg_num;
    return val;
}

/**
 * Create an integer constant value
 */
IRValue* ir_value_const_int(int64_t value, IRType* type) {
    IRValue* val = (IRValue*)ir_alloc(sizeof(IRValue), _Alignof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_CONST_INT;
    val->type = type ? type : IR_TYPE_I64_T;
    val->data.const_int = value;
    return val;
}

/**
 * Create a string constant value
 */
IRValue* ir_value_const_str(const char* str, int id) {
    IRValue* val = (IRValue*)ir_alloc(sizeof(IRValue), _Alignof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_CONST_STR;
    val->type = ir_type_ptr(IR_TYPE_I8_T);
    val->data.const_str.data = str ? ir_strdup(str) : NULL;
    val->data.const_str.id = id;
    return val;
}

IRValue* ir_value_baa_str(const char* str, int id) {
    IRValue* val = (IRValue*)ir_alloc(sizeof(IRValue), _Alignof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_BAA_STR;
    val->type = ir_type_ptr(IR_TYPE_CHAR_T);
    val->data.const_str.data = str ? ir_strdup(str) : NULL;
    val->data.const_str.id = id;
    return val;
}

/**
 * Create a block reference value
 */
IRValue* ir_value_block(IRBlock* block) {
    IRValue* val = (IRValue*)ir_alloc(sizeof(IRValue), _Alignof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_BLOCK;
    val->type = NULL;  // Blocks don't have a type
    val->data.block = block;
    return val;
}

/**
 * Create a global variable reference value
 */
IRValue* ir_value_global(const char* name, IRType* type) {
    IRValue* val = (IRValue*)ir_alloc(sizeof(IRValue), _Alignof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_GLOBAL;
    val->type = type ? ir_type_ptr(type) : NULL;
    val->data.global_name = name ? ir_strdup(name) : NULL;
    return val;
}

/**
 * Create a function reference value
 */
IRValue* ir_value_func_ref(const char* name, IRType* type) {
    IRValue* val = (IRValue*)ir_alloc(sizeof(IRValue), _Alignof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_FUNC;
    val->type = type;
    val->data.global_name = name ? ir_strdup(name) : NULL;
    return val;
}

/**
 * Free a value
 */
void ir_value_free(IRValue* val) {
    (void)val;
    // تُدار الذاكرة عبر الساحة (Arena): لا تحرير فردي للقيم.
}

// ============================================================================
// IR Instruction Construction
// ============================================================================

/**
 * Create a new instruction
 */
IRInst* ir_inst_new(IROp op, IRType* type, int dest) {
    IRInst* inst = (IRInst*)ir_alloc(sizeof(IRInst), _Alignof(IRInst));
    if (!inst) return NULL;
    
    inst->op = op;
    inst->type = type;
    inst->id = -1;
    inst->dest = dest;
    inst->operand_count = 0;
    for (int i = 0; i < 4; i++) {
        inst->operands[i] = NULL;
    }
    inst->cmp_pred = IR_CMP_EQ;  // Default
    inst->phi_entries = NULL;
    inst->call_target = NULL;
    inst->call_callee = NULL;
    inst->call_args = NULL;
    inst->call_arg_count = 0;
    inst->src_file = NULL;
    inst->src_line = 0;
    inst->src_col = 0;
    inst->dbg_name = NULL;

    inst->parent = NULL;
    inst->prev = NULL;
    inst->next = NULL;
    
    return inst;
}

/**
 * Add an operand to an instruction
 */
void ir_inst_add_operand(IRInst* inst, IRValue* operand) {
    if (!inst || !operand) return;
    if (inst->operand_count >= 4) {
        fprintf(stderr, "خطأ: تجاوز الحد الأقصى للمعاملات (٤)\n");
        return;
    }
    inst->operands[inst->operand_count++] = operand;

    // أي تغيير في المعاملات يُبطل كاش Def-Use.
    if (inst->parent && inst->parent->parent) {
        ir_func_invalidate_defuse(inst->parent->parent);
    }
}

/**
 * Set source location for an instruction
 */
void ir_inst_set_loc(IRInst* inst, const char* file, int line, int col) {
    if (!inst) return;
    inst->src_file = file;
    inst->src_line = line;
    inst->src_col = col;
}

void ir_inst_set_dbg_name(IRInst* inst, const char* name) {
    if (!inst) return;
    inst->dbg_name = name;
}

/**
 * Create a binary operation (add, sub, mul, div, etc.)
 */
IRInst* ir_inst_binary(IROp op, IRType* type, int dest, IRValue* lhs, IRValue* rhs) {
    IRInst* inst = ir_inst_new(op, type, dest);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, lhs);
    ir_inst_add_operand(inst, rhs);
    return inst;
}

/**
 * Create a unary operation (neg, not)
 */
IRInst* ir_inst_unary(IROp op, IRType* type, int dest, IRValue* operand) {
    IRInst* inst = ir_inst_new(op, type, dest);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, operand);
    return inst;
}

/**
 * Create a comparison instruction
 */
IRInst* ir_inst_cmp(IRCmpPred pred, int dest, IRValue* lhs, IRValue* rhs) {
    IRInst* inst = ir_inst_new(IR_OP_CMP, IR_TYPE_I1_T, dest);
    if (!inst) return NULL;
    inst->cmp_pred = pred;
    ir_inst_add_operand(inst, lhs);
    ir_inst_add_operand(inst, rhs);
    return inst;
}

/**
 * Create an alloca instruction
 */
IRInst* ir_inst_alloca(IRType* type, int dest) {
    IRType* ptr_type = ir_type_ptr(type);
    IRInst* inst = ir_inst_new(IR_OP_ALLOCA, ptr_type, dest);
    return inst;
}

/**
 * Create a load instruction
 */
IRInst* ir_inst_load(IRType* type, int dest, IRValue* ptr) {
    IRInst* inst = ir_inst_new(IR_OP_LOAD, type, dest);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, ptr);
    return inst;
}

/**
 * Create a store instruction
 */
IRInst* ir_inst_store(IRValue* value, IRValue* ptr) {
    IRInst* inst = ir_inst_new(IR_OP_STORE, IR_TYPE_VOID_T, -1);  // No destination
    if (!inst) return NULL;
    ir_inst_add_operand(inst, value);
    ir_inst_add_operand(inst, ptr);
    return inst;
}

/**
 * @brief إنشاء تعليمة إزاحة مؤشر.
 *
 * الصيغة:
 *   %dst = إزاحة_مؤشر <ptr_type> base, index
 *
 * الدلالة:
 * - base والنتيجة لهما نفس نوع المؤشر.
 * - index فهرس عناصر (وليس بايتات) ويُضرب ضمنياً في حجم pointee.
 */
IRInst* ir_inst_ptr_offset(IRType* ptr_type, int dest, IRValue* base, IRValue* index) {
    IRInst* inst = ir_inst_new(IR_OP_PTR_OFFSET, ptr_type, dest);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, base);
    ir_inst_add_operand(inst, index);
    return inst;
}

/**
 * Create an unconditional branch
 */
IRInst* ir_inst_br(IRBlock* target) {
    IRInst* inst = ir_inst_new(IR_OP_BR, IR_TYPE_VOID_T, -1);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, ir_value_block(target));
    return inst;
}

/**
 * Create a conditional branch
 */
IRInst* ir_inst_br_cond(IRValue* cond, IRBlock* if_true, IRBlock* if_false) {
    IRInst* inst = ir_inst_new(IR_OP_BR_COND, IR_TYPE_VOID_T, -1);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, cond);
    ir_inst_add_operand(inst, ir_value_block(if_true));
    ir_inst_add_operand(inst, ir_value_block(if_false));
    return inst;
}

/**
 * Create a return instruction
 */
IRInst* ir_inst_ret(IRValue* value) {
    IRType* type = value ? value->type : IR_TYPE_VOID_T;
    IRInst* inst = ir_inst_new(IR_OP_RET, type, -1);
    if (!inst) return NULL;
    if (value) {
        ir_inst_add_operand(inst, value);
    }
    return inst;
}

/**
 * Create a call instruction
 */
IRInst* ir_inst_call(const char* target, IRType* ret_type, int dest, 
                     IRValue** args, int arg_count) {
    IRInst* inst = ir_inst_new(IR_OP_CALL, ret_type, dest);
    if (!inst) return NULL;
    
    inst->call_target = target ? ir_strdup(target) : NULL;
    inst->call_callee = NULL;
    inst->call_arg_count = arg_count;
    
    // Copy arguments
    if (arg_count > 0 && args) {
        inst->call_args = (IRValue**)ir_alloc((size_t)arg_count * sizeof(IRValue*), _Alignof(IRValue*));
        if (!inst->call_args) {
            inst->call_arg_count = 0;
        } else {
            for (int i = 0; i < arg_count; i++) {
                inst->call_args[i] = args[i];
            }
        }
    }
    
    return inst;
}

/**
 * Create an indirect call instruction
 */
IRInst* ir_inst_call_indirect(IRValue* callee, IRType* ret_type, int dest,
                              IRValue** args, int arg_count)
{
    IRInst* inst = ir_inst_new(IR_OP_CALL, ret_type, dest);
    if (!inst) return NULL;

    inst->call_target = NULL;
    inst->call_callee = callee;
    inst->call_arg_count = arg_count;

    // Copy arguments
    if (arg_count > 0 && args) {
        inst->call_args = (IRValue**)ir_alloc((size_t)arg_count * sizeof(IRValue*), _Alignof(IRValue*));
        if (!inst->call_args) {
            inst->call_arg_count = 0;
        } else {
            for (int i = 0; i < arg_count; i++) {
                inst->call_args[i] = args[i];
            }
        }
    }

    return inst;
}

/**
 * Create a phi node
 */
IRInst* ir_inst_phi(IRType* type, int dest) {
    IRInst* inst = ir_inst_new(IR_OP_PHI, type, dest);
    if (!inst) return NULL;
    inst->phi_entries = NULL;  // Entries added later
    return inst;
}

/**
 * Add an incoming value to a phi node
 */
void ir_inst_phi_add(IRInst* phi, IRValue* value, IRBlock* block) {
    if (!phi || phi->op != IR_OP_PHI || !value || !block) return;
    
    IRPhiEntry* entry = (IRPhiEntry*)ir_alloc(sizeof(IRPhiEntry), _Alignof(IRPhiEntry));
    if (!entry) return;
    
    entry->value = value;
    entry->block = block;
    entry->next = phi->phi_entries;
    phi->phi_entries = entry;

    // إضافة مدخل فاي تغيّر الاستعمالات.
    if (phi->parent && phi->parent->parent) {
        ir_func_invalidate_defuse(phi->parent->parent);
    }
}

/**
 * Free an instruction
 */
void ir_inst_free(IRInst* inst) {
    (void)inst;
    // تُدار الذاكرة عبر الساحة (Arena): لا تحرير فردي للتعليمات.
}

#include "ir_containers.c"
#include "ir_printing.c"
