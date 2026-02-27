/**
 * @file ir_lower.c
 * @brief خفض AST إلى IR (التعبيرات + الجمل + تدفق التحكم).
 * @version 0.3.0.5
 *
 * Implements lowering from validated AST into Baa IR using IRBuilder.
 *
 * v0.3.0.3: Expression lowering
 * v0.3.0.4: Statement lowering (var decl/assign/return/print/read)
 * v0.3.0.5: Control-flow lowering (if/while/for/switch/break/continue)
 */

#include "ir_lower.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

// ============================================================================
// Locals table helpers
// ============================================================================

static IRLowerBinding* find_local(IRLowerCtx* ctx, const char* name) {
    if (!ctx || !name) return NULL;
    // البحث من الأحدث للأقدم لدعم النطاقات المتداخلة.
    for (int i = ctx->local_count - 1; i >= 0; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            return &ctx->locals[i];
        }
    }
    return NULL;
}

static void ir_lower_report_error(IRLowerCtx* ctx, const Node* node, const char* fmt, ...)
{
    if (ctx) ctx->had_error = 1;

    const char* file = (node && node->filename) ? node->filename : "<غير_معروف>";
    int line = (node && node->line > 0) ? node->line : 1;
    int col = (node && node->col > 0) ? node->col : 1;

    fprintf(stderr, "خطأ (تحويل IR): %s:%d:%d: ", file, line, col);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}

static int ir_lower_current_scope_start(IRLowerCtx* ctx) {
    if (!ctx) return 0;
    if (ctx->scope_depth <= 0) return 0;
    return ctx->scope_stack[ctx->scope_depth - 1];
}

static void ir_lower_scope_push(IRLowerCtx* ctx) {
    if (!ctx) return;
    if (ctx->scope_depth >= (int)(sizeof(ctx->scope_stack) / sizeof(ctx->scope_stack[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد مكدس النطاقات.");
        return;
    }
    ctx->scope_stack[ctx->scope_depth++] = ctx->local_count;
}

static void ir_lower_scope_pop(IRLowerCtx* ctx) {
    if (!ctx) return;
    if (ctx->scope_depth <= 0) {
        ir_lower_report_error(ctx, NULL, "نقص في مكدس النطاقات.");
        return;
    }
    int start = ctx->scope_stack[--ctx->scope_depth];
    if (start < 0) start = 0;
    if (start > ctx->local_count) start = ctx->local_count;
    ctx->local_count = start;
}

void ir_lower_ctx_init(IRLowerCtx* ctx, IRBuilder* builder) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->builder = builder;
    ctx->had_error = 0;

    // v0.3.0.5 control-flow lowering state
    ctx->label_counter = 0;
    ctx->cf_depth = 0;
    ctx->scope_depth = 0;
    ctx->current_func_name = NULL;
    ctx->static_local_counter = 0;
    ctx->enable_bounds_checks = false;
    ctx->program_root = NULL;
}

void ir_lower_bind_local(IRLowerCtx* ctx, const char* name, int ptr_reg, IRType* value_type) {
    if (!ctx || !name) return;

    // ابحث فقط داخل النطاق الحالي: إن وجد، حدّث الربط.
    int start = ir_lower_current_scope_start(ctx);
    for (int i = ctx->local_count - 1; i >= start; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].ptr_reg = ptr_reg;
            ctx->locals[i].value_type = value_type;
            ctx->locals[i].global_name = NULL;
            ctx->locals[i].is_static_storage = false;
            ctx->locals[i].is_array = false;
            ctx->locals[i].array_rank = 0;
            ctx->locals[i].array_dims = NULL;
            ctx->locals[i].array_elem_type = TYPE_INT;
            ctx->locals[i].array_elem_type_name = NULL;
            return;
        }
    }

    if (ctx->local_count >= (int)(sizeof(ctx->locals) / sizeof(ctx->locals[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد جدول المتغيرات المحلية أثناء ربط '%s'.", name);
        return;
    }

    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].ptr_reg = ptr_reg;
    ctx->locals[ctx->local_count].value_type = value_type;
    ctx->locals[ctx->local_count].global_name = NULL;
    ctx->locals[ctx->local_count].is_static_storage = false;
    ctx->locals[ctx->local_count].is_array = false;
    ctx->locals[ctx->local_count].array_rank = 0;
    ctx->locals[ctx->local_count].array_dims = NULL;
    ctx->locals[ctx->local_count].array_elem_type = TYPE_INT;
    ctx->locals[ctx->local_count].array_elem_type_name = NULL;
    ctx->local_count++;
}

void ir_lower_bind_local_static(IRLowerCtx* ctx, const char* name,
                                const char* global_name, IRType* value_type) {
    if (!ctx || !name || !global_name) return;

    int start = ir_lower_current_scope_start(ctx);
    for (int i = ctx->local_count - 1; i >= start; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].ptr_reg = -1;
            ctx->locals[i].value_type = value_type;
            ctx->locals[i].global_name = global_name;
            ctx->locals[i].is_static_storage = true;
            ctx->locals[i].is_array = false;
            ctx->locals[i].array_rank = 0;
            ctx->locals[i].array_dims = NULL;
            ctx->locals[i].array_elem_type = TYPE_INT;
            ctx->locals[i].array_elem_type_name = NULL;
            return;
        }
    }

    if (ctx->local_count >= (int)(sizeof(ctx->locals) / sizeof(ctx->locals[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد جدول المتغيرات المحلية أثناء ربط '%s'.", name);
        return;
    }

    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].ptr_reg = -1;
    ctx->locals[ctx->local_count].value_type = value_type;
    ctx->locals[ctx->local_count].global_name = global_name;
    ctx->locals[ctx->local_count].is_static_storage = true;
    ctx->locals[ctx->local_count].is_array = false;
    ctx->locals[ctx->local_count].array_rank = 0;
    ctx->locals[ctx->local_count].array_dims = NULL;
    ctx->locals[ctx->local_count].array_elem_type = TYPE_INT;
    ctx->locals[ctx->local_count].array_elem_type_name = NULL;
    ctx->local_count++;
}

static void ir_lower_set_local_array_meta(IRLowerCtx* ctx,
                                          const char* name,
                                          int array_rank,
                                          const int* array_dims,
                                          DataType elem_type,
                                          const char* elem_type_name)
{
    if (!ctx || !name) return;
    for (int i = ctx->local_count - 1; i >= 0; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].is_array = true;
            ctx->locals[i].array_rank = array_rank;
            ctx->locals[i].array_dims = array_dims;
            ctx->locals[i].array_elem_type = elem_type;
            ctx->locals[i].array_elem_type_name = elem_type_name;
            return;
        }
    }
}

// ============================================================================
// Expression/Statement lowering helpers
// ============================================================================

static void ir_lower_set_loc(IRBuilder* builder, const Node* node) {
    if (!builder) return;

    if (!node || !node->filename || node->line <= 0) {
        ir_builder_clear_loc(builder);
        return;
    }

    ir_builder_set_loc(builder, node->filename, node->line, node->col);
}

static void ir_lower_tag_last_inst(IRBuilder* builder, IROp expected_op, int expected_dest,
                                   const char* dbg_name) {
    if (!builder || !builder->insert_block) return;
    if (!dbg_name) return;

    IRInst* last = builder->insert_block->last;
    if (!last) return;
    if (last->op != expected_op) return;
    if (expected_dest >= 0 && last->dest != expected_dest) return;

    ir_inst_set_dbg_name(last, dbg_name);
}

// forward decls
static IRValue* ensure_i64(IRLowerCtx* ctx, IRValue* v);
static IRValue* cast_to(IRLowerCtx* ctx, IRValue* v, IRType* to);
static IRValue* ir_lower_global_init_value(IRBuilder* builder, Node* expr, IRType* expected_type);

static IRType* get_char_ptr_type(IRModule* module) {
    if (!module) return ir_type_ptr(IR_TYPE_CHAR_T);
    ir_module_set_current(module);
    return ir_type_ptr(IR_TYPE_CHAR_T);
}

static const char* ir_lower_binding_storage_name(const IRLowerBinding* b)
{
    if (!b) return NULL;
    if (b->is_static_storage && b->global_name) return b->global_name;
    return b->name;
}

static char* ir_lower_make_static_label(IRLowerCtx* ctx, const char* local_name)
{
    if (!ctx || !ctx->builder || !ctx->builder->module) return NULL;

    IRModule* m = ctx->builder->module;
    const char* func_name = ctx->current_func_name ? ctx->current_func_name : "func";
    const char* name = local_name ? local_name : "var";
    int id = ctx->static_local_counter++;

    int n = snprintf(NULL, 0, "__baa_static_%s_%s_%d", func_name, name, id);
    if (n <= 0) {
        ir_lower_report_error(ctx, NULL, "فشل توليد اسم متغير ساكن.");
        return NULL;
    }

    char* out = (char*)ir_arena_alloc(&m->arena, (size_t)n + 1, _Alignof(char));
    if (!out) {
        ir_lower_report_error(ctx, NULL, "فشل تخصيص اسم متغير ساكن.");
        return NULL;
    }

    (void)snprintf(out, (size_t)n + 1, "__baa_static_%s_%s_%d", func_name, name, id);
    return out;
}

static uint64_t pack_utf8_codepoint(uint32_t cp)
{
    // استبدال المحرف غير الصالح بـ U+FFFD
    const uint32_t repl = 0xFFFDu;
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        cp = repl;
    }

    unsigned char b[4] = {0, 0, 0, 0};
    unsigned len = 0;

    if (cp <= 0x7Fu) {
        b[0] = (unsigned char)cp;
        len = 1;
    } else if (cp <= 0x7FFu) {
        b[0] = (unsigned char)(0xC0u | ((cp >> 6) & 0x1Fu));
        b[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
        len = 2;
    } else if (cp <= 0xFFFFu) {
        b[0] = (unsigned char)(0xE0u | ((cp >> 12) & 0x0Fu));
        b[1] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
        len = 3;
    } else {
        b[0] = (unsigned char)(0xF0u | ((cp >> 18) & 0x07u));
        b[1] = (unsigned char)(0x80u | ((cp >> 12) & 0x3Fu));
        b[2] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
        len = 4;
    }

    uint64_t bytes_field = (uint64_t)b[0] |
                           ((uint64_t)b[1] << 8) |
                           ((uint64_t)b[2] << 16) |
                           ((uint64_t)b[3] << 24);
    return bytes_field | ((uint64_t)len << 32);
}

static IRType* ir_type_from_datatype(IRModule* module, DataType t) {
    switch (t) {
        case TYPE_BOOL:   return IR_TYPE_I1_T;
        case TYPE_I8:     return IR_TYPE_I8_T;
        case TYPE_I16:    return IR_TYPE_I16_T;
        case TYPE_I32:    return IR_TYPE_I32_T;
        case TYPE_U8:     return IR_TYPE_U8_T;
        case TYPE_U16:    return IR_TYPE_U16_T;
        case TYPE_U32:    return IR_TYPE_U32_T;
        case TYPE_U64:    return IR_TYPE_U64_T;
        case TYPE_STRING: return get_char_ptr_type(module);
        case TYPE_POINTER: return ir_type_ptr(IR_TYPE_I8_T);
        case TYPE_CHAR:   return IR_TYPE_CHAR_T;
        case TYPE_FLOAT:  return IR_TYPE_F64_T;
        case TYPE_VOID:   return IR_TYPE_VOID_T;
        case TYPE_ENUM:   return IR_TYPE_I64_T;
        case TYPE_INT:
        default:          return IR_TYPE_I64_T;
    }
}

static int64_t ir_lower_pointer_elem_size(const Node* ptr_expr)
{
    if (!ptr_expr) return 1;

    int depth = ptr_expr->inferred_ptr_depth;
    DataType base = ptr_expr->inferred_ptr_base_type;

    if (depth > 1) return 8;
    if (depth <= 0) return 1;

    switch (base)
    {
        case TYPE_BOOL:
        case TYPE_I8:
        case TYPE_U8:
            return 1;
        case TYPE_I16:
        case TYPE_U16:
            return 2;
        case TYPE_I32:
        case TYPE_U32:
            return 4;
        case TYPE_INT:
        case TYPE_U64:
        case TYPE_CHAR:
        case TYPE_FLOAT:
        case TYPE_ENUM:
        case TYPE_STRING:
        case TYPE_POINTER:
            return 8;
        default:
            return 1;
    }
}

static IRValue* cast_i1_to_i64(IRLowerCtx* ctx, IRValue* v)
{
    if (!ctx || !ctx->builder || !v) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (v->type && v->type->kind == IR_TYPE_I64) return v;
    int r = ir_builder_emit_cast(ctx->builder, v, IR_TYPE_I64_T);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* lower_char_decode_to_i64(IRLowerCtx* ctx, IRValue* ch)
{
    if (!ctx || !ctx->builder || !ch) return ir_value_const_int(0, IR_TYPE_I64_T);

    // التمثيل المعبأ: i64 = bytes(32-bit) + len * 2^32
    IRValue* packed = ch;
    if (!packed->type || packed->type->kind != IR_TYPE_I64) {
        int pr = ir_builder_emit_cast(ctx->builder, ch, IR_TYPE_I64_T);
        packed = ir_value_reg(pr, IR_TYPE_I64_T);
    }

    IRValue* c_2p32 = ir_value_const_int(4294967296LL, IR_TYPE_I64_T);
    int len_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* len = ir_value_reg(len_r, IR_TYPE_I64_T);

    int bytes_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    IRValue* c256 = ir_value_const_int(256, IR_TYPE_I64_T);

    int b0_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, bytes, c256);
    IRValue* b0 = ir_value_reg(b0_r, IR_TYPE_I64_T);
    int t1_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, bytes, c256);
    IRValue* t1 = ir_value_reg(t1_r, IR_TYPE_I64_T);
    int b1_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, t1, c256);
    IRValue* b1 = ir_value_reg(b1_r, IR_TYPE_I64_T);
    int t2_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, t1, c256);
    IRValue* t2 = ir_value_reg(t2_r, IR_TYPE_I64_T);
    int b2_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, t2, c256);
    IRValue* b2 = ir_value_reg(b2_r, IR_TYPE_I64_T);
    int t3_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, t2, c256);
    IRValue* t3 = ir_value_reg(t3_r, IR_TYPE_I64_T);
    int b3_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, t3, c256);
    IRValue* b3 = ir_value_reg(b3_r, IR_TYPE_I64_T);

    // flags f1..f4
    IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
    IRValue* two = ir_value_const_int(2, IR_TYPE_I64_T);
    IRValue* three = ir_value_const_int(3, IR_TYPE_I64_T);
    IRValue* four = ir_value_const_int(4, IR_TYPE_I64_T);

    int f1b = ir_builder_emit_cmp_eq(ctx->builder, len, one);
    int f2b = ir_builder_emit_cmp_eq(ctx->builder, len, two);
    int f3b = ir_builder_emit_cmp_eq(ctx->builder, len, three);
    int f4b = ir_builder_emit_cmp_eq(ctx->builder, len, four);

    IRValue* f1 = cast_i1_to_i64(ctx, ir_value_reg(f1b, IR_TYPE_I1_T));
    IRValue* f2 = cast_i1_to_i64(ctx, ir_value_reg(f2b, IR_TYPE_I1_T));
    IRValue* f3 = cast_i1_to_i64(ctx, ir_value_reg(f3b, IR_TYPE_I1_T));
    IRValue* f4 = cast_i1_to_i64(ctx, ir_value_reg(f4b, IR_TYPE_I1_T));

    // cp1 = b0
    IRValue* cp1 = b0;

    // cp2 = (b0-0xC0)*64 + (b1-0x80)
    int a0_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b0, ir_value_const_int(0xC0, IR_TYPE_I64_T));
    IRValue* a0 = ir_value_reg(a0_r, IR_TYPE_I64_T);
    int a0s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, a0, ir_value_const_int(64, IR_TYPE_I64_T));
    IRValue* a0s = ir_value_reg(a0s_r, IR_TYPE_I64_T);
    int a1_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b1, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* a1 = ir_value_reg(a1_r, IR_TYPE_I64_T);
    int cp2_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, a0s, a1);
    IRValue* cp2 = ir_value_reg(cp2_r, IR_TYPE_I64_T);

    // cp3 = (b0-0xE0)*4096 + (b1-0x80)*64 + (b2-0x80)
    int b0m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b0, ir_value_const_int(0xE0, IR_TYPE_I64_T));
    IRValue* b0m = ir_value_reg(b0m_r, IR_TYPE_I64_T);
    int b0s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0m, ir_value_const_int(4096, IR_TYPE_I64_T));
    IRValue* b0s = ir_value_reg(b0s_r, IR_TYPE_I64_T);
    int b1m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b1, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* b1m = ir_value_reg(b1m_r, IR_TYPE_I64_T);
    int b1s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1m, ir_value_const_int(64, IR_TYPE_I64_T));
    IRValue* b1s = ir_value_reg(b1s_r, IR_TYPE_I64_T);
    int b2m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b2, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* b2m = ir_value_reg(b2m_r, IR_TYPE_I64_T);
    int t3a_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0s, b1s);
    IRValue* t3a = ir_value_reg(t3a_r, IR_TYPE_I64_T);
    int cp3_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, t3a, b2m);
    IRValue* cp3 = ir_value_reg(cp3_r, IR_TYPE_I64_T);

    // cp4 = (b0-0xF0)*262144 + (b1-0x80)*4096 + (b2-0x80)*64 + (b3-0x80)
    int c0m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b0, ir_value_const_int(0xF0, IR_TYPE_I64_T));
    IRValue* c0m = ir_value_reg(c0m_r, IR_TYPE_I64_T);
    int c0s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, c0m, ir_value_const_int(262144, IR_TYPE_I64_T));
    IRValue* c0s = ir_value_reg(c0s_r, IR_TYPE_I64_T);
    int c1m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b1, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* c1m = ir_value_reg(c1m_r, IR_TYPE_I64_T);
    int c1s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, c1m, ir_value_const_int(4096, IR_TYPE_I64_T));
    IRValue* c1s = ir_value_reg(c1s_r, IR_TYPE_I64_T);
    int c2m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b2, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* c2m = ir_value_reg(c2m_r, IR_TYPE_I64_T);
    int c2s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, c2m, ir_value_const_int(64, IR_TYPE_I64_T));
    IRValue* c2s = ir_value_reg(c2s_r, IR_TYPE_I64_T);
    int c3m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b3, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* c3m = ir_value_reg(c3m_r, IR_TYPE_I64_T);
    int t4a_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, c0s, c1s);
    IRValue* t4a = ir_value_reg(t4a_r, IR_TYPE_I64_T);
    int t4b_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, t4a, c2s);
    IRValue* t4b = ir_value_reg(t4b_r, IR_TYPE_I64_T);
    int cp4_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, t4b, c3m);
    IRValue* cp4 = ir_value_reg(cp4_r, IR_TYPE_I64_T);

    // اختيار حسب الأعلام: cp = cp1*f1 + cp2*f2 + cp3*f3 + cp4*f4
    int cp1m_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, cp1, f1);
    int cp2m_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, cp2, f2);
    int cp3m_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, cp3, f3);
    int cp4m_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, cp4, f4);
    IRValue* cp1m = ir_value_reg(cp1m_r, IR_TYPE_I64_T);
    IRValue* cp2m = ir_value_reg(cp2m_r, IR_TYPE_I64_T);
    IRValue* cp3m = ir_value_reg(cp3m_r, IR_TYPE_I64_T);
    IRValue* cp4m = ir_value_reg(cp4m_r, IR_TYPE_I64_T);
    int s12_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, cp1m, cp2m);
    IRValue* s12 = ir_value_reg(s12_r, IR_TYPE_I64_T);
    int s34_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, cp3m, cp4m);
    IRValue* s34 = ir_value_reg(s34_r, IR_TYPE_I64_T);
    int cp_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, s12, s34);
    return ir_value_reg(cp_r, IR_TYPE_I64_T);
}

