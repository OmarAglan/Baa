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
}

void ir_lower_bind_local(IRLowerCtx* ctx, const char* name, int ptr_reg, IRType* value_type) {
    if (!ctx || !name) return;

    // ابحث فقط داخل النطاق الحالي: إن وجد، حدّث الربط.
    int start = ir_lower_current_scope_start(ctx);
    for (int i = ctx->local_count - 1; i >= start; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].ptr_reg = ptr_reg;
            ctx->locals[i].value_type = value_type;
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
    ctx->local_count++;
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

static IRType* get_char_ptr_type(IRModule* module) {
    if (!module) return ir_type_ptr(IR_TYPE_CHAR_T);
    ir_module_set_current(module);
    return ir_type_ptr(IR_TYPE_CHAR_T);
}

static uint64_t pack_utf8_codepoint(uint32_t cp)
{
    // U+FFFD replacement
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
        case TYPE_STRING: return get_char_ptr_type(module);
        case TYPE_CHAR:   return IR_TYPE_CHAR_T;
        case TYPE_FLOAT:  return IR_TYPE_F64_T;
        case TYPE_ENUM:   return IR_TYPE_I64_T;
        case TYPE_INT:
        default:          return IR_TYPE_I64_T;
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

    // packed: i64(value) = bytes(32-bit) + len * 2^32
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

    // select by flags: cp = cp1*f1 + cp2*f2 + cp3*f3 + cp4*f4
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

    // len = 1 + (cpv > 0x7F) + (cpv > 0x7FF) + (cpv > 0xFFFF)
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

    // len1
    IRValue* b0_1 = cpv;
    IRValue* b1_1 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* b2_1 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* b3_1 = ir_value_const_int(0, IR_TYPE_I64_T);

    // len2
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

    // len3
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

    // len4
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

    // select bytes by flags
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

    if (to->kind == IR_TYPE_I64 && v->type && v->type->kind == IR_TYPE_CHAR) {
        return lower_char_decode_to_i64(ctx, v);
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
        default:     return IR_OP_NOP;
    }
}

static IRCmpPred ir_cmp_to_pred(OpType op) {
    switch (op) {
        case OP_EQ:  return IR_CMP_EQ;
        case OP_NEQ: return IR_CMP_NE;
        case OP_GT:  return IR_CMP_GT;
        case OP_LT:  return IR_CMP_LT;
        case OP_GTE: return IR_CMP_GE;
        case OP_LTE: return IR_CMP_LE;
        default:     return IR_CMP_EQ;
    }
}

static IRValue* lower_call_expr(IRLowerCtx* ctx, Node* expr) {
    if (!ctx || !ctx->builder || !expr) return ir_builder_const_i64(0);

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
        args[i++] = lower_expr(ctx, a);
    }

    // For now (matching current semantic assumptions), calls return i64.
    int dest = ir_builder_emit_call(ctx->builder, expr->data.call.name, IR_TYPE_I64_T, args, arg_count);

    if (args) free(args);

    if (dest < 0) {
        // Void call used as expression: return 0 (placeholder).
        return ir_builder_const_i64(0);
    }

    return ir_value_reg(dest, IR_TYPE_I64_T);
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
                if (b->value_type && b->value_type->kind == IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "لا يمكن استخدام المصفوفة '%s' بدون فهرس.", name ? name : "???");
                    return ir_builder_const_i64(0);
                }
                IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

                IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
                IRValue* ptr = ir_value_reg(b->ptr_reg, ptr_type);
                int loaded = ir_builder_emit_load(ctx->builder, value_type, ptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
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
            IRType* arr_t = NULL;
            IRValue* base_ptr = NULL;

            IRLowerBinding* b = find_local(ctx, name);
            if (b) {
                if (!b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
                    (void)lower_expr(ctx, expr->data.array_op.index);
                    return ir_builder_const_i64(0);
                }
                arr_t = b->value_type;
                IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(arr_t));

                IRType* elem_t0 = arr_t->data.array.element ? arr_t->data.array.element : IR_TYPE_I64_T;
                IRType* elem_ptr_t0 = ir_type_ptr(elem_t0);

                // base: ptr(array) -> cast -> ptr(elem)
                int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t0);
                base_ptr = ir_value_reg(base_reg, elem_ptr_t0);
            } else {
                // مصفوفة عامة: نأخذ عنوانها عبر @name ونُعاملها كمؤشر إلى المصفوفة.
                IRGlobal* g = NULL;
                if (ctx->builder && ctx->builder->module && name) {
                    g = ir_module_find_global(ctx->builder->module, name);
                }
                if (!g || !g->type || g->type->kind != IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "مصفوفة غير معرّفة '%s'.", name ? name : "???");
                    (void)lower_expr(ctx, expr->data.array_op.index);
                    return ir_builder_const_i64(0);
                }
                arr_t = g->type;

                // مهم: لا نستخدم cast على global لأن MOV سيقرأ القيمة من الذاكرة.
                // نُعامل @name مباشرةً كمؤشر إلى العنصر الأول (مثل C: array decays to pointer).
                IRType* elem_t0 = arr_t->data.array.element ? arr_t->data.array.element : IR_TYPE_I64_T;
                base_ptr = ir_value_global(name, elem_t0);
            }

            IRType* elem_t = arr_t->data.array.element ? arr_t->data.array.element : IR_TYPE_I64_T;
            IRType* elem_ptr_t = ir_type_ptr(elem_t);

            // تأكد من أن base_ptr نوعه ptr(elem)
            if (!base_ptr) {
                ir_lower_report_error(ctx, expr, "فشل بناء عنوان عنصر المصفوفة '%s'.", name ? name : "???");
                (void)lower_expr(ctx, expr->data.array_op.index);
                return ir_builder_const_i64(0);
            }

            IRValue* idx = lower_expr(ctx, expr->data.array_op.index);
            if (idx && idx->type && idx->type->kind != IR_TYPE_I64) {
                int idx64 = ir_builder_emit_cast(ctx->builder, idx, IR_TYPE_I64_T);
                idx = ir_value_reg(idx64, IR_TYPE_I64_T);
            }

            int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
            IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

            int loaded = ir_builder_emit_load(ctx->builder, elem_t, elem_ptr);
            ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
            return ir_value_reg(loaded, elem_t);
        }

        case NODE_BIN_OP: {
            OpType op = expr->data.bin_op.op;

            // Arithmetic
            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                IRValue* lhs = ensure_i64(ctx, lower_expr(ctx, expr->data.bin_op.left));
                IRValue* rhs = ensure_i64(ctx, lower_expr(ctx, expr->data.bin_op.right));

                IRType* type = IR_TYPE_I64_T;
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

            // Comparison
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                IRValue* lhs = lhs0;
                IRValue* rhs = rhs0;

                // وحّد النوع عند المقارنة لتجنب i32 vs i64.
                if (!lhs || !rhs || !lhs->type || !rhs->type || !ir_types_equal(lhs->type, rhs->type)) {
                    lhs = ensure_i64(ctx, lhs0);
                    rhs = ensure_i64(ctx, rhs0);
                }

                IRCmpPred pred = ir_cmp_to_pred(op);
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

            if (op == UOP_NEG) {
                IRValue* operand = ensure_i64(ctx, lower_expr(ctx, expr->data.unary_op.operand));
                int dest = ir_builder_emit_neg(ctx->builder, IR_TYPE_I64_T, operand);
                return ir_value_reg(dest, IR_TYPE_I64_T);
            }

            if (op == UOP_NOT) {
                // Logical NOT: ensure operand is i1 then emit نفي.
                IRValue* operand = lower_to_bool(ctx, lower_expr(ctx, expr->data.unary_op.operand));
                int dest = ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, operand);
                return ir_value_reg(dest, IR_TYPE_I1_T);
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

        case NODE_FLOAT:
            return ir_value_const_int((int64_t)expr->data.float_lit.bits, IR_TYPE_F64_T);

        case NODE_CHAR:
        {
            uint64_t packed = pack_utf8_codepoint((uint32_t)expr->data.char_lit.value);
            return ir_value_const_int((int64_t)packed, IR_TYPE_CHAR_T);
        }

        case NODE_STRING:
            return ir_builder_const_baa_string(ctx->builder, expr->data.string_lit.value);

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
                IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
                base_ptr = ir_value_reg(c, ptr_i8_t);
            }

            IRValue* idx = ir_value_const_int((int64_t)expr->data.member_access.member_offset, IR_TYPE_I64_T);
            int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
            IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);

            IRType* field_val_t = NULL;
            switch (expr->data.member_access.member_type) {
                case TYPE_BOOL: field_val_t = IR_TYPE_I1_T; break;
                case TYPE_CHAR: field_val_t = IR_TYPE_I32_T; break;
                case TYPE_STRING: field_val_t = get_char_ptr_type(m); break;
                case TYPE_ENUM:
                case TYPE_INT:
                default: field_val_t = IR_TYPE_I64_T; break;
            }

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
    if (stmt->data.array_decl.size <= 0) {
        ir_lower_report_error(ctx, stmt, "حجم المصفوفة غير صالح.");
        return;
    }

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* elem_t = IR_TYPE_I64_T;
    IRType* arr_t = ir_type_array(elem_t, stmt->data.array_decl.size);

    int ptr_reg = ir_builder_emit_alloca(ctx->builder, arr_t);
    ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.array_decl.name);

    ir_lower_bind_local(ctx, stmt->data.array_decl.name, ptr_reg, arr_t);

    // تهيئة المصفوفة كما في C:
    // - إذا وُجدت قائمة تهيئة (حتى `{}`)، نُهيّئ العناصر المحددة ثم نملأ الباقي بأصفار.
    if (!stmt->data.array_decl.has_init) {
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
    while (v && i < stmt->data.array_decl.size) {
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
    for (; i < stmt->data.array_decl.size; i++) {
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
    IRType* arr_t = NULL;
    IRValue* arr_ptr = NULL;
    bool is_global_arr = false;

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        if (!b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
            ir_lower_report_error(ctx, stmt, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
            (void)lower_expr(ctx, stmt->data.array_op.index);
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }
        arr_t = b->value_type;
        arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(arr_t));
    } else {
        // مصفوفة عامة
        IRGlobal* g = NULL;
        if (ctx->builder && ctx->builder->module && name) {
            g = ir_module_find_global(ctx->builder->module, name);
        }
        if (!g || !g->type || g->type->kind != IR_TYPE_ARRAY) {
            ir_lower_report_error(ctx, stmt, "تعيين إلى مصفوفة غير معرّفة '%s'.", name ? name : "???");
            (void)lower_expr(ctx, stmt->data.array_op.index);
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }
        arr_t = g->type;
        // مهم: لا نستخدم cast على global لأن MOV سيقرأ القيمة من الذاكرة.
        // سنستخدم @name مباشرةً كقاعدة ptr(elem) لاحقاً.
        is_global_arr = true;
    }

    IRType* elem_t = arr_t->data.array.element ? arr_t->data.array.element : IR_TYPE_I64_T;
    IRType* elem_ptr_t = ir_type_ptr(elem_t);

    IRValue* base_ptr = NULL;
    if (is_global_arr) {
        base_ptr = ir_value_global(name, elem_t);
    } else {
        int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
        base_ptr = ir_value_reg(base_reg, elem_ptr_t);
    }

    if (!base_ptr) {
        ir_lower_report_error(ctx, stmt, "فشل بناء عنوان عنصر المصفوفة '%s'.", name ? name : "???");
        (void)lower_expr(ctx, stmt->data.array_op.index);
        (void)lower_expr(ctx, stmt->data.array_op.value);
        return;
    }

    IRValue* idx = lower_expr(ctx, stmt->data.array_op.index);
    if (idx && idx->type && idx->type->kind != IR_TYPE_I64) {
        int idx64 = ir_builder_emit_cast(ctx->builder, idx, IR_TYPE_I64_T);
        idx = ir_value_reg(idx64, IR_TYPE_I64_T);
    }

    int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
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
        IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
        int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
        base_ptr = ir_value_reg(c, ptr_i8_t);
    }

    IRValue* idx = ir_value_const_int((int64_t)target->data.member_access.member_offset, IR_TYPE_I64_T);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
    IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);

    IRType* field_val_t = NULL;
    switch (target->data.member_access.member_type) {
        case TYPE_BOOL: field_val_t = IR_TYPE_I1_T; break;
        case TYPE_CHAR: field_val_t = IR_TYPE_I32_T; break;
        case TYPE_STRING: field_val_t = get_char_ptr_type(m); break;
        case TYPE_ENUM:
        case TYPE_INT:
        default: field_val_t = IR_TYPE_I64_T; break;
    }
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