static IRValue* lower_codepoint_encode_to_char(IRLowerCtx* ctx, IRValue* cp_in)
{
    if (!ctx || !ctx->builder || !cp_in) return ir_value_const_int(0, IR_TYPE_CHAR_T);

    IRValue* cp = ensure_i64(ctx, cp_in);

    // invalid = (cp > 0x10FFFF) || (0xD800 <= cp <= 0xDFFF)
    int gtmax_b = ir_builder_emit_cmp_gt(ctx->builder, cp, ir_value_const_int(0x10FFFF, IR_TYPE_I64_T));
    IRValue* gtmax = ir_value_reg(gtmax_b, IR_TYPE_I1_T);
    int ge_b = ir_builder_emit_cmp_ge(ctx->builder, cp, ir_value_const_int(0xD800, IR_TYPE_I64_T));
    IRValue* ge = ir_value_reg(ge_b, IR_TYPE_I1_T);
    int le_b = ir_builder_emit_cmp_le(ctx->builder, cp, ir_value_const_int(0xDFFF, IR_TYPE_I64_T));
    IRValue* le = ir_value_reg(le_b, IR_TYPE_I1_T);
    int sur_b = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ge, le);
    IRValue* sur = ir_value_reg(sur_b, IR_TYPE_I1_T);
    int inv_b = ir_builder_emit_or(ctx->builder, IR_TYPE_I1_T, gtmax, sur);
    IRValue* inv = ir_value_reg(inv_b, IR_TYPE_I1_T);
    IRValue* inv64 = cast_i1_to_i64(ctx, inv);

    // cp = cp + inv*(0xFFFD - cp)
    int dcp_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0xFFFD, IR_TYPE_I64_T), cp);
    IRValue* dcp = ir_value_reg(dcp_r, IR_TYPE_I64_T);
    int adj_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, inv64, dcp);
    IRValue* adj = ir_value_reg(adj_r, IR_TYPE_I64_T);
    int cpv_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, cp, adj);
    IRValue* cpv = ir_value_reg(cpv_r, IR_TYPE_I64_T);

    // الطول = 1 + (cpv > 0x7F) + (cpv > 0x7FF) + (cpv > 0xFFFF)
    int g1_b = ir_builder_emit_cmp_gt(ctx->builder, cpv, ir_value_const_int(0x7F, IR_TYPE_I64_T));
    int g2_b = ir_builder_emit_cmp_gt(ctx->builder, cpv, ir_value_const_int(0x7FF, IR_TYPE_I64_T));
    int g3_b = ir_builder_emit_cmp_gt(ctx->builder, cpv, ir_value_const_int(0xFFFF, IR_TYPE_I64_T));
    IRValue* g1 = cast_i1_to_i64(ctx, ir_value_reg(g1_b, IR_TYPE_I1_T));
    IRValue* g2 = cast_i1_to_i64(ctx, ir_value_reg(g2_b, IR_TYPE_I1_T));
    IRValue* g3 = cast_i1_to_i64(ctx, ir_value_reg(g3_b, IR_TYPE_I1_T));
    int tlen1_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(1, IR_TYPE_I64_T), g1);
    IRValue* tlen1 = ir_value_reg(tlen1_r, IR_TYPE_I64_T);
    int tlen2_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, tlen1, g2);
    IRValue* tlen2 = ir_value_reg(tlen2_r, IR_TYPE_I64_T);
    int len_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, tlen2, g3);
    IRValue* len = ir_value_reg(len_r, IR_TYPE_I64_T);

    // if invalid, force len = 3
    int dlen_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(3, IR_TYPE_I64_T), len);
    IRValue* dlen = ir_value_reg(dlen_r, IR_TYPE_I64_T);
    int ladj_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, inv64, dlen);
    IRValue* ladj = ir_value_reg(ladj_r, IR_TYPE_I64_T);
    int lenv_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, len, ladj);
    IRValue* lenv = ir_value_reg(lenv_r, IR_TYPE_I64_T);

    // flags for lenv
    int f1b = ir_builder_emit_cmp_eq(ctx->builder, lenv, ir_value_const_int(1, IR_TYPE_I64_T));
    int f2b = ir_builder_emit_cmp_eq(ctx->builder, lenv, ir_value_const_int(2, IR_TYPE_I64_T));
    int f3b = ir_builder_emit_cmp_eq(ctx->builder, lenv, ir_value_const_int(3, IR_TYPE_I64_T));
    int f4b = ir_builder_emit_cmp_eq(ctx->builder, lenv, ir_value_const_int(4, IR_TYPE_I64_T));
    IRValue* f1 = cast_i1_to_i64(ctx, ir_value_reg(f1b, IR_TYPE_I1_T));
    IRValue* f2 = cast_i1_to_i64(ctx, ir_value_reg(f2b, IR_TYPE_I1_T));
    IRValue* f3 = cast_i1_to_i64(ctx, ir_value_reg(f3b, IR_TYPE_I1_T));
    IRValue* f4 = cast_i1_to_i64(ctx, ir_value_reg(f4b, IR_TYPE_I1_T));

    // compute bytes for each length
    IRValue* d64 = ir_value_const_int(64, IR_TYPE_I64_T);
    IRValue* d4096 = ir_value_const_int(4096, IR_TYPE_I64_T);
    IRValue* d262144 = ir_value_const_int(262144, IR_TYPE_I64_T);

    // طول 1
    IRValue* b0_1 = cpv;
    IRValue* b1_1 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* b2_1 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* b3_1 = ir_value_const_int(0, IR_TYPE_I64_T);

    // طول 2
    int q2_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    int r2_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* q2 = ir_value_reg(q2_r, IR_TYPE_I64_T);
    IRValue* r2 = ir_value_reg(r2_r, IR_TYPE_I64_T);
    int b0_2r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0xC0, IR_TYPE_I64_T), q2);
    int b1_2r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r2);
    IRValue* b0_2 = ir_value_reg(b0_2r, IR_TYPE_I64_T);
    IRValue* b1_2 = ir_value_reg(b1_2r, IR_TYPE_I64_T);
    IRValue* b2_2 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* b3_2 = ir_value_const_int(0, IR_TYPE_I64_T);

    // طول 3
    int q31_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d4096);
    IRValue* q31 = ir_value_reg(q31_r, IR_TYPE_I64_T);
    int q32_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* q32 = ir_value_reg(q32_r, IR_TYPE_I64_T);
    int q32m_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, q32, d64);
    IRValue* q32m = ir_value_reg(q32m_r, IR_TYPE_I64_T);
    int r3_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* r3 = ir_value_reg(r3_r, IR_TYPE_I64_T);
    int b0_3r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0xE0, IR_TYPE_I64_T), q31);
    int b1_3r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q32m);
    int b2_3r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r3);
    IRValue* b0_3 = ir_value_reg(b0_3r, IR_TYPE_I64_T);
    IRValue* b1_3 = ir_value_reg(b1_3r, IR_TYPE_I64_T);
    IRValue* b2_3 = ir_value_reg(b2_3r, IR_TYPE_I64_T);
    IRValue* b3_3 = ir_value_const_int(0, IR_TYPE_I64_T);

    // طول 4
    int q41_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d262144);
    IRValue* q41 = ir_value_reg(q41_r, IR_TYPE_I64_T);
    int q42_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d4096);
    IRValue* q42 = ir_value_reg(q42_r, IR_TYPE_I64_T);
    int q42m_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, q42, d64);
    IRValue* q42m = ir_value_reg(q42m_r, IR_TYPE_I64_T);
    int q43_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* q43 = ir_value_reg(q43_r, IR_TYPE_I64_T);
    int q43m_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, q43, d64);
    IRValue* q43m = ir_value_reg(q43m_r, IR_TYPE_I64_T);
    int r4_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* r4 = ir_value_reg(r4_r, IR_TYPE_I64_T);
    int b0_4r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0xF0, IR_TYPE_I64_T), q41);
    int b1_4r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q42m);
    int b2_4r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q43m);
    int b3_4r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r4);
    IRValue* b0_4 = ir_value_reg(b0_4r, IR_TYPE_I64_T);
    IRValue* b1_4 = ir_value_reg(b1_4r, IR_TYPE_I64_T);
    IRValue* b2_4 = ir_value_reg(b2_4r, IR_TYPE_I64_T);
    IRValue* b3_4 = ir_value_reg(b3_4r, IR_TYPE_I64_T);

    // اختيار البايتات حسب الأعلام
    IRValue* b0s1 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0_1, f1), IR_TYPE_I64_T);
    IRValue* b0s2 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0_2, f2), IR_TYPE_I64_T);
    IRValue* b0s3 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0_3, f3), IR_TYPE_I64_T);
    IRValue* b0s4 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0_4, f4), IR_TYPE_I64_T);
    IRValue* b1s1v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1_1, f1), IR_TYPE_I64_T);
    IRValue* b1s2v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1_2, f2), IR_TYPE_I64_T);
    IRValue* b1s3v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1_3, f3), IR_TYPE_I64_T);
    IRValue* b1s4v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1_4, f4), IR_TYPE_I64_T);
    IRValue* b2s1v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2_1, f1), IR_TYPE_I64_T);
    IRValue* b2s2v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2_2, f2), IR_TYPE_I64_T);
    IRValue* b2s3v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2_3, f3), IR_TYPE_I64_T);
    IRValue* b2s4v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2_4, f4), IR_TYPE_I64_T);
    IRValue* b3s1v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3_1, f1), IR_TYPE_I64_T);
    IRValue* b3s2v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3_2, f2), IR_TYPE_I64_T);
    IRValue* b3s3v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3_3, f3), IR_TYPE_I64_T);
    IRValue* b3s4v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3_4, f4), IR_TYPE_I64_T);

    IRValue* b0 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0s1, b0s2), IR_TYPE_I64_T),
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0s3, b0s4), IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b1 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b1s1v, b1s2v), IR_TYPE_I64_T),
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b1s3v, b1s4v), IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b2 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b2s1v, b2s2v), IR_TYPE_I64_T),
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b2s3v, b2s4v), IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b3 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b3s1v, b3s2v), IR_TYPE_I64_T),
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b3s3v, b3s4v), IR_TYPE_I64_T)), IR_TYPE_I64_T);

    // bytes_field = b0 + b1*256 + b2*65536 + b3*16777216
    int b1s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1, ir_value_const_int(256, IR_TYPE_I64_T));
    int b2s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2, ir_value_const_int(65536, IR_TYPE_I64_T));
    int b3s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3, ir_value_const_int(16777216, IR_TYPE_I64_T));
    IRValue* b1s = ir_value_reg(b1s_r, IR_TYPE_I64_T);
    IRValue* b2s = ir_value_reg(b2s_r, IR_TYPE_I64_T);
    IRValue* b3s = ir_value_reg(b3s_r, IR_TYPE_I64_T);
    int tbf_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0, b1s);
    IRValue* tbf = ir_value_reg(tbf_r, IR_TYPE_I64_T);
    int tbf2_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, tbf, b2s);
    IRValue* tbf2 = ir_value_reg(tbf2_r, IR_TYPE_I64_T);
    int bytesf_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, tbf2, b3s);
    IRValue* bytesf = ir_value_reg(bytesf_r, IR_TYPE_I64_T);

    int len_shift_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, lenv, ir_value_const_int(4294967296LL, IR_TYPE_I64_T));
    IRValue* lsh = ir_value_reg(len_shift_r, IR_TYPE_I64_T);
    int packed_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, bytesf, lsh);
    IRValue* packedv = ir_value_reg(packed_r, IR_TYPE_I64_T);

    int cr = ir_builder_emit_cast(ctx->builder, packedv, IR_TYPE_CHAR_T);
    return ir_value_reg(cr, IR_TYPE_CHAR_T);
}

static IRValue* ensure_i64(IRLowerCtx* ctx, IRValue* v)
{
    if (!ctx || !ctx->builder || !v) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (v->type && v->type->kind == IR_TYPE_CHAR) {
        return lower_char_decode_to_i64(ctx, v);
    }
    if (v->type && v->type->kind == IR_TYPE_I64) return v;
    int r = ir_builder_emit_cast(ctx->builder, v, IR_TYPE_I64_T);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* cast_to(IRLowerCtx* ctx, IRValue* v, IRType* to)
{
    if (!ctx || !ctx->builder || !v || !to) return v;
    if (v->type && ir_types_equal(v->type, to)) return v;

    if (v->type && v->type->kind == IR_TYPE_CHAR && to->kind != IR_TYPE_CHAR)
    {
        // فك الترميز إلى i64 ثم التحويل إلى الوجهة المطلوبة.
        IRValue* cp64 = lower_char_decode_to_i64(ctx, v);
        if (to->kind == IR_TYPE_I64) return cp64;
        int r = ir_builder_emit_cast(ctx->builder, cp64, to);
        return ir_value_reg(r, to);
    }
    if (to->kind == IR_TYPE_CHAR) {
        // نعتبر التحويل إلى حرف على أنه ترميز UTF-8 من نقطة-كود (i64).
        if (v->type && v->type->kind == IR_TYPE_I64) {
            return lower_codepoint_encode_to_char(ctx, v);
        }
        // دعم: i32 -> حرف
        if (v->type && v->type->kind == IR_TYPE_I32) {
            IRValue* cp64 = ensure_i64(ctx, v);
            return lower_codepoint_encode_to_char(ctx, cp64);
        }
    }

    int r = ir_builder_emit_cast(ctx->builder, v, to);
    return ir_value_reg(r, to);
}

// ============================================================================
// Control-flow helpers (v0.3.0.5)
// ============================================================================

static int next_label_id(IRLowerCtx* ctx) {
    if (!ctx) return 0;
    return ctx->label_counter++;
}

static IRBlock* cf_create_block(IRLowerCtx* ctx, const char* base_label) {
    if (!ctx || !ctx->builder || !base_label) return NULL;

    char label[128];
    snprintf(label, sizeof(label), "%s_%d", base_label, next_label_id(ctx));
    return ir_builder_create_block(ctx->builder, label);
}

static void cf_push(IRLowerCtx* ctx, IRBlock* break_target, IRBlock* continue_target) {
    if (!ctx) return;
    if (ctx->cf_depth >= (int)(sizeof(ctx->break_targets) / sizeof(ctx->break_targets[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد مكدس التحكم (break/continue).");
        return;
    }
    ctx->break_targets[ctx->cf_depth] = break_target;
    ctx->continue_targets[ctx->cf_depth] = continue_target;
    ctx->cf_depth++;
}

static void cf_pop(IRLowerCtx* ctx) {
    if (!ctx) return;
    if (ctx->cf_depth <= 0) {
        ir_lower_report_error(ctx, NULL, "نقص في مكدس التحكم (break/continue).");
        return;
    }
    ctx->cf_depth--;
}

static IRBlock* cf_current_break(IRLowerCtx* ctx) {
    if (!ctx || ctx->cf_depth <= 0) return NULL;
    return ctx->break_targets[ctx->cf_depth - 1];
}

static IRBlock* cf_current_continue(IRLowerCtx* ctx) {
    if (!ctx || ctx->cf_depth <= 0) return NULL;
    return ctx->continue_targets[ctx->cf_depth - 1];
}

static int ir_lower_index_count(Node* indices)
{
    int n = 0;
    for (Node* p = indices; p; p = p->next) n++;
    return n;
}

static Node* ir_lower_nth_index(Node* indices, int n)
{
    if (n < 0) return NULL;
    int i = 0;
    for (Node* p = indices; p; p = p->next, i++) {
        if (i == n) return p;
    }
    return NULL;
}

static Node* ir_lower_find_global_array_decl(IRLowerCtx* ctx, const char* name)
{
    if (!ctx || !ctx->program_root || !name) return NULL;
    if (ctx->program_root->type != NODE_PROGRAM) return NULL;

    for (Node* d = ctx->program_root->data.program.declarations; d; d = d->next) {
        if (d->type != NODE_ARRAY_DECL) continue;
        if (!d->data.array_decl.is_global) continue;
        if (!d->data.array_decl.name) continue;
        if (strcmp(d->data.array_decl.name, name) == 0) {
            return d;
        }
    }
    return NULL;
}

static void ir_lower_emit_abort_path(IRLowerCtx* ctx)
{
    if (!ctx || !ctx->builder) return;

    ir_builder_emit_call_void(ctx->builder, "abort", NULL, 0);

    IRFunc* f = ctx->builder->current_func;
    if (!f || !f->ret_type || f->ret_type->kind == IR_TYPE_VOID) {
        ir_builder_emit_ret_void(ctx->builder);
    } else {
        IRValue* z = ir_value_const_int(0, f->ret_type);
        ir_builder_emit_ret(ctx->builder, z);
    }
}

static void ir_lower_emit_debug_bounds_check(IRLowerCtx* ctx, const Node* site, IRValue* idx, int dim)
{
    if (!ctx || !ctx->builder || !idx || !ctx->enable_bounds_checks) return;
    if (dim <= 0) return;

    IRValue* i64_idx = ensure_i64(ctx, idx);
    IRValue* zero = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* dimv = ir_value_const_int((int64_t)dim, IR_TYPE_I64_T);

    int ge = ir_builder_emit_cmp_ge(ctx->builder, i64_idx, zero);
    int lt = ir_builder_emit_cmp_lt(ctx->builder, i64_idx, dimv);
    IRValue* ge_v = ir_value_reg(ge, IR_TYPE_I1_T);
    IRValue* lt_v = ir_value_reg(lt, IR_TYPE_I1_T);
    int okb = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ge_v, lt_v);
    IRValue* ok = ir_value_reg(okb, IR_TYPE_I1_T);

    IRBlock* pass = cf_create_block(ctx, "حدود_سليم");
    IRBlock* fail = cf_create_block(ctx, "حدود_فشل");
    if (!pass || !fail) return;

    ir_builder_emit_br_cond(ctx->builder, ok, pass, fail);

    ir_builder_set_insert_point(ctx->builder, fail);
    ir_lower_set_loc(ctx->builder, site);
    ir_lower_emit_abort_path(ctx);

    ir_builder_set_insert_point(ctx->builder, pass);
    ir_lower_set_loc(ctx->builder, site);
}

static IRValue* ir_lower_build_linear_index(IRLowerCtx* ctx,
                                            const Node* site,
                                            Node* indices,
                                            int index_count,
                                            const int* dims,
                                            int rank)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (!indices) return ir_value_const_int(0, IR_TYPE_I64_T);

    int effective_count = index_count > 0 ? index_count : ir_lower_index_count(indices);
    if (effective_count <= 0) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* linear = NULL;
    int i = 0;
    for (Node* idx_node = indices; idx_node && i < effective_count; idx_node = idx_node->next, i++) {
        IRValue* idx0 = lower_expr(ctx, idx_node);
        IRValue* idx = ensure_i64(ctx, idx0);

        if (dims && i < rank) {
            ir_lower_emit_debug_bounds_check(ctx, site, idx, dims[i]);
        }

        if (!linear) {
            linear = idx;
            continue;
        }

        int dim = (dims && i < rank) ? dims[i] : 0;
        if (dim > 0) {
            int mr = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, linear,
                                         ir_value_const_int((int64_t)dim, IR_TYPE_I64_T));
            linear = ir_value_reg(mr, IR_TYPE_I64_T);
        }

        int ar = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, linear, idx);
        linear = ir_value_reg(ar, IR_TYPE_I64_T);
    }

    if (rank > 0 && effective_count != rank) {
        ir_lower_report_error(ctx, site,
                              "عدد فهارس المصفوفة غير صحيح أثناء خفض IR (expected=%d, got=%d).",
                              rank, effective_count);
    }

    return linear ? linear : ir_value_const_int(0, IR_TYPE_I64_T);
}