static void lower_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    const char* name = stmt->data.assign_stmt.name;

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

        IRValue* rhs = lower_expr(ctx, stmt->data.assign_stmt.expression);

        if (rhs && rhs->type && value_type && !ir_types_equal(rhs->type, value_type)) {
            rhs = cast_to(ctx, rhs, value_type);
        }

        // خزن <rhs>, %ptr
        (void)value_type; // reserved for future casts/type checks
        IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
        IRValue* ptr = ir_value_reg(b->ptr_reg, ptr_type);
        ir_lower_set_loc(ctx->builder, stmt);
        ir_builder_emit_store(ctx->builder, rhs, ptr);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, name);
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

    // packed i64 = bytes + len*2^32
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

    // NUL at buf[len]
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

    // افتراضي: طباعة كعدد صحيح.
    IRValue* v64 = value ? ensure_i64(ctx, value) : ir_value_const_int(0, IR_TYPE_I64_T);
    lower_printf_cstr(ctx, stmt, "%d\n", v64);
}

static void lower_read(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRLowerBinding* b = find_local(ctx, stmt->data.read_stmt.var_name);
    if (!b) {
        ir_lower_report_error(ctx, stmt, "قراءة إلى متغير غير معروف '%s'.",
                              stmt->data.read_stmt.var_name ? stmt->data.read_stmt.var_name : "???");
        return;
    }

    // scanf("%d", &var)
    IRValue* fmt_val = ir_builder_const_string(ctx->builder, "%d");
    IRType* ptr_type = ir_type_ptr((b->value_type) ? b->value_type : IR_TYPE_I64_T);
    IRValue* ptr = ir_value_reg(b->ptr_reg, ptr_type);

    IRValue* args[2] = { fmt_val, ptr };

    // Builtin: اقرأ(ptr) → scanf(fmt, ptr)
    ir_builder_emit_call_void(ctx->builder, "اقرأ", args, 2);
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

static IRValue* ir_lower_global_init_value(IRBuilder* builder, Node* expr, IRType* expected_type) {
    if (!builder) return NULL;

    if (!expr) {
        return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
    }

    switch (expr->type) {
        case NODE_INT:
            return ir_value_const_int((int64_t)expr->data.integer.value,
                                      expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_BOOL:
            return ir_value_const_int(expr->data.bool_lit.value ? 1 : 0,
                                      expected_type ? expected_type : IR_TYPE_I1_T);

        case NODE_FLOAT:
            return ir_value_const_int((int64_t)expr->data.float_lit.bits,
                                      expected_type ? expected_type : IR_TYPE_F64_T);

        case NODE_CHAR:
            if (expected_type && expected_type->kind == IR_TYPE_CHAR) {
                uint64_t packed = pack_utf8_codepoint((uint32_t)expr->data.char_lit.value);
                return ir_value_const_int((int64_t)packed, IR_TYPE_CHAR_T);
            }
            return ir_value_const_int((int64_t)expr->data.char_lit.value,
                                      expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_STRING:
            // Adds to module string table and returns pointer value.
            return ir_builder_const_baa_string(builder, expr->data.string_lit.value);

        case NODE_MEMBER_ACCESS:
            if (expr->data.member_access.is_enum_value) {
                return ir_value_const_int(expr->data.member_access.enum_value,
                                          expected_type ? expected_type : IR_TYPE_I64_T);
            }
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_UNARY_OP:
        case NODE_BIN_OP:
        {
            int64_t v = 0;
            if (ir_lower_eval_const_i64(expr, &v)) {
                return ir_value_const_int(v, expected_type ? expected_type : IR_TYPE_I64_T);
            }
            // Non-constant global initializers are not supported yet; fall back to zero.
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
        }

        default:
            // Non-constant global initializers are not supported yet; fall back to zero.
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
    }
}

IRModule* ir_lower_program(Node* program, const char* module_name) {
    if (!program || program->type != NODE_PROGRAM) return NULL;

    IRModule* module = ir_module_new(module_name ? module_name : "module");
    if (!module) return NULL;

    IRBuilder* builder = ir_builder_new(module);
    if (!builder) {
        ir_module_free(module);
        return NULL;
    }

    // Walk top-level declarations: globals + functions
    for (Node* decl = program->data.program.declarations; decl; decl = decl->next) {
        // Global arrays
        if (decl->type == NODE_ARRAY_DECL && decl->data.array_decl.is_global) {
            if (!decl->data.array_decl.name) continue;

            IRType* elem_t = IR_TYPE_I64_T;
            IRType* arr_t = ir_type_array(elem_t, decl->data.array_decl.size);

            IRGlobal* g = ir_builder_create_global(builder, decl->data.array_decl.name, arr_t,
                                                   decl->data.array_decl.is_const ? 1 : 0);
            if (!g) continue;

            g->has_init_list = decl->data.array_decl.has_init ? true : false;

            if (decl->data.array_decl.has_init) {
                int n = decl->data.array_decl.init_count;
                if (n < 0) n = 0;
                if (decl->data.array_decl.size > 0 && n > decl->data.array_decl.size) {
                    n = decl->data.array_decl.size;
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
                (void)ir_builder_create_global(builder, decl->data.var_decl.name, bytes_t,
                                               decl->data.var_decl.is_const ? 1 : 0);
                continue;
            }

            IRType* gtype = ir_type_from_datatype(module, decl->data.var_decl.type);
            IRValue* init = ir_lower_global_init_value(builder, decl->data.var_decl.expression, gtype);

            (void)ir_builder_create_global_init(builder, decl->data.var_decl.name, gtype, init,
                                                decl->data.var_decl.is_const ? 1 : 0);
            continue;
        }

        // Functions
        if (decl->type == NODE_FUNC_DEF) {
            IRType* ret_type = ir_type_from_datatype(module, decl->data.func_def.return_type);
            IRFunc* func = ir_builder_create_func(builder, decl->data.func_def.name, ret_type);
            if (!func) continue;

            func->is_prototype = decl->data.func_def.is_prototype;

            // Prototypes don't have a body or blocks.
            if (func->is_prototype) {
                // Still add parameters for signature printing.
                for (Node* p = decl->data.func_def.params; p; p = p->next) {
                    if (p->type != NODE_VAR_DECL) continue;
                    IRType* ptype = ir_type_from_datatype(module, p->data.var_decl.type);
                    (void)ir_builder_add_param(builder, p->data.var_decl.name, ptype);
                }
                continue;
            }

            // Entry block
            IRBlock* entry = ir_builder_create_block(builder, "بداية");
            ir_builder_set_insert_point(builder, entry);

            IRLowerCtx ctx;
            ir_lower_ctx_init(&ctx, builder);

            // نطاق الدالة: المعاملات + جسم الدالة
            ir_lower_scope_push(&ctx);

            // Parameters: add params, then spill into allocas so existing lowering can treat them like locals.
            for (Node* p = decl->data.func_def.params; p; p = p->next) {
                if (p->type != NODE_VAR_DECL) continue;

                IRType* ptype = ir_type_from_datatype(module, p->data.var_decl.type);
                const char* pname = p->data.var_decl.name ? p->data.var_decl.name : NULL;

                int preg = ir_builder_add_param(builder, pname, ptype);

                if (pname) {
                    // اربط تعليمات تهيئة معامل الدالة بموقعه في المصدر.
                    ir_lower_set_loc(builder, p);

                    int ptr_reg = ir_builder_emit_alloca(builder, ptype);
                    ir_lower_tag_last_inst(builder, IR_OP_ALLOCA, ptr_reg, pname);
                    ir_lower_bind_local(&ctx, pname, ptr_reg, ptype);

                    IRValue* val = ir_value_reg(preg, ptype);   // %معامل<n>
                    IRType* ptr_type = ir_type_ptr(ptype ? ptype : IR_TYPE_I64_T);
                    IRValue* ptr = ir_value_reg(ptr_reg, ptr_type);
                    ir_lower_set_loc(builder, p);
                    ir_builder_emit_store(builder, val, ptr);
                    ir_lower_tag_last_inst(builder, IR_OP_STORE, -1, pname);
                }
            }

            // Lower function body (NODE_BLOCK)
            if (decl->data.func_def.body) {
                lower_stmt(&ctx, decl->data.func_def.body);
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