static IRValue* lower_to_bool(IRLowerCtx* ctx, IRValue* value) {
    if (!ctx || !ctx->builder || !value) return ir_value_const_int(0, IR_TYPE_I1_T);

    // If already i1, keep it.
    if (value->type && value->type->kind == IR_TYPE_I1) {
        return value;
    }

    // Compare value != 0 to produce i1.
    IRType* vt = value->type ? value->type : IR_TYPE_I64_T;
    IRValue* zero = ir_value_const_int(0, vt);

    int reg = ir_builder_emit_cmp_ne(ctx->builder, value, zero);
    return ir_value_reg(reg, IR_TYPE_I1_T);
}

static IROp ir_binop_to_irop(OpType op) {
    switch (op) {
        case OP_ADD: return IR_OP_ADD;
        case OP_SUB: return IR_OP_SUB;
        case OP_MUL: return IR_OP_MUL;
        case OP_DIV: return IR_OP_DIV;
        case OP_MOD: return IR_OP_MOD;
        case OP_BIT_AND: return IR_OP_AND;
        case OP_BIT_OR:  return IR_OP_OR;
        case OP_BIT_XOR: return IR_OP_XOR;
        case OP_SHL:     return IR_OP_SHL;
        case OP_SHR:     return IR_OP_SHR;
        default:     return IR_OP_NOP;
    }
}

static IRCmpPred ir_cmp_to_pred(OpType op, bool is_unsigned) {
    switch (op) {
        case OP_EQ:  return IR_CMP_EQ;
        case OP_NEQ: return IR_CMP_NE;
        case OP_GT:  return is_unsigned ? IR_CMP_UGT : IR_CMP_GT;
        case OP_LT:  return is_unsigned ? IR_CMP_ULT : IR_CMP_LT;
        case OP_GTE: return is_unsigned ? IR_CMP_UGE : IR_CMP_GE;
        case OP_LTE: return is_unsigned ? IR_CMP_ULE : IR_CMP_LE;
        default:     return IR_CMP_EQ;
    }
}

static bool lower_datatype_is_unsigned_int(DataType t)
{
    return t == TYPE_U8 || t == TYPE_U16 || t == TYPE_U32 || t == TYPE_U64;
}

static int lower_datatype_int_rank(DataType t)
{
    switch (t)
    {
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_I16:
        case TYPE_U16:
        case TYPE_I32:
        case TYPE_U32:
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_ENUM:
            return 32;
        case TYPE_INT:
        case TYPE_U64:
            return 64;
        default:
            return 0;
    }
}

static DataType lower_int_promote_datatype(DataType t)
{
    switch (t)
    {
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_ENUM:
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_I16:
        case TYPE_U16:
            return TYPE_I32;
        case TYPE_I32:
        case TYPE_U32:
        case TYPE_INT:
        case TYPE_U64:
            return t;
        default:
            return t;
    }
}

static DataType lower_unsigned_for_rank(int rank)
{
    return (rank >= 64) ? TYPE_U64 : TYPE_U32;
}

static DataType lower_usual_arith_datatype(DataType a, DataType b)
{
    DataType pa = lower_int_promote_datatype(a);
    DataType pb = lower_int_promote_datatype(b);

    if (pa == pb)
        return pa;

    int ra = lower_datatype_int_rank(pa);
    int rb = lower_datatype_int_rank(pb);
    if (ra == 0 || rb == 0)
        return TYPE_INT;

    bool ua = lower_datatype_is_unsigned_int(pa);
    bool ub = lower_datatype_is_unsigned_int(pb);
    if (ua == ub)
        return (ra >= rb) ? pa : pb;

    DataType u = ua ? pa : pb;
    DataType s = ua ? pb : pa;
    int ru = ua ? ra : rb;
    int rs = ua ? rb : ra;

    if (ru > rs)
        return u;

    if (rs > ru)
        return s;

    return lower_unsigned_for_rank(rs);
}

static IRType* lower_common_int_type(IRModule* module, DataType a, DataType b)
{
    DataType ct = lower_usual_arith_datatype(a, b);
    IRType* irt = ir_type_from_datatype(module, ct);
    if (!irt)
        return IR_TYPE_I64_T;
    return irt;
}

static void ir_lower_eval_call_args(IRLowerCtx* ctx, Node* args)
{
    for (Node* a = args; a; a = a->next) {
        (void)lower_expr(ctx, a);
    }
}

static IRValue* ir_lower_as_char_ptr(IRLowerCtx* ctx, IRValue* v)
{
    IRModule* m = (ctx && ctx->builder) ? ctx->builder->module : NULL;
    if (m) ir_module_set_current(m);
    IRType* char_ptr_t = get_char_ptr_type(m);
    if (!v) return ir_value_const_int(0, char_ptr_t);
    if (v->type && ir_types_equal(v->type, char_ptr_t)) return v;
    int cr = ir_builder_emit_cast(ctx->builder, v, char_ptr_t);
    return ir_value_reg(cr, char_ptr_t);
}

static IRValue* ir_lower_load_char_at(IRLowerCtx* ctx, IRValue* base, IRValue* idx)
{
    if (!ctx || !ctx->builder || !base || !idx) {
        return ir_value_const_int(0, IR_TYPE_CHAR_T);
    }
    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRValue* i64_idx = ensure_i64(ctx, idx);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, char_ptr_t, base, i64_idx);
    IRValue* elem_ptr = ir_value_reg(ep, char_ptr_t);
    int lr = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, elem_ptr);
    return ir_value_reg(lr, IR_TYPE_CHAR_T);
}

static void ir_lower_store_char_at(IRLowerCtx* ctx, IRValue* base, IRValue* idx, IRValue* ch)
{
    if (!ctx || !ctx->builder || !base || !idx || !ch) return;
    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRValue* i64_idx = ensure_i64(ctx, idx);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, char_ptr_t, base, i64_idx);
    IRValue* elem_ptr = ir_value_reg(ep, char_ptr_t);
    IRValue* v = cast_to(ctx, ch, IR_TYPE_CHAR_T);
    ir_builder_emit_store(ctx->builder, v, elem_ptr);
}

static IRValue* ir_lower_builtin_strlen_from_value(IRLowerCtx* ctx, const Node* site, IRValue* str_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRValue* base = ir_lower_as_char_ptr(ctx, str_in);
    if (!base) base = ir_value_const_int(0, char_ptr_t);

    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* idx_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    IRBlock* head = cf_create_block(ctx, "طول_نص_تحقق");
    IRBlock* body = cf_create_block(ctx, "طول_نص_جسم");
    IRBlock* done = cf_create_block(ctx, "طول_نص_نهاية");
    if (!head || !body || !done) {
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);

    IRValue* ch = ir_lower_load_char_at(ctx, base, idx);
    IRValue* ch_i64 = ensure_i64(ctx, ch);
    int is_end_r = ir_builder_emit_cmp_eq(b, ch_i64, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(is_end_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, done, body);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    int inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* inc = ir_value_reg(inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int len_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    return ir_value_reg(len_r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_strcmp_from_values(IRLowerCtx* ctx, const Node* site, IRValue* a_in, IRValue* b_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRValue* a = ir_lower_as_char_ptr(ctx, a_in);
    IRValue* c = ir_lower_as_char_ptr(ctx, b_in);

    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* idx_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    int res_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), res_ptr);

    IRBlock* head = cf_create_block(ctx, "قارن_نص_تحقق");
    IRBlock* same = cf_create_block(ctx, "قارن_نص_متساوي");
    IRBlock* diff = cf_create_block(ctx, "قارن_نص_مختلف");
    IRBlock* step = cf_create_block(ctx, "قارن_نص_زيادة");
    IRBlock* done = cf_create_block(ctx, "قارن_نص_نهاية");
    if (!head || !same || !diff || !step || !done) {
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);

    IRValue* ach = ir_lower_load_char_at(ctx, a, idx);
    IRValue* bch = ir_lower_load_char_at(ctx, c, idx);
    IRValue* ai = ensure_i64(ctx, ach);
    IRValue* bi = ensure_i64(ctx, bch);

    int eq_r = ir_builder_emit_cmp_eq(b, ai, bi);
    IRValue* eq = ir_value_reg(eq_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, eq, same, diff);

    ir_builder_set_insert_point(b, same);
    ir_lower_set_loc(b, site);
    int end_r = ir_builder_emit_cmp_eq(b, ai, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(end_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, done, step);

    ir_builder_set_insert_point(b, step);
    ir_lower_set_loc(b, site);
    int inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* inc = ir_value_reg(inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, diff);
    ir_lower_set_loc(b, site);
    int delta_r = ir_builder_emit_sub(b, IR_TYPE_I64_T, ai, bi);
    IRValue* delta = ir_value_reg(delta_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, delta, res_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, IR_TYPE_I64_T, res_ptr);
    return ir_value_reg(out_r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_copy_from_value(IRLowerCtx* ctx, const Node* site, IRValue* src_in)
{
    if (!ctx || !ctx->builder) {
        return ir_value_const_int(0, get_char_ptr_type(NULL));
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    IRValue* src = ir_lower_as_char_ptr(ctx, src_in);
    IRValue* len = ir_lower_builtin_strlen_from_value(ctx, site, src);

    int plus1_r = ir_builder_emit_add(b, IR_TYPE_I64_T, len, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* total = ir_value_reg(plus1_r, IR_TYPE_I64_T);
    int bytes_r = ir_builder_emit_mul(b, IR_TYPE_I64_T, total, ir_value_const_int(8, IR_TYPE_I64_T));
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    IRValue* margs[1] = { bytes };
    int raw_reg = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
    if (raw_reg < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc.");
        return ir_value_const_int(0, char_ptr_t);
    }
    IRValue* raw_ptr = ir_value_reg(raw_reg, i8_ptr_t);
    int dst_cast_r = ir_builder_emit_cast(b, raw_ptr, char_ptr_t);
    IRValue* dst = ir_value_reg(dst_cast_r, char_ptr_t);

    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    IRValue* null_char_ptr = ir_value_const_int(0, char_ptr_t);
    ir_builder_emit_store(b, null_char_ptr, res_ptr);

    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    int is_null_r = ir_builder_emit_cmp_eq(b, dst, null_char_ptr);
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* init = cf_create_block(ctx, "نسخ_نص_تهيئة");
    IRBlock* head = cf_create_block(ctx, "نسخ_نص_تحقق");
    IRBlock* body = cf_create_block(ctx, "نسخ_نص_جسم");
    IRBlock* done = cf_create_block(ctx, "نسخ_نص_نهاية");
    if (!init || !head || !body || !done) {
        return ir_value_const_int(0, char_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, done, init);
    }

    ir_builder_set_insert_point(b, init);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, dst, res_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);
    IRValue* ch = ir_lower_load_char_at(ctx, src, idx);
    ir_lower_store_char_at(ctx, dst, idx, ch);
    IRValue* chi = ensure_i64(ctx, ch);
    int is_end_r = ir_builder_emit_cmp_eq(b, chi, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(is_end_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, done, body);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    int inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* inc = ir_value_reg(inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(out_r, char_ptr_t);
}

static IRValue* ir_lower_builtin_concat_from_values(IRLowerCtx* ctx, const Node* site, IRValue* a_in, IRValue* b_in)
{
    if (!ctx || !ctx->builder) {
        return ir_value_const_int(0, get_char_ptr_type(NULL));
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    IRValue* a = ir_lower_as_char_ptr(ctx, a_in);
    IRValue* c = ir_lower_as_char_ptr(ctx, b_in);

    IRValue* len_a = ir_lower_builtin_strlen_from_value(ctx, site, a);
    IRValue* len_b = ir_lower_builtin_strlen_from_value(ctx, site, c);

    int sum_r = ir_builder_emit_add(b, IR_TYPE_I64_T, len_a, len_b);
    IRValue* sum = ir_value_reg(sum_r, IR_TYPE_I64_T);
    int total_r = ir_builder_emit_add(b, IR_TYPE_I64_T, sum, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* total = ir_value_reg(total_r, IR_TYPE_I64_T);
    int bytes_r = ir_builder_emit_mul(b, IR_TYPE_I64_T, total, ir_value_const_int(8, IR_TYPE_I64_T));
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    IRValue* margs[1] = { bytes };
    int raw_reg = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
    if (raw_reg < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc.");
        return ir_value_const_int(0, char_ptr_t);
    }
    IRValue* raw_ptr = ir_value_reg(raw_reg, i8_ptr_t);
    int dst_cast_r = ir_builder_emit_cast(b, raw_ptr, char_ptr_t);
    IRValue* dst = ir_value_reg(dst_cast_r, char_ptr_t);

    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    IRValue* null_char_ptr = ir_value_const_int(0, char_ptr_t);
    ir_builder_emit_store(b, null_char_ptr, res_ptr);

    int idx_a_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* idx_a_ptr = ir_value_reg(idx_a_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_a_ptr);

    int idx_b_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* idx_b_ptr = ir_value_reg(idx_b_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_b_ptr);

    int is_null_r = ir_builder_emit_cmp_eq(b, dst, null_char_ptr);
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* init = cf_create_block(ctx, "دمج_نص_تهيئة");
    IRBlock* a_head = cf_create_block(ctx, "دمج_نص_أ_تحقق");
    IRBlock* a_body = cf_create_block(ctx, "دمج_نص_أ_جسم");
    IRBlock* a_done = cf_create_block(ctx, "دمج_نص_أ_نهاية");
    IRBlock* b_head = cf_create_block(ctx, "دمج_نص_ب_تحقق");
    IRBlock* b_body = cf_create_block(ctx, "دمج_نص_ب_جسم");
    IRBlock* done = cf_create_block(ctx, "دمج_نص_نهاية");
    if (!init || !a_head || !a_body || !a_done || !b_head || !b_body || !done) {
        return ir_value_const_int(0, char_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, done, init);
    }

    ir_builder_set_insert_point(b, init);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, dst, res_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_a_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_b_ptr);
    ir_builder_emit_br(b, a_head);

    ir_builder_set_insert_point(b, a_head);
    ir_lower_set_loc(b, site);
    int ia_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_a_ptr);
    IRValue* ia = ir_value_reg(ia_r, IR_TYPE_I64_T);
    int cond_a_r = ir_builder_emit_cmp_lt(b, ia, len_a);
    IRValue* cond_a = ir_value_reg(cond_a_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cond_a, a_body, a_done);

    ir_builder_set_insert_point(b, a_body);
    ir_lower_set_loc(b, site);
    IRValue* cha = ir_lower_load_char_at(ctx, a, ia);
    ir_lower_store_char_at(ctx, dst, ia, cha);
    int ia_inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, ia, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* ia_inc = ir_value_reg(ia_inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, ia_inc, idx_a_ptr);
    ir_builder_emit_br(b, a_head);

    ir_builder_set_insert_point(b, a_done);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_b_ptr);
    ir_builder_emit_br(b, b_head);

    ir_builder_set_insert_point(b, b_head);
    ir_lower_set_loc(b, site);
    int ib_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_b_ptr);
    IRValue* ib = ir_value_reg(ib_r, IR_TYPE_I64_T);
    int dst_off_r = ir_builder_emit_add(b, IR_TYPE_I64_T, len_a, ib);
    IRValue* dst_off = ir_value_reg(dst_off_r, IR_TYPE_I64_T);
    IRValue* chb = ir_lower_load_char_at(ctx, c, ib);
    ir_lower_store_char_at(ctx, dst, dst_off, chb);
    IRValue* chb_i64 = ensure_i64(ctx, chb);
    int end_b_r = ir_builder_emit_cmp_eq(b, chb_i64, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* end_b = ir_value_reg(end_b_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, end_b, done, b_body);

    ir_builder_set_insert_point(b, b_body);
    ir_lower_set_loc(b, site);
    int ib_inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, ib, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* ib_inc = ir_value_reg(ib_inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, ib_inc, idx_b_ptr);
    ir_builder_emit_br(b, b_head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(out_r, char_ptr_t);
}

static IRValue* ir_lower_builtin_free_value(IRLowerCtx* ctx, const Node* site, IRValue* v)
{
    if (!ctx || !ctx->builder) return ir_builder_const_i64(0);
    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* as_char_ptr = ir_lower_as_char_ptr(ctx, v);
    IRValue* as_i8 = cast_to(ctx, as_char_ptr, i8_ptr_t);
    IRValue* args[1] = { as_i8 };
    ir_lower_set_loc(ctx->builder, site);
    ir_builder_emit_call_void(ctx->builder, "free", args, 1);
    return ir_builder_const_i64(0);
}

static IRValue* lower_call_expr(IRLowerCtx* ctx, Node* expr) {
    if (!ctx || !ctx->builder || !expr) return ir_builder_const_i64(0);

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    // دوال السلاسل المدمجة في v0.3.9 (سلوك C-like مع أسماء عربية).
    if (expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;
        Node* a2 = a1 ? a1->next : NULL;

        if (strcmp(n, "طول_نص") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'طول_نص' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* sv = lower_expr(ctx, a0);
            return ir_lower_builtin_strlen_from_value(ctx, expr, sv);
        }

        if (strcmp(n, "قارن_نص") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'قارن_نص' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* av = lower_expr(ctx, a0);
            IRValue* bv = lower_expr(ctx, a1);
            return ir_lower_builtin_strcmp_from_values(ctx, expr, av, bv);
        }

        if (strcmp(n, "نسخ_نص") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'نسخ_نص' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, get_char_ptr_type(m));
            }
            IRValue* sv = lower_expr(ctx, a0);
            return ir_lower_builtin_copy_from_value(ctx, expr, sv);
        }

        if (strcmp(n, "دمج_نص") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'دمج_نص' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, get_char_ptr_type(m));
            }
            IRValue* av = lower_expr(ctx, a0);
            IRValue* bv = lower_expr(ctx, a1);
            return ir_lower_builtin_concat_from_values(ctx, expr, av, bv);
        }

        if (strcmp(n, "حرر_نص") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'حرر_نص' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* sv = lower_expr(ctx, a0);
            return ir_lower_builtin_free_value(ctx, expr, sv);
        }
    }

    // حاول العثور على توقيع الدالة داخل وحدة IR (لمواءمة أنواع المعاملات/الإرجاع).
    IRFunc* callee = NULL;
    if (m && expr->data.call.name) {
        callee = ir_module_find_func(m, expr->data.call.name);
    }

    IRType* ret_type = NULL;
    if (callee && callee->ret_type) {
        ret_type = callee->ret_type;
    } else {
        ret_type = ir_type_from_datatype(m, expr->inferred_type);
    }
    if (!ret_type) ret_type = IR_TYPE_I64_T;

    // Count args
    int arg_count = 0;
    for (Node* a = expr->data.call.args; a; a = a->next) arg_count++;

    IRValue** args = NULL;
    if (arg_count > 0) {
        args = (IRValue**)malloc(sizeof(IRValue*) * (size_t)arg_count);
        if (!args) {
            ir_lower_report_error(ctx, expr, "فشل تخصيص معاملات النداء.");
            return ir_builder_const_i64(0);
        }
    }

    int i = 0;
    for (Node* a = expr->data.call.args; a; a = a->next) {
        IRValue* av = lower_expr(ctx, a);
        if (callee && i < callee->param_count && callee->params && callee->params[i].type) {
            av = cast_to(ctx, av, callee->params[i].type);
        }
        args[i++] = av;
    }

    int dest = ir_builder_emit_call(ctx->builder, expr->data.call.name, ret_type, args, arg_count);

    if (args) free(args);

    if (dest < 0) {
        // Void call used as expression: return 0 (placeholder).
        return ir_builder_const_i64(0);
    }

    return ir_value_reg(dest, ret_type);
}

static IRValue* lower_lvalue_address(IRLowerCtx* ctx, Node* expr, IRType** out_pointee_type)
{
    if (out_pointee_type) *out_pointee_type = IR_TYPE_I64_T;
    if (!ctx || !ctx->builder || !expr) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));

    if (expr->type == NODE_VAR_REF) {
        const char* name = expr->data.var_ref.name;
        IRLowerBinding* b = find_local(ctx, name);
        if (b) {
            IRType* vt = b->value_type ? b->value_type : IR_TYPE_I64_T;
            if (out_pointee_type) *out_pointee_type = vt;
            if (b->is_static_storage) {
                return ir_value_global(ir_lower_binding_storage_name(b), vt);
            }
            return ir_value_reg(b->ptr_reg, ir_type_ptr(vt));
        }

        IRGlobal* g = NULL;
        if (ctx->builder && ctx->builder->module && name) {
            g = ir_module_find_global(ctx->builder->module, name);
        }
        if (g && g->type) {
            if (out_pointee_type) *out_pointee_type = g->type;
            return ir_value_global(name, g->type);
        }

        ir_lower_report_error(ctx, expr, "لا يمكن أخذ عنوان متغير غير معروف '%s'.", name ? name : "???");
        return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
    }

    if (expr->type == NODE_UNARY_OP && expr->data.unary_op.op == UOP_DEREF) {
        IRValue* p = lower_expr(ctx, expr->data.unary_op.operand);
        if (out_pointee_type) {
            IRType* t = ir_type_from_datatype(ctx->builder->module, expr->inferred_type);
            if (!t || t->kind == IR_TYPE_VOID) t = IR_TYPE_I64_T;
            *out_pointee_type = t;
        }
        return p;
    }

    if (expr->type == NODE_MEMBER_ACCESS && expr->data.member_access.is_struct_member) {
        IRType* ptr_i8_t = ir_type_ptr(IR_TYPE_I8_T);
        IRValue* base_ptr = NULL;
        if (expr->data.member_access.root_is_global) {
            base_ptr = ir_value_global(expr->data.member_access.root_var, IR_TYPE_I8_T);
        } else {
            IRLowerBinding* b = find_local(ctx, expr->data.member_access.root_var);
            if (!b || !b->value_type) {
                ir_lower_report_error(ctx, expr, "هيكل محلي غير معروف '%s'.", expr->data.member_access.root_var);
                return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
            }
            if (b->is_static_storage) {
                base_ptr = ir_value_global(ir_lower_binding_storage_name(b), IR_TYPE_I8_T);
            } else {
                IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
                base_ptr = ir_value_reg(c, ptr_i8_t);
            }
        }
        IRValue* idx = ir_value_const_int((int64_t)expr->data.member_access.member_offset, IR_TYPE_I64_T);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
        IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);
        IRType* ft = ir_type_from_datatype(ctx->builder->module, expr->data.member_access.member_type);
        if (!ft || ft->kind == IR_TYPE_VOID) ft = IR_TYPE_I64_T;
        if (out_pointee_type) *out_pointee_type = ft;
        int fp = ir_builder_emit_cast(ctx->builder, byte_ptr, ir_type_ptr(ft));
        return ir_value_reg(fp, ir_type_ptr(ft));
    }

    ir_lower_report_error(ctx, expr, "أخذ العنوان '&' لهذا التعبير غير مدعوم.");
    return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
}

// ============================================================================
// Expression lowering (v0.3.0.3)
// ============================================================================

IRValue* lower_expr(IRLowerCtx* ctx, Node* expr) {
    if (!ctx || !ctx->builder || !expr) {
        return ir_builder_const_i64(0);
    }

    // تعيين موقع المصدر لهذه العقدة قبل توليد أي تعليمات.
    ir_lower_set_loc(ctx->builder, expr);

    switch (expr->type) {
        // -----------------------------------------------------------------
        // v0.3.0.3 required nodes
        // -----------------------------------------------------------------
        case NODE_INT:
            return ir_builder_const_i64((int64_t)expr->data.integer.value);

        case NODE_VAR_REF: {
            const char* name = expr->data.var_ref.name;

            // 1) Local variable (stack slot)
            IRLowerBinding* b = find_local(ctx, name);
            if (b) {
                const char* storage_name = ir_lower_binding_storage_name(b);
                if (b->value_type && b->value_type->kind == IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "لا يمكن استخدام المصفوفة '%s' بدون فهرس.", name ? name : "???");
                    return ir_builder_const_i64(0);
                }
                IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

                IRValue* ptr = NULL;
                if (b->is_static_storage) {
                    ptr = ir_value_global(storage_name, value_type);
                } else {
                    IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
                    ptr = ir_value_reg(b->ptr_reg, ptr_type);
                }
                int loaded = ir_builder_emit_load(ctx->builder, value_type, ptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, storage_name);
                return ir_value_reg(loaded, value_type);
            }

            // 2) Global variable (module scope)
            IRGlobal* g = NULL;
            if (ctx->builder && ctx->builder->module && name) {
                g = ir_module_find_global(ctx->builder->module, name);
            }
            if (g) {
                // Globals are addressable; load from @name
                IRValue* gptr = ir_value_global(name, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
                return ir_value_reg(loaded, g->type);
            }

            ir_lower_report_error(ctx, expr, "متغير غير معروف '%s'.", name ? name : "???");
            return ir_builder_const_i64(0);
        }

        case NODE_ARRAY_ACCESS: {
            const char* name = expr->data.array_op.name;
            int index_count = expr->data.array_op.index_count;
            if (index_count <= 0) index_count = ir_lower_index_count(expr->data.array_op.indices);

            IRLowerBinding* b = find_local(ctx, name);
            if (b) {
                const char* storage_name = ir_lower_binding_storage_name(b);

                // نص: الاسم يشير إلى قيمة ptr<char> مخزنة في alloca/رمز ساكن.
                if (!b->is_array &&
                    b->value_type && b->value_type->kind == IR_TYPE_PTR &&
                    b->value_type->data.pointee && b->value_type->data.pointee->kind == IR_TYPE_CHAR)
                {
                    Node* idx_node = expr->data.array_op.indices;
                    if (!idx_node) {
                        ir_lower_report_error(ctx, expr, "فهرس النص '%s' مفقود.", name ? name : "???");
                        return ir_builder_const_i64(0);
                    }

                    IRType* str_t = b->value_type;
                    IRValue* str_ptr = NULL;
                    if (b->is_static_storage) {
                        IRValue* gptr = ir_value_global(storage_name, str_t);
                        int loaded = ir_builder_emit_load(ctx->builder, str_t, gptr);
                        str_ptr = ir_value_reg(loaded, str_t);
                    } else {
                        IRType* ptr_to_str_t = ir_type_ptr(str_t);
                        IRValue* slot = ir_value_reg(b->ptr_reg, ptr_to_str_t);
                        int loaded = ir_builder_emit_load(ctx->builder, str_t, slot);
                        str_ptr = ir_value_reg(loaded, str_t);
                    }

                    IRValue* idx = ensure_i64(ctx, lower_expr(ctx, idx_node));
                    int ep = ir_builder_emit_ptr_offset(ctx->builder, str_t, str_ptr, idx);
                    IRValue* elem_ptr = ir_value_reg(ep, str_t);
                    int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, elem_ptr);
                    return ir_value_reg(ch, IR_TYPE_CHAR_T);
                }

                if (!b->is_array || !b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
                    for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                        (void)lower_expr(ctx, idx);
                    }
                    return ir_builder_const_i64(0);
                }

                bool elem_is_agg = (b->array_elem_type == TYPE_STRUCT || b->array_elem_type == TYPE_UNION);
                IRType* elem_t = elem_is_agg
                    ? IR_TYPE_I8_T
                    : (b->value_type->data.array.element ? b->value_type->data.array.element : IR_TYPE_I64_T);
                IRType* elem_ptr_t = ir_type_ptr(elem_t);
                bool elem_is_string = (!elem_is_agg &&
                                       elem_t && elem_t->kind == IR_TYPE_PTR &&
                                       elem_t->data.pointee &&
                                       elem_t->data.pointee->kind == IR_TYPE_CHAR);

                IRValue* base_ptr = NULL;
                if (b->is_static_storage) {
                    base_ptr = ir_value_global(storage_name, elem_t);
                } else {
                    IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                    int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
                    base_ptr = ir_value_reg(base_reg, elem_ptr_t);
                }

                if (!base_ptr) {
                    ir_lower_report_error(ctx, expr, "فشل بناء عنوان عنصر المصفوفة '%s'.", name ? name : "???");
                    for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                        (void)lower_expr(ctx, idx);
                    }
                    return ir_builder_const_i64(0);
                }

                if (elem_is_string && b->array_rank > 0 && index_count == b->array_rank + 1) {
                    IRValue* outer_linear = ir_lower_build_linear_index(ctx, expr,
                                                                        expr->data.array_op.indices,
                                                                        b->array_rank,
                                                                        b->array_dims,
                                                                        b->array_rank);
                    int ep_outer = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, outer_linear);
                    IRValue* outer_ptr = ir_value_reg(ep_outer, elem_ptr_t);
                    int ld_str = ir_builder_emit_load(ctx->builder, elem_t, outer_ptr);
                    IRValue* str_ptr = ir_value_reg(ld_str, elem_t);

                    Node* inner_node = ir_lower_nth_index(expr->data.array_op.indices, b->array_rank);
                    IRValue* inner_idx = ensure_i64(ctx, lower_expr(ctx, inner_node));
                    int ep_inner = ir_builder_emit_ptr_offset(ctx->builder, elem_t, str_ptr, inner_idx);
                    IRValue* ch_ptr = ir_value_reg(ep_inner, elem_t);
                    int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, ch_ptr);
                    return ir_value_reg(ch, IR_TYPE_CHAR_T);
                }

                IRValue* linear = ir_lower_build_linear_index(ctx, expr,
                                                              expr->data.array_op.indices,
                                                              index_count,
                                                              b->array_dims,
                                                              b->array_rank);
                int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
                IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

                if (elem_is_agg) {
                    ir_lower_report_error(ctx, expr, "استخدام عنصر مصفوفة مركبة كقيمة غير مدعوم.");
                    return ir_builder_const_i64(0);
                }

                int loaded = ir_builder_emit_load(ctx->builder, elem_t, elem_ptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
                return ir_value_reg(loaded, elem_t);
            }

            IRGlobal* g = NULL;
            if (ctx->builder && ctx->builder->module && name) {
                g = ir_module_find_global(ctx->builder->module, name);
            }
            if (!g || !g->type) {
                ir_lower_report_error(ctx, expr, "مصفوفة/نص غير معرّف '%s'.", name ? name : "???");
                for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                    (void)lower_expr(ctx, idx);
                }
                return ir_builder_const_i64(0);
            }

            // نص عام: global يحمل ptr<char>، لذا نحمّله أولاً ثم نفهرسه.
            if (g->type->kind == IR_TYPE_PTR && g->type->data.pointee &&
                g->type->data.pointee->kind == IR_TYPE_CHAR)
            {
                Node* idx_node = expr->data.array_op.indices;
                if (!idx_node) {
                    ir_lower_report_error(ctx, expr, "فهرس النص '%s' مفقود.", name ? name : "???");
                    return ir_builder_const_i64(0);
                }

                IRValue* gptr = ir_value_global(name, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                IRValue* str_ptr = ir_value_reg(loaded, g->type);

                IRValue* idx = ensure_i64(ctx, lower_expr(ctx, idx_node));
                int ep = ir_builder_emit_ptr_offset(ctx->builder, g->type, str_ptr, idx);
                IRValue* elem_ptr = ir_value_reg(ep, g->type);
                int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, elem_ptr);
                return ir_value_reg(ch, IR_TYPE_CHAR_T);
            }

            if (g->type->kind != IR_TYPE_ARRAY) {
                ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة.", name ? name : "???");
                for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                    (void)lower_expr(ctx, idx);
                }
                return ir_builder_const_i64(0);
            }

            Node* gdecl = ir_lower_find_global_array_decl(ctx, name);
            int rank = 1;
            const int* dims = NULL;
            DataType elem_dt = TYPE_INT;
            if (gdecl && gdecl->type == NODE_ARRAY_DECL) {
                rank = gdecl->data.array_decl.dim_count > 0 ? gdecl->data.array_decl.dim_count : 1;
                dims = gdecl->data.array_decl.dims;
                elem_dt = gdecl->data.array_decl.element_type;
            }

            bool elem_is_agg = (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION);
            IRType* elem_t = elem_is_agg
                ? IR_TYPE_I8_T
                : (g->type->data.array.element ? g->type->data.array.element : IR_TYPE_I64_T);
            IRType* elem_ptr_t = ir_type_ptr(elem_t);
            bool elem_is_string = (!elem_is_agg &&
                                   elem_t && elem_t->kind == IR_TYPE_PTR &&
                                   elem_t->data.pointee &&
                                   elem_t->data.pointee->kind == IR_TYPE_CHAR);
            IRValue* base_ptr = ir_value_global(name, elem_t);

            if (elem_is_string && rank > 0 && index_count == rank + 1) {
                IRValue* outer_linear = ir_lower_build_linear_index(ctx, expr,
                                                                    expr->data.array_op.indices,
                                                                    rank,
                                                                    dims,
                                                                    rank);
                int ep_outer = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, outer_linear);
                IRValue* outer_ptr = ir_value_reg(ep_outer, elem_ptr_t);
                int ld_str = ir_builder_emit_load(ctx->builder, elem_t, outer_ptr);
                IRValue* str_ptr = ir_value_reg(ld_str, elem_t);

                Node* inner_node = ir_lower_nth_index(expr->data.array_op.indices, rank);
                IRValue* inner_idx = ensure_i64(ctx, lower_expr(ctx, inner_node));
                int ep_inner = ir_builder_emit_ptr_offset(ctx->builder, elem_t, str_ptr, inner_idx);
                IRValue* ch_ptr = ir_value_reg(ep_inner, elem_t);
                int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, ch_ptr);
                return ir_value_reg(ch, IR_TYPE_CHAR_T);
            }

            IRValue* linear = ir_lower_build_linear_index(ctx, expr,
                                                          expr->data.array_op.indices,
                                                          index_count,
                                                          dims,
                                                          rank);
            int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
            IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

            if (elem_is_agg) {
                ir_lower_report_error(ctx, expr, "استخدام عنصر مصفوفة مركبة كقيمة غير مدعوم.");
                return ir_builder_const_i64(0);
            }

            int loaded = ir_builder_emit_load(ctx->builder, elem_t, elem_ptr);
            ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
            return ir_value_reg(loaded, elem_t);
        }

        case NODE_BIN_OP: {
            OpType op = expr->data.bin_op.op;

            // Arithmetic
            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                if (expr->inferred_type == TYPE_POINTER) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);

                    bool left_ptr = expr->data.bin_op.left && expr->data.bin_op.left->inferred_type == TYPE_POINTER;
                    IRValue* base_ptr = left_ptr ? lhs0 : rhs0;
                    IRValue* delta_v = left_ptr ? rhs0 : lhs0;
                    IRType* ptr_t = (base_ptr && base_ptr->type && base_ptr->type->kind == IR_TYPE_PTR)
                        ? base_ptr->type
                        : ir_type_ptr(IR_TYPE_I8_T);
                    base_ptr = cast_to(ctx, base_ptr, ptr_t);
                    IRValue* delta = ensure_i64(ctx, delta_v);

                    Node* pnode = left_ptr ? expr->data.bin_op.left : expr->data.bin_op.right;
                    int64_t elem_size = ir_lower_pointer_elem_size(pnode);
                    if (elem_size > 1) {
                        int mr = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, delta,
                                                     ir_value_const_int(elem_size, IR_TYPE_I64_T));
                        delta = ir_value_reg(mr, IR_TYPE_I64_T);
                    }

                    if (op == OP_SUB) {
                        int nr = ir_builder_emit_neg(ctx->builder, IR_TYPE_I64_T, delta);
                        delta = ir_value_reg(nr, IR_TYPE_I64_T);
                    }
                    int pr = ir_builder_emit_ptr_offset(ctx->builder, ptr_t, base_ptr, delta);
                    return ir_value_reg(pr, ptr_t);
                }

                if (op == OP_SUB &&
                    expr->data.bin_op.left && expr->data.bin_op.right &&
                    expr->data.bin_op.left->inferred_type == TYPE_POINTER &&
                    expr->data.bin_op.right->inferred_type == TYPE_POINTER &&
                    expr->inferred_type != TYPE_POINTER) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                    IRValue* lhs = cast_to(ctx, lhs0, IR_TYPE_I64_T);
                    IRValue* rhs = cast_to(ctx, rhs0, IR_TYPE_I64_T);

                    int dr = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, lhs, rhs);
                    IRValue* diff_bytes = ir_value_reg(dr, IR_TYPE_I64_T);

                    int64_t elem_size = ir_lower_pointer_elem_size(expr->data.bin_op.left);
                    if (elem_size > 1) {
                        int qr = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T,
                                                     diff_bytes,
                                                     ir_value_const_int(elem_size, IR_TYPE_I64_T));
                        return ir_value_reg(qr, IR_TYPE_I64_T);
                    }

                    return diff_bytes;
                }

                // عشري
                if (expr->inferred_type == TYPE_FLOAT) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                    IRValue* lhs = cast_to(ctx, lhs0, IR_TYPE_F64_T);
                    IRValue* rhs = cast_to(ctx, rhs0, IR_TYPE_F64_T);

                    IRType* type = IR_TYPE_F64_T;
                    int dest = -1;

                    switch (ir_binop_to_irop(op)) {
                        case IR_OP_ADD: dest = ir_builder_emit_add(ctx->builder, type, lhs, rhs); break;
                        case IR_OP_SUB: dest = ir_builder_emit_sub(ctx->builder, type, lhs, rhs); break;
                        case IR_OP_MUL: dest = ir_builder_emit_mul(ctx->builder, type, lhs, rhs); break;
                        case IR_OP_DIV: dest = ir_builder_emit_div(ctx->builder, type, lhs, rhs); break;
                        case IR_OP_MOD:
                            ir_lower_report_error(ctx, expr, "عملية باقي غير مدعومة على 'عشري'.");
                            return ir_value_const_int(0, IR_TYPE_F64_T);
                        default:
                            ir_lower_report_error(ctx, expr, "عملية حسابية غير مدعومة.");
                            return ir_value_const_int(0, IR_TYPE_F64_T);
                    }

                    return ir_value_reg(dest, type);
                }

                IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);

                DataType lt = expr->data.bin_op.left ? expr->data.bin_op.left->inferred_type : TYPE_INT;
                DataType rt = expr->data.bin_op.right ? expr->data.bin_op.right->inferred_type : TYPE_INT;

                IRType* type = lower_common_int_type(ctx->builder ? ctx->builder->module : NULL, lt, rt);
                IRValue* lhs = cast_to(ctx, lhs0, type);
                IRValue* rhs = cast_to(ctx, rhs0, type);
                int dest = -1;

                switch (ir_binop_to_irop(op)) {
                    case IR_OP_ADD: dest = ir_builder_emit_add(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_SUB: dest = ir_builder_emit_sub(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_MUL: dest = ir_builder_emit_mul(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_DIV: dest = ir_builder_emit_div(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_MOD: dest = ir_builder_emit_mod(ctx->builder, type, lhs, rhs); break;
                    default:
                        ir_lower_report_error(ctx, expr, "عملية حسابية غير مدعومة.");
                        return ir_builder_const_i64(0);
                }

                return ir_value_reg(dest, type);
            }

            // Bitwise / Shift
            if (op == OP_BIT_AND || op == OP_BIT_OR || op == OP_BIT_XOR ||
                op == OP_SHL || op == OP_SHR) {
                IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);

                DataType lt = expr->data.bin_op.left ? expr->data.bin_op.left->inferred_type : TYPE_INT;
                DataType rt = expr->data.bin_op.right ? expr->data.bin_op.right->inferred_type : TYPE_INT;

                IRType* type = NULL;
                if (op == OP_SHL || op == OP_SHR) {
                    DataType left_promoted = lower_int_promote_datatype(lt);
                    type = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, left_promoted);
                } else {
                    type = lower_common_int_type(ctx->builder ? ctx->builder->module : NULL, lt, rt);
                }
                if (!type) {
                    type = IR_TYPE_I64_T;
                }

                IRValue* lhs = cast_to(ctx, lhs0, type);
                IRValue* rhs = cast_to(ctx, rhs0, type);
                int dest = -1;

                switch (op) {
                    case OP_BIT_AND:
                        dest = ir_builder_emit_and(ctx->builder, type, lhs, rhs);
                        break;
                    case OP_BIT_OR:
                        dest = ir_builder_emit_or(ctx->builder, type, lhs, rhs);
                        break;
                    case OP_BIT_XOR:
                        dest = ir_builder_emit_xor(ctx->builder, type, lhs, rhs);
                        break;
                    case OP_SHL:
                        dest = ir_builder_emit_shl(ctx->builder, type, lhs, rhs);
                        break;
                    case OP_SHR:
                        dest = ir_builder_emit_shr(ctx->builder, type, lhs, rhs);
                        break;
                    default:
                        ir_lower_report_error(ctx, expr, "عملية بتية غير مدعومة.");
                        return ir_builder_const_i64(0);
                }

                return ir_value_reg(dest, type);
            }

            // Comparison
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                if (expr->data.bin_op.left && expr->data.bin_op.right &&
                    (expr->data.bin_op.left->inferred_type == TYPE_POINTER || expr->data.bin_op.right->inferred_type == TYPE_POINTER)) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                    IRType* ptr_t = (lhs0 && lhs0->type && lhs0->type->kind == IR_TYPE_PTR)
                        ? lhs0->type
                        : ((rhs0 && rhs0->type && rhs0->type->kind == IR_TYPE_PTR) ? rhs0->type : ir_type_ptr(IR_TYPE_I8_T));
                    IRValue* lhs = cast_to(ctx, lhs0, ptr_t);
                    IRValue* rhs = cast_to(ctx, rhs0, ptr_t);
                    IRCmpPred pred = ir_cmp_to_pred(op, false);
                    int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs, rhs);
                    return ir_value_reg(dest, IR_TYPE_I1_T);
                }

                IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                IRValue* lhs = lhs0;
                IRValue* rhs = rhs0;

                DataType lt = expr->data.bin_op.left ? expr->data.bin_op.left->inferred_type : TYPE_INT;
                DataType rt = expr->data.bin_op.right ? expr->data.bin_op.right->inferred_type : TYPE_INT;
                IRType* ct = NULL;
                if (lt == TYPE_FLOAT || rt == TYPE_FLOAT) {
                    ct = IR_TYPE_F64_T;
                } else {
                    ct = lower_common_int_type(ctx->builder ? ctx->builder->module : NULL, lt, rt);
                }

                // ترتيب الحروف يعتمد على نقطة-الكود، لا على تمثيل البايتات المعبأ.
                if (lhs0 && rhs0 && lhs0->type && rhs0->type &&
                    lhs0->type->kind == IR_TYPE_CHAR && rhs0->type->kind == IR_TYPE_CHAR &&
                    (op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE))
                {
                    lhs = ensure_i64(ctx, lhs0);
                    rhs = ensure_i64(ctx, rhs0);
                    ct = IR_TYPE_I64_T;
                }

                // طبّق الترقيات/التحويلات C-like قبل المقارنة.
                lhs = cast_to(ctx, lhs, ct);
                rhs = cast_to(ctx, rhs, ct);

                bool is_unsigned = false;
                if (ct && (ct->kind == IR_TYPE_U8 || ct->kind == IR_TYPE_U16 || ct->kind == IR_TYPE_U32 || ct->kind == IR_TYPE_U64))
                    is_unsigned = true;

                IRCmpPred pred = ir_cmp_to_pred(op, is_unsigned);
                int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs, rhs);
                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            // Logical AND/OR (non-short-circuit form for now; proper short-circuit will be handled
            // during control-flow lowering by introducing blocks).
            if (op == OP_AND || op == OP_OR) {
                IRValue* lhs = lower_to_bool(ctx, lower_expr(ctx, expr->data.bin_op.left));
                IRValue* rhs = lower_to_bool(ctx, lower_expr(ctx, expr->data.bin_op.right));

                int dest = -1;
                if (op == OP_AND) dest = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, lhs, rhs);
                else dest = ir_builder_emit_or(ctx->builder, IR_TYPE_I1_T, lhs, rhs);

                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            ir_lower_report_error(ctx, expr, "عملية ثنائية غير مدعومة.");
            return ir_builder_const_i64(0);
        }

        case NODE_UNARY_OP: {
            UnaryOpType op = expr->data.unary_op.op;

            if (op == UOP_ADDR) {
                IRType* pointee = IR_TYPE_I64_T;
                IRValue* addr = lower_lvalue_address(ctx, expr->data.unary_op.operand, &pointee);
                IRType* ptr_t = ir_type_ptr(pointee ? pointee : IR_TYPE_I64_T);
                if (!addr->type || !ir_types_equal(addr->type, ptr_t)) {
                    addr = cast_to(ctx, addr, ptr_t);
                }
                return addr;
            }

            if (op == UOP_DEREF) {
                IRValue* p0 = lower_expr(ctx, expr->data.unary_op.operand);
                IRType* target_t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, expr->inferred_type);
                if (!target_t || target_t->kind == IR_TYPE_VOID) {
                    target_t = IR_TYPE_I64_T;
                }
                IRType* ptr_t = ir_type_ptr(target_t);
                IRValue* p = cast_to(ctx, p0, ptr_t);
                int lr = ir_builder_emit_load(ctx->builder, target_t, p);
                return ir_value_reg(lr, target_t);
            }

            if (op == UOP_NEG) {
                IRValue* operand0 = lower_expr(ctx, expr->data.unary_op.operand);
                DataType dt = expr->inferred_type;
                if (dt == TYPE_FLOAT) {
                    IRValue* operand = cast_to(ctx, operand0, IR_TYPE_F64_T);
                    int dest = ir_builder_emit_neg(ctx->builder, IR_TYPE_F64_T, operand);
                    return ir_value_reg(dest, IR_TYPE_F64_T);
                }

                IRType* t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, dt);
                if (!t) t = IR_TYPE_I64_T;
                IRValue* operand = cast_to(ctx, operand0, t);
                int dest = ir_builder_emit_neg(ctx->builder, t, operand);
                return ir_value_reg(dest, t);
            }

            if (op == UOP_NOT) {
                // نفي منطقي: !(x) == (x == 0)
                IRValue* operand = lower_to_bool(ctx, lower_expr(ctx, expr->data.unary_op.operand));
                IRValue* zero = ir_value_const_int(0, IR_TYPE_I1_T);
                int dest = ir_builder_emit_cmp_eq(ctx->builder, operand, zero);
                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            if (op == UOP_BIT_NOT) {
                IRValue* operand0 = lower_expr(ctx, expr->data.unary_op.operand);
                DataType dt = expr->inferred_type;
                IRType* t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, dt);
                if (!t || t->kind == IR_TYPE_VOID) {
                    t = IR_TYPE_I64_T;
                }
                IRValue* operand = cast_to(ctx, operand0, t);
                int dest = ir_builder_emit_not(ctx->builder, t, operand);
                return ir_value_reg(dest, t);
            }

            // ++ / -- in expression form will be addressed later (statement lowering / lvalues).
            ir_lower_report_error(ctx, expr, "عملية أحادية غير مدعومة (%d).", (int)op);
            return ir_builder_const_i64(0);
        }

        case NODE_CALL_EXPR:
            return lower_call_expr(ctx, expr);

        // -----------------------------------------------------------------
        // Extra nodes (not in v0.3.0.3 checklist, but useful for robustness)
        // -----------------------------------------------------------------
        case NODE_BOOL:
            return ir_value_const_int(expr->data.bool_lit.value ? 1 : 0, IR_TYPE_I1_T);

        case NODE_NULL:
            return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));

        case NODE_FLOAT:
            return ir_value_const_int((int64_t)expr->data.float_lit.bits, IR_TYPE_F64_T);

        case NODE_CHAR:
        {
            uint64_t packed = pack_utf8_codepoint((uint32_t)expr->data.char_lit.value);
            return ir_value_const_int((int64_t)packed, IR_TYPE_CHAR_T);
        }

        case NODE_STRING:
            return ir_builder_const_baa_string(ctx->builder, expr->data.string_lit.value);

        case NODE_SIZEOF:
            if (!expr->data.sizeof_expr.size_known) {
                ir_lower_report_error(ctx, expr, "فشل حساب 'حجم' أثناء التحليل الدلالي.");
                return ir_value_const_int(0, IR_TYPE_I64_T);
            }
            return ir_value_const_int(expr->data.sizeof_expr.size_bytes, IR_TYPE_I64_T);

        case NODE_CAST: {
            IRValue* src = lower_expr(ctx, expr->data.cast_expr.expression);
            IRType* to_t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL,
                                                 expr->data.cast_expr.target_type);
            if (!to_t) to_t = IR_TYPE_I64_T;
            return cast_to(ctx, src, to_t);
        }

        case NODE_MEMBER_ACCESS: {
            // ملاحظة: التحليل الدلالي يملأ معلومات الوصول (root/offset/type).
            if (expr->data.member_access.is_enum_value) {
                return ir_value_const_int(expr->data.member_access.enum_value, IR_TYPE_I64_T);
            }

            if (!expr->data.member_access.is_struct_member || !expr->data.member_access.root_var) {
                ir_lower_report_error(ctx, expr, "وصول عضو غير صالح في مسار IR.");
                return ir_builder_const_i64(0);
            }

            if (expr->data.member_access.member_type == TYPE_STRUCT ||
                expr->data.member_access.member_type == TYPE_UNION) {
                ir_lower_report_error(ctx, expr, "قراءة عضو هيكل من نوع هيكل كقيمة غير مدعومة.");
                return ir_builder_const_i64(0);
            }

            IRModule* m = ctx->builder->module;
            if (m) ir_module_set_current(m);

            IRType* ptr_i8_t = ir_type_ptr(IR_TYPE_I8_T);
            IRValue* base_ptr = NULL;

            if (expr->data.member_access.root_is_global) {
                base_ptr = ir_value_global(expr->data.member_access.root_var, IR_TYPE_I8_T);
            } else {
                IRLowerBinding* b = find_local(ctx, expr->data.member_access.root_var);
                if (!b || !b->value_type) {
                    ir_lower_report_error(ctx, expr, "هيكل محلي غير معروف '%s'.",
                                          expr->data.member_access.root_var);
                    return ir_builder_const_i64(0);
                }
                if (b->is_static_storage) {
                    base_ptr = ir_value_global(ir_lower_binding_storage_name(b), IR_TYPE_I8_T);
                } else {
                    IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                    int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
                    base_ptr = ir_value_reg(c, ptr_i8_t);
                }
            }

            IRValue* idx = ir_value_const_int((int64_t)expr->data.member_access.member_offset, IR_TYPE_I64_T);
            int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
            IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);

            IRType* field_val_t = ir_type_from_datatype(m, expr->data.member_access.member_type);
            if (!field_val_t || field_val_t->kind == IR_TYPE_VOID) field_val_t = IR_TYPE_I64_T;

            IRType* field_ptr_t = ir_type_ptr(field_val_t);
            int fp = ir_builder_emit_cast(ctx->builder, byte_ptr, field_ptr_t);
            IRValue* field_ptr = ir_value_reg(fp, field_ptr_t);
            int r = ir_builder_emit_load(ctx->builder, field_val_t, field_ptr);
            return ir_value_reg(r, field_val_t);
        }

        default:
            ir_lower_report_error(ctx, expr, "عقدة تعبير غير مدعومة (%d).", (int)expr->type);
            return ir_builder_const_i64(0);
    }
}

// ============================================================================
// Statement lowering (v0.3.0.4)
// ============================================================================

static void lower_array_decl(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    // التصريحات العامة تُخفض على مستوى الوحدة داخل ir_lower_program().
    if (stmt->data.array_decl.is_global) {
        fprintf(stderr, "IR Lower Warning: global array decl '%s' ignored in statement lowering\n",
                stmt->data.array_decl.name ? stmt->data.array_decl.name : "???");
        return;
    }
    if (stmt->data.array_decl.total_elems <= 0 || stmt->data.array_decl.total_elems > INT_MAX) {
        ir_lower_report_error(ctx, stmt, "حجم المصفوفة غير صالح.");
        return;
    }

    const int total_count = (int)stmt->data.array_decl.total_elems;
    DataType elem_dt = stmt->data.array_decl.element_type;
    const char* elem_type_name = stmt->data.array_decl.element_type_name;
    bool elem_is_agg = (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION);

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* elem_t = NULL;
    IRType* arr_t = NULL;
    int elem_size_bytes = 0;

    if (elem_is_agg) {
        elem_size_bytes = stmt->data.array_decl.element_struct_size;
        if (elem_size_bytes <= 0) {
            ir_lower_report_error(ctx, stmt, "حجم عنصر المصفوفة المركبة غير صالح.");
            return;
        }
        int64_t total_bytes64 = stmt->data.array_decl.total_elems * (int64_t)elem_size_bytes;
        if (total_bytes64 <= 0 || total_bytes64 > INT_MAX) {
            ir_lower_report_error(ctx, stmt, "حجم تخزين المصفوفة المركبة كبير جداً.");
            return;
        }
        elem_t = IR_TYPE_I8_T;
        arr_t = ir_type_array(IR_TYPE_I8_T, (int)total_bytes64);
    } else {
        elem_t = ir_type_from_datatype(m, elem_dt);
        if (!elem_t || elem_t->kind == IR_TYPE_VOID) elem_t = IR_TYPE_I64_T;
        arr_t = ir_type_array(elem_t, total_count);
    }

    if (!arr_t) {
        ir_lower_report_error(ctx, stmt, "فشل إنشاء نوع مصفوفة أثناء خفض IR.");
        return;
    }

    if (stmt->data.array_decl.is_static) {
        if (!m) {
            ir_lower_report_error(ctx, stmt, "وحدة IR غير متاحة لتعريف مصفوفة ساكنة.");
            return;
        }
        ir_module_set_current(m);

        char* gname = ir_lower_make_static_label(ctx, stmt->data.array_decl.name);
        if (!gname) return;

        IRGlobal* g = ir_builder_create_global(ctx->builder, gname, arr_t,
                                               stmt->data.array_decl.is_const ? 1 : 0);
        if (!g) {
            ir_lower_report_error(ctx, stmt, "فشل إنشاء مصفوفة ساكنة '%s'.",
                                  stmt->data.array_decl.name ? stmt->data.array_decl.name : "???");
            return;
        }
        g->is_internal = true;
        g->has_init_list = stmt->data.array_decl.has_init ? true : false;

        if (stmt->data.array_decl.has_init && !elem_is_agg) {
            int n = stmt->data.array_decl.init_count;
            if (n < 0) n = 0;
            if (n > total_count) {
                n = total_count;
            }

            IRValue** elems = NULL;
            if (n > 0) {
                elems = (IRValue**)ir_arena_calloc(&m->arena, (size_t)n, sizeof(IRValue*), _Alignof(IRValue*));
                if (!elems) {
                    ir_lower_report_error(ctx, stmt, "فشل تخصيص مهيئات مصفوفة ساكنة '%s'.",
                                          stmt->data.array_decl.name ? stmt->data.array_decl.name : "???");
                    return;
                }
            }

            int i = 0;
            for (Node* v = stmt->data.array_decl.init_values; v && i < n; v = v->next, i++) {
                elems[i] = ir_lower_global_init_value(ctx->builder, v, elem_t);
            }

            g->init_elems = elems;
            g->init_elem_count = n;
        } else if (stmt->data.array_decl.has_init && elem_is_agg && stmt->data.array_decl.init_count > 0) {
            ir_lower_report_error(ctx, stmt, "تهيئة مصفوفات الأنواع المركبة غير مدعومة حالياً في خفض IR.");
        }

        ir_lower_bind_local_static(ctx, stmt->data.array_decl.name, gname, arr_t);
        ir_lower_set_local_array_meta(ctx,
                                      stmt->data.array_decl.name,
                                      stmt->data.array_decl.dim_count,
                                      stmt->data.array_decl.dims,
                                      elem_dt,
                                      elem_type_name);
        return;
    }

    int ptr_reg = ir_builder_emit_alloca(ctx->builder, arr_t);
    ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.array_decl.name);

    ir_lower_bind_local(ctx, stmt->data.array_decl.name, ptr_reg, arr_t);
    ir_lower_set_local_array_meta(ctx,
                                  stmt->data.array_decl.name,
                                  stmt->data.array_decl.dim_count,
                                  stmt->data.array_decl.dims,
                                  elem_dt,
                                  elem_type_name);

    // تهيئة المصفوفة كما في C:
    // - إذا وُجدت قائمة تهيئة (حتى `{}`)، نُهيّئ العناصر المحددة ثم نملأ الباقي بأصفار.
    if (!stmt->data.array_decl.has_init) {
        return;
    }
    if (elem_is_agg) {
        if (stmt->data.array_decl.init_count > 0) {
            ir_lower_report_error(ctx, stmt, "تهيئة مصفوفات الأنواع المركبة غير مدعومة حالياً في خفض IR.");
        }
        return;
    }

    IRType* arr_ptr_t = ir_type_ptr(arr_t);
    IRType* elem_ptr_t = ir_type_ptr(elem_t);
    IRValue* arr_ptr = ir_value_reg(ptr_reg, arr_ptr_t);

    // base: cast ptr(array) -> ptr(elem)
    int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
    IRValue* base_ptr = ir_value_reg(base_reg, elem_ptr_t);

    int i = 0;
    Node* v = stmt->data.array_decl.init_values;
    while (v && i < total_count) {
        // اربط موقع التوليد بموقع عنصر التهيئة.
        ir_lower_set_loc(ctx->builder, v);

        IRValue* idx = ir_value_const_int((int64_t)i, IR_TYPE_I64_T);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
        IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

        IRValue* val = lower_expr(ctx, v);
        if (val && val->type && !ir_types_equal(val->type, elem_t)) {
            int cv = ir_builder_emit_cast(ctx->builder, val, elem_t);
            val = ir_value_reg(cv, elem_t);
        }

        ir_builder_emit_store(ctx->builder, val, elem_ptr);
        i++;
        v = v->next;
    }

    // تعبئة الباقي بأصفار
    for (; i < total_count; i++) {
        ir_lower_set_loc(ctx->builder, stmt);
        IRValue* idx = ir_value_const_int((int64_t)i, IR_TYPE_I64_T);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
        IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);
        IRValue* z = ir_value_const_int(0, elem_t);
        ir_builder_emit_store(ctx->builder, z, elem_ptr);
    }
}

static void lower_var_decl(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    // Ignore global declarations here (globals are lowered at module scope later).
    if (stmt->data.var_decl.is_global) {
        fprintf(stderr, "IR Lower Warning: global var decl '%s' ignored in statement lowering\n",
                stmt->data.var_decl.name ? stmt->data.var_decl.name : "???");
        return;
    }

    if (stmt->data.var_decl.is_static) {
        IRModule* m = ctx->builder->module;
        if (!m) {
            ir_lower_report_error(ctx, stmt, "وحدة IR غير متاحة لتعريف متغير ساكن.");
            return;
        }
        ir_module_set_current(m);

        char* gname = ir_lower_make_static_label(ctx, stmt->data.var_decl.name);
        if (!gname) return;

        if (stmt->data.var_decl.type == TYPE_STRUCT || stmt->data.var_decl.type == TYPE_UNION) {
            int n = stmt->data.var_decl.struct_size;
            if (n <= 0) {
                ir_lower_report_error(ctx, stmt, "حجم الهيكل غير صالح أثناء خفض IR.");
                return;
            }

            IRType* bytes_t = ir_type_array(IR_TYPE_I8_T, n);
            IRGlobal* g = ir_builder_create_global(ctx->builder, gname, bytes_t,
                                                   stmt->data.var_decl.is_const ? 1 : 0);
            if (!g) {
                ir_lower_report_error(ctx, stmt, "فشل إنشاء متغير ساكن '%s'.",
                                      stmt->data.var_decl.name ? stmt->data.var_decl.name : "???");
                return;
            }
            g->is_internal = true;
            ir_lower_bind_local_static(ctx, stmt->data.var_decl.name, gname, bytes_t);
            return;
        }

        IRType* value_type = ir_type_from_datatype(m, stmt->data.var_decl.type);
        IRValue* init = NULL;
        if (stmt->data.var_decl.expression) {
            init = ir_lower_global_init_value(ctx->builder, stmt->data.var_decl.expression, value_type);
        } else {
            init = ir_value_const_int(0, value_type);
        }

        IRGlobal* g = ir_builder_create_global_init(ctx->builder, gname, value_type, init,
                                                    stmt->data.var_decl.is_const ? 1 : 0);
        if (!g) {
            ir_lower_report_error(ctx, stmt, "فشل إنشاء متغير ساكن '%s'.",
                                  stmt->data.var_decl.name ? stmt->data.var_decl.name : "???");
            return;
        }
        g->is_internal = true;
        ir_lower_bind_local_static(ctx, stmt->data.var_decl.name, gname, value_type);
        return;
    }

    // نوع مركب محلي (هيكل/اتحاد): نخزّنه ككتلة بايتات (array<i8, N>) على المكدس.
    if (stmt->data.var_decl.type == TYPE_STRUCT || stmt->data.var_decl.type == TYPE_UNION) {
        int n = stmt->data.var_decl.struct_size;
        if (n <= 0) {
            ir_lower_report_error(ctx, stmt, "حجم الهيكل غير صالح أثناء خفض IR.");
            return;
        }

        IRType* bytes_t = ir_type_array(IR_TYPE_I8_T, n);
        int ptr_reg = ir_builder_emit_alloca(ctx->builder, bytes_t);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.var_decl.name);
        ir_lower_bind_local(ctx, stmt->data.var_decl.name, ptr_reg, bytes_t);
        return;
    }

    IRType* value_type = ir_type_from_datatype(ctx && ctx->builder ? ctx->builder->module : NULL,
                                               stmt->data.var_decl.type);

    // %ptr = حجز <value_type>
    int ptr_reg = ir_builder_emit_alloca(ctx->builder, value_type);

    // ربط اسم المتغير بتعليمة الحجز للتتبع.
    ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.var_decl.name);

    // Bind name -> ptr reg so NODE_VAR_REF can emit حمل.
    ir_lower_bind_local(ctx, stmt->data.var_decl.name, ptr_reg, value_type);

    // Initializer is required for locals in the current language rules.
    IRValue* init = NULL;
    if (stmt->data.var_decl.expression) {
        init = lower_expr(ctx, stmt->data.var_decl.expression);
    } else {
        // Fallback (should not happen for locals; parser enforces initializer).
        init = ir_value_const_int(0, value_type);
    }

    if (init && init->type && value_type && !ir_types_equal(init->type, value_type)) {
        init = cast_to(ctx, init, value_type);
    }

    // خزن <init>, %ptr
    IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
    IRValue* ptr = ir_value_reg(ptr_reg, ptr_type);

    // نعيد الموقع إلى تصريح المتغير كي تُنسب عملية الخزن لسطر التصريح.
    ir_lower_set_loc(ctx->builder, stmt);
    ir_builder_emit_store(ctx->builder, init, ptr);
    ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, stmt->data.var_decl.name);
}

static void lower_array_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    const char* name = stmt->data.array_op.name;
    int index_count = stmt->data.array_op.index_count;
    if (index_count <= 0) index_count = ir_lower_index_count(stmt->data.array_op.indices);

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        const char* storage_name = ir_lower_binding_storage_name(b);
        if (!b->is_array || !b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
            ir_lower_report_error(ctx, stmt, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
            for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
                (void)lower_expr(ctx, idx);
            }
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }
        if (b->array_elem_type == TYPE_STRUCT || b->array_elem_type == TYPE_UNION) {
            ir_lower_report_error(ctx, stmt, "تعيين عنصر مصفوفة مركبة غير مدعوم حالياً.");
            for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
                (void)lower_expr(ctx, idx);
            }
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }

        IRType* elem_t = b->value_type->data.array.element ? b->value_type->data.array.element : IR_TYPE_I64_T;
        IRType* elem_ptr_t = ir_type_ptr(elem_t);

        IRValue* base_ptr = NULL;
        if (b->is_static_storage) {
            base_ptr = ir_value_global(storage_name, elem_t);
        } else {
            IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
            int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
            base_ptr = ir_value_reg(base_reg, elem_ptr_t);
        }

        if (!base_ptr) {
            ir_lower_report_error(ctx, stmt, "فشل بناء عنوان عنصر المصفوفة '%s'.", name ? name : "???");
            for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
                (void)lower_expr(ctx, idx);
            }
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }

        IRValue* linear = ir_lower_build_linear_index(ctx, stmt,
                                                      stmt->data.array_op.indices,
                                                      index_count,
                                                      b->array_dims,
                                                      b->array_rank);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
        IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

        IRValue* val = lower_expr(ctx, stmt->data.array_op.value);
        if (val && val->type && !ir_types_equal(val->type, elem_t)) {
            int cv = ir_builder_emit_cast(ctx->builder, val, elem_t);
            val = ir_value_reg(cv, elem_t);
        }

        ir_builder_emit_store(ctx->builder, val, elem_ptr);
        return;
    }

    // مصفوفة عامة
    IRGlobal* g = NULL;
    if (ctx->builder && ctx->builder->module && name) {
        g = ir_module_find_global(ctx->builder->module, name);
    }
    if (!g || !g->type || g->type->kind != IR_TYPE_ARRAY) {
        ir_lower_report_error(ctx, stmt, "تعيين إلى مصفوفة غير معرّفة '%s'.", name ? name : "???");
        for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
            (void)lower_expr(ctx, idx);
        }
        (void)lower_expr(ctx, stmt->data.array_op.value);
        return;
    }

    Node* gdecl = ir_lower_find_global_array_decl(ctx, name);
    int rank = 1;
    const int* dims = NULL;
    DataType elem_dt = TYPE_INT;
    if (gdecl && gdecl->type == NODE_ARRAY_DECL) {
        rank = gdecl->data.array_decl.dim_count > 0 ? gdecl->data.array_decl.dim_count : 1;
        dims = gdecl->data.array_decl.dims;
        elem_dt = gdecl->data.array_decl.element_type;
    }
    if (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION) {
        ir_lower_report_error(ctx, stmt, "تعيين عنصر مصفوفة مركبة غير مدعوم حالياً.");
        for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
            (void)lower_expr(ctx, idx);
        }
        (void)lower_expr(ctx, stmt->data.array_op.value);
        return;
    }

    IRType* elem_t = g->type->data.array.element ? g->type->data.array.element : IR_TYPE_I64_T;
    IRType* elem_ptr_t = ir_type_ptr(elem_t);
    IRValue* base_ptr = ir_value_global(name, elem_t);

    IRValue* linear = ir_lower_build_linear_index(ctx, stmt,
                                                  stmt->data.array_op.indices,
                                                  index_count,
                                                  dims,
                                                  rank);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
    IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

    IRValue* val = lower_expr(ctx, stmt->data.array_op.value);
    if (val && val->type && !ir_types_equal(val->type, elem_t)) {
        int cv = ir_builder_emit_cast(ctx->builder, val, elem_t);
        val = ir_value_reg(cv, elem_t);
    }

    ir_builder_emit_store(ctx->builder, val, elem_ptr);
}

static void lower_member_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    Node* target = stmt->data.member_assign.target;
    Node* value = stmt->data.member_assign.value;
    if (!target || target->type != NODE_MEMBER_ACCESS) {
        ir_lower_report_error(ctx, stmt, "هدف إسناد العضو غير صالح.");
        (void)lower_expr(ctx, value);
        return;
    }

    if (!target->data.member_access.is_struct_member || !target->data.member_access.root_var) {
        ir_lower_report_error(ctx, stmt, "إسناد عضو يتطلب عضواً هيكلياً محلولاً دلالياً.");
        (void)lower_expr(ctx, value);
        return;
    }

    if (target->data.member_access.member_type == TYPE_STRUCT ||
        target->data.member_access.member_type == TYPE_UNION) {
        ir_lower_report_error(ctx, stmt, "إسناد عضو من نوع هيكل غير مدعوم حالياً.");
        (void)lower_expr(ctx, value);
        return;
    }

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* ptr_i8_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* base_ptr = NULL;

    if (target->data.member_access.root_is_global) {
        base_ptr = ir_value_global(target->data.member_access.root_var, IR_TYPE_I8_T);
    } else {
        IRLowerBinding* b = find_local(ctx, target->data.member_access.root_var);
        if (!b || !b->value_type) {
            ir_lower_report_error(ctx, stmt, "هيكل محلي غير معروف '%s'.", target->data.member_access.root_var);
            (void)lower_expr(ctx, value);
            return;
        }
        if (b->is_static_storage) {
            base_ptr = ir_value_global(ir_lower_binding_storage_name(b), IR_TYPE_I8_T);
        } else {
            IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
            int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
            base_ptr = ir_value_reg(c, ptr_i8_t);
        }
    }

    IRValue* idx = ir_value_const_int((int64_t)target->data.member_access.member_offset, IR_TYPE_I64_T);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
    IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);

    IRType* field_val_t = ir_type_from_datatype(m, target->data.member_access.member_type);
    if (!field_val_t || field_val_t->kind == IR_TYPE_VOID) field_val_t = IR_TYPE_I64_T;
    IRType* field_ptr_t = ir_type_ptr(field_val_t);
    int fp = ir_builder_emit_cast(ctx->builder, byte_ptr, field_ptr_t);
    IRValue* field_ptr = ir_value_reg(fp, field_ptr_t);

    IRValue* rhs = lower_expr(ctx, value);
    if (rhs && rhs->type && !ir_types_equal(rhs->type, field_val_t)) {
        int cv = ir_builder_emit_cast(ctx->builder, rhs, field_val_t);
        rhs = ir_value_reg(cv, field_val_t);
    }

    ir_builder_emit_store(ctx->builder, rhs, field_ptr);
}

static void lower_deref_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;
    Node* target = stmt->data.deref_assign.target;
    Node* value = stmt->data.deref_assign.value;
    if (!target || target->type != NODE_UNARY_OP || target->data.unary_op.op != UOP_DEREF) {
        ir_lower_report_error(ctx, stmt, "هدف الإسناد عبر المؤشر غير صالح.");
        if (value) (void)lower_expr(ctx, value);
        return;
    }

    IRValue* p0 = lower_expr(ctx, target->data.unary_op.operand);
    IRType* value_t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, target->inferred_type);
    if (!value_t || value_t->kind == IR_TYPE_VOID) value_t = IR_TYPE_I64_T;
    IRType* ptr_t = ir_type_ptr(value_t);
    IRValue* ptr = cast_to(ctx, p0, ptr_t);

    IRValue* rhs = lower_expr(ctx, value);
    if (rhs && rhs->type && !ir_types_equal(rhs->type, value_t)) {
        rhs = cast_to(ctx, rhs, value_t);
    }

    ir_builder_emit_store(ctx->builder, rhs, ptr);
}

static void lower_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    const char* name = stmt->data.assign_stmt.name;

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        const char* storage_name = ir_lower_binding_storage_name(b);
        IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

        IRValue* rhs = lower_expr(ctx, stmt->data.assign_stmt.expression);

        if (rhs && rhs->type && value_type && !ir_types_equal(rhs->type, value_type)) {
            rhs = cast_to(ctx, rhs, value_type);
        }

        // خزن <rhs>, %ptr
        (void)value_type; // reserved for future casts/type checks
        IRValue* ptr = NULL;
        if (b->is_static_storage) {
            ptr = ir_value_global(storage_name, value_type);
        } else {
            IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
            ptr = ir_value_reg(b->ptr_reg, ptr_type);
        }
        ir_lower_set_loc(ctx->builder, stmt);
        ir_builder_emit_store(ctx->builder, rhs, ptr);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, storage_name);
        return;
    }

    // Assignment to a global (module scope)
    IRGlobal* g = NULL;
    if (ctx->builder && ctx->builder->module && name) {
        g = ir_module_find_global(ctx->builder->module, name);
    }
    if (g) {
        IRValue* rhs = lower_expr(ctx, stmt->data.assign_stmt.expression);

        if (rhs && rhs->type && g->type && !ir_types_equal(rhs->type, g->type)) {
            rhs = cast_to(ctx, rhs, g->type);
        }

        IRValue* gptr = ir_value_global(name, g->type);
        ir_lower_set_loc(ctx->builder, stmt);
        ir_builder_emit_store(ctx->builder, rhs, gptr);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, name);
        return;
    }

    ir_lower_report_error(ctx, stmt, "تعيين إلى متغير غير معروف '%s'.", name ? name : "???");
}

static void lower_return(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRValue* value = NULL;
    if (stmt->data.return_stmt.expression) {
        value = lower_expr(ctx, stmt->data.return_stmt.expression);
    }

    IRFunc* f = ir_builder_get_func(ctx->builder);
    if (f && f->ret_type && value && value->type && !ir_types_equal(f->ret_type, value->type)) {
        value = cast_to(ctx, value, f->ret_type);
    }
    ir_builder_emit_ret(ctx->builder, value);
}

static void lower_printf_cstr(IRLowerCtx* ctx, Node* stmt, const char* fmt, IRValue* arg)
{
    if (!ctx || !ctx->builder || !fmt) return;
    if (stmt) ir_lower_set_loc(ctx->builder, stmt);

    IRValue* fmt_val = ir_builder_const_string(ctx->builder, fmt);

    if (!arg) {
        IRValue* args1[1] = { fmt_val };
        ir_builder_emit_call_void(ctx->builder, "اطبع", args1, 1);
        return;
    }

    IRValue* args2[2] = { fmt_val, arg };
    ir_builder_emit_call_void(ctx->builder, "اطبع", args2, 2);
}

static void lower_print_char_packed(IRLowerCtx* ctx, Node* stmt, IRValue* ch, const char* fmt)
{
    if (!ctx || !ctx->builder || !ch || !fmt) return;

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* buf_arr_t = ir_type_array(IR_TYPE_I8_T, 5);
    int buf_ptr_reg = ir_builder_emit_alloca(b, buf_arr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, ir_type_ptr(buf_arr_t));

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    int base_reg = ir_builder_emit_cast(b, buf_ptr, i8_ptr_t);
    IRValue* base_ptr = ir_value_reg(base_reg, i8_ptr_t);

    // i64 معبأ = bytes + len*2^32
    int pr = ir_builder_emit_cast(b, ch, IR_TYPE_I64_T);
    IRValue* packed = ir_value_reg(pr, IR_TYPE_I64_T);

    IRValue* c_2p32 = ir_value_const_int(4294967296LL, IR_TYPE_I64_T);
    int len_r = ir_builder_emit_div(b, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* len = ir_value_reg(len_r, IR_TYPE_I64_T);

    int bytes_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    IRValue* c256 = ir_value_const_int(256, IR_TYPE_I64_T);

    int b0_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes, c256);
    IRValue* b0 = ir_value_reg(b0_r, IR_TYPE_I64_T);
    int t1_r = ir_builder_emit_div(b, IR_TYPE_I64_T, bytes, c256);
    IRValue* t1 = ir_value_reg(t1_r, IR_TYPE_I64_T);
    int b1_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, t1, c256);
    IRValue* b1 = ir_value_reg(b1_r, IR_TYPE_I64_T);
    int t2_r = ir_builder_emit_div(b, IR_TYPE_I64_T, t1, c256);
    IRValue* t2 = ir_value_reg(t2_r, IR_TYPE_I64_T);
    int b2_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, t2, c256);
    IRValue* b2 = ir_value_reg(b2_r, IR_TYPE_I64_T);
    int t3_r = ir_builder_emit_div(b, IR_TYPE_I64_T, t2, c256);
    IRValue* t3 = ir_value_reg(t3_r, IR_TYPE_I64_T);
    int b3_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, t3, c256);
    IRValue* b3 = ir_value_reg(b3_r, IR_TYPE_I64_T);

    IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
    IRValue* idx2 = ir_value_const_int(2, IR_TYPE_I64_T);
    IRValue* idx3 = ir_value_const_int(3, IR_TYPE_I64_T);
    IRValue* idx4 = ir_value_const_int(4, IR_TYPE_I64_T);

    int p0r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0);
    int p1r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1);
    int p2r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx2);
    int p3r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx3);
    int p4r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx4);

    ir_builder_emit_store(b, cast_to(ctx, b0, IR_TYPE_I8_T), ir_value_reg(p0r, i8_ptr_t));
    ir_builder_emit_store(b, cast_to(ctx, b1, IR_TYPE_I8_T), ir_value_reg(p1r, i8_ptr_t));
    ir_builder_emit_store(b, cast_to(ctx, b2, IR_TYPE_I8_T), ir_value_reg(p2r, i8_ptr_t));
    ir_builder_emit_store(b, cast_to(ctx, b3, IR_TYPE_I8_T), ir_value_reg(p3r, i8_ptr_t));
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), ir_value_reg(p4r, i8_ptr_t));

    // كتابة NUL في buf[len]
    int pend = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, len);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), ir_value_reg(pend, i8_ptr_t));

    lower_printf_cstr(ctx, stmt, fmt, base_ptr);
}

static void lower_print_baa_string(IRLowerCtx* ctx, Node* stmt, IRValue* str_ptr)
{
    if (!ctx || !ctx->builder || !str_ptr) return;

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = ir_type_ptr(IR_TYPE_CHAR_T);
    IRValue* base = str_ptr;
    if (!base->type || base->type->kind != IR_TYPE_PTR) {
        int cr = ir_builder_emit_cast(b, base, char_ptr_t);
        base = ir_value_reg(cr, char_ptr_t);
    }

    // idx_ptr = alloca i64
    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* idx_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    IRBlock* head = cf_create_block(ctx, "اطبع_نص_حلقة");
    IRBlock* body = cf_create_block(ctx, "اطبع_نص_جسم");
    IRBlock* done = cf_create_block(ctx, "اطبع_نص_نهاية");

    if (!ir_builder_is_block_terminated(b))
        ir_builder_emit_br(b, head);

    // head
    ir_builder_set_insert_point(b, head);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);

    int ep = ir_builder_emit_ptr_offset(b, char_ptr_t, base, idx);
    IRValue* elem_ptr = ir_value_reg(ep, char_ptr_t);
    int ch_r = ir_builder_emit_load(b, IR_TYPE_CHAR_T, elem_ptr);
    IRValue* ch = ir_value_reg(ch_r, IR_TYPE_CHAR_T);

    int chi_r = ir_builder_emit_cast(b, ch, IR_TYPE_I64_T);
    IRValue* chi = ir_value_reg(chi_r, IR_TYPE_I64_T);
    int is_end_r = ir_builder_emit_cmp_eq(b, chi, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(is_end_r, IR_TYPE_I1_T);

    if (!ir_builder_is_block_terminated(b))
        ir_builder_emit_br_cond(b, is_end, done, body);

    // body
    ir_builder_set_insert_point(b, body);
    lower_print_char_packed(ctx, stmt, ch, "%s");

    int inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* inc = ir_value_reg(inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    if (!ir_builder_is_block_terminated(b))
        ir_builder_emit_br(b, head);

    // done
    ir_builder_set_insert_point(b, done);
    lower_printf_cstr(ctx, stmt, "\n", NULL);
}

static void lower_print(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRValue* value = lower_expr(ctx, stmt->data.print_stmt.expression);

    if (value && value->type && value->type->kind == IR_TYPE_CHAR) {
        lower_print_char_packed(ctx, stmt, value, "%s\n");
        return;
    }

    if (value && value->type && value->type->kind == IR_TYPE_PTR &&
        value->type->data.pointee && value->type->data.pointee->kind == IR_TYPE_CHAR) {
        lower_print_baa_string(ctx, stmt, value);
        return;
    }

    // عشري
    if (stmt->data.print_stmt.expression && stmt->data.print_stmt.expression->inferred_type == TYPE_FLOAT)
    {
        IRValue* vf = value ? cast_to(ctx, value, IR_TYPE_F64_T) : ir_value_const_int(0, IR_TYPE_F64_T);
        lower_printf_cstr(ctx, stmt, "%g\n", vf);
        return;
    }

    // افتراضي: طباعة عدد صحيح.
    // الأنواع غير الموقّعة تُطبع كـ unsigned عبر تحويلها إلى ط٦٤.
    DataType dt = (stmt->data.print_stmt.expression) ? stmt->data.print_stmt.expression->inferred_type : TYPE_INT;
    if (dt == TYPE_U8 || dt == TYPE_U16 || dt == TYPE_U32 || dt == TYPE_U64)
    {
        IRValue* vu = value ? cast_to(ctx, value, IR_TYPE_U64_T) : ir_value_const_int(0, IR_TYPE_U64_T);
        lower_printf_cstr(ctx, stmt, "%llu\n", vu);
        return;
    }

    IRValue* v64 = value ? ensure_i64(ctx, value) : ir_value_const_int(0, IR_TYPE_I64_T);
    lower_printf_cstr(ctx, stmt, "%lld\n", v64);
}

static void lower_read(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRLowerBinding* b = find_local(ctx, stmt->data.read_stmt.var_name);
    if (!b) {
        ir_lower_report_error(ctx, stmt, "قراءة إلى متغير غير معروف '%s'.",
                              stmt->data.read_stmt.var_name ? stmt->data.read_stmt.var_name : "???");
        return;
    }

    IRType* vt = (b->value_type) ? b->value_type : IR_TYPE_I64_T;
    IRType* ptr_type = ir_type_ptr(vt);
    IRValue* ptr = ir_value_reg(b->ptr_reg, ptr_type);

    // i64/u64 يمكن القراءة مباشرة إلى المتغير. للأحجام الأصغر نقرأ إلى مؤقت i64 ثم نقتطع.
    if (vt && (vt->kind == IR_TYPE_I64 || vt->kind == IR_TYPE_U64))
    {
        const char* fmt = (vt->kind == IR_TYPE_U64) ? "%llu" : "%lld";
        IRValue* fmt_val = ir_builder_const_string(ctx->builder, fmt);
        IRValue* args[2] = { fmt_val, ptr };
        ir_builder_emit_call_void(ctx->builder, "اقرأ", args, 2);
        return;
    }

    // tmp_ptr = alloca i64
    int tmp_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
    IRType* tmp_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* tmp_ptr = ir_value_reg(tmp_ptr_reg, tmp_ptr_t);
    ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), tmp_ptr);

    IRValue* fmt_val = ir_builder_const_string(ctx->builder, "%lld");
    IRValue* args[2] = { fmt_val, tmp_ptr };
    ir_builder_emit_call_void(ctx->builder, "اقرأ", args, 2);

    int tmp_r = ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, tmp_ptr);
    IRValue* tmp_v = ir_value_reg(tmp_r, IR_TYPE_I64_T);
    IRValue* cv = cast_to(ctx, tmp_v, vt);
    ir_builder_emit_store(ctx->builder, cv, ptr);
}

static IRValue* lower_condition_i1(IRLowerCtx* ctx, Node* cond_expr) {
    // Ensure the condition is i1 for br_cond.
    return lower_to_bool(ctx, lower_expr(ctx, cond_expr));
}

static void lower_if_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRBlock* then_bb = cf_create_block(ctx, "فرع_صواب");
    IRBlock* merge_bb = cf_create_block(ctx, "دمج");
    IRBlock* else_bb = NULL;

    if (stmt->data.if_stmt.else_branch) {
        else_bb = cf_create_block(ctx, "فرع_خطأ");
    }

    IRValue* cond = lower_condition_i1(ctx, stmt->data.if_stmt.condition);

    if (!ir_builder_is_block_terminated(ctx->builder)) {
        if (else_bb) {
            ir_builder_emit_br_cond(ctx->builder, cond, then_bb, else_bb);
        } else {
            ir_builder_emit_br_cond(ctx->builder, cond, then_bb, merge_bb);
        }
    }

    // then
    ir_builder_set_insert_point(ctx->builder, then_bb);
    ir_lower_scope_push(ctx);
    lower_stmt(ctx, stmt->data.if_stmt.then_branch);
    ir_lower_scope_pop(ctx);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, merge_bb);
    }

    // else
    if (else_bb) {
        ir_builder_set_insert_point(ctx->builder, else_bb);
        ir_lower_scope_push(ctx);
        lower_stmt(ctx, stmt->data.if_stmt.else_branch);
        ir_lower_scope_pop(ctx);
        if (!ir_builder_is_block_terminated(ctx->builder)) {
            ir_builder_emit_br(ctx->builder, merge_bb);
        }
    }

    // merge
    ir_builder_set_insert_point(ctx->builder, merge_bb);
}

static void lower_while_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRBlock* header_bb = cf_create_block(ctx, "حلقة_تحقق");
    IRBlock* body_bb = cf_create_block(ctx, "حلقة_جسم");
    IRBlock* exit_bb = cf_create_block(ctx, "حلقة_نهاية");

    // Jump to header from current block
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    // header: evaluate condition
    ir_builder_set_insert_point(ctx->builder, header_bb);
    IRValue* cond = lower_condition_i1(ctx, stmt->data.while_stmt.condition);
    ir_builder_emit_br_cond(ctx->builder, cond, body_bb, exit_bb);

    // body
    cf_push(ctx, exit_bb, header_bb); // break -> exit, continue -> header
    ir_builder_set_insert_point(ctx->builder, body_bb);
    ir_lower_scope_push(ctx);
    lower_stmt(ctx, stmt->data.while_stmt.body);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }
    ir_lower_scope_pop(ctx);
    cf_pop(ctx);

    // continue after loop
    ir_builder_set_insert_point(ctx->builder, exit_bb);
}

static void lower_for_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // نطاق حلقة for: يمنع تسرب متغيرات init خارج الحلقة.
    ir_lower_scope_push(ctx);

    // Init runs in the current block
    if (stmt->data.for_stmt.init) {
        Node* init = stmt->data.for_stmt.init;
        if (init->type == NODE_VAR_DECL || init->type == NODE_ASSIGN || init->type == NODE_BLOCK) {
            lower_stmt(ctx, init);
        } else {
            (void)lower_expr(ctx, init);
        }
    }

    IRBlock* header_bb = cf_create_block(ctx, "لكل_تحقق");
    IRBlock* body_bb = cf_create_block(ctx, "لكل_جسم");
    IRBlock* inc_bb = cf_create_block(ctx, "لكل_زيادة");
    IRBlock* exit_bb = cf_create_block(ctx, "لكل_نهاية");

    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    // header
    ir_builder_set_insert_point(ctx->builder, header_bb);
    if (stmt->data.for_stmt.condition) {
        IRValue* cond = lower_condition_i1(ctx, stmt->data.for_stmt.condition);
        ir_builder_emit_br_cond(ctx->builder, cond, body_bb, exit_bb);
    } else {
        ir_builder_emit_br(ctx->builder, body_bb);
    }

    // body
    cf_push(ctx, exit_bb, inc_bb); // break -> exit, continue -> increment
    ir_builder_set_insert_point(ctx->builder, body_bb);
    lower_stmt(ctx, stmt->data.for_stmt.body);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, inc_bb);
    }
    cf_pop(ctx);

    // increment
    ir_builder_set_insert_point(ctx->builder, inc_bb);
    if (stmt->data.for_stmt.increment) {
        Node* inc = stmt->data.for_stmt.increment;
        if (inc->type == NODE_ASSIGN || inc->type == NODE_VAR_DECL || inc->type == NODE_BLOCK) {
            lower_stmt(ctx, inc);
        } else {
            (void)lower_expr(ctx, inc);
        }
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    ir_builder_set_insert_point(ctx->builder, exit_bb);

    ir_lower_scope_pop(ctx);
}

static void lower_break_stmt(IRLowerCtx* ctx) {
    if (!ctx || !ctx->builder) return;
    IRBlock* target = cf_current_break(ctx);
    if (!target) {
        ir_lower_report_error(ctx, NULL, "استعمال 'break' خارج حلقة/اختر.");
        return;
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, target);
    }
}

static void lower_continue_stmt(IRLowerCtx* ctx) {
    if (!ctx || !ctx->builder) return;
    IRBlock* target = cf_current_continue(ctx);
    if (!target) {
        ir_lower_report_error(ctx, NULL, "استعمال 'continue' خارج حلقة.");
        return;
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, target);
    }
}

static void lower_switch_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // Gather cases
    int case_count = 0;
    int non_default_count = 0;
    Node* c = stmt->data.switch_stmt.cases;
    while (c) {
        case_count++;
        if (!c->data.case_stmt.is_default) non_default_count++;
        c = c->next;
    }

    IRBlock* end_bb = cf_create_block(ctx, "نهاية_اختر");

    // Switch doesn't define a continue target (inherits from outer loop, if any)
    cf_push(ctx, end_bb, cf_current_continue(ctx));

    IRBlock** case_blocks = NULL;
    Node** case_nodes = NULL;
    IRBlock** non_default_blocks = NULL;
    Node** non_default_nodes = NULL;
    IRBlock* default_bb = NULL;
    Node* default_node = NULL;

    if (case_count > 0) {
        case_blocks = (IRBlock**)malloc(sizeof(IRBlock*) * (size_t)case_count);
        case_nodes = (Node**)malloc(sizeof(Node*) * (size_t)case_count);
    }
    if (non_default_count > 0) {
        non_default_blocks = (IRBlock**)malloc(sizeof(IRBlock*) * (size_t)non_default_count);
        non_default_nodes = (Node**)malloc(sizeof(Node*) * (size_t)non_default_count);
    }

    // Create blocks for all cases in source order (for fallthrough)
    int idx = 0;
    int nd_idx = 0;
    c = stmt->data.switch_stmt.cases;
    while (c) {
        IRBlock* bb = NULL;
        if (c->data.case_stmt.is_default) {
            bb = cf_create_block(ctx, "افتراضي");
            default_bb = bb;
            default_node = c;
        } else {
            bb = cf_create_block(ctx, "حالة");
            if (non_default_blocks && non_default_nodes) {
                non_default_blocks[nd_idx] = bb;
                non_default_nodes[nd_idx] = c;
                nd_idx++;
            }
        }

        if (case_blocks && case_nodes) {
            case_blocks[idx] = bb;
            case_nodes[idx] = c;
            idx++;
        }

        c = c->next;
    }

    // Evaluate switch expression (should be integer per semantic rules)
    IRValue* sw_val = lower_expr(ctx, stmt->data.switch_stmt.expression);
    IRType* sw_type = sw_val && sw_val->type ? sw_val->type : IR_TYPE_I64_T;

    int sw_is_reg = sw_val && sw_val->kind == IR_VAL_REG;
    int sw_reg = sw_is_reg ? sw_val->data.reg_num : -1;
    int64_t sw_const = (!sw_is_reg && sw_val && sw_val->kind == IR_VAL_CONST_INT) ? sw_val->data.const_int : 0;

    // Dispatch comparisons (chain)
    if (non_default_count == 0) {
        // No explicit cases: jump to default or end.
        if (!ir_builder_is_block_terminated(ctx->builder)) {
            ir_builder_emit_br(ctx->builder, default_bb ? default_bb : end_bb);
        }
    } else {
        IRBlock* current_check = ir_builder_get_insert_block(ctx->builder);

        for (int i = 0; i < non_default_count; i++) {
            Node* case_node = non_default_nodes[i];
            IRBlock* true_bb = non_default_blocks[i];
            IRBlock* false_bb = NULL;

            if (i == non_default_count - 1) {
                false_bb = default_bb ? default_bb : end_bb;
            } else {
                false_bb = cf_create_block(ctx, "فحص");
            }

            // Compare sw == case_value
            IRValue* lhs = sw_is_reg ? ir_value_reg(sw_reg, sw_type) : ir_value_const_int(sw_const, sw_type);
            IRValue* rhs = lower_expr(ctx, case_node->data.case_stmt.value);
            int cmp_reg = ir_builder_emit_cmp_eq(ctx->builder, lhs, rhs);
            IRValue* cmp_val = ir_value_reg(cmp_reg, IR_TYPE_I1_T);

            ir_builder_emit_br_cond(ctx->builder, cmp_val, true_bb, false_bb);

            // Continue chain in the false block (only if it's a check block we created)
            if (i < non_default_count - 1) {
                ir_builder_set_insert_point(ctx->builder, false_bb);
                current_check = false_bb;
            } else {
                (void)current_check;
            }
        }
    }

    // Emit case bodies with fallthrough semantics (explicit br to next case block)
    for (int i = 0; i < case_count; i++) {
        IRBlock* bb = case_blocks[i];
        Node* case_node = case_nodes[i];

        ir_builder_set_insert_point(ctx->builder, bb);
        ir_lower_scope_push(ctx);
        lower_stmt_list(ctx, case_node->data.case_stmt.body);
        ir_lower_scope_pop(ctx);

        if (!ir_builder_is_block_terminated(ctx->builder)) {
            IRBlock* next_bb = (i + 1 < case_count) ? case_blocks[i + 1] : end_bb;
            ir_builder_emit_br(ctx->builder, next_bb);
        }
    }

    // Continue after switch
    ir_builder_set_insert_point(ctx->builder, end_bb);

    if (case_blocks) free(case_blocks);
    if (case_nodes) free(case_nodes);
    if (non_default_blocks) free(non_default_blocks);
    if (non_default_nodes) free(non_default_nodes);
    (void)default_node;

    cf_pop(ctx); // pop switch break target
}

void lower_stmt_list(IRLowerCtx* ctx, Node* first_stmt) {
    Node* s = first_stmt;
    while (s) {
        lower_stmt(ctx, s);
        s = s->next;
    }
}

void lower_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // افتراضي: اربط أي تعليمات صادرة من هذه الجملة بموقعها.
    ir_lower_set_loc(ctx->builder, stmt);

    switch (stmt->type) {
        case NODE_BLOCK:
            ir_lower_scope_push(ctx);
            lower_stmt_list(ctx, stmt->data.block.statements);
            ir_lower_scope_pop(ctx);
            return;

        case NODE_VAR_DECL:
            lower_var_decl(ctx, stmt);
            return;

        case NODE_ARRAY_DECL:
            lower_array_decl(ctx, stmt);
            return;

        case NODE_ASSIGN:
            lower_assign(ctx, stmt);
            return;

        case NODE_MEMBER_ASSIGN:
            lower_member_assign(ctx, stmt);
            return;

        case NODE_DEREF_ASSIGN:
            lower_deref_assign(ctx, stmt);
            return;

        case NODE_ARRAY_ASSIGN:
            lower_array_assign(ctx, stmt);
            return;

        case NODE_RETURN:
            lower_return(ctx, stmt);
            return;

        case NODE_PRINT:
            lower_print(ctx, stmt);
            return;

        case NODE_READ:
            lower_read(ctx, stmt);
            return;

        case NODE_IF:
            lower_if_stmt(ctx, stmt);
            return;

        case NODE_WHILE:
            lower_while_stmt(ctx, stmt);
            return;

        case NODE_FOR:
            lower_for_stmt(ctx, stmt);
            return;

        case NODE_SWITCH:
            lower_switch_stmt(ctx, stmt);
            return;

        case NODE_BREAK:
            lower_break_stmt(ctx);
            return;

        case NODE_CONTINUE:
            lower_continue_stmt(ctx);
            return;

        case NODE_CALL_STMT: {
            // Lower call statement via expression lowering.
            // Construct a temporary NODE_CALL_EXPR-compatible view.
            Node temp;
            memset(&temp, 0, sizeof(temp));
            temp.type = NODE_CALL_EXPR;
            temp.data.call = stmt->data.call;
            temp.filename = stmt->filename;
            temp.line = stmt->line;
            temp.col = stmt->col;
            (void)lower_expr(ctx, &temp);
            return;
        }

        default:
            ir_lower_report_error(ctx, stmt, "عقدة جملة غير مدعومة (%d).", (int)stmt->type);
            return;
    }
}

// ============================================================================
// Program lowering (v0.3.0.7)
// ============================================================================

static uint64_t ir_lower_u64_add_wrap(uint64_t a, uint64_t b) { return a + b; }
static uint64_t ir_lower_u64_sub_wrap(uint64_t a, uint64_t b) { return a - b; }
static uint64_t ir_lower_u64_mul_wrap(uint64_t a, uint64_t b) { return a * b; }

/**
 * @brief تقييم تعبير ثابت للأعداد الصحيحة لاستخدامه في تهيئة العوام.
 *
 * الهدف هنا هو دعم تهيئة العوام بقيم قابلة للإصدار في .data (مثل C):
 * - يسمح بعمليات حسابية بسيطة وتعبيرات مقارنة/منطقية تنتج 0/1.
 * - يطبق التفاف 2's complement لتفادي UB في C عند تجاوز المدى.
 */
static int ir_lower_eval_const_i64(Node* expr, int64_t* out_value) {
    if (!out_value) return 0;
    *out_value = 0;
    if (!expr) return 1;

    switch (expr->type) {
        case NODE_INT:
            *out_value = (int64_t)expr->data.integer.value;
            return 1;
        case NODE_BOOL:
            *out_value = expr->data.bool_lit.value ? 1 : 0;
            return 1;
        case NODE_CHAR:
            *out_value = (int64_t)expr->data.char_lit.value;
            return 1;

        case NODE_UNARY_OP:
        {
            int64_t v = 0;
            if (!ir_lower_eval_const_i64(expr->data.unary_op.operand, &v)) return 0;

            if (expr->data.unary_op.op == UOP_NEG) {
                uint64_t uv = (uint64_t)v;
                *out_value = (int64_t)ir_lower_u64_sub_wrap(0u, uv);
                return 1;
            }
            if (expr->data.unary_op.op == UOP_NOT) {
                *out_value = (v == 0) ? 1 : 0;
                return 1;
            }
            return 0;
        }

        case NODE_BIN_OP:
        {
            int64_t a = 0;
            int64_t b = 0;
            if (!ir_lower_eval_const_i64(expr->data.bin_op.left, &a)) return 0;
            if (!ir_lower_eval_const_i64(expr->data.bin_op.right, &b)) return 0;

            uint64_t ua = (uint64_t)a;
            uint64_t ub = (uint64_t)b;

            switch (expr->data.bin_op.op) {
                case OP_ADD: *out_value = (int64_t)ir_lower_u64_add_wrap(ua, ub); return 1;
                case OP_SUB: *out_value = (int64_t)ir_lower_u64_sub_wrap(ua, ub); return 1;
                case OP_MUL: *out_value = (int64_t)ir_lower_u64_mul_wrap(ua, ub); return 1;

                case OP_DIV:
                    if (b == 0) return 0;
                    if (a == INT64_MIN && b == -1) { *out_value = INT64_MIN; return 1; }
                    *out_value = a / b;
                    return 1;
                case OP_MOD:
                    if (b == 0) return 0;
                    if (a == INT64_MIN && b == -1) { *out_value = 0; return 1; }
                    *out_value = a % b;
                    return 1;

                case OP_EQ:  *out_value = (a == b) ? 1 : 0; return 1;
                case OP_NEQ: *out_value = (a != b) ? 1 : 0; return 1;
                case OP_LT:  *out_value = (a <  b) ? 1 : 0; return 1;
                case OP_GT:  *out_value = (a >  b) ? 1 : 0; return 1;
                case OP_LTE: *out_value = (a <= b) ? 1 : 0; return 1;
                case OP_GTE: *out_value = (a >= b) ? 1 : 0; return 1;

                case OP_AND: *out_value = ((a != 0) && (b != 0)) ? 1 : 0; return 1;
                case OP_OR:  *out_value = ((a != 0) || (b != 0)) ? 1 : 0; return 1;
                default:
                    return 0;
            }
        }

        default:
            return 0;
    }
}

static int64_t ir_lower_trunc_const_to_type(int64_t v, IRType* t)
{
    if (!t) return v;
    if (t->kind == IR_TYPE_I1) return v ? 1 : 0;

    int bits = ir_type_bits(t);
    if (bits <= 0 || bits >= 64) return v;

    uint64_t mask = (1ULL << (unsigned)bits) - 1ULL;
    uint64_t u = ((uint64_t)v) & mask;

    if (t->kind == IR_TYPE_U8 || t->kind == IR_TYPE_U16 || t->kind == IR_TYPE_U32)
        return (int64_t)u;

    uint64_t sign_bit = 1ULL << (unsigned)(bits - 1);
    if (u & sign_bit) u |= ~mask;
    return (int64_t)u;
}

static IRValue* ir_lower_global_init_value(IRBuilder* builder, Node* expr, IRType* expected_type) {
    if (!builder) return NULL;

    if (!expr) {
        return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
    }

    switch (expr->type) {
        case NODE_INT:
        {
            IRType* t = expected_type ? expected_type : IR_TYPE_I64_T;
            int64_t v = (int64_t)expr->data.integer.value;
            v = ir_lower_trunc_const_to_type(v, t);
            return ir_value_const_int(v, t);
        }

        case NODE_BOOL:
        {
            IRType* t = expected_type ? expected_type : IR_TYPE_I1_T;
            int64_t v = expr->data.bool_lit.value ? 1 : 0;
            v = ir_lower_trunc_const_to_type(v, t);
            return ir_value_const_int(v, t);
        }

        case NODE_FLOAT:
            return ir_value_const_int((int64_t)expr->data.float_lit.bits,
                                      expected_type ? expected_type : IR_TYPE_F64_T);

        case NODE_CHAR:
            if (expected_type && expected_type->kind == IR_TYPE_CHAR) {
                uint64_t packed = pack_utf8_codepoint((uint32_t)expr->data.char_lit.value);
                return ir_value_const_int((int64_t)packed, IR_TYPE_CHAR_T);
            }
            {
                IRType* t = expected_type ? expected_type : IR_TYPE_I64_T;
                int64_t v = (int64_t)expr->data.char_lit.value;
                v = ir_lower_trunc_const_to_type(v, t);
                return ir_value_const_int(v, t);
            }

        case NODE_STRING:
            // Adds to module string table and returns pointer value.
            return ir_builder_const_baa_string(builder, expr->data.string_lit.value);

        case NODE_NULL:
            return ir_value_const_int(0, expected_type ? expected_type : ir_type_ptr(IR_TYPE_I8_T));

        case NODE_MEMBER_ACCESS:
            if (expr->data.member_access.is_enum_value) {
                return ir_value_const_int(expr->data.member_access.enum_value,
                                          expected_type ? expected_type : IR_TYPE_I64_T);
            }
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_CAST:
            return ir_lower_global_init_value(builder,
                                              expr->data.cast_expr.expression,
                                              expected_type);

        case NODE_UNARY_OP:
        case NODE_BIN_OP:
        {
            int64_t v = 0;
            if (ir_lower_eval_const_i64(expr, &v)) {
                IRType* t = expected_type ? expected_type : IR_TYPE_I64_T;
                v = ir_lower_trunc_const_to_type(v, t);
                return ir_value_const_int(v, t);
            }
            // Non-constant global initializers are not supported yet; fall back to zero.
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
        }

        default:
            // Non-constant global initializers are not supported yet; fall back to zero.
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
    }
}

IRModule* ir_lower_program(Node* program, const char* module_name, bool enable_bounds_checks) {
    if (!program || program->type != NODE_PROGRAM) return NULL;

    IRModule* module = ir_module_new(module_name ? module_name : "module");
    if (!module) return NULL;

    IRBuilder* builder = ir_builder_new(module);
    if (!builder) {
        ir_module_free(module);
        return NULL;
    }

    // -----------------------------------------------------------------
    // 0) إنشاء تواقيع الدوال أولاً (يدعم الاستدعاء قبل التعريف)
    // -----------------------------------------------------------------
    for (Node* decl0 = program->data.program.declarations; decl0; decl0 = decl0->next) {
        if (decl0->type != NODE_FUNC_DEF) continue;
        if (!decl0->data.func_def.name) continue;

        IRFunc* existing = ir_module_find_func(module, decl0->data.func_def.name);
        if (existing) {
            if (!decl0->data.func_def.is_prototype) {
                existing->is_prototype = false;
            }
            continue;
        }

        IRType* ret_type = ir_type_from_datatype(module, decl0->data.func_def.return_type);
        IRFunc* f0 = ir_builder_create_func(builder, decl0->data.func_def.name, ret_type);
        if (!f0) continue;
        f0->is_prototype = decl0->data.func_def.is_prototype;

        for (Node* p = decl0->data.func_def.params; p; p = p->next) {
            if (p->type != NODE_VAR_DECL) continue;
            IRType* ptype = ir_type_from_datatype(module, p->data.var_decl.type);
            (void)ir_builder_add_param(builder, p->data.var_decl.name, ptype);
        }
    }

    // لا نريد ترك current_func على آخر توقيع.
    ir_builder_set_func(builder, NULL);

    // Walk top-level declarations: globals + functions
    for (Node* decl = program->data.program.declarations; decl; decl = decl->next) {
        // Global arrays
        if (decl->type == NODE_ARRAY_DECL && decl->data.array_decl.is_global) {
            if (!decl->data.array_decl.name) continue;

            if (decl->data.array_decl.total_elems <= 0 || decl->data.array_decl.total_elems > INT_MAX) {
                fprintf(stderr, "IR Lower Error: global array '%s' has invalid total size\n",
                        decl->data.array_decl.name ? decl->data.array_decl.name : "???");
                continue;
            }

            int total_count = (int)decl->data.array_decl.total_elems;
            DataType elem_dt = decl->data.array_decl.element_type;
            bool elem_is_agg = (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION);

            IRType* elem_t = NULL;
            IRType* arr_t = NULL;

            if (elem_is_agg) {
                int elem_size = decl->data.array_decl.element_struct_size;
                if (elem_size <= 0) {
                    fprintf(stderr, "IR Lower Error: global array '%s' has invalid aggregate element size\n",
                            decl->data.array_decl.name ? decl->data.array_decl.name : "???");
                    continue;
                }
                int64_t total_bytes64 = decl->data.array_decl.total_elems * (int64_t)elem_size;
                if (total_bytes64 <= 0 || total_bytes64 > INT_MAX) {
                    fprintf(stderr, "IR Lower Error: global array '%s' is too large\n",
                            decl->data.array_decl.name ? decl->data.array_decl.name : "???");
                    continue;
                }
                elem_t = IR_TYPE_I8_T;
                arr_t = ir_type_array(IR_TYPE_I8_T, (int)total_bytes64);
            } else {
                elem_t = ir_type_from_datatype(module, elem_dt);
                if (!elem_t || elem_t->kind == IR_TYPE_VOID) elem_t = IR_TYPE_I64_T;
                arr_t = ir_type_array(elem_t, total_count);
            }
            if (!arr_t) continue;

            IRGlobal* g = ir_builder_create_global(builder, decl->data.array_decl.name, arr_t,
                                                   decl->data.array_decl.is_const ? 1 : 0);
            if (!g) continue;
            g->is_internal = decl->data.array_decl.is_static ? true : false;

            g->has_init_list = decl->data.array_decl.has_init ? true : false;

            if (decl->data.array_decl.has_init && !elem_is_agg) {
                int n = decl->data.array_decl.init_count;
                if (n < 0) n = 0;
                if (n > total_count) {
                    n = total_count;
                }

                // نخزن فقط العناصر المعطاة؛ الباقي يُعتبر صفراً كما في C.
                IRValue** elems = NULL;
                if (n > 0) {
                    elems = (IRValue**)ir_arena_calloc(&module->arena, (size_t)n, sizeof(IRValue*), _Alignof(IRValue*));
                    if (!elems) {
                        ir_builder_free(builder);
                        ir_module_free(module);
                        return NULL;
                    }
                }

                int i = 0;
                for (Node* v = decl->data.array_decl.init_values; v && i < n; v = v->next, i++) {
                    elems[i] = ir_lower_global_init_value(builder, v, elem_t);
                }

                g->init_elems = elems;
                g->init_elem_count = n;
            } else if (decl->data.array_decl.has_init && elem_is_agg && decl->data.array_decl.init_count > 0) {
                fprintf(stderr, "IR Lower Error: aggregate array initializer not supported for '%s'\n",
                        decl->data.array_decl.name ? decl->data.array_decl.name : "???");
            }

            continue;
        }

        // Globals
        if (decl->type == NODE_VAR_DECL && decl->data.var_decl.is_global) {
            if (decl->data.var_decl.type == TYPE_STRUCT || decl->data.var_decl.type == TYPE_UNION) {
                int n = decl->data.var_decl.struct_size;
                if (n <= 0) {
                    fprintf(stderr, "IR Lower Error: global struct '%s' has invalid size\n",
                            decl->data.var_decl.name ? decl->data.var_decl.name : "???");
                    continue;
                }
                IRType* bytes_t = ir_type_array(IR_TYPE_I8_T, n);
                IRGlobal* g = ir_builder_create_global(builder, decl->data.var_decl.name, bytes_t,
                                                       decl->data.var_decl.is_const ? 1 : 0);
                if (g) {
                    g->is_internal = decl->data.var_decl.is_static ? true : false;
                }
                continue;
            }

            IRType* gtype = ir_type_from_datatype(module, decl->data.var_decl.type);
            IRValue* init = ir_lower_global_init_value(builder, decl->data.var_decl.expression, gtype);

            IRGlobal* g = ir_builder_create_global_init(builder, decl->data.var_decl.name, gtype, init,
                                                        decl->data.var_decl.is_const ? 1 : 0);
            if (g) {
                g->is_internal = decl->data.var_decl.is_static ? true : false;
            }
            continue;
        }

        // Functions
        if (decl->type == NODE_FUNC_DEF) {
            if (!decl->data.func_def.name) continue;

            IRFunc* func = ir_module_find_func(module, decl->data.func_def.name);
            if (!func) {
                IRType* ret_type = ir_type_from_datatype(module, decl->data.func_def.return_type);
                func = ir_builder_create_func(builder, decl->data.func_def.name, ret_type);
                if (!func) continue;
                func->is_prototype = decl->data.func_def.is_prototype;
                for (Node* p = decl->data.func_def.params; p; p = p->next) {
                    if (p->type != NODE_VAR_DECL) continue;
                    IRType* ptype = ir_type_from_datatype(module, p->data.var_decl.type);
                    (void)ir_builder_add_param(builder, p->data.var_decl.name, ptype);
                }
            }

            // Prototypes don't have a body or blocks.
            if (decl->data.func_def.is_prototype || func->is_prototype) {
                continue;
            }

            ir_builder_set_func(builder, func);

            // Entry block
            IRBlock* entry = ir_builder_create_block(builder, "بداية");
            ir_builder_set_insert_point(builder, entry);

            IRLowerCtx ctx;
            ir_lower_ctx_init(&ctx, builder);
            ctx.current_func_name = decl->data.func_def.name;
            ctx.static_local_counter = 0;
            ctx.enable_bounds_checks = enable_bounds_checks;
            ctx.program_root = program;

            // نطاق الدالة: المعاملات + جسم الدالة
            ir_lower_scope_push(&ctx);

            // Parameters: spill into allocas so existing lowering can treat them like locals.
            int pi = 0;
            for (Node* p = decl->data.func_def.params; p; p = p->next) {
                if (p->type != NODE_VAR_DECL) continue;

                const char* pname = p->data.var_decl.name ? p->data.var_decl.name : NULL;
                if (!pname) { pi++; continue; }
                if (pi < 0 || pi >= func->param_count) { pi++; continue; }

                IRType* ptype = func->params[pi].type;
                int preg = func->params[pi].reg;

                // اربط تعليمات تهيئة معامل الدالة بموقعه في المصدر.
                ir_lower_set_loc(builder, p);

                int ptr_reg = ir_builder_emit_alloca(builder, ptype);
                ir_lower_tag_last_inst(builder, IR_OP_ALLOCA, ptr_reg, pname);
                ir_lower_bind_local(&ctx, pname, ptr_reg, ptype);

                IRValue* val = ir_value_reg(preg, ptype);
                IRType* ptr_type = ir_type_ptr(ptype ? ptype : IR_TYPE_I64_T);
                IRValue* ptr = ir_value_reg(ptr_reg, ptr_type);
                ir_lower_set_loc(builder, p);
                ir_builder_emit_store(builder, val, ptr);
                ir_lower_tag_last_inst(builder, IR_OP_STORE, -1, pname);

                pi++;
            }

            // Lower function body (NODE_BLOCK)
            if (decl->data.func_def.body) {
                lower_stmt(&ctx, decl->data.func_def.body);
            }

            // إذا وصلنا لنهاية الكتلة الحالية بدون منهي، نُغلق الدالة بشكل صريح.
            if (!ir_builder_is_block_terminated(builder)) {
                if (func->ret_type && func->ret_type->kind == IR_TYPE_VOID) {
                    ir_builder_emit_ret_void(builder);
                } else {
                    ir_lower_report_error(&ctx, decl,
                                          "الدالة '%s' قد تصل إلى النهاية بدون قيمة إرجاع.",
                                          decl->data.func_def.name ? decl->data.func_def.name : "???");
                    IRType* rt = func->ret_type ? func->ret_type : IR_TYPE_I64_T;
                    ir_builder_emit_ret(builder, ir_value_const_int(0, rt));
                }
            }

            // خروج نطاق الدالة
            ir_lower_scope_pop(&ctx);

            if (ctx.had_error) {
                ir_builder_free(builder);
                ir_module_free(module);
                return NULL;
            }
        }
    }

    ir_builder_free(builder);
    return module;
}
