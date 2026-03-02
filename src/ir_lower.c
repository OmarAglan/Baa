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
#include "target.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <limits.h>

// ============================================================================
// ثوابت/أسماء خاصة بنقطة الدخول
// ============================================================================

#define BAA_ENTRY_FUNC_NAME "الرئيسية"
#define BAA_WRAPPED_USER_MAIN_NAME "__baa_user_main"
#define BAA_INLINE_ASM_PSEUDO_CALL "__baa_inline_asm_v0406"

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

static bool ir_lower_ast_func_is_main_with_args(const Node* func_def)
{
    if (!func_def || func_def->type != NODE_FUNC_DEF) return false;
    if (!func_def->data.func_def.name) return false;
    if (strcmp(func_def->data.func_def.name, BAA_ENTRY_FUNC_NAME) != 0) return false;

    // نتحقق من أن التوقيع هو: صحيح الرئيسية(صحيح، نص[])
    // ملاحظة: `نص[]` داخل المعاملات تُحلل كسكر نحوي إلى `TYPE_POINTER` (base=TYPE_STRING, depth=1).
    const Node* p0 = NULL;
    const Node* p1 = NULL;
    int pc = 0;
    for (const Node* p = func_def->data.func_def.params; p; p = p->next) {
        if (p->type != NODE_VAR_DECL) continue;
        if (pc == 0) p0 = p;
        if (pc == 1) p1 = p;
        pc++;
        if (pc > 2) break;
    }

    if (pc != 2 || !p0 || !p1) return false;

    bool ok0 = (p0->data.var_decl.type == TYPE_INT);
    bool ok1 = (p1->data.var_decl.type == TYPE_POINTER &&
                p1->data.var_decl.ptr_base_type == TYPE_STRING &&
                p1->data.var_decl.ptr_depth == 1);

    return ok0 && ok1;
}

// ---------------------------------------------------------------------------
// Forward declarations (لأن بعض الدوال المساعدة تُعرّف لاحقاً في هذا الملف)
// ---------------------------------------------------------------------------

static void ir_lower_set_loc(IRBuilder* builder, const Node* node);
static IRType* get_char_ptr_type(IRModule* module);
static IRValue* cast_to(IRLowerCtx* ctx, IRValue* v, IRType* to);
static IRBlock* cf_create_block(IRLowerCtx* ctx, const char* base_label);
static IRValue* cast_i1_to_i64(IRLowerCtx* ctx, IRValue* v);
static uint64_t pack_utf8_codepoint(uint32_t cp);

/**
 * @brief تحويل C-string (char*) إلى نص باء (حرف[]) على heap.
 *
 * هذا التحويل يُستخدم خصيصاً لجسر ABI في `الرئيسية(عدد، معاملات)` حيث يمرر C Runtime
 * `char** argv`، بينما تمثيل `نص` في باء هو مصفوفة `حرف` (i64) محزومة UTF-8.
 *
 * ملاحظة: لا نقوم بفك ترميز Unicode بالكامل، بل نقوم بحزم تسلسل UTF-8 (1..4 بايت)
 * كما هو إلى صيغة `حرف` (bytes_field | (len<<32)). عند إدخال غير صالح نستبدل بـ U+FFFD.
 */
#if 0
static IRValue* ir_lower_cstr_to_baa_string_alloc(IRLowerCtx* ctx, const Node* site, IRValue* cstr_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_CHAR_T));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* u8_ptr_t = ir_type_ptr(IR_TYPE_U8_T);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);

    IRValue* cstr = cast_to(ctx, cstr_in, i8_ptr_t);

    // نتيجة قابلة للكتابة عبر تحكم التدفق (NULL -> NULL)
    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    if (res_ptr_reg < 0) {
        ir_lower_report_error(ctx, site, "فشل إنشاء alloca لنتيجة تحويل argv.");
        return ir_value_const_int(0, char_ptr_t);
    }
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);

    IRBlock* b_null = cf_create_block(ctx, "تحويل_argv_NULL");
    IRBlock* b_work = cf_create_block(ctx, "تحويل_argv_عمل");
    IRBlock* b_ret  = cf_create_block(ctx, "تحويل_argv_إرجاع");
    if (!b_null || !b_work || !b_ret) {
        return ir_value_const_int(0, char_ptr_t);
    }

    int is_null_r = ir_builder_emit_cmp_eq(b, cstr, ir_value_const_int(0, i8_ptr_t));
    ir_builder_emit_br_cond(b, ir_value_reg(is_null_r, IR_TYPE_I1_T), b_null, b_work);

    ir_builder_set_insert_point(b, b_null);
    ir_lower_set_loc(b, site);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_work);
    ir_lower_set_loc(b, site);

    // bytes_len = strlen(cstr)
    IRValue* sargs[1] = { cast_to(ctx, cstr, i8_ptr_t) };
    int bytes_len_r = ir_builder_emit_call(b, "strlen", IR_TYPE_I64_T, sargs, 1);
    if (bytes_len_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء strlen أثناء تحويل argv.");
        ir_builder_emit_br(b, b_ret);
    } else {
        IRValue* bytes_len = ir_value_reg(bytes_len_r, IR_TYPE_I64_T);

        // alloc_bytes = (bytes_len + 1) * 8
        IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
        IRValue* two = ir_value_const_int(2, IR_TYPE_I64_T);
        IRValue* three = ir_value_const_int(3, IR_TYPE_I64_T);
        IRValue* four = ir_value_const_int(4, IR_TYPE_I64_T);
        IRValue* eight = ir_value_const_int(8, IR_TYPE_I64_T);
        IRValue* sh8 = ir_value_const_int(8, IR_TYPE_I64_T);
        IRValue* sh16 = ir_value_const_int(16, IR_TYPE_I64_T);
        IRValue* sh24 = ir_value_const_int(24, IR_TYPE_I64_T);
        IRValue* sh32 = ir_value_const_int(32, IR_TYPE_I64_T);

        int bl1_r = ir_builder_emit_add(b, IR_TYPE_I64_T, bytes_len, one);
        IRValue* bl1 = ir_value_reg(bl1_r, IR_TYPE_I64_T);
        int alloc_bytes_r = ir_builder_emit_mul(b, IR_TYPE_I64_T, bl1, eight);
        IRValue* alloc_bytes = ir_value_reg(alloc_bytes_r, IR_TYPE_I64_T);

        IRValue* margs[1] = { alloc_bytes };
        int raw_ptr_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
        if (raw_ptr_r < 0) {
            ir_lower_report_error(ctx, site, "فشل خفض نداء malloc أثناء تحويل argv.");
            ir_builder_emit_br(b, b_ret);
        } else {
            IRValue* raw_ptr = ir_value_reg(raw_ptr_r, i8_ptr_t);
            IRValue* dst = ir_value_reg(ir_builder_emit_cast(b, raw_ptr, char_ptr_t), char_ptr_t);

            // in_idx/out_idx
            int in_idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
            int out_idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
            IRValue* in_idx_ptr = ir_value_reg(in_idx_ptr_reg, i64_ptr_t);
            IRValue* out_idx_ptr = ir_value_reg(out_idx_ptr_reg, i64_ptr_t);
            ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), in_idx_ptr);
            ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), out_idx_ptr);

            IRValue* cstr_u8 = ir_value_reg(ir_builder_emit_cast(b, cstr, u8_ptr_t), u8_ptr_t);

            uint64_t repl_bits_u = pack_utf8_codepoint(0xFFFDu);
            IRValue* repl_ch = ir_value_const_int((int64_t)repl_bits_u, IR_TYPE_CHAR_T);

            IRBlock* head = cf_create_block(ctx, "تحويل_argv_تحقق");
            IRBlock* disp = cf_create_block(ctx, "تحويل_argv_توزيع");
            IRBlock* c1 = cf_create_block(ctx, "تحويل_argv_حالة1");
            IRBlock* c2 = cf_create_block(ctx, "تحويل_argv_حالة2");
            IRBlock* c3 = cf_create_block(ctx, "تحويل_argv_حالة3");
            IRBlock* c4 = cf_create_block(ctx, "تحويل_argv_حالة4");
            IRBlock* inv = cf_create_block(ctx, "تحويل_argv_غير_صالح");
            IRBlock* done = cf_create_block(ctx, "تحويل_argv_انتهاء");
            if (!head || !disp || !c1 || !c2 || !c3 || !c4 || !inv || !done) {
                ir_lower_report_error(ctx, site, "فشل إنشاء كتل تحويل argv.");
                ir_builder_emit_br(b, b_ret);
            } else {
                ir_builder_emit_br(b, head);

                // head: اقرأ b0، إن كان NUL نُنهي
                ir_builder_set_insert_point(b, head);
                ir_lower_set_loc(b, site);
                IRValue* in_idx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, in_idx_ptr), IR_TYPE_I64_T);
                IRValue* out_idx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_idx_ptr), IR_TYPE_I64_T);

                IRValue* b0p = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in_idx), u8_ptr_t);
                IRValue* b0 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b0p), IR_TYPE_U8_T);
                int b0z_r = ir_builder_emit_cmp_eq(b, b0, ir_value_const_int(0, IR_TYPE_U8_T));
                ir_builder_emit_br_cond(b, ir_value_reg(b0z_r, IR_TYPE_I1_T), done, disp);

                // disp: حدد طول UTF-8 واذهب للحالة المناسبة
                ir_builder_set_insert_point(b, disp);
                ir_lower_set_loc(b, site);

                IRValue* b0u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                               ir_value_reg(ir_builder_emit_cast(b, b0, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                               ir_value_const_int(255, IR_TYPE_I64_T)),
                                            IR_TYPE_I64_T);

                IRValue* m80 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0x80, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* mE0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* mF0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* mF8 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* is1 = ir_value_reg(ir_builder_emit_cmp_eq(b, m80, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
                IRValue* is2 = ir_value_reg(ir_builder_emit_cmp_eq(b, mE0, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
                IRValue* is3 = ir_value_reg(ir_builder_emit_cmp_eq(b, mF0, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
                IRValue* is4 = ir_value_reg(ir_builder_emit_cmp_eq(b, mF8, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I1_T);

                IRValue* l1 = cast_i1_to_i64(ctx, is1);
                IRValue* l2 = cast_i1_to_i64(ctx, is2);
                IRValue* l3 = cast_i1_to_i64(ctx, is3);
                IRValue* l4 = cast_i1_to_i64(ctx, is4);

                IRValue* t2 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l2, two), IR_TYPE_I64_T);
                IRValue* t3 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l3, three), IR_TYPE_I64_T);
                IRValue* t4 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l4, four), IR_TYPE_I64_T);

                IRValue* len0 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T,
                                                                 ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, l1, t2), IR_TYPE_I64_T),
                                                                 ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, t3, t4), IR_TYPE_I64_T)),
                                             IR_TYPE_I64_T);

                int is1r = ir_builder_emit_cmp_eq(b, len0, one);
                int is2r = ir_builder_emit_cmp_eq(b, len0, two);
                int is3r = ir_builder_emit_cmp_eq(b, len0, three);
                int is4r = ir_builder_emit_cmp_eq(b, len0, four);

                ir_builder_emit_br_cond(b, ir_value_reg(is1r, IR_TYPE_I1_T), c1,
                                        ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I1_T,
                                                                       ir_value_reg(is2r, IR_TYPE_I1_T),
                                                                       ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I1_T,
                                                                                                      ir_value_reg(is3r, IR_TYPE_I1_T),
                                                                                                      ir_value_reg(is4r, IR_TYPE_I1_T)),
                                                                                   IR_TYPE_I1_T)),
                                                     IR_TYPE_I1_T),
                                        c2, inv);

                // حالة1
                ir_builder_set_insert_point(b, c1);
                ir_lower_set_loc(b, site);
                IRValue* outp1 = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out_idx), char_ptr_t);
                IRValue* l1sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, one, sh32), IR_TYPE_I64_T);
                IRValue* packed1_i64 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u, l1sh), IR_TYPE_I64_T);
                IRValue* packed1 = ir_value_reg(ir_builder_emit_cast(b, packed1_i64, IR_TYPE_CHAR_T), IR_TYPE_CHAR_T);
                ir_builder_emit_store(b, packed1, outp1);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out_idx, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx, one), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // حالة2
                ir_builder_set_insert_point(b, c2);
                ir_lower_set_loc(b, site);
                ir_builder_emit_br_cond(b, ir_value_reg(is2r, IR_TYPE_I1_T), c2, c3);

                // حزم/تحقق حالة2
                IRBlock* c2pack = cf_create_block(ctx, "تحويل_argv_حالة2_حزم");
                if (!c2pack) c2pack = inv;
                ir_builder_set_insert_point(b, c2);
                ir_lower_set_loc(b, site);
                IRValue* in1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx, one), IR_TYPE_I64_T);
                IRValue* b1p = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in1), u8_ptr_t);
                IRValue* b1 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b1p), IR_TYPE_U8_T);
                IRValue* b1u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                ir_value_reg(ir_builder_emit_cast(b, b1, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                ir_value_const_int(255, IR_TYPE_I64_T)),
                                             IR_TYPE_I64_T);
                IRValue* b1m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                int b1ok = ir_builder_emit_cmp_eq(b, b1m, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b1nz = ir_builder_emit_cmp_ne(b, b1u, ir_value_const_int(0, IR_TYPE_I64_T));
                int ok2 = ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1ok, IR_TYPE_I1_T), ir_value_reg(b1nz, IR_TYPE_I1_T));
                ir_builder_emit_br_cond(b, ir_value_reg(ok2, IR_TYPE_I1_T), c2pack, inv);

                ir_builder_set_insert_point(b, c2pack);
                ir_lower_set_loc(b, site);
                IRValue* b1sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u, sh8), IR_TYPE_I64_T);
                IRValue* bytes2 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u, b1sh), IR_TYPE_I64_T);
                IRValue* l2sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, two, sh32), IR_TYPE_I64_T);
                IRValue* packed2_i64 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes2, l2sh), IR_TYPE_I64_T);
                IRValue* packed2 = ir_value_reg(ir_builder_emit_cast(b, packed2_i64, IR_TYPE_CHAR_T), IR_TYPE_CHAR_T);
                IRValue* outp2 = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out_idx), char_ptr_t);
                ir_builder_emit_store(b, packed2, outp2);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out_idx, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx, two), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // حالة3
                ir_builder_set_insert_point(b, c3);
                ir_lower_set_loc(b, site);
                ir_builder_emit_br_cond(b, ir_value_reg(is3r, IR_TYPE_I1_T), c3, c4);

                IRBlock* c3pack = cf_create_block(ctx, "تحويل_argv_حالة3_حزم");
                if (!c3pack) c3pack = inv;
                ir_builder_set_insert_point(b, c3);
                ir_lower_set_loc(b, site);
                IRValue* in2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx, two), IR_TYPE_I64_T);
                IRValue* b2p = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in2), u8_ptr_t);
                IRValue* b2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b2p), IR_TYPE_U8_T);
                IRValue* b2u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                ir_value_reg(ir_builder_emit_cast(b, b2, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                ir_value_const_int(255, IR_TYPE_I64_T)),
                                             IR_TYPE_I64_T);
                IRValue* b2m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                int b2ok = ir_builder_emit_cmp_eq(b, b2m, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b2nz = ir_builder_emit_cmp_ne(b, b2u, ir_value_const_int(0, IR_TYPE_I64_T));
                int ok3 = ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(ok2, IR_TYPE_I1_T),
                                              ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T,
                                                                              ir_value_reg(b2ok, IR_TYPE_I1_T),
                                                                              ir_value_reg(b2nz, IR_TYPE_I1_T)),
                                                           IR_TYPE_I1_T));
                ir_builder_emit_br_cond(b, ir_value_reg(ok3, IR_TYPE_I1_T), c3pack, inv);

                ir_builder_set_insert_point(b, c3pack);
                ir_lower_set_loc(b, site);
                IRValue* b2sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b2u, sh16), IR_TYPE_I64_T);
                IRValue* bytes3 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes2, b2sh), IR_TYPE_I64_T);
                IRValue* l3sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, three, sh32), IR_TYPE_I64_T);
                IRValue* packed3_i64 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes3, l3sh), IR_TYPE_I64_T);
                IRValue* packed3 = ir_value_reg(ir_builder_emit_cast(b, packed3_i64, IR_TYPE_CHAR_T), IR_TYPE_CHAR_T);
                IRValue* outp3 = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out_idx), char_ptr_t);
                ir_builder_emit_store(b, packed3, outp3);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out_idx, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx, three), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // حالة4
                ir_builder_set_insert_point(b, c4);
                ir_lower_set_loc(b, site);
                ir_builder_emit_br_cond(b, ir_value_reg(is4r, IR_TYPE_I1_T), c4, inv);

                IRBlock* c4pack = cf_create_block(ctx, "تحويل_argv_حالة4_حزم");
                if (!c4pack) c4pack = inv;
                ir_builder_set_insert_point(b, c4);
                ir_lower_set_loc(b, site);
                IRValue* in3 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx, three), IR_TYPE_I64_T);
                IRValue* b3p = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in3), u8_ptr_t);
                IRValue* b3 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b3p), IR_TYPE_U8_T);
                IRValue* b3u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                ir_value_reg(ir_builder_emit_cast(b, b3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                ir_value_const_int(255, IR_TYPE_I64_T)),
                                             IR_TYPE_I64_T);
                IRValue* b3m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b3u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                int b3ok = ir_builder_emit_cmp_eq(b, b3m, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b3nz = ir_builder_emit_cmp_ne(b, b3u, ir_value_const_int(0, IR_TYPE_I64_T));
                int ok4 = ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(ok3, IR_TYPE_I1_T),
                                              ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T,
                                                                              ir_value_reg(b3ok, IR_TYPE_I1_T),
                                                                              ir_value_reg(b3nz, IR_TYPE_I1_T)),
                                                           IR_TYPE_I1_T));
                ir_builder_emit_br_cond(b, ir_value_reg(ok4, IR_TYPE_I1_T), c4pack, inv);

                ir_builder_set_insert_point(b, c4pack);
                ir_lower_set_loc(b, site);
                IRValue* b3sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b3u, sh24), IR_TYPE_I64_T);
                IRValue* bytes4 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes3, b3sh), IR_TYPE_I64_T);
                IRValue* l4sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, four, sh32), IR_TYPE_I64_T);
                IRValue* packed4_i64 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes4, l4sh), IR_TYPE_I64_T);
                IRValue* packed4 = ir_value_reg(ir_builder_emit_cast(b, packed4_i64, IR_TYPE_CHAR_T), IR_TYPE_CHAR_T);
                IRValue* outp4 = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out_idx), char_ptr_t);
                ir_builder_emit_store(b, packed4, outp4);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out_idx, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx, four), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // غير صالح
                ir_builder_set_insert_point(b, inv);
                ir_lower_set_loc(b, site);
                IRValue* outpinv = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out_idx), char_ptr_t);
                ir_builder_emit_store(b, repl_ch, outpinv);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out_idx, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx, one), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // done: ضع NULL-terminator واكتب النتيجة
                ir_builder_set_insert_point(b, done);
                ir_lower_set_loc(b, site);
                IRValue* outpend = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out_idx), char_ptr_t);
                ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_CHAR_T), outpend);
                ir_builder_emit_store(b, dst, res_ptr);
                ir_builder_emit_br(b, b_ret);
            }
        }
    }

    ir_builder_set_insert_point(b, b_ret);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(r, char_ptr_t);
}
#endif

static IRValue* ir_lower_cstr_to_baa_string_alloc(IRLowerCtx* ctx, const Node* site, IRValue* cstr_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_CHAR_T));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* u8_ptr_t = ir_type_ptr(IR_TYPE_U8_T);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);

    IRValue* cstr = cast_to(ctx, cstr_in, i8_ptr_t);

    // نُعيد NULL عند NULL (حتى لا نسقط في strlen/malloc).
    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    if (res_ptr_reg < 0) {
        ir_lower_report_error(ctx, site, "فشل إنشاء alloca لنتيجة تحويل argv.");
        return ir_value_const_int(0, char_ptr_t);
    }
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);

    IRBlock* b_null = cf_create_block(ctx, "تحويل_argv_NULL");
    IRBlock* b_work = cf_create_block(ctx, "تحويل_argv_عمل");
    IRBlock* b_ret  = cf_create_block(ctx, "تحويل_argv_إرجاع");
    if (!b_null || !b_work || !b_ret) {
        return ir_value_const_int(0, char_ptr_t);
    }

    int is_null_r = ir_builder_emit_cmp_eq(b, cstr, ir_value_const_int(0, i8_ptr_t));
    ir_builder_emit_br_cond(b, ir_value_reg(is_null_r, IR_TYPE_I1_T), b_null, b_work);

    ir_builder_set_insert_point(b, b_null);
    ir_lower_set_loc(b, site);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_work);
    ir_lower_set_loc(b, site);

    // bytes_len = strlen(cstr)
    IRValue* sargs[1] = { cstr };
    int bytes_len_r = ir_builder_emit_call(b, "strlen", IR_TYPE_I64_T, sargs, 1);
    if (bytes_len_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء strlen أثناء تحويل argv.");
        ir_builder_emit_br(b, b_ret);
    } else {
        IRValue* bytes_len = ir_value_reg(bytes_len_r, IR_TYPE_I64_T);

        IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
        IRValue* two = ir_value_const_int(2, IR_TYPE_I64_T);
        IRValue* three = ir_value_const_int(3, IR_TYPE_I64_T);
        IRValue* four = ir_value_const_int(4, IR_TYPE_I64_T);
        IRValue* eight = ir_value_const_int(8, IR_TYPE_I64_T);

        IRValue* sh8 = ir_value_const_int(8, IR_TYPE_I64_T);
        IRValue* sh16 = ir_value_const_int(16, IR_TYPE_I64_T);
        IRValue* sh24 = ir_value_const_int(24, IR_TYPE_I64_T);
        IRValue* sh32 = ir_value_const_int(32, IR_TYPE_I64_T);

        // alloc_bytes = (bytes_len + 1) * 8  (أقصى حد: كل بايت -> حرف واحد)
        IRValue* bl1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, bytes_len, one), IR_TYPE_I64_T);
        IRValue* alloc_bytes = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, bl1, eight), IR_TYPE_I64_T);

        IRValue* margs[1] = { alloc_bytes };
        int raw_ptr_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
        if (raw_ptr_r < 0) {
            ir_lower_report_error(ctx, site, "فشل خفض نداء malloc أثناء تحويل argv.");
            ir_builder_emit_br(b, b_ret);
        } else {
            IRValue* raw_ptr = ir_value_reg(raw_ptr_r, i8_ptr_t);
            IRValue* dst = ir_value_reg(ir_builder_emit_cast(b, raw_ptr, char_ptr_t), char_ptr_t);

            // مؤشرات الحلقة
            IRValue* in_idx_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
            IRValue* out_idx_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
            ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), in_idx_ptr);
            ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), out_idx_ptr);

            // تخزين مؤقت للحالة الحالية (لتجنب مشاكل SSA مع تعدد الكتل)
            IRValue* b0u_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
            IRValue* len0_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
            ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), b0u_ptr);
            ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), len0_ptr);

            IRValue* cstr_u8 = ir_value_reg(ir_builder_emit_cast(b, cstr, u8_ptr_t), u8_ptr_t);

            uint64_t repl_bits_u = pack_utf8_codepoint(0xFFFDu);
            IRValue* repl_ch = ir_value_const_int((int64_t)repl_bits_u, IR_TYPE_CHAR_T);

            IRBlock* head = cf_create_block(ctx, "تحويل_argv_تحقق");
            IRBlock* disp = cf_create_block(ctx, "تحويل_argv_توزيع");
            IRBlock* d2 = cf_create_block(ctx, "تحويل_argv_د2");
            IRBlock* d3 = cf_create_block(ctx, "تحويل_argv_د3");
            IRBlock* d4 = cf_create_block(ctx, "تحويل_argv_د4");
            IRBlock* c1 = cf_create_block(ctx, "تحويل_argv_حالة1");
            IRBlock* c2b = cf_create_block(ctx, "تحويل_argv_حالة2");
            IRBlock* c2pack = cf_create_block(ctx, "تحويل_argv_حالة2_حزم");
            IRBlock* c3b = cf_create_block(ctx, "تحويل_argv_حالة3");
            IRBlock* c3pack = cf_create_block(ctx, "تحويل_argv_حالة3_حزم");
            IRBlock* c4b = cf_create_block(ctx, "تحويل_argv_حالة4");
            IRBlock* c4pack = cf_create_block(ctx, "تحويل_argv_حالة4_حزم");
            IRBlock* inv = cf_create_block(ctx, "تحويل_argv_غير_صالح");
            IRBlock* done = cf_create_block(ctx, "تحويل_argv_انتهاء");

            if (!head || !disp || !d2 || !d3 || !d4 || !c1 || !c2b || !c2pack ||
                !c3b || !c3pack || !c4b || !c4pack || !inv || !done) {
                ir_lower_report_error(ctx, site, "فشل إنشاء كتل تحويل argv.");
                ir_builder_emit_br(b, b_ret);
            } else {
                ir_builder_emit_br(b, head);

                // head: اقرأ البايت الأول؛ إن كان NUL انتهِ.
                ir_builder_set_insert_point(b, head);
                ir_lower_set_loc(b, site);
                IRValue* in_idx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, in_idx_ptr), IR_TYPE_I64_T);
                IRValue* b0p = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in_idx), u8_ptr_t);
                IRValue* b0 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b0p), IR_TYPE_U8_T);
                int b0z_r = ir_builder_emit_cmp_eq(b, b0, ir_value_const_int(0, IR_TYPE_U8_T));
                ir_builder_emit_br_cond(b, ir_value_reg(b0z_r, IR_TYPE_I1_T), done, disp);

                // disp: احسب len0 من البايت الأول (UTF-8) ثم وزّع.
                ir_builder_set_insert_point(b, disp);
                ir_lower_set_loc(b, site);
                IRValue* b0u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                               ir_value_reg(ir_builder_emit_cast(b, b0, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                               ir_value_const_int(255, IR_TYPE_I64_T)),
                                            IR_TYPE_I64_T);

                IRValue* m80 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0x80, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* mE0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* mF0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* mF8 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF8, IR_TYPE_I64_T)), IR_TYPE_I64_T);

                IRValue* is1 = ir_value_reg(ir_builder_emit_cmp_eq(b, m80, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
                IRValue* is2 = ir_value_reg(ir_builder_emit_cmp_eq(b, mE0, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
                IRValue* is3 = ir_value_reg(ir_builder_emit_cmp_eq(b, mF0, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
                IRValue* is4 = ir_value_reg(ir_builder_emit_cmp_eq(b, mF8, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I1_T);

                IRValue* l1 = cast_i1_to_i64(ctx, is1);
                IRValue* l2 = cast_i1_to_i64(ctx, is2);
                IRValue* l3 = cast_i1_to_i64(ctx, is3);
                IRValue* l4 = cast_i1_to_i64(ctx, is4);

                IRValue* t2v = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l2, two), IR_TYPE_I64_T);
                IRValue* t3v = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l3, three), IR_TYPE_I64_T);
                IRValue* t4v = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l4, four), IR_TYPE_I64_T);
                IRValue* len0 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T,
                                                                 ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, l1, t2v), IR_TYPE_I64_T),
                                                                 ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, t3v, t4v), IR_TYPE_I64_T)),
                                             IR_TYPE_I64_T);

                ir_builder_emit_store(b, b0u, b0u_ptr);
                ir_builder_emit_store(b, len0, len0_ptr);

                int is1r = ir_builder_emit_cmp_eq(b, len0, one);
                ir_builder_emit_br_cond(b, ir_value_reg(is1r, IR_TYPE_I1_T), c1, d2);

                ir_builder_set_insert_point(b, d2);
                ir_lower_set_loc(b, site);
                int is2r = ir_builder_emit_cmp_eq(b, len0, two);
                ir_builder_emit_br_cond(b, ir_value_reg(is2r, IR_TYPE_I1_T), c2b, d3);

                ir_builder_set_insert_point(b, d3);
                ir_lower_set_loc(b, site);
                int is3r = ir_builder_emit_cmp_eq(b, len0, three);
                ir_builder_emit_br_cond(b, ir_value_reg(is3r, IR_TYPE_I1_T), c3b, d4);

                ir_builder_set_insert_point(b, d4);
                ir_lower_set_loc(b, site);
                int is4r = ir_builder_emit_cmp_eq(b, len0, four);
                ir_builder_emit_br_cond(b, ir_value_reg(is4r, IR_TYPE_I1_T), c4b, inv);

                // حالة1: بايت واحد.
                ir_builder_set_insert_point(b, c1);
                ir_lower_set_loc(b, site);
                IRValue* out_idx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_idx_ptr), IR_TYPE_I64_T);
                IRValue* in_idx1 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, in_idx_ptr), IR_TYPE_I64_T);
                IRValue* b0u1 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, b0u_ptr), IR_TYPE_I64_T);
                IRValue* outp1 = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out_idx), char_ptr_t);
                IRValue* l1sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, one, sh32), IR_TYPE_I64_T);
                IRValue* packed1_i64 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u1, l1sh), IR_TYPE_I64_T);
                IRValue* packed1 = ir_value_reg(ir_builder_emit_cast(b, packed1_i64, IR_TYPE_CHAR_T), IR_TYPE_CHAR_T);
                ir_builder_emit_store(b, packed1, outp1);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out_idx, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_idx1, one), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // حالة2: بايتان (تحقق من استمرار واحد).
                ir_builder_set_insert_point(b, c2b);
                ir_lower_set_loc(b, site);
                IRValue* out2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_idx_ptr), IR_TYPE_I64_T);
                IRValue* in2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, in_idx_ptr), IR_TYPE_I64_T);
                IRValue* b0u2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, b0u_ptr), IR_TYPE_I64_T);
                IRValue* in2_1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in2, one), IR_TYPE_I64_T);
                IRValue* b1p = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in2_1), u8_ptr_t);
                IRValue* b1 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b1p), IR_TYPE_U8_T);
                IRValue* b1u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                ir_value_reg(ir_builder_emit_cast(b, b1, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                ir_value_const_int(255, IR_TYPE_I64_T)),
                                             IR_TYPE_I64_T);
                IRValue* b1m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                int b1ok = ir_builder_emit_cmp_eq(b, b1m, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b1nz = ir_builder_emit_cmp_ne(b, b1u, ir_value_const_int(0, IR_TYPE_I64_T));
                int ok2 = ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1ok, IR_TYPE_I1_T), ir_value_reg(b1nz, IR_TYPE_I1_T));
                ir_builder_emit_br_cond(b, ir_value_reg(ok2, IR_TYPE_I1_T), c2pack, inv);

                ir_builder_set_insert_point(b, c2pack);
                ir_lower_set_loc(b, site);
                IRValue* b1sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u, sh8), IR_TYPE_I64_T);
                IRValue* bytes2 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u2, b1sh), IR_TYPE_I64_T);
                IRValue* l2sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, two, sh32), IR_TYPE_I64_T);
                IRValue* packed2_i64 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes2, l2sh), IR_TYPE_I64_T);
                IRValue* packed2 = ir_value_reg(ir_builder_emit_cast(b, packed2_i64, IR_TYPE_CHAR_T), IR_TYPE_CHAR_T);
                IRValue* outp2 = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out2), char_ptr_t);
                ir_builder_emit_store(b, packed2, outp2);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out2, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in2, two), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // حالة3: ثلاثة بايتات (تحقق من استمرارين).
                ir_builder_set_insert_point(b, c3b);
                ir_lower_set_loc(b, site);
                IRValue* out3 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_idx_ptr), IR_TYPE_I64_T);
                IRValue* in3 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, in_idx_ptr), IR_TYPE_I64_T);
                IRValue* b0u3 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, b0u_ptr), IR_TYPE_I64_T);
                IRValue* in3_1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in3, one), IR_TYPE_I64_T);
                IRValue* in3_2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in3, two), IR_TYPE_I64_T);
                IRValue* b1p3 = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in3_1), u8_ptr_t);
                IRValue* b2p3 = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in3_2), u8_ptr_t);
                IRValue* b1_3 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b1p3), IR_TYPE_U8_T);
                IRValue* b2_3 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b2p3), IR_TYPE_U8_T);
                IRValue* b1u3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                 ir_value_reg(ir_builder_emit_cast(b, b1_3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                 ir_value_const_int(255, IR_TYPE_I64_T)),
                                              IR_TYPE_I64_T);
                IRValue* b2u3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                 ir_value_reg(ir_builder_emit_cast(b, b2_3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                 ir_value_const_int(255, IR_TYPE_I64_T)),
                                              IR_TYPE_I64_T);
                IRValue* b1m3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u3, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* b2m3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2u3, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                int b1ok3 = ir_builder_emit_cmp_eq(b, b1m3, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b2ok3 = ir_builder_emit_cmp_eq(b, b2m3, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b1nz3 = ir_builder_emit_cmp_ne(b, b1u3, ir_value_const_int(0, IR_TYPE_I64_T));
                int b2nz3 = ir_builder_emit_cmp_ne(b, b2u3, ir_value_const_int(0, IR_TYPE_I64_T));
                int ok3 = ir_builder_emit_and(b, IR_TYPE_I1_T,
                                              ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1ok3, IR_TYPE_I1_T), ir_value_reg(b2ok3, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                              ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1nz3, IR_TYPE_I1_T), ir_value_reg(b2nz3, IR_TYPE_I1_T)), IR_TYPE_I1_T));
                ir_builder_emit_br_cond(b, ir_value_reg(ok3, IR_TYPE_I1_T), c3pack, inv);

                ir_builder_set_insert_point(b, c3pack);
                ir_lower_set_loc(b, site);
                IRValue* b1sh3 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u3, sh8), IR_TYPE_I64_T);
                IRValue* b2sh3 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b2u3, sh16), IR_TYPE_I64_T);
                IRValue* bytes3 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                                  ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u3, b1sh3), IR_TYPE_I64_T),
                                                                  b2sh3),
                                               IR_TYPE_I64_T);
                IRValue* l3sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, three, sh32), IR_TYPE_I64_T);
                IRValue* packed3_i64 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes3, l3sh), IR_TYPE_I64_T);
                IRValue* packed3 = ir_value_reg(ir_builder_emit_cast(b, packed3_i64, IR_TYPE_CHAR_T), IR_TYPE_CHAR_T);
                IRValue* outp3 = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out3), char_ptr_t);
                ir_builder_emit_store(b, packed3, outp3);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out3, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in3, three), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // حالة4: أربعة بايتات (تحقق من 3 استمرار).
                ir_builder_set_insert_point(b, c4b);
                ir_lower_set_loc(b, site);
                IRValue* out4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_idx_ptr), IR_TYPE_I64_T);
                IRValue* in4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, in_idx_ptr), IR_TYPE_I64_T);
                IRValue* b0u4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, b0u_ptr), IR_TYPE_I64_T);
                IRValue* in4_1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in4, one), IR_TYPE_I64_T);
                IRValue* in4_2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in4, two), IR_TYPE_I64_T);
                IRValue* in4_3 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in4, three), IR_TYPE_I64_T);
                IRValue* b1p4 = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in4_1), u8_ptr_t);
                IRValue* b2p4 = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in4_2), u8_ptr_t);
                IRValue* b3p4 = ir_value_reg(ir_builder_emit_ptr_offset(b, u8_ptr_t, cstr_u8, in4_3), u8_ptr_t);
                IRValue* b1_4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b1p4), IR_TYPE_U8_T);
                IRValue* b2_4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b2p4), IR_TYPE_U8_T);
                IRValue* b3_4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_U8_T, b3p4), IR_TYPE_U8_T);
                IRValue* b1u4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                 ir_value_reg(ir_builder_emit_cast(b, b1_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                 ir_value_const_int(255, IR_TYPE_I64_T)),
                                              IR_TYPE_I64_T);
                IRValue* b2u4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                 ir_value_reg(ir_builder_emit_cast(b, b2_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                 ir_value_const_int(255, IR_TYPE_I64_T)),
                                              IR_TYPE_I64_T);
                IRValue* b3u4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                                 ir_value_reg(ir_builder_emit_cast(b, b3_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                                 ir_value_const_int(255, IR_TYPE_I64_T)),
                                              IR_TYPE_I64_T);
                IRValue* b1m4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* b2m4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                IRValue* b3m4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b3u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
                int b1ok4 = ir_builder_emit_cmp_eq(b, b1m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b2ok4 = ir_builder_emit_cmp_eq(b, b2m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b3ok4 = ir_builder_emit_cmp_eq(b, b3m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
                int b1nz4 = ir_builder_emit_cmp_ne(b, b1u4, ir_value_const_int(0, IR_TYPE_I64_T));
                int b2nz4 = ir_builder_emit_cmp_ne(b, b2u4, ir_value_const_int(0, IR_TYPE_I64_T));
                int b3nz4 = ir_builder_emit_cmp_ne(b, b3u4, ir_value_const_int(0, IR_TYPE_I64_T));
                int ok4 = ir_builder_emit_and(b, IR_TYPE_I1_T,
                                              ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T,
                                                                              ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1ok4, IR_TYPE_I1_T), ir_value_reg(b2ok4, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                                                              ir_value_reg(b3ok4, IR_TYPE_I1_T)),
                                                           IR_TYPE_I1_T),
                                              ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T,
                                                                              ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1nz4, IR_TYPE_I1_T), ir_value_reg(b2nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                                                              ir_value_reg(b3nz4, IR_TYPE_I1_T)),
                                                           IR_TYPE_I1_T));
                ir_builder_emit_br_cond(b, ir_value_reg(ok4, IR_TYPE_I1_T), c4pack, inv);

                ir_builder_set_insert_point(b, c4pack);
                ir_lower_set_loc(b, site);
                IRValue* b1sh4 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u4, sh8), IR_TYPE_I64_T);
                IRValue* b2sh4 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b2u4, sh16), IR_TYPE_I64_T);
                IRValue* b3sh4 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b3u4, sh24), IR_TYPE_I64_T);
                IRValue* bytes4 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                                  ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                                                                  ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u4, b1sh4), IR_TYPE_I64_T),
                                                                                                  b2sh4),
                                                                               IR_TYPE_I64_T),
                                                                  b3sh4),
                                               IR_TYPE_I64_T);
                IRValue* l4sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, four, sh32), IR_TYPE_I64_T);
                IRValue* packed4_i64 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes4, l4sh), IR_TYPE_I64_T);
                IRValue* packed4 = ir_value_reg(ir_builder_emit_cast(b, packed4_i64, IR_TYPE_CHAR_T), IR_TYPE_CHAR_T);
                IRValue* outp4 = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, out4), char_ptr_t);
                ir_builder_emit_store(b, packed4, outp4);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out4, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in4, four), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // inv: استبدال بـ U+FFFD واستهلاك بايت واحد.
                ir_builder_set_insert_point(b, inv);
                ir_lower_set_loc(b, site);
                IRValue* outx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_idx_ptr), IR_TYPE_I64_T);
                IRValue* inx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, in_idx_ptr), IR_TYPE_I64_T);
                IRValue* outpx = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, outx), char_ptr_t);
                ir_builder_emit_store(b, repl_ch, outpx);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, outx, one), IR_TYPE_I64_T), out_idx_ptr);
                ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, inx, one), IR_TYPE_I64_T), in_idx_ptr);
                ir_builder_emit_br(b, head);

                // done: ضع NULL-terminator واكتب النتيجة.
                ir_builder_set_insert_point(b, done);
                ir_lower_set_loc(b, site);
                IRValue* outd = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_idx_ptr), IR_TYPE_I64_T);
                IRValue* outpend = ir_value_reg(ir_builder_emit_ptr_offset(b, char_ptr_t, dst, outd), char_ptr_t);
                ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_CHAR_T), outpend);
                ir_builder_emit_store(b, dst, res_ptr);
                ir_builder_emit_br(b, b_ret);
            }
        }
    }

    ir_builder_set_insert_point(b, b_ret);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(r, char_ptr_t);
}

static bool ir_lower_emit_main_args_wrapper(IRBuilder* builder,
                                           Node* program_root,
                                           const Node* site,
                                           const BaaTarget* target)
{
    if (!builder || !builder->module) return false;

    IRModule* m = builder->module;
    ir_module_set_current(m);

    // يجب أن تكون دالة المستخدم موجودة قبل إنشاء الغلاف.
    IRFunc* user = ir_module_find_func(m, BAA_WRAPPED_USER_MAIN_NAME);
    if (!user) {
        fprintf(stderr, "خطأ (تحويل IR): لم يتم العثور على '%s' لإنشاء غلاف الرئيسية.\n", BAA_WRAPPED_USER_MAIN_NAME);
        return false;
    }

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_pp_t = ir_type_ptr(i8_ptr_t);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    // إنشاء دالة الغلاف: اسمها في IR هو 'الرئيسية' وسيحوّلها backend إلى 'main'.
    IRFunc* wrap = ir_builder_create_func(builder, BAA_ENTRY_FUNC_NAME, IR_TYPE_I64_T);
    if (!wrap) {
        fprintf(stderr, "خطأ (تحويل IR): فشل إنشاء دالة غلاف '%s'.\n", BAA_ENTRY_FUNC_NAME);
        return false;
    }
    wrap->is_prototype = false;

    // توقيع C ABI: (argc: i64, argv: i8**)
    (void)ir_builder_add_param(builder, "عدد", IR_TYPE_I64_T);
    (void)ir_builder_add_param(builder, "argv", i8_pp_t);

    ir_builder_set_func(builder, wrap);

    IRBlock* entry = ir_builder_create_block(builder, "بداية");
    if (!entry) return false;
    ir_builder_set_insert_point(builder, entry);

    IRLowerCtx ctx;
    ir_lower_ctx_init(&ctx, builder);
    ctx.current_func_name = BAA_ENTRY_FUNC_NAME;
    ctx.program_root = program_root;
    ctx.target = target;

    // القيم القادمة من C runtime
    IRValue* argc_v = ir_value_reg(wrap->params[0].reg, IR_TYPE_I64_T);
    IRValue* argv_v = ir_value_reg(wrap->params[1].reg, i8_pp_t);

    // baa_argv: مصفوفة مؤشرات إلى نص باء (ptr<char>[])
    IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
    IRValue* eight = ir_value_const_int(8, IR_TYPE_I64_T);
    IRValue* argc_p1 = ir_value_reg(ir_builder_emit_add(builder, IR_TYPE_I64_T, argc_v, one), IR_TYPE_I64_T);
    IRValue* alloc_bytes = ir_value_reg(ir_builder_emit_mul(builder, IR_TYPE_I64_T, argc_p1, eight), IR_TYPE_I64_T);

    IRValue* margs[1] = { alloc_bytes };
    int raw_arr_r = ir_builder_emit_call(builder, "malloc", i8_ptr_t, margs, 1);
    if (raw_arr_r < 0) {
        ir_lower_report_error(&ctx, site, "فشل خفض نداء malloc أثناء إنشاء argv لباء.");
        return false;
    }
    IRValue* raw_arr = ir_value_reg(raw_arr_r, i8_ptr_t);
    IRValue* baa_argv = ir_value_reg(ir_builder_emit_cast(builder, raw_arr, char_ptr_ptr_t), char_ptr_ptr_t);

    // i = 0
    IRValue* i_ptr = ir_value_reg(ir_builder_emit_alloca(builder, IR_TYPE_I64_T), ir_type_ptr(IR_TYPE_I64_T));
    ir_builder_emit_store(builder, ir_value_const_int(0, IR_TYPE_I64_T), i_ptr);

    IRBlock* head = cf_create_block(&ctx, "غلاف_الرئيسية_تحقق");
    IRBlock* body = cf_create_block(&ctx, "غلاف_الرئيسية_جسم");
    IRBlock* done = cf_create_block(&ctx, "غلاف_الرئيسية_نهاية");
    if (!head || !body || !done) return false;

    ir_builder_emit_br(builder, head);

    // head: i < argc ?
    ir_builder_set_insert_point(builder, head);
    ir_lower_set_loc(builder, site);
    IRValue* i_v = ir_value_reg(ir_builder_emit_load(builder, IR_TYPE_I64_T, i_ptr), IR_TYPE_I64_T);
    int lt_r = ir_builder_emit_cmp_lt(builder, i_v, argc_v);
    ir_builder_emit_br_cond(builder, ir_value_reg(lt_r, IR_TYPE_I1_T), body, done);

    // body: cstr = argv[i] -> baa_str -> baa_argv[i]
    ir_builder_set_insert_point(builder, body);
    ir_lower_set_loc(builder, site);
    IRValue* argv_i_addr = ir_value_reg(ir_builder_emit_ptr_offset(builder, i8_pp_t, argv_v, i_v), i8_pp_t);
    IRValue* cstr = ir_value_reg(ir_builder_emit_load(builder, i8_ptr_t, argv_i_addr), i8_ptr_t);
    IRValue* baa_str = ir_lower_cstr_to_baa_string_alloc(&ctx, site, cstr);

    IRValue* baa_slot = ir_value_reg(ir_builder_emit_ptr_offset(builder, char_ptr_ptr_t, baa_argv, i_v), char_ptr_ptr_t);
    ir_builder_emit_store(builder, baa_str, baa_slot);

    IRValue* i_next = ir_value_reg(ir_builder_emit_add(builder, IR_TYPE_I64_T, i_v, one), IR_TYPE_I64_T);
    ir_builder_emit_store(builder, i_next, i_ptr);
    ir_builder_emit_br(builder, head);

    // done: baa_argv[argc] = NULL; call user main; return
    ir_builder_set_insert_point(builder, done);
    ir_lower_set_loc(builder, site);
    IRValue* end_slot = ir_value_reg(ir_builder_emit_ptr_offset(builder, char_ptr_ptr_t, baa_argv, argc_v), char_ptr_ptr_t);
    ir_builder_emit_store(builder, ir_value_const_int(0, char_ptr_t), end_slot);

    IRValue* baa_argv_void = ir_value_reg(ir_builder_emit_cast(builder, baa_argv, i8_ptr_t), i8_ptr_t);
    IRValue* uargs[2] = { argc_v, baa_argv_void };
    int rv = ir_builder_emit_call(builder, BAA_WRAPPED_USER_MAIN_NAME, IR_TYPE_I64_T, uargs, 2);
    if (rv < 0) {
        ir_lower_report_error(&ctx, site, "فشل خفض نداء '%s' من غلاف الرئيسية.", BAA_WRAPPED_USER_MAIN_NAME);
        ir_builder_emit_ret_int(builder, 0);
        return false;
    }

    ir_builder_emit_ret(builder, ir_value_reg(rv, IR_TYPE_I64_T));
    return ctx.had_error ? false : true;
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
    ctx->current_func_is_variadic = false;
    ctx->current_func_fixed_params = 0;
    ctx->current_func_va_base_reg = -1;
    ctx->enable_bounds_checks = false;
    ctx->program_root = NULL;
    ctx->target = NULL;
}

void ir_lower_bind_local(IRLowerCtx* ctx, const char* name, int ptr_reg, IRType* value_type,
                         DataType ptr_base_type, const char* ptr_base_type_name, int ptr_depth) {
    if (!ctx || !name) return;
    bool is_ptr = (ptr_depth > 0);

    // ابحث فقط داخل النطاق الحالي: إن وجد، حدّث الربط.
    int start = ir_lower_current_scope_start(ctx);
    for (int i = ctx->local_count - 1; i >= start; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].ptr_reg = ptr_reg;
            ctx->locals[i].value_type = value_type;
            ctx->locals[i].global_name = NULL;
            ctx->locals[i].is_static_storage = false;
            ctx->locals[i].is_pointer = is_ptr;
            ctx->locals[i].ptr_base_type = is_ptr ? ptr_base_type : TYPE_INT;
            ctx->locals[i].ptr_base_type_name = is_ptr ? ptr_base_type_name : NULL;
            ctx->locals[i].ptr_depth = is_ptr ? ptr_depth : 0;
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
    ctx->locals[ctx->local_count].is_pointer = is_ptr;
    ctx->locals[ctx->local_count].ptr_base_type = is_ptr ? ptr_base_type : TYPE_INT;
    ctx->locals[ctx->local_count].ptr_base_type_name = is_ptr ? ptr_base_type_name : NULL;
    ctx->locals[ctx->local_count].ptr_depth = is_ptr ? ptr_depth : 0;
    ctx->locals[ctx->local_count].is_array = false;
    ctx->locals[ctx->local_count].array_rank = 0;
    ctx->locals[ctx->local_count].array_dims = NULL;
    ctx->locals[ctx->local_count].array_elem_type = TYPE_INT;
    ctx->locals[ctx->local_count].array_elem_type_name = NULL;
    ctx->local_count++;
}

void ir_lower_bind_local_static(IRLowerCtx* ctx, const char* name,
                                const char* global_name, IRType* value_type,
                                DataType ptr_base_type, const char* ptr_base_type_name, int ptr_depth) {
    if (!ctx || !name || !global_name) return;
    bool is_ptr = (ptr_depth > 0);

    int start = ir_lower_current_scope_start(ctx);
    for (int i = ctx->local_count - 1; i >= start; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].ptr_reg = -1;
            ctx->locals[i].value_type = value_type;
            ctx->locals[i].global_name = global_name;
            ctx->locals[i].is_static_storage = true;
            ctx->locals[i].is_pointer = is_ptr;
            ctx->locals[i].ptr_base_type = is_ptr ? ptr_base_type : TYPE_INT;
            ctx->locals[i].ptr_base_type_name = is_ptr ? ptr_base_type_name : NULL;
            ctx->locals[i].ptr_depth = is_ptr ? ptr_depth : 0;
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
    ctx->locals[ctx->local_count].is_pointer = is_ptr;
    ctx->locals[ctx->local_count].ptr_base_type = is_ptr ? ptr_base_type : TYPE_INT;
    ctx->locals[ctx->local_count].ptr_base_type_name = is_ptr ? ptr_base_type_name : NULL;
    ctx->locals[ctx->local_count].ptr_depth = is_ptr ? ptr_depth : 0;
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
        case TYPE_FUNC_PTR: return ir_type_func(IR_TYPE_VOID_T, NULL, 0);
        case TYPE_CHAR:   return IR_TYPE_CHAR_T;
        case TYPE_FLOAT:  return IR_TYPE_F64_T;
        case TYPE_VOID:   return IR_TYPE_VOID_T;
        case TYPE_ENUM:   return IR_TYPE_I64_T;
        case TYPE_INT:
        default:          return IR_TYPE_I64_T;
    }
}

static IRType* ir_type_from_funcsig(IRModule* module, const FuncPtrSig* sig)
{
    if (!sig) {
        return ir_type_func(IR_TYPE_VOID_T, NULL, 0);
    }

    IRType* ret_t = ir_type_from_datatype(module, sig->return_type);
    if (!ret_t) ret_t = IR_TYPE_VOID_T;

    int pc = sig->param_count;
    if (pc < 0) pc = 0;

    IRType* params_local[64];
    IRType** params = NULL;
    if (pc > 0) {
        if (pc > (int)(sizeof(params_local) / sizeof(params_local[0]))) {
            pc = (int)(sizeof(params_local) / sizeof(params_local[0]));
        }
        for (int i = 0; i < pc; i++) {
            DataType pt = sig->param_types ? sig->param_types[i] : TYPE_INT;
            // لا ندعم حالياً أنواع مؤشر دالة داخل توقيع مؤشر دالة.
            if (pt == TYPE_FUNC_PTR) {
                params_local[i] = ir_type_func(IR_TYPE_VOID_T, NULL, 0);
            } else {
                params_local[i] = ir_type_from_datatype(module, pt);
            }
            if (!params_local[i]) params_local[i] = IR_TYPE_I64_T;
        }
        params = params_local;
    }

    return ir_type_func(ret_t, params, pc);
}

static IRType* ir_type_from_datatype_ex(IRModule* module, DataType t, const FuncPtrSig* sig)
{
    if (t == TYPE_FUNC_PTR) {
        return ir_type_from_funcsig(module, sig);
    }
    return ir_type_from_datatype(module, t);
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

static Node* ir_lower_find_global_var_decl(IRLowerCtx* ctx, const char* name)
{
    if (!ctx || !ctx->program_root || !name) return NULL;
    if (ctx->program_root->type != NODE_PROGRAM) return NULL;

    for (Node* d = ctx->program_root->data.program.declarations; d; d = d->next) {
        if (d->type != NODE_VAR_DECL) continue;
        if (!d->data.var_decl.is_global) continue;
        if (!d->data.var_decl.name) continue;
        if (strcmp(d->data.var_decl.name, name) == 0) {
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

static IRValue* ir_lower_char_bits_to_i64(IRLowerCtx* ctx, IRValue* ch)
{
    if (!ctx || !ctx->builder || !ch) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (ch->type && ch->type->kind == IR_TYPE_I64) return ch;
    // ملاحظة: هذا تحويل "بتّي" للحصول على الحقول المعبأة (bytes + len) بدون فك ترميز الحرف.
    int r = ir_builder_emit_cast(ctx->builder, ch, IR_TYPE_I64_T);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_i64_bits_to_char(IRLowerCtx* ctx, IRValue* bits)
{
    if (!ctx || !ctx->builder || !bits) return ir_value_const_int(0, IR_TYPE_CHAR_T);
    if (bits->type && bits->type->kind == IR_TYPE_CHAR) return bits;
    int r = ir_builder_emit_cast(ctx->builder, bits, IR_TYPE_CHAR_T);
    return ir_value_reg(r, IR_TYPE_CHAR_T);
}

/**
 * @brief تحويل نص باء (حرف[]) إلى C-string (char*) UTF-8 مخصص على heap.
 *
 * ملاحظة: إن كان النص NULL (عدم)، يعيد NULL.
 * الملكية: يجب تحرير النتيجة عبر free/تحرير_ذاكرة.
 */
static IRValue* ir_lower_baa_string_to_cstr_alloc(IRLowerCtx* ctx, const Node* site, IRValue* str_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

    IRValue* base = ir_lower_as_char_ptr(ctx, str_in);
    if (!base) base = ir_value_const_int(0, char_ptr_t);

    int res_ptr_reg = ir_builder_emit_alloca(b, i8_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, ir_type_ptr(i8_ptr_t));
    ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), res_ptr);

    IRValue* base_i8 = cast_to(ctx, base, i8_ptr_t);
    int is_null_r = ir_builder_emit_cmp_eq(b, base_i8, ir_value_const_int(0, i8_ptr_t));
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* nullb = cf_create_block(ctx, "نص_إلى_سي_فارغ");
    IRBlock* notnull = cf_create_block(ctx, "نص_إلى_سي_غير_فارغ");
    IRBlock* final_done = cf_create_block(ctx, "نص_إلى_سي_نهاية");
    if (!nullb || !notnull || !final_done) {
        return ir_value_const_int(0, i8_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, nullb, notnull);
    }

    ir_builder_set_insert_point(b, nullb);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), res_ptr);
    ir_builder_emit_br(b, final_done);

    ir_builder_set_insert_point(b, notnull);
    ir_lower_set_loc(b, site);

    // 1) حساب طول البايتات
    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int blen_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
    IRValue* blen_ptr = ir_value_reg(blen_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), blen_ptr);

    IRBlock* count_head = cf_create_block(ctx, "نص_إلى_سي_عد_تحقق");
    IRBlock* count_step = cf_create_block(ctx, "نص_إلى_سي_عد_خطوة");
    IRBlock* count_done = cf_create_block(ctx, "نص_إلى_سي_عد_نهاية");
    if (!count_head || !count_step || !count_done) {
        ir_builder_emit_br(b, final_done);
    } else {
        ir_builder_emit_br(b, count_head);
    }

    ir_builder_set_insert_point(b, count_head);
    ir_lower_set_loc(b, site);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);
    int ep = ir_builder_emit_ptr_offset(b, char_ptr_t, base, idx);
    IRValue* elem_ptr = ir_value_reg(ep, char_ptr_t);
    int ch_r = ir_builder_emit_load(b, IR_TYPE_CHAR_T, elem_ptr);
    IRValue* ch = ir_value_reg(ch_r, IR_TYPE_CHAR_T);
    IRValue* ch_bits = ir_lower_char_bits_to_i64(ctx, ch);
    int end_r = ir_builder_emit_cmp_eq(b, ch_bits, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(end_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, count_done, count_step);

    ir_builder_set_insert_point(b, count_step);
    ir_lower_set_loc(b, site);
    IRValue* len64 = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, ch_bits,
                                                      ir_value_const_int(32, IR_TYPE_I64_T)),
                                  IR_TYPE_I64_T);
    int blen_r = ir_builder_emit_load(b, IR_TYPE_I64_T, blen_ptr);
    IRValue* blen = ir_value_reg(blen_r, IR_TYPE_I64_T);
    int blen2_r = ir_builder_emit_add(b, IR_TYPE_I64_T, blen, len64);
    IRValue* blen2 = ir_value_reg(blen2_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, blen2, blen_ptr);
    int idx2_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* idx2 = ir_value_reg(idx2_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, idx2, idx_ptr);
    ir_builder_emit_br(b, count_head);

    // 2) تخصيص وملء المخزن
    ir_builder_set_insert_point(b, count_done);
    ir_lower_set_loc(b, site);
    int blen3_r = ir_builder_emit_load(b, IR_TYPE_I64_T, blen_ptr);
    IRValue* blen3 = ir_value_reg(blen3_r, IR_TYPE_I64_T);
    int alloc_n_r = ir_builder_emit_add(b, IR_TYPE_I64_T, blen3, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* alloc_n = ir_value_reg(alloc_n_r, IR_TYPE_I64_T);
    IRValue* margs[1] = { alloc_n };
    int buf_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
    if (buf_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc أثناء تحويل نص إلى C-string.");
        ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), res_ptr);
        ir_builder_emit_br(b, final_done);
        ir_builder_set_insert_point(b, final_done);
        ir_lower_set_loc(b, site);
        int r_fail = ir_builder_emit_load(b, i8_ptr_t, res_ptr);
        return ir_value_reg(r_fail, i8_ptr_t);
    }
    IRValue* buf = ir_value_reg(buf_r, i8_ptr_t);

    int fidx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int out_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* fidx_ptr = ir_value_reg(fidx_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
    IRValue* out_ptr = ir_value_reg(out_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), fidx_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), out_ptr);

    IRBlock* fill_head = cf_create_block(ctx, "نص_إلى_سي_ملء_تحقق");
    IRBlock* fill_body = cf_create_block(ctx, "نص_إلى_سي_ملء_جسم");
    IRBlock* fill_done = cf_create_block(ctx, "نص_إلى_سي_ملء_نهاية");
    if (!fill_head || !fill_body || !fill_done) {
        ir_builder_emit_store(b, buf, res_ptr);
        ir_builder_emit_br(b, final_done);
    } else {
        ir_builder_emit_br(b, fill_head);
    }

    ir_builder_set_insert_point(b, fill_head);
    ir_lower_set_loc(b, site);
    int fidx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, fidx_ptr);
    IRValue* fidx = ir_value_reg(fidx_r, IR_TYPE_I64_T);
    int ep2 = ir_builder_emit_ptr_offset(b, char_ptr_t, base, fidx);
    IRValue* elem_ptr2 = ir_value_reg(ep2, char_ptr_t);
    int ch2_r = ir_builder_emit_load(b, IR_TYPE_CHAR_T, elem_ptr2);
    IRValue* ch2 = ir_value_reg(ch2_r, IR_TYPE_CHAR_T);
    IRValue* ch2_bits = ir_lower_char_bits_to_i64(ctx, ch2);
    int end2_r = ir_builder_emit_cmp_eq(b, ch2_bits, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* end2 = ir_value_reg(end2_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, end2, fill_done, fill_body);

    ir_builder_set_insert_point(b, fill_body);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr);
    IRValue* out = ir_value_reg(out_r, IR_TYPE_I64_T);

    IRValue* len2 = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, ch2_bits,
                                                     ir_value_const_int(32, IR_TYPE_I64_T)),
                                 IR_TYPE_I64_T);
    IRValue* bytes_field = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, ch2_bits,
                                                           ir_value_const_int(4294967295LL, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);

    IRValue* b0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, bytes_field,
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b1s = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, bytes_field,
                                                    ir_value_const_int(8, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b1 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1s,
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b2s = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, bytes_field,
                                                    ir_value_const_int(16, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b2 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2s,
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b3s = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, bytes_field,
                                                    ir_value_const_int(24, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b3s,
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);

    // اكتب b0 دائماً
    int dp0 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, out);
    IRValue* p0 = ir_value_reg(dp0, i8_ptr_t);
    IRValue* b0_i8 = cast_to(ctx, b0, IR_TYPE_I8_T);
    ir_builder_emit_store(b, b0_i8, p0);
    IRValue* out1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out,
                                                     ir_value_const_int(1, IR_TYPE_I64_T)),
                                 IR_TYPE_I64_T);
    ir_builder_emit_store(b, out1, out_ptr);

    IRBlock* b1b = cf_create_block(ctx, "نص_إلى_سي_ملء_b1");
    IRBlock* a1b = cf_create_block(ctx, "نص_إلى_سي_ملء_بعد_b1");
    IRBlock* b2b = cf_create_block(ctx, "نص_إلى_سي_ملء_b2");
    IRBlock* a2b = cf_create_block(ctx, "نص_إلى_سي_ملء_بعد_b2");
    IRBlock* b3b = cf_create_block(ctx, "نص_إلى_سي_ملء_b3");
    IRBlock* a3b = cf_create_block(ctx, "نص_إلى_سي_ملء_بعد_b3");
    if (!b1b || !a1b || !b2b || !a2b || !b3b || !a3b) {
        // في حالة فشل إنشاء الكتل، نكمل بشكل محافظ.
        int fidx2_r = ir_builder_emit_add(b, IR_TYPE_I64_T, fidx, ir_value_const_int(1, IR_TYPE_I64_T));
        ir_builder_emit_store(b, ir_value_reg(fidx2_r, IR_TYPE_I64_T), fidx_ptr);
        ir_builder_emit_br(b, fill_head);
    } else {
        int c2_r = ir_builder_emit_cmp_ge(b, len2, ir_value_const_int(2, IR_TYPE_I64_T));
        ir_builder_emit_br_cond(b, ir_value_reg(c2_r, IR_TYPE_I1_T), b1b, a1b);

        ir_builder_set_insert_point(b, b1b);
        ir_lower_set_loc(b, site);
        int out1r = ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr);
        IRValue* out1v = ir_value_reg(out1r, IR_TYPE_I64_T);
        int dp1 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, out1v);
        IRValue* p1 = ir_value_reg(dp1, i8_ptr_t);
        ir_builder_emit_store(b, cast_to(ctx, b1, IR_TYPE_I8_T), p1);
        IRValue* out2v = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out1v,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
        ir_builder_emit_store(b, out2v, out_ptr);
        ir_builder_emit_br(b, a1b);

        ir_builder_set_insert_point(b, a1b);
        ir_lower_set_loc(b, site);
        int c3_r = ir_builder_emit_cmp_ge(b, len2, ir_value_const_int(3, IR_TYPE_I64_T));
        ir_builder_emit_br_cond(b, ir_value_reg(c3_r, IR_TYPE_I1_T), b2b, a2b);

        ir_builder_set_insert_point(b, b2b);
        ir_lower_set_loc(b, site);
        int out2r = ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr);
        IRValue* out2 = ir_value_reg(out2r, IR_TYPE_I64_T);
        int dp2 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, out2);
        IRValue* p2 = ir_value_reg(dp2, i8_ptr_t);
        ir_builder_emit_store(b, cast_to(ctx, b2, IR_TYPE_I8_T), p2);
        IRValue* out3v = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out2,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
        ir_builder_emit_store(b, out3v, out_ptr);
        ir_builder_emit_br(b, a2b);

        ir_builder_set_insert_point(b, a2b);
        ir_lower_set_loc(b, site);
        int c4_r = ir_builder_emit_cmp_ge(b, len2, ir_value_const_int(4, IR_TYPE_I64_T));
        ir_builder_emit_br_cond(b, ir_value_reg(c4_r, IR_TYPE_I1_T), b3b, a3b);

        ir_builder_set_insert_point(b, b3b);
        ir_lower_set_loc(b, site);
        int out3r = ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr);
        IRValue* out3 = ir_value_reg(out3r, IR_TYPE_I64_T);
        int dp3 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, out3);
        IRValue* p3 = ir_value_reg(dp3, i8_ptr_t);
        ir_builder_emit_store(b, cast_to(ctx, b3, IR_TYPE_I8_T), p3);
        IRValue* out4v = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out3,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
        ir_builder_emit_store(b, out4v, out_ptr);
        ir_builder_emit_br(b, a3b);

        ir_builder_set_insert_point(b, a3b);
        ir_lower_set_loc(b, site);
        // التالي
        IRValue* fidx2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, fidx,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
        ir_builder_emit_store(b, fidx2, fidx_ptr);
        ir_builder_emit_br(b, fill_head);
    }

    ir_builder_set_insert_point(b, fill_done);
    ir_lower_set_loc(b, site);
    // ضع NULL-terminator
    int dpn = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, blen3);
    IRValue* pn = ir_value_reg(dpn, i8_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), pn);
    ir_builder_emit_store(b, buf, res_ptr);
    ir_builder_emit_br(b, final_done);

    ir_builder_set_insert_point(b, final_done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, i8_ptr_t, res_ptr);
    return ir_value_reg(r, i8_ptr_t);
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

static IRValue* ir_lower_builtin_read_line_from_file(IRLowerCtx* ctx, const Node* site, IRValue* file_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, get_char_ptr_type(NULL));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* i32_ptr_t = ir_type_ptr(IR_TYPE_I32_T);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);

    IRValue* nulls = ir_value_const_int(0, char_ptr_t);
    IRValue* file = cast_to(ctx, file_in, i8_ptr_t);

    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    ir_builder_emit_store(b, nulls, res_ptr);

    int buf_ptr_reg = ir_builder_emit_alloca(b, i8_ptr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, i8_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), buf_ptr);

    int len_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int cap_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* len_ptr = ir_value_reg(len_ptr_reg, i64_ptr_t);
    IRValue* cap_ptr = ir_value_reg(cap_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), len_ptr);
    ir_builder_emit_store(b, ir_value_const_int(256, IR_TYPE_I64_T), cap_ptr);

    IRValue* cap0 = ir_value_const_int(256, IR_TYPE_I64_T);
    IRValue* margs0[1] = { cap0 };
    ir_lower_set_loc(b, site);
    int buf0_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs0, 1);
    if (buf0_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc في اقرأ_سطر.");
        return nulls;
    }
    IRValue* buf0 = ir_value_reg(buf0_r, i8_ptr_t);
    ir_builder_emit_store(b, buf0, buf_ptr);

    int last_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I32_T);
    IRValue* last_ptr = ir_value_reg(last_ptr_reg, i32_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I32_T), last_ptr);

    IRBlock* head = cf_create_block(ctx, "اقرأ_سطر_تحقق");
    IRBlock* body = cf_create_block(ctx, "اقرأ_سطر_جسم");
    IRBlock* grow = cf_create_block(ctx, "اقرأ_سطر_توسيع");
    IRBlock* after_grow = cf_create_block(ctx, "اقرأ_سطر_بعد_توسيع");
    IRBlock* done_read = cf_create_block(ctx, "اقرأ_سطر_بعد_قراءة");
    IRBlock* eofret = cf_create_block(ctx, "اقرأ_سطر_EOF");
    IRBlock* parse = cf_create_block(ctx, "اقرأ_سطر_تحويل");
    IRBlock* final_done = cf_create_block(ctx, "اقرأ_سطر_نهاية");
    if (!head || !body || !grow || !after_grow || !done_read || !eofret || !parse || !final_done) {
        return nulls;
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    IRValue* fargs[1] = { file };
    int ch_r = ir_builder_emit_call(b, "fgetc", IR_TYPE_I32_T, fargs, 1);
    IRValue* ch = ir_value_reg(ch_r, IR_TYPE_I32_T);
    ir_builder_emit_store(b, ch, last_ptr);
    int is_eof_r = ir_builder_emit_cmp_eq(b, ch, ir_value_const_int(-1, IR_TYPE_I32_T));
    int is_nl_r = ir_builder_emit_cmp_eq(b, ch, ir_value_const_int(10, IR_TYPE_I32_T));
    IRValue* stop = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I1_T,
                                                   ir_value_reg(is_eof_r, IR_TYPE_I1_T),
                                                   ir_value_reg(is_nl_r, IR_TYPE_I1_T)),
                                 IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, stop, done_read, body);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    IRValue* lenv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* capv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, cap_ptr), IR_TYPE_I64_T);
    IRValue* len1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, lenv, ir_value_const_int(1, IR_TYPE_I64_T)),
                                 IR_TYPE_I64_T);
    IRValue* need_grow = ir_value_reg(ir_builder_emit_cmp_ge(b, len1, capv), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, need_grow, grow, after_grow);

    ir_builder_set_insert_point(b, grow);
    ir_lower_set_loc(b, site);
    IRValue* cap2 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, capv, ir_value_const_int(2, IR_TYPE_I64_T)),
                                 IR_TYPE_I64_T);
    IRValue* old_buf = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* rargs[2] = { old_buf, cap2 };
    int new_buf_r = ir_builder_emit_call(b, "realloc", i8_ptr_t, rargs, 2);
    if (new_buf_r < 0) {
        IRValue* f0[1] = { old_buf };
        ir_builder_emit_call_void(b, "free", f0, 1);
        ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), buf_ptr);
        ir_builder_emit_store(b, ir_value_const_int(-1, IR_TYPE_I32_T), last_ptr);
        ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), len_ptr);
        ir_builder_emit_br(b, done_read);
    }
    ir_builder_emit_store(b, ir_value_reg(new_buf_r, i8_ptr_t), buf_ptr);
    ir_builder_emit_store(b, cap2, cap_ptr);
    ir_builder_emit_br(b, after_grow);

    ir_builder_set_insert_point(b, after_grow);
    ir_lower_set_loc(b, site);
    IRValue* buf = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    int dstp = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, lenv);
    ir_builder_emit_store(b, cast_to(ctx, ch, IR_TYPE_I8_T), ir_value_reg(dstp, i8_ptr_t));
    ir_builder_emit_store(b, len1, len_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done_read);
    ir_lower_set_loc(b, site);
    IRValue* lastv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I32_T, last_ptr), IR_TYPE_I32_T);
    IRValue* len2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* eof_empty = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T,
                                                        ir_value_reg(ir_builder_emit_cmp_eq(b, lastv, ir_value_const_int(-1, IR_TYPE_I32_T)), IR_TYPE_I1_T),
                                                        ir_value_reg(ir_builder_emit_cmp_eq(b, len2, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T)),
                                      IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, eof_empty, eofret, parse);

    ir_builder_set_insert_point(b, eofret);
    ir_lower_set_loc(b, site);
    IRValue* bf = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* fbf[1] = { bf };
    ir_builder_emit_call_void(b, "free", fbf, 1);
    ir_builder_emit_store(b, nulls, res_ptr);
    ir_builder_emit_br(b, final_done);

    ir_builder_set_insert_point(b, parse);
    ir_lower_set_loc(b, site);
    IRValue* buf2 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    int nul_p = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, len2);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), ir_value_reg(nul_p, i8_ptr_t));

    IRValue* chars_count = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, len2, ir_value_const_int(1, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
    IRValue* chars_bytes = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, chars_count, ir_value_const_int(8, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
    IRValue* cargs[1] = { chars_bytes };
    int raw_chars_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, cargs, 1);
    if (raw_chars_r < 0) {
        IRValue* f1[1] = { buf2 };
        ir_builder_emit_call_void(b, "free", f1, 1);
        ir_builder_emit_store(b, nulls, res_ptr);
        ir_builder_emit_br(b, final_done);
    }
    IRValue* chars = cast_to(ctx, ir_value_reg(raw_chars_r, i8_ptr_t), char_ptr_t);

    int i_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int j_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int bits_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* i_ptr = ir_value_reg(i_ptr_reg, i64_ptr_t);
    IRValue* j_ptr = ir_value_reg(j_ptr_reg, i64_ptr_t);
    IRValue* bits_ptr = ir_value_reg(bits_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), i_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), j_ptr);

    uint64_t repl_bits_u = pack_utf8_codepoint(0xFFFDu);
    IRValue* repl_bits = ir_value_const_int((int64_t)repl_bits_u, IR_TYPE_I64_T);

    int next_i_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* next_i_ptr = ir_value_reg(next_i_ptr_reg, i64_ptr_t);

    IRBlock* p_head = cf_create_block(ctx, "اقرأ_سطر_تفكيك_تحقق");
    IRBlock* p_disp = cf_create_block(ctx, "اقرأ_سطر_تفكيك_توزيع");
    IRBlock* p_d2 = cf_create_block(ctx, "اقرأ_سطر_تفكيك_د2");
    IRBlock* p_d3 = cf_create_block(ctx, "اقرأ_سطر_تفكيك_د3");
    IRBlock* p_d4 = cf_create_block(ctx, "اقرأ_سطر_تفكيك_د4");
    IRBlock* p_d5 = cf_create_block(ctx, "اقرأ_سطر_تفكيك_د5");
    IRBlock* c1 = cf_create_block(ctx, "اقرأ_سطر_حالة1");
    IRBlock* c2 = cf_create_block(ctx, "اقرأ_سطر_حالة2");
    IRBlock* c3 = cf_create_block(ctx, "اقرأ_سطر_حالة3");
    IRBlock* c4 = cf_create_block(ctx, "اقرأ_سطر_حالة4");
    IRBlock* inv = cf_create_block(ctx, "اقرأ_سطر_غير_صالح");
    IRBlock* p_store = cf_create_block(ctx, "اقرأ_سطر_تخزين");
    IRBlock* p_done = cf_create_block(ctx, "اقرأ_سطر_تفكيك_نهاية");
    if (!p_head || !p_disp || !p_d2 || !p_d3 || !p_d4 || !p_d5 ||
        !c1 || !c2 || !c3 || !c4 || !inv || !p_store || !p_done) {
        IRValue* f2[1] = { buf2 };
        ir_builder_emit_call_void(b, "free", f2, 1);
        ir_builder_emit_store(b, chars, res_ptr);
        ir_builder_emit_br(b, final_done);
    }

    ir_builder_emit_br(b, p_head);

    ir_builder_set_insert_point(b, p_head);
    ir_lower_set_loc(b, site);
    IRValue* i = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, i_ptr), IR_TYPE_I64_T);
    IRValue* cont = ir_value_reg(ir_builder_emit_cmp_lt(b, i, len2), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cont, p_disp, p_done);

    ir_builder_set_insert_point(b, p_disp);
    ir_lower_set_loc(b, site);
    int bp0 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i);
    IRValue* b0s = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp0, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b0u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                   ir_value_reg(ir_builder_emit_cast(b, b0s, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);

    // len0 = 1/2/3/4 حسب البايت الأول (UTF-8)
    IRValue* m80 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0x80, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* mE0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* mF0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* mF8 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* is1 = ir_value_reg(ir_builder_emit_cmp_eq(b, m80, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* is2 = ir_value_reg(ir_builder_emit_cmp_eq(b, mE0, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* is3 = ir_value_reg(ir_builder_emit_cmp_eq(b, mF0, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* is4 = ir_value_reg(ir_builder_emit_cmp_eq(b, mF8, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* l1 = cast_i1_to_i64(ctx, is1);
    IRValue* l2 = cast_i1_to_i64(ctx, is2);
    IRValue* l3 = cast_i1_to_i64(ctx, is3);
    IRValue* l4 = cast_i1_to_i64(ctx, is4);
    IRValue* t2 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l2, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* t3 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l3, ir_value_const_int(3, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* t4 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l4, ir_value_const_int(4, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* len0 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T,
                                                    ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, l1, t2), IR_TYPE_I64_T),
                                                    ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, t3, t4), IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);

    int b0z = ir_builder_emit_cmp_eq(b, b0u, ir_value_const_int(0, IR_TYPE_I64_T));
    int oklead = ir_builder_emit_and(b, IR_TYPE_I1_T,
                                    ir_value_reg(ir_builder_emit_not(b, IR_TYPE_I1_T, ir_value_reg(b0z, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                    ir_value_reg(ir_builder_emit_cmp_gt(b, len0, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T));
    ir_builder_emit_br_cond(b, ir_value_reg(oklead, IR_TYPE_I1_T), p_d2, inv);

    ir_builder_set_insert_point(b, p_d2);
    ir_lower_set_loc(b, site);
    int is1r = ir_builder_emit_cmp_eq(b, len0, ir_value_const_int(1, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(is1r, IR_TYPE_I1_T), c1, p_d3);

    ir_builder_set_insert_point(b, p_d3);
    ir_lower_set_loc(b, site);
    int is2r = ir_builder_emit_cmp_eq(b, len0, ir_value_const_int(2, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(is2r, IR_TYPE_I1_T), c2, p_d4);

    ir_builder_set_insert_point(b, p_d4);
    ir_lower_set_loc(b, site);
    int is3r = ir_builder_emit_cmp_eq(b, len0, ir_value_const_int(3, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(is3r, IR_TYPE_I1_T), c3, p_d5);

    ir_builder_set_insert_point(b, p_d5);
    ir_lower_set_loc(b, site);
    int is4r = ir_builder_emit_cmp_eq(b, len0, ir_value_const_int(4, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(is4r, IR_TYPE_I1_T), c4, inv);

    // حالة 1: بايت واحد
    ir_builder_set_insert_point(b, c1);
    ir_lower_set_loc(b, site);
    IRValue* packed1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, b0u, ir_value_const_int(4294967296LL, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    ir_builder_emit_store(b, packed1, bits_ptr);
    ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T), next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // helpers لقراءة بايت + تحويله لـ i64 (0..255)
    // حالة2
    ir_builder_set_insert_point(b, c2);
    ir_lower_set_loc(b, site);
    IRValue* i2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int in2 = ir_builder_emit_cmp_le(b, i2, len2);
    IRBlock* c2ok = cf_create_block(ctx, "اقرأ_سطر_حالة2_ok");
    if (!c2ok) c2ok = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(in2, IR_TYPE_I1_T), c2ok, inv);
    ir_builder_set_insert_point(b, c2ok);
    ir_lower_set_loc(b, site);
    IRValue* i1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp1 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i1);
    IRValue* b1s = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp1, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b1u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                   ir_value_reg(ir_builder_emit_cast(b, b1s, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b1m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b1ok = ir_builder_emit_cmp_eq(b, b1m, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b1nz = ir_builder_emit_cmp_ne(b, b1u, ir_value_const_int(0, IR_TYPE_I64_T));
    int ok2 = ir_builder_emit_and(b, IR_TYPE_I1_T,
                                 ir_value_reg(b1ok, IR_TYPE_I1_T),
                                 ir_value_reg(b1nz, IR_TYPE_I1_T));
    IRBlock* c2pack = cf_create_block(ctx, "اقرأ_سطر_حالة2_حزم");
    if (!c2pack) c2pack = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(ok2, IR_TYPE_I1_T), c2pack, inv);
    ir_builder_set_insert_point(b, c2pack);
    ir_lower_set_loc(b, site);
    IRValue* b1sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* bytes2 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u, b1sh), IR_TYPE_I64_T);
    IRValue* l2sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, ir_value_const_int(2, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* packed2 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes2, l2sh), IR_TYPE_I64_T);
    ir_builder_emit_store(b, packed2, bits_ptr);
    ir_builder_emit_store(b, i2, next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // حالة3
    ir_builder_set_insert_point(b, c3);
    ir_lower_set_loc(b, site);
    IRValue* i3 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(3, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int in3 = ir_builder_emit_cmp_le(b, i3, len2);
    IRBlock* c3ok = cf_create_block(ctx, "اقرأ_سطر_حالة3_ok");
    if (!c3ok) c3ok = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(in3, IR_TYPE_I1_T), c3ok, inv);
    ir_builder_set_insert_point(b, c3ok);
    ir_lower_set_loc(b, site);
    IRValue* i1b = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp1b = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i1b);
    IRValue* b1s3 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp1b, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b1u3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                    ir_value_reg(ir_builder_emit_cast(b, b1s3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                    ir_value_const_int(255, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b1m3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u3, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b1ok3 = ir_builder_emit_cmp_eq(b, b1m3, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b1nz3 = ir_builder_emit_cmp_ne(b, b1u3, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* i2b = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp2 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i2b);
    IRValue* b2s = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp2, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b2u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                   ir_value_reg(ir_builder_emit_cast(b, b2s, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b2m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b2ok = ir_builder_emit_cmp_eq(b, b2m, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b2nz = ir_builder_emit_cmp_ne(b, b2u, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* okb1 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1ok3, IR_TYPE_I1_T), ir_value_reg(b1nz3, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    IRValue* okb2 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b2ok, IR_TYPE_I1_T), ir_value_reg(b2nz, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    int ok3 = ir_builder_emit_and(b, IR_TYPE_I1_T, okb1, okb2);
    IRBlock* c3pack = cf_create_block(ctx, "اقرأ_سطر_حالة3_حزم");
    if (!c3pack) c3pack = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(ok3, IR_TYPE_I1_T), c3pack, inv);
    ir_builder_set_insert_point(b, c3pack);
    ir_lower_set_loc(b, site);
    IRValue* b1sh3 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u3, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b2sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b2u, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* bytes3 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                     ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u, b1sh3), IR_TYPE_I64_T),
                                                     b2sh),
                                   IR_TYPE_I64_T);
    IRValue* l3sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, ir_value_const_int(3, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* packed3 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes3, l3sh), IR_TYPE_I64_T);
    ir_builder_emit_store(b, packed3, bits_ptr);
    ir_builder_emit_store(b, i3, next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // حالة4 (مشابهة، محافظة)
    ir_builder_set_insert_point(b, c4);
    ir_lower_set_loc(b, site);
    IRValue* i4 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(4, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int in4 = ir_builder_emit_cmp_le(b, i4, len2);
    IRBlock* c4ok = cf_create_block(ctx, "اقرأ_سطر_حالة4_ok");
    if (!c4ok) c4ok = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(in4, IR_TYPE_I1_T), c4ok, inv);
    ir_builder_set_insert_point(b, c4ok);
    ir_lower_set_loc(b, site);
    IRValue* i1c = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp1c = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i1c);
    IRValue* b1s4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp1c, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b1u4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                    ir_value_reg(ir_builder_emit_cast(b, b1s4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                    ir_value_const_int(255, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b1m4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b1ok4 = ir_builder_emit_cmp_eq(b, b1m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b1nz4 = ir_builder_emit_cmp_ne(b, b1u4, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* i2c = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp2c = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i2c);
    IRValue* b2s4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp2c, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b2u4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                    ir_value_reg(ir_builder_emit_cast(b, b2s4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                    ir_value_const_int(255, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b2m4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b2ok4 = ir_builder_emit_cmp_eq(b, b2m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b2nz4 = ir_builder_emit_cmp_ne(b, b2u4, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* i3b = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(3, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp3 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i3b);
    IRValue* b3s = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp3, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b3u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                   ir_value_reg(ir_builder_emit_cast(b, b3s, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b3m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b3u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b3ok = ir_builder_emit_cmp_eq(b, b3m, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b3nz = ir_builder_emit_cmp_ne(b, b3u, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* okb14 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1ok4, IR_TYPE_I1_T), ir_value_reg(b1nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    IRValue* okb24 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b2ok4, IR_TYPE_I1_T), ir_value_reg(b2nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    IRValue* okb34 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b3ok, IR_TYPE_I1_T), ir_value_reg(b3nz, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    int ok12 = ir_builder_emit_and(b, IR_TYPE_I1_T, okb14, okb24);
    int ok4 = ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(ok12, IR_TYPE_I1_T), okb34);
    IRBlock* c4pack = cf_create_block(ctx, "اقرأ_سطر_حالة4_حزم");
    if (!c4pack) c4pack = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(ok4, IR_TYPE_I1_T), c4pack, inv);
    ir_builder_set_insert_point(b, c4pack);
    ir_lower_set_loc(b, site);
    IRValue* b3sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b3u, ir_value_const_int(24, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b1sh4 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u4, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b2sh4 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b2u4, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* bytes4 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                     ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                                                    ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u, b1sh4), IR_TYPE_I64_T),
                                                                                    b2sh4),
                                                                  IR_TYPE_I64_T),
                                                     b3sh),
                                   IR_TYPE_I64_T);
    IRValue* l4sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, ir_value_const_int(4, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* packed4 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes4, l4sh), IR_TYPE_I64_T);
    ir_builder_emit_store(b, packed4, bits_ptr);
    ir_builder_emit_store(b, i4, next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // invalid: replacement + advance 1
    ir_builder_set_insert_point(b, inv);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, repl_bits, bits_ptr);
    ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T), next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // store: chars[j]=bits; j++; i=next_i
    ir_builder_set_insert_point(b, p_store);
    ir_lower_set_loc(b, site);
    IRValue* j = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, j_ptr), IR_TYPE_I64_T);
    IRValue* bitsv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, bits_ptr), IR_TYPE_I64_T);
    int epc = ir_builder_emit_ptr_offset(b, char_ptr_t, chars, j);
    ir_builder_emit_store(b, ir_lower_i64_bits_to_char(ctx, bitsv), ir_value_reg(epc, char_ptr_t));
    ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, j, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T), j_ptr);
    IRValue* ni = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, next_i_ptr), IR_TYPE_I64_T);
    ir_builder_emit_store(b, ni, i_ptr);
    ir_builder_emit_br(b, p_head);

    ir_builder_set_insert_point(b, p_done);
    ir_lower_set_loc(b, site);
    IRValue* j2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, j_ptr), IR_TYPE_I64_T);
    int ept = ir_builder_emit_ptr_offset(b, char_ptr_t, chars, j2);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_CHAR_T), ir_value_reg(ept, char_ptr_t));
    IRValue* f2[1] = { buf2 };
    ir_builder_emit_call_void(b, "free", f2, 1);
    ir_builder_emit_store(b, chars, res_ptr);
    ir_builder_emit_br(b, final_done);

    ir_builder_set_insert_point(b, final_done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(out_r, char_ptr_t);
}

// ============================================================================
// تنسيق عربي للإخراج/الإدخال (v0.4.0)
// ============================================================================

#define BAA_FMT_MAX_SPECS 128

typedef enum {
    BAA_FMT_SPEC_I64,
    BAA_FMT_SPEC_U64,
    BAA_FMT_SPEC_HEX,
    BAA_FMT_SPEC_STR,
    BAA_FMT_SPEC_CHAR,
    BAA_FMT_SPEC_F64,
    BAA_FMT_SPEC_F64_SCI,
    BAA_FMT_SPEC_PTR,
} BaaFmtSpecKind;

typedef struct {
    BaaFmtSpecKind kind;
    int width;
    bool has_width;
} BaaFmtSpec;

static bool baa_utf8_decode_one_fmt(const char* s, uint32_t* out_cp, int* out_len)
{
    if (!s || !out_cp || !out_len) return false;
    const unsigned char b0 = (unsigned char)s[0];
    if (b0 == 0) return false;

    if ((b0 & 0x80u) == 0x00u) {
        *out_cp = (uint32_t)b0;
        *out_len = 1;
        return true;
    }
    if ((b0 & 0xE0u) == 0xC0u) {
        const unsigned char b1 = (unsigned char)s[1];
        if ((b1 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        *out_len = 2;
        return true;
    }
    if ((b0 & 0xF0u) == 0xE0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        if ((b1 & 0xC0u) != 0x80u) return false;
        if ((b2 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x0Fu) << 12) |
                  ((uint32_t)(b1 & 0x3Fu) << 6) |
                  (uint32_t)(b2 & 0x3Fu);
        *out_len = 3;
        return true;
    }
    if ((b0 & 0xF8u) == 0xF0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        const unsigned char b3 = (unsigned char)s[3];
        if ((b1 & 0xC0u) != 0x80u) return false;
        if ((b2 & 0xC0u) != 0x80u) return false;
        if ((b3 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x07u) << 18) |
                  ((uint32_t)(b1 & 0x3Fu) << 12) |
                  ((uint32_t)(b2 & 0x3Fu) << 6) |
                  (uint32_t)(b3 & 0x3Fu);
        *out_len = 4;
        return true;
    }

    return false;
}

static bool fmt_buf_append(char** buf, size_t* len, size_t* cap, const char* s)
{
    if (!buf || !len || !cap || !s) return false;
    size_t n = strlen(s);
    if (*cap < *len + n + 1) {
        size_t ncap = (*cap == 0) ? 64 : *cap;
        while (ncap < *len + n + 1) ncap *= 2;
        char* nb = (char*)realloc(*buf, ncap);
        if (!nb) return false;
        *buf = nb;
        *cap = ncap;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = 0;
    return true;
}

static bool fmt_buf_append_ch(char** buf, size_t* len, size_t* cap, char ch)
{
    char tmp[2] = { ch, 0 };
    return fmt_buf_append(buf, len, cap, tmp);
}

static bool baa_fmt_translate_ar(IRLowerCtx* ctx, const Node* site, const char* fmt, bool is_input,
                                 char** out_cfmt, BaaFmtSpec* out_specs, int* out_spec_count)
{
    if (!out_cfmt || !out_specs || !out_spec_count) return false;
    *out_cfmt = NULL;
    *out_spec_count = 0;
    if (!fmt) return false;

    char* obuf = NULL;
    size_t olen = 0;
    size_t ocap = 0;

    const char* p = fmt;
    while (*p) {
        if (*p != '%') {
            if (!fmt_buf_append_ch(&obuf, &olen, &ocap, *p)) goto oom;
            p++;
            continue;
        }

        // %%
        if (p[1] == '%') {
            if (!fmt_buf_append(&obuf, &olen, &ocap, "%%")) goto oom;
            p += 2;
            continue;
        }

        if (!fmt_buf_append_ch(&obuf, &olen, &ocap, '%')) goto oom;
        p++;

        // flags
        while (*p && (*p == '-' || *p == '+' || *p == '0' || *p == ' ' || *p == '#')) {
            if (!fmt_buf_append_ch(&obuf, &olen, &ocap, *p)) goto oom;
            p++;
        }

        int width = 0;
        bool has_width = false;
        while (*p && isdigit((unsigned char)*p)) {
            has_width = true;
            width = width * 10 + (int)(*p - '0');
            if (width > 1000000) width = 1000000;
            if (!fmt_buf_append_ch(&obuf, &olen, &ocap, *p)) goto oom;
            p++;
        }

        if (*p == '.') {
            if (!fmt_buf_append_ch(&obuf, &olen, &ocap, '.')) goto oom;
            p++;
            if (*p == '*') {
                ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: الدقة باستخدام '*' غير مدعومة حالياً.");
                free(obuf);
                return false;
            }
            while (*p && isdigit((unsigned char)*p)) {
                if (!fmt_buf_append_ch(&obuf, &olen, &ocap, *p)) goto oom;
                p++;
            }
        }

        uint32_t cp = 0;
        int ulen = 0;
        if (!baa_utf8_decode_one_fmt(p, &cp, &ulen) || ulen <= 0) {
            ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: محرف UTF-8 غير صالح بعد '%%'.");
            free(obuf);
            return false;
        }

        if (*out_spec_count >= BAA_FMT_MAX_SPECS) {
            ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: عدد المواصفات كبير جداً (الحد %d).", BAA_FMT_MAX_SPECS);
            free(obuf);
            return false;
        }

        BaaFmtSpec spec;
        memset(&spec, 0, sizeof(spec));
        spec.width = width;
        spec.has_width = has_width;

        if (cp == 0x0635u) { // ص
            spec.kind = BAA_FMT_SPEC_I64;
            if (!fmt_buf_append(&obuf, &olen, &ocap, "lld")) goto oom;
        } else if (cp == 0x0637u) { // ط
            spec.kind = BAA_FMT_SPEC_U64;
            if (!fmt_buf_append(&obuf, &olen, &ocap, "llu")) goto oom;
        } else if (cp == 0x0633u) { // س
            spec.kind = BAA_FMT_SPEC_HEX;
            if (!fmt_buf_append(&obuf, &olen, &ocap, "llx")) goto oom;
        } else if (cp == 0x0646u) { // ن
            spec.kind = BAA_FMT_SPEC_STR;
            if (is_input && (!has_width || width <= 0)) {
                ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: %%ن في الإدخال يتطلب عرضاً رقمياً (مثلاً %%64ن).");
                free(obuf);
                return false;
            }
            if (!fmt_buf_append(&obuf, &olen, &ocap, "s")) goto oom;
        } else if (cp == 0x062Du) { // ح
            spec.kind = BAA_FMT_SPEC_CHAR;
            if (is_input) {
                ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: %%ح غير مدعوم في الإدخال حالياً.");
                free(obuf);
                return false;
            }
            if (!fmt_buf_append(&obuf, &olen, &ocap, "s")) goto oom;
        } else if (cp == 0x0639u) { // ع
            spec.kind = BAA_FMT_SPEC_F64;
            if (!fmt_buf_append(&obuf, &olen, &ocap, is_input ? "lf" : "f")) goto oom;
        } else if (cp == 0x0623u) { // أ (صيغة علمية)
            spec.kind = BAA_FMT_SPEC_F64_SCI;
            if (!fmt_buf_append(&obuf, &olen, &ocap, is_input ? "le" : "e")) goto oom;
        } else if (cp == 0x0645u) { // م
            spec.kind = BAA_FMT_SPEC_PTR;
            if (is_input) {
                ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: %%م غير مدعوم في الإدخال حالياً.");
                free(obuf);
                return false;
            }
            if (!fmt_buf_append(&obuf, &olen, &ocap, "p")) goto oom;
        } else {
            ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: مواصفة غير معروفة بعد '%%'.");
            free(obuf);
            return false;
        }

        out_specs[(*out_spec_count)++] = spec;
        p += ulen;
    }

    *out_cfmt = obuf;
    return true;

oom:
    ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: نفدت الذاكرة أثناء بناء نص التنسيق.");
    free(obuf);
    return false;
}

static IRValue* ir_lower_char_to_cstr_tmp(IRLowerCtx* ctx, const Node* site, IRValue* ch)
{
    if (!ctx || !ctx->builder || !ch) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

    IRType* buf_arr_t = ir_type_array(IR_TYPE_I8_T, 5);
    int buf_ptr_reg = ir_builder_emit_alloca(b, buf_arr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, ir_type_ptr(buf_arr_t));

    int base_reg = ir_builder_emit_cast(b, buf_ptr, i8_ptr_t);
    IRValue* base_ptr = ir_value_reg(base_reg, i8_ptr_t);

    // i64 معبأ = bytes + len*2^32
    int pr = ir_builder_emit_cast(b, ch, IR_TYPE_I64_T);
    IRValue* packed = ir_value_reg(pr, IR_TYPE_I64_T);

    IRValue* c_2p32 = ir_value_const_int(4294967296LL, IR_TYPE_I64_T);
    IRValue* c256 = ir_value_const_int(256, IR_TYPE_I64_T);

    int len_r = ir_builder_emit_div(b, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* len = ir_value_reg(len_r, IR_TYPE_I64_T);

    int bytes_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    int b0_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes, c256);
    IRValue* b0 = ir_value_reg(b0_r, IR_TYPE_I64_T);
    int bytes1_r = ir_builder_emit_div(b, IR_TYPE_I64_T, bytes, c256);
    IRValue* bytes1 = ir_value_reg(bytes1_r, IR_TYPE_I64_T);

    int b1_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes1, c256);
    IRValue* b1 = ir_value_reg(b1_r, IR_TYPE_I64_T);
    int bytes2_r = ir_builder_emit_div(b, IR_TYPE_I64_T, bytes1, c256);
    IRValue* bytes2 = ir_value_reg(bytes2_r, IR_TYPE_I64_T);

    int b2_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes2, c256);
    IRValue* b2 = ir_value_reg(b2_r, IR_TYPE_I64_T);
    int bytes3_r = ir_builder_emit_div(b, IR_TYPE_I64_T, bytes2, c256);
    IRValue* bytes3 = ir_value_reg(bytes3_r, IR_TYPE_I64_T);

    int b3_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes3, c256);
    IRValue* b3 = ir_value_reg(b3_r, IR_TYPE_I64_T);

    IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
    IRValue* idx2 = ir_value_const_int(2, IR_TYPE_I64_T);
    IRValue* idx3 = ir_value_const_int(3, IR_TYPE_I64_T);

    IRValue* b0p = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0), i8_ptr_t);
    IRValue* b1p = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1), i8_ptr_t);
    IRValue* b2p = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx2), i8_ptr_t);
    IRValue* b3p = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx3), i8_ptr_t);

    if (site) ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, cast_to(ctx, b0, IR_TYPE_I8_T), b0p);
    ir_builder_emit_store(b, cast_to(ctx, b1, IR_TYPE_I8_T), b1p);
    ir_builder_emit_store(b, cast_to(ctx, b2, IR_TYPE_I8_T), b2p);
    ir_builder_emit_store(b, cast_to(ctx, b3, IR_TYPE_I8_T), b3p);

    IRValue* tp = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, len), i8_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), tp);

    return base_ptr;
}

static IRValue* ir_lower_cstr_or_literal_on_null(IRLowerCtx* ctx, const Node* site, IRValue* cstr, const char* literal)
{
    if (!ctx || !ctx->builder || !literal) return cstr;

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);

    IRValue* p = cstr ? cast_to(ctx, cstr, i8_ptr_t) : ir_value_const_int(0, i8_ptr_t);
    IRValue* lit = ir_builder_const_string(b, literal);
    if (!lit) return p;

    int res_ptr_reg = ir_builder_emit_alloca(b, i8_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, i8_ptr_ptr_t);
    ir_builder_emit_store(b, p, res_ptr);

    int is_null_r = ir_builder_emit_cmp_eq(b, p, ir_value_const_int(0, i8_ptr_t));
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* b_null = cf_create_block(ctx, "تنسيق_نص_NULL");
    IRBlock* b_ok = cf_create_block(ctx, "تنسيق_نص_OK");
    IRBlock* b_ret = cf_create_block(ctx, "تنسيق_نص_إرجاع");
    if (!b_null || !b_ok || !b_ret) {
        int r0 = ir_builder_emit_load(b, i8_ptr_t, res_ptr);
        return ir_value_reg(r0, i8_ptr_t);
    }

    ir_builder_emit_br_cond(b, is_null, b_null, b_ok);

    ir_builder_set_insert_point(b, b_null);
    if (site) ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, lit, res_ptr);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_ok);
    if (site) ir_lower_set_loc(b, site);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_ret);
    if (site) ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, i8_ptr_t, res_ptr);
    return ir_value_reg(r, i8_ptr_t);
}

static IRValue* ir_lower_builtin_print_format_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_format_string_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_scan_format_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_read_line_stdin_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_read_num_stdin_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_variadic_start(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_variadic_next(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_variadic_end(IRLowerCtx* ctx, Node* call_expr);
static IRValue* lower_lvalue_address(IRLowerCtx* ctx, Node* expr, IRType** out_pointee_type);

static bool ir_lower_variadic_extract_type(Node* type_arg, DataType* out_type)
{
    if (!out_type) return false;
    *out_type = TYPE_INT;
    if (!type_arg) return false;
    if (type_arg->type != NODE_SIZEOF || !type_arg->data.sizeof_expr.has_type_form) return false;
    *out_type = type_arg->data.sizeof_expr.target_type;
    return true;
}

static IRValue* ir_lower_pack_variadic_args(IRLowerCtx* ctx, Node* site, IRValue** args, int arg_count)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* f64_ptr_t = ir_type_ptr(IR_TYPE_F64_T);

    int slots = arg_count > 0 ? arg_count : 1;
    IRType* buf_arr_t = ir_type_array(IR_TYPE_I8_T, slots * 8);
    if (!buf_arr_t) {
        ir_lower_report_error(ctx, site, "فشل إنشاء مخزن معاملات متغيرة.");
        return ir_value_const_int(0, i8_ptr_t);
    }

    int buf_reg = ir_builder_emit_alloca(b, buf_arr_t);
    IRValue* buf_arr_ptr = ir_value_reg(buf_reg, ir_type_ptr(buf_arr_t));
    int base_cast = ir_builder_emit_cast(b, buf_arr_ptr, i8_ptr_t);
    IRValue* base_i8 = ir_value_reg(base_cast, i8_ptr_t);

    for (int i = 0; i < arg_count; i++) {
        IRValue* av = (args && args[i]) ? args[i] : ir_value_const_int(0, IR_TYPE_I64_T);
        IRType* at = av->type ? av->type : IR_TYPE_I64_T;

        if (site) ir_lower_set_loc(b, site);
        int slot_r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_i8,
                                                ir_value_const_int((int64_t)i * 8, IR_TYPE_I64_T));
        IRValue* slot_i8 = ir_value_reg(slot_r, i8_ptr_t);

        if (at->kind == IR_TYPE_F64) {
            int cp = ir_builder_emit_cast(b, slot_i8, f64_ptr_t);
            IRValue* slot_f64 = ir_value_reg(cp, f64_ptr_t);
            IRValue* fv = cast_to(ctx, av, IR_TYPE_F64_T);
            ir_builder_emit_store(b, fv, slot_f64);
            continue;
        }

        if (at->kind == IR_TYPE_PTR) {
            int cp = ir_builder_emit_cast(b, slot_i8, i8_ptr_ptr_t);
            IRValue* slot_ptr = ir_value_reg(cp, i8_ptr_ptr_t);
            IRValue* pv = cast_to(ctx, av, i8_ptr_t);
            ir_builder_emit_store(b, pv, slot_ptr);
            continue;
        }

        int cp = ir_builder_emit_cast(b, slot_i8, i64_ptr_t);
        IRValue* slot_i64 = ir_value_reg(cp, i64_ptr_t);
        IRValue* iv = ensure_i64(ctx, av);
        ir_builder_emit_store(b, iv, slot_i64);
    }

    return base_i8;
}

static IRValue* ir_lower_builtin_variadic_start(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    if (!ctx->current_func_is_variadic || ctx->current_func_va_base_reg < 0) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'بدء_معاملات' خارج دالة متغيرة المعاملات.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    Node* a0 = call_expr->data.call.args;
    Node* a1 = a0 ? a0->next : NULL;
    if (!a0 || !a1 || a1->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'بدء_معاملات' يتطلب معاملين.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }
    if (a0->type != NODE_VAR_REF) {
        ir_lower_report_error(ctx, a0, "المعامل الأول في 'بدء_معاملات' يجب أن يكون متغيراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* cursor_pointee = IR_TYPE_I64_T;
    IRValue* cursor_addr = lower_lvalue_address(ctx, a0, &cursor_pointee);
    if (!cursor_pointee || cursor_pointee->kind != IR_TYPE_PTR) {
        ir_lower_report_error(ctx, a0, "متغير قائمة المعاملات يجب أن يكون مؤشراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* base = ir_value_reg(ctx->current_func_va_base_reg, i8_ptr_t);
    int64_t off = (int64_t)ctx->current_func_fixed_params * 8;
    if (off > 0) {
        int pr = ir_builder_emit_ptr_offset(ctx->builder, i8_ptr_t, base,
                                            ir_value_const_int(off, IR_TYPE_I64_T));
        base = ir_value_reg(pr, i8_ptr_t);
    }

    if (call_expr) ir_lower_set_loc(ctx->builder, call_expr);
    ir_builder_emit_store(ctx->builder, cast_to(ctx, base, cursor_pointee), cursor_addr);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_variadic_end(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* a0 = call_expr->data.call.args;
    if (!a0 || a0->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'نهاية_معاملات' يتطلب معاملاً واحداً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }
    if (a0->type != NODE_VAR_REF) {
        ir_lower_report_error(ctx, a0, "المعامل الأول في 'نهاية_معاملات' يجب أن يكون متغيراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* cursor_pointee = IR_TYPE_I64_T;
    IRValue* cursor_addr = lower_lvalue_address(ctx, a0, &cursor_pointee);
    if (!cursor_pointee || cursor_pointee->kind != IR_TYPE_PTR) {
        ir_lower_report_error(ctx, a0, "متغير قائمة المعاملات يجب أن يكون مؤشراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    if (call_expr) ir_lower_set_loc(ctx->builder, call_expr);
    ir_builder_emit_store(ctx->builder, ir_value_const_int(0, cursor_pointee), cursor_addr);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_variadic_next(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    if (!ctx->current_func_is_variadic) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'معامل_تالي' خارج دالة متغيرة المعاملات.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    Node* a0 = call_expr->data.call.args;
    Node* a1 = a0 ? a0->next : NULL;
    if (!a0 || !a1 || a1->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'معامل_تالي' يتطلب معاملين.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }
    if (a0->type != NODE_VAR_REF) {
        ir_lower_report_error(ctx, a0, "المعامل الأول في 'معامل_تالي' يجب أن يكون متغيراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    DataType want = TYPE_INT;
    if (!ir_lower_variadic_extract_type(a1, &want)) {
        ir_lower_report_error(ctx, a1, "المعامل الثاني في 'معامل_تالي' يجب أن يكون نوعاً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* cursor_pointee = IR_TYPE_I64_T;
    IRValue* cursor_addr = lower_lvalue_address(ctx, a0, &cursor_pointee);
    if (!cursor_pointee || cursor_pointee->kind != IR_TYPE_PTR) {
        ir_lower_report_error(ctx, a0, "متغير قائمة المعاملات يجب أن يكون مؤشراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* f64_ptr_t = ir_type_ptr(IR_TYPE_F64_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);

    if (call_expr) ir_lower_set_loc(b, call_expr);
    int cur_r = ir_builder_emit_load(b, cursor_pointee, cursor_addr);
    IRValue* cur_raw = ir_value_reg(cur_r, cursor_pointee);
    IRValue* cur_i8 = cast_to(ctx, cur_raw, i8_ptr_t);

    IRType* load_t = IR_TYPE_I64_T;
    IRType* ret_t = ir_type_from_datatype(m, want);
    if (!ret_t || ret_t->kind == IR_TYPE_VOID) ret_t = IR_TYPE_I64_T;

    if (want == TYPE_FLOAT) {
        load_t = IR_TYPE_F64_T;
    } else if (want == TYPE_POINTER || want == TYPE_STRING) {
        load_t = i8_ptr_t;
    }

    IRValue* slot_ptr = NULL;
    if (load_t == IR_TYPE_F64_T) {
        int cp = ir_builder_emit_cast(b, cur_i8, f64_ptr_t);
        slot_ptr = ir_value_reg(cp, f64_ptr_t);
    } else if (load_t == i8_ptr_t) {
        int cp = ir_builder_emit_cast(b, cur_i8, i8_ptr_ptr_t);
        slot_ptr = ir_value_reg(cp, i8_ptr_ptr_t);
    } else {
        int cp = ir_builder_emit_cast(b, cur_i8, i64_ptr_t);
        slot_ptr = ir_value_reg(cp, i64_ptr_t);
    }

    int val_r = ir_builder_emit_load(b, load_t, slot_ptr);
    IRValue* val = ir_value_reg(val_r, load_t);

    int next_r = ir_builder_emit_ptr_offset(b, i8_ptr_t, cur_i8, ir_value_const_int(8, IR_TYPE_I64_T));
    IRValue* next_i8 = ir_value_reg(next_r, i8_ptr_t);
    ir_builder_emit_store(b, cast_to(ctx, next_i8, cursor_pointee), cursor_addr);

    if (load_t != ret_t) {
        val = cast_to(ctx, val, ret_t);
    }
    return val;
}

static IRValue* lower_call_expr(IRLowerCtx* ctx, Node* expr) {
    if (!ctx || !ctx->builder || !expr) return ir_builder_const_i64(0);

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    bool allow_format_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        if (find_local(ctx, n0)) allow_format_builtins = false;
        if (m && ir_module_find_global(m, n0)) allow_format_builtins = false;
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            if (f0 && !f0->is_prototype) allow_format_builtins = false;
        }
    }

    if (allow_format_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        if (strcmp(n, "اطبع_منسق") == 0) {
            return ir_lower_builtin_print_format_ar(ctx, expr);
        }
        if (strcmp(n, "نسق") == 0) {
            return ir_lower_builtin_format_string_ar(ctx, expr);
        }
        if (strcmp(n, "اقرأ_منسق") == 0) {
            return ir_lower_builtin_scan_format_ar(ctx, expr);
        }
        if (strcmp(n, "اقرأ_سطر") == 0) {
            // اقرأ_سطر() من stdin فقط. اقرأ_سطر(ملف) يبقى مسار ملفات (builtins v0.3.12).
            if (!expr->data.call.args) {
                return ir_lower_builtin_read_line_stdin_ar(ctx, expr);
            }
        }
        if (strcmp(n, "اقرأ_رقم") == 0) {
            return ir_lower_builtin_read_num_stdin_ar(ctx, expr);
        }
        if (strcmp(n, "بدء_معاملات") == 0) {
            return ir_lower_builtin_variadic_start(ctx, expr);
        }
        if (strcmp(n, "معامل_تالي") == 0) {
            return ir_lower_builtin_variadic_next(ctx, expr);
        }
        if (strcmp(n, "نهاية_معاملات") == 0) {
            return ir_lower_builtin_variadic_end(ctx, expr);
        }
    }

    // دوال السلاسل المدمجة في v0.3.9 (سلوك C-like مع أسماء عربية).
    bool allow_string_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        // إذا كان هناك رمز/تخزين يحمل نفس الاسم، يجب احترام shadowing وعدم تحويله إلى builtin.
        if (find_local(ctx, n0)) {
            allow_string_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_string_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            // إذا كانت الدالة مجرد prototype (مثلاً من ترويسة stdlib)، نسمح للـ builtin أن يطغى عليها.
            // أما إذا كانت لها جسم داخل الوحدة، فهذه دالة مستخدم ويجب احترامها.
            if (f0 && !f0->is_prototype) {
                allow_string_builtins = false;
            }
        }
    }

    if (allow_string_builtins && expr->data.call.name) {
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

    // دوال الذاكرة المدمجة في v0.3.11: حجز/تحرير/إعادة_حجز/نسخ_ذاكرة/تعيين_ذاكرة.
    bool allow_mem_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        // نفس قواعد الـ shadowing: متغير/تخزين بنفس الاسم يمنع تحويله إلى builtin.
        if (find_local(ctx, n0)) {
            allow_mem_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_mem_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            // إذا كانت الدالة prototype فقط (مثلاً من stdlib)، نسمح للـ builtin.
            if (f0 && !f0->is_prototype) {
                allow_mem_builtins = false;
            }
        }
    }

    if (allow_mem_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;
        Node* a2 = a1 ? a1->next : NULL;
        IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

        if (strcmp(n, "حجز_ذاكرة") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'حجز_ذاكرة' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }
            IRValue* size_v = lower_expr(ctx, a0);
            IRValue* size_i64 = ensure_i64(ctx, size_v);
            IRValue* args[1] = { size_i64 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "malloc", i8_ptr_t, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء malloc.");
                return ir_value_const_int(0, i8_ptr_t);
            }
            return ir_value_reg(r, i8_ptr_t);
        }

        if (strcmp(n, "تحرير_ذاكرة") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'تحرير_ذاكرة' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* pv = lower_expr(ctx, a0);
            IRValue* p_i8 = cast_to(ctx, pv, i8_ptr_t);
            IRValue* args[1] = { p_i8 };
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_call_void(ctx->builder, "free", args, 1);
            return ir_builder_const_i64(0);
        }

        if (strcmp(n, "إعادة_حجز") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'إعادة_حجز' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }
            IRValue* pv = lower_expr(ctx, a0);
            IRValue* size_v = lower_expr(ctx, a1);
            IRValue* p_i8 = cast_to(ctx, pv, i8_ptr_t);
            IRValue* size_i64 = ensure_i64(ctx, size_v);
            IRValue* args[2] = { p_i8, size_i64 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "realloc", i8_ptr_t, args, 2);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء realloc.");
                return ir_value_const_int(0, i8_ptr_t);
            }
            return ir_value_reg(r, i8_ptr_t);
        }

        if (strcmp(n, "نسخ_ذاكرة") == 0) {
            if (!a0 || !a1 || !a2 || a2->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'نسخ_ذاكرة' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }
            IRValue* dv = lower_expr(ctx, a0);
            IRValue* sv = lower_expr(ctx, a1);
            IRValue* nv = lower_expr(ctx, a2);
            IRValue* dst = cast_to(ctx, dv, i8_ptr_t);
            IRValue* src = cast_to(ctx, sv, i8_ptr_t);
            IRValue* n_i64 = ensure_i64(ctx, nv);
            IRValue* args[3] = { dst, src, n_i64 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "memcpy", i8_ptr_t, args, 3);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء memcpy.");
                return ir_value_const_int(0, i8_ptr_t);
            }
            return ir_value_reg(r, i8_ptr_t);
        }

        if (strcmp(n, "تعيين_ذاكرة") == 0) {
            if (!a0 || !a1 || !a2 || a2->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'تعيين_ذاكرة' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }
            IRValue* pv = lower_expr(ctx, a0);
            IRValue* vv = lower_expr(ctx, a1);
            IRValue* nv = lower_expr(ctx, a2);
            IRValue* p_i8 = cast_to(ctx, pv, i8_ptr_t);
            IRValue* v_i64 = ensure_i64(ctx, vv);
            IRValue* n_i64 = ensure_i64(ctx, nv);
            IRValue* args[3] = { p_i8, v_i64, n_i64 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "memset", i8_ptr_t, args, 3);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء memset.");
                return ir_value_const_int(0, i8_ptr_t);
            }
            return ir_value_reg(r, i8_ptr_t);
        }
    }

    bool allow_math_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        if (find_local(ctx, n0)) {
            allow_math_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_math_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            if (f0 && !f0->is_prototype) {
                allow_math_builtins = false;
            }
        }
    }

    if (allow_math_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;

        if (strcmp(n, "جذر_تربيعي") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'جذر_تربيعي' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* in_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "sqrt", IR_TYPE_F64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء sqrt.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "أس") == 0) {
            if (!a0 || !a1 || a1->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'أس' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* base_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* exp_v = cast_to(ctx, lower_expr(ctx, a1), IR_TYPE_F64_T);
            IRValue* args[2] = { base_v, exp_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "pow", IR_TYPE_F64_T, args, 2);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء pow.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "جيب") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'جيب' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* in_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "sin", IR_TYPE_F64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء sin.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "جيب_تمام") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'جيب_تمام' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* in_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "cos", IR_TYPE_F64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء cos.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "ظل") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'ظل' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* in_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "tan", IR_TYPE_F64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء tan.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "مطلق") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'مطلق' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }

            IRValue* in_v = ensure_i64(ctx, lower_expr(ctx, a0));
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "llabs", IR_TYPE_I64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء llabs.");
                return ir_builder_const_i64(0);
            }
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "عشوائي") == 0) {
            if (a0) {
                ir_lower_report_error(ctx, expr, "استدعاء 'عشوائي' لا يقبل معاملات.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }

            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "rand", IR_TYPE_I32_T, NULL, 0);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء rand.");
                return ir_builder_const_i64(0);
            }
            return cast_to(ctx, ir_value_reg(r, IR_TYPE_I32_T), IR_TYPE_I64_T);
        }
    }

    bool allow_system_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        if (find_local(ctx, n0)) {
            allow_system_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_system_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            if (f0 && !f0->is_prototype) {
                allow_system_builtins = false;
            }
        }
    }

    if (allow_system_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;

        IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
        IRType* char_ptr_t = get_char_ptr_type(m);

        if (strcmp(n, "متغير_بيئة") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'متغير_بيئة' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, char_ptr_t);
            }

            IRValue* key_i8 = NULL;
            bool free_key = false;
            if (a0->type == NODE_STRING && a0->data.string_lit.value) {
                key_i8 = ir_builder_const_string(ctx->builder, a0->data.string_lit.value);
            } else {
                IRValue* key_baa = lower_expr(ctx, a0);
                key_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, key_baa);
                free_key = true;
            }

            int out_ptr_reg = ir_builder_emit_alloca(ctx->builder, i8_ptr_t);
            IRValue* out_ptr = ir_value_reg(out_ptr_reg, ir_type_ptr(i8_ptr_t));
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), out_ptr);

            int key_null_r = ir_builder_emit_cmp_eq(ctx->builder, key_i8, ir_value_const_int(0, i8_ptr_t));
            IRValue* key_null = ir_value_reg(key_null_r, IR_TYPE_I1_T);

            IRBlock* nullb = cf_create_block(ctx, "متغير_بيئة_مفتاح_فارغ");
            IRBlock* callb = cf_create_block(ctx, "متغير_بيئة_نداء");
            IRBlock* done = cf_create_block(ctx, "متغير_بيئة_نهاية");
            if (!nullb || !callb || !done) {
                if (free_key) {
                    IRValue* fargs[1] = { key_i8 };
                    ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
                }
                return ir_value_const_int(0, char_ptr_t);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, key_null, nullb, callb);
            }

            ir_builder_set_insert_point(ctx->builder, nullb);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), out_ptr);
            if (free_key) {
                IRValue* fargs[1] = { key_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, callb);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* args[1] = { key_i8 };
            int r = ir_builder_emit_call(ctx->builder, "getenv", i8_ptr_t, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء getenv.");
                ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), out_ptr);
            } else {
                ir_builder_emit_store(ctx->builder, ir_value_reg(r, i8_ptr_t), out_ptr);
            }
            if (free_key) {
                IRValue* fargs[1] = { key_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, done);
            ir_lower_set_loc(ctx->builder, expr);
            int raw_r = ir_builder_emit_load(ctx->builder, i8_ptr_t, out_ptr);
            IRValue* raw = ir_value_reg(raw_r, i8_ptr_t);
            return ir_lower_cstr_to_baa_string_alloc(ctx, expr, raw);
        }

        if (strcmp(n, "نفذ_أمر") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'نفذ_أمر' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }

            IRValue* cmd_i8 = NULL;
            bool free_cmd = false;
            if (a0->type == NODE_STRING && a0->data.string_lit.value) {
                cmd_i8 = ir_builder_const_string(ctx->builder, a0->data.string_lit.value);
            } else {
                IRValue* cmd_baa = lower_expr(ctx, a0);
                cmd_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, cmd_baa);
                free_cmd = true;
            }

            int ret_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* ret_ptr = ir_value_reg(ret_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
            ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);

            int cmd_null_r = ir_builder_emit_cmp_eq(ctx->builder, cmd_i8, ir_value_const_int(0, i8_ptr_t));
            IRValue* cmd_null = ir_value_reg(cmd_null_r, IR_TYPE_I1_T);

            IRBlock* nullb = cf_create_block(ctx, "نفذ_أمر_فارغ");
            IRBlock* callb = cf_create_block(ctx, "نفذ_أمر_نداء");
            IRBlock* done = cf_create_block(ctx, "نفذ_أمر_نهاية");
            if (!nullb || !callb || !done) {
                if (free_cmd) {
                    IRValue* fargs[1] = { cmd_i8 };
                    ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
                }
                return ir_builder_const_i64(-1);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, cmd_null, nullb, callb);
            }

            ir_builder_set_insert_point(ctx->builder, nullb);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);
            if (free_cmd) {
                IRValue* fargs[1] = { cmd_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, callb);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* args[1] = { cmd_i8 };
            int r = ir_builder_emit_call(ctx->builder, "system", IR_TYPE_I32_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء system.");
                ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);
            } else {
                ir_builder_emit_store(ctx->builder,
                                      cast_to(ctx, ir_value_reg(r, IR_TYPE_I32_T), IR_TYPE_I64_T),
                                      ret_ptr);
            }
            if (free_cmd) {
                IRValue* fargs[1] = { cmd_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, done);
            ir_lower_set_loc(ctx->builder, expr);
            int ret_r = ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, ret_ptr);
            return ir_value_reg(ret_r, IR_TYPE_I64_T);
        }
    }

    bool allow_time_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        if (find_local(ctx, n0)) {
            allow_time_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_time_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            if (f0 && !f0->is_prototype) {
                allow_time_builtins = false;
            }
        }
    }

    if (allow_time_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;

        IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
        IRType* char_ptr_t = get_char_ptr_type(m);

        if (strcmp(n, "وقت_حالي") == 0) {
            if (a0) {
                ir_lower_report_error(ctx, expr, "استدعاء 'وقت_حالي' لا يقبل معاملات.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }

            IRValue* args[1] = { ir_value_const_int(0, i8_ptr_t) };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "time", IR_TYPE_I64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء time.");
                return ir_builder_const_i64(-1);
            }
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "وقت_كنص") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'وقت_كنص' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, char_ptr_t);
            }

            IRValue* ts = ensure_i64(ctx, lower_expr(ctx, a0));
            int ts_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* ts_ptr = ir_value_reg(ts_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
            ir_builder_emit_store(ctx->builder, ts, ts_ptr);

            IRValue* ts_arg = cast_to(ctx, ts_ptr, i8_ptr_t);
            IRValue* args[1] = { ts_arg };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "ctime", i8_ptr_t, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء ctime.");
                return ir_value_const_int(0, char_ptr_t);
            }

            IRValue* raw = ir_value_reg(r, i8_ptr_t);
            return ir_lower_cstr_to_baa_string_alloc(ctx, expr, raw);
        }
    }

    bool allow_file_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        // نفس قواعد الـ shadowing: متغير/تخزين بنفس الاسم يمنع تحويله إلى builtin.
        if (find_local(ctx, n0)) {
            allow_file_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_file_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            // إذا كانت الدالة prototype فقط (مثلاً من stdlib)، نسمح للـ builtin.
            if (f0 && !f0->is_prototype) {
                allow_file_builtins = false;
            }
        }
    }

    if (allow_file_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;
        Node* a2 = a1 ? a1->next : NULL;

        IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
        IRType* char_ptr_t = get_char_ptr_type(m);

        bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);
        const char* fn_tell = is_win ? "_ftelli64" : "ftello";
        const char* fn_seek = is_win ? "_fseeki64" : "fseeko";

        if (strcmp(n, "فتح_ملف") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'فتح_ملف' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }

            IRValue* path_i8 = NULL;
            IRValue* mode_i8 = NULL;
            bool free_path = false;
            bool free_mode = false;

            if (a0->type == NODE_STRING && a0->data.string_lit.value) {
                path_i8 = ir_builder_const_string(ctx->builder, a0->data.string_lit.value);
            } else {
                IRValue* pv = lower_expr(ctx, a0);
                path_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, pv);
                free_path = true;
            }

            if (a1->type == NODE_STRING && a1->data.string_lit.value) {
                const char* ms = a1->data.string_lit.value;
                if (strcmp(ms, "قراءة") == 0) mode_i8 = ir_builder_const_string(ctx->builder, "r");
                else if (strcmp(ms, "كتابة") == 0) mode_i8 = ir_builder_const_string(ctx->builder, "w");
                else if (strcmp(ms, "إضافة") == 0) mode_i8 = ir_builder_const_string(ctx->builder, "a");
                else mode_i8 = ir_builder_const_string(ctx->builder, ms);
            } else {
                IRValue* mv = lower_expr(ctx, a1);
                mode_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, mv);
                free_mode = true;
            }

            int res_ptr_reg = ir_builder_emit_alloca(ctx->builder, i8_ptr_t);
            IRValue* res_ptr = ir_value_reg(res_ptr_reg, ir_type_ptr(i8_ptr_t));
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), res_ptr);

            int pnull_r = ir_builder_emit_cmp_eq(ctx->builder, path_i8, ir_value_const_int(0, i8_ptr_t));
            int mnull_r = ir_builder_emit_cmp_eq(ctx->builder, mode_i8, ir_value_const_int(0, i8_ptr_t));
            IRValue* pnull = ir_value_reg(pnull_r, IR_TYPE_I1_T);
            IRValue* mnull = ir_value_reg(mnull_r, IR_TYPE_I1_T);
            int anynull_r = ir_builder_emit_or(ctx->builder, IR_TYPE_I1_T, pnull, mnull);
            IRValue* anynull = ir_value_reg(anynull_r, IR_TYPE_I1_T);

            IRBlock* callb = cf_create_block(ctx, "فتح_ملف_نداء");
            IRBlock* nullb = cf_create_block(ctx, "فتح_ملف_فارغ");
            IRBlock* done = cf_create_block(ctx, "فتح_ملف_نهاية");
            if (!callb || !nullb || !done) {
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, anynull, nullb, callb);
            }

            ir_builder_set_insert_point(ctx->builder, nullb);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), res_ptr);
            if (free_path) {
                IRValue* fargs[1] = { path_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            if (free_mode) {
                IRValue* fargs[1] = { mode_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, callb);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* fargs2[2] = { path_i8, mode_i8 };
            int fr = ir_builder_emit_call(ctx->builder, "fopen", i8_ptr_t, fargs2, 2);
            if (fr < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء fopen.");
                ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), res_ptr);
            } else {
                ir_builder_emit_store(ctx->builder, ir_value_reg(fr, i8_ptr_t), res_ptr);
            }
            if (free_path) {
                IRValue* fargs[1] = { path_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            if (free_mode) {
                IRValue* fargs[1] = { mode_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, done);
            ir_lower_set_loc(ctx->builder, expr);
            int out_r = ir_builder_emit_load(ctx->builder, i8_ptr_t, res_ptr);
            return ir_value_reg(out_r, i8_ptr_t);
        }

        if (strcmp(n, "اغلق_ملف") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اغلق_ملف' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* fv = lower_expr(ctx, a0);
            IRValue* f_i8 = cast_to(ctx, fv, i8_ptr_t);
            IRValue* args[1] = { f_i8 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "fclose", IR_TYPE_I32_T, args, 1);
            if (r < 0) return ir_builder_const_i64(-1);
            return cast_to(ctx, ir_value_reg(r, IR_TYPE_I32_T), IR_TYPE_I64_T);
        }

        if (strcmp(n, "اقرأ_حرف") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اقرأ_حرف' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_CHAR_T);
            }

            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);

            IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
            int bits_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* bits_ptr = ir_value_reg(bits_ptr_reg, i64_ptr_t);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), bits_ptr);

            uint64_t repl_bits_u = pack_utf8_codepoint(0xFFFDu);
            IRValue* repl_bits = ir_value_const_int((int64_t)repl_bits_u, IR_TYPE_I64_T);

            ir_lower_set_loc(ctx->builder, expr);
            IRValue* fargs0[1] = { fv };
            int ch0_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch0 = ir_value_reg(ch0_r, IR_TYPE_I32_T);
            int eof0_r = ir_builder_emit_cmp_eq(ctx->builder, ch0, ir_value_const_int(-1, IR_TYPE_I32_T));

            IRBlock* b_eof = cf_create_block(ctx, "اقرأ_حرف_EOF");
            IRBlock* b_disp = cf_create_block(ctx, "اقرأ_حرف_توزيع");
            IRBlock* b_d2 = cf_create_block(ctx, "اقرأ_حرف_د2");
            IRBlock* b_d3 = cf_create_block(ctx, "اقرأ_حرف_د3");
            IRBlock* b_d4 = cf_create_block(ctx, "اقرأ_حرف_د4");
            IRBlock* b_c1 = cf_create_block(ctx, "اقرأ_حرف_حالة1");
            IRBlock* b_c2 = cf_create_block(ctx, "اقرأ_حرف_حالة2");
            IRBlock* b_c3 = cf_create_block(ctx, "اقرأ_حرف_حالة3");
            IRBlock* b_c4 = cf_create_block(ctx, "اقرأ_حرف_حالة4");
            IRBlock* b_inv = cf_create_block(ctx, "اقرأ_حرف_غير_صالح");
            IRBlock* b_done = cf_create_block(ctx, "اقرأ_حرف_نهاية");
            if (!b_eof || !b_disp || !b_d2 || !b_d3 || !b_d4 ||
                !b_c1 || !b_c2 || !b_c3 || !b_c4 || !b_inv || !b_done) {
                return ir_value_const_int(0, IR_TYPE_CHAR_T);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, ir_value_reg(eof0_r, IR_TYPE_I1_T), b_eof, b_disp);
            }

            ir_builder_set_insert_point(ctx->builder, b_eof);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_disp);
            ir_lower_set_loc(ctx->builder, expr);

            // b0u = (uint8)ch0
            IRValue* b0u = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                           ir_value_reg(ir_builder_emit_cast(ctx->builder, ch0, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                           ir_value_const_int(255, IR_TYPE_I64_T)),
                                       IR_TYPE_I64_T);

            // len0 = 1/2/3/4 حسب البايت الأول (UTF-8)
            IRValue* m80 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(0x80, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* mE0 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* mF0 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* mF8 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* is1 = ir_value_reg(ir_builder_emit_cmp_eq(ctx->builder, m80, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
            IRValue* is2 = ir_value_reg(ir_builder_emit_cmp_eq(ctx->builder, mE0, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
            IRValue* is3 = ir_value_reg(ir_builder_emit_cmp_eq(ctx->builder, mF0, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
            IRValue* is4 = ir_value_reg(ir_builder_emit_cmp_eq(ctx->builder, mF8, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
            IRValue* l1 = cast_i1_to_i64(ctx, is1);
            IRValue* l2 = cast_i1_to_i64(ctx, is2);
            IRValue* l3 = cast_i1_to_i64(ctx, is3);
            IRValue* l4 = cast_i1_to_i64(ctx, is4);
            IRValue* t2 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, l2, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* t3 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, l3, ir_value_const_int(3, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* t4 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, l4, ir_value_const_int(4, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* len0 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, l1, t2), IR_TYPE_I64_T),
                                                            ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, t3, t4), IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);

            // نرفض NUL داخل الملف لأنه يكسر نموذج C-string.
            int b0z = ir_builder_emit_cmp_eq(ctx->builder, b0u, ir_value_const_int(0, IR_TYPE_I64_T));
            int oklead = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                            ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b0z, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                            ir_value_reg(ir_builder_emit_cmp_gt(ctx->builder, len0, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(oklead, IR_TYPE_I1_T), b_d2, b_inv);

            ir_builder_set_insert_point(ctx->builder, b_d2);
            ir_lower_set_loc(ctx->builder, expr);
            int is1r = ir_builder_emit_cmp_eq(ctx->builder, len0, ir_value_const_int(1, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is1r, IR_TYPE_I1_T), b_c1, b_d3);

            ir_builder_set_insert_point(ctx->builder, b_d3);
            ir_lower_set_loc(ctx->builder, expr);
            int is2r = ir_builder_emit_cmp_eq(ctx->builder, len0, ir_value_const_int(2, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is2r, IR_TYPE_I1_T), b_c2, b_d4);

            ir_builder_set_insert_point(ctx->builder, b_d4);
            ir_lower_set_loc(ctx->builder, expr);
            int is3r = ir_builder_emit_cmp_eq(ctx->builder, len0, ir_value_const_int(3, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is3r, IR_TYPE_I1_T), b_c3, b_c4);

            // حالة1: بايت واحد
            ir_builder_set_insert_point(ctx->builder, b_c1);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* packed1 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(4294967296LL, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            ir_builder_emit_store(ctx->builder, packed1, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            // حالة2: بايتان
            ir_builder_set_insert_point(ctx->builder, b_c2);
            ir_lower_set_loc(ctx->builder, expr);
            int ch1_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch1 = ir_value_reg(ch1_r, IR_TYPE_I32_T);
            int eof1 = ir_builder_emit_cmp_eq(ctx->builder, ch1, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b1u = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                           ir_value_reg(ir_builder_emit_cast(ctx->builder, ch1, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                           ir_value_const_int(255, IR_TYPE_I64_T)),
                                       IR_TYPE_I64_T);
            IRValue* b1m = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b1u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b1ok = ir_builder_emit_cmp_eq(ctx->builder, b1m, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b1nz = ir_builder_emit_cmp_ne(ctx->builder, b1u, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb1 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b1ok, IR_TYPE_I1_T), ir_value_reg(b1nz, IR_TYPE_I1_T)), IR_TYPE_I1_T);
            IRValue* ok2 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                           ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof1, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                                           okb1),
                                       IR_TYPE_I1_T);
            IRBlock* b_c2pack = cf_create_block(ctx, "اقرأ_حرف_حالة2_حزم");
            if (!b_c2pack) b_c2pack = b_inv;
            ir_builder_emit_br_cond(ctx->builder, ok2, b_c2pack, b_inv);
            ir_builder_set_insert_point(ctx->builder, b_c2pack);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* b1sh = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b1u, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* bytes2 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, b0u, b1sh), IR_TYPE_I64_T);
            IRValue* l2sh = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(2, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* packed2 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, bytes2, l2sh), IR_TYPE_I64_T);
            ir_builder_emit_store(ctx->builder, packed2, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            // حالة3: ثلاثة بايتات
            ir_builder_set_insert_point(ctx->builder, b_c3);
            ir_lower_set_loc(ctx->builder, expr);
            int ch1_3_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch1_3 = ir_value_reg(ch1_3_r, IR_TYPE_I32_T);
            int eof1_3 = ir_builder_emit_cmp_eq(ctx->builder, ch1_3, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b1u3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch1_3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b1m3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b1u3, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b1ok3 = ir_builder_emit_cmp_eq(ctx->builder, b1m3, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b1nz3 = ir_builder_emit_cmp_ne(ctx->builder, b1u3, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb13 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b1ok3, IR_TYPE_I1_T), ir_value_reg(b1nz3, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            int ch2_3_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch2_3 = ir_value_reg(ch2_3_r, IR_TYPE_I32_T);
            int eof2_3 = ir_builder_emit_cmp_eq(ctx->builder, ch2_3, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b2u3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch2_3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b2m3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b2u3, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b2ok3 = ir_builder_emit_cmp_eq(ctx->builder, b2m3, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b2nz3 = ir_builder_emit_cmp_ne(ctx->builder, b2u3, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb23 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b2ok3, IR_TYPE_I1_T), ir_value_reg(b2nz3, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            IRValue* noeof3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof1_3, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof2_3, IR_TYPE_I1_T)), IR_TYPE_I1_T)),
                                          IR_TYPE_I1_T);
            IRValue* ok3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, noeof3,
                                                           ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, okb13, okb23), IR_TYPE_I1_T)),
                                       IR_TYPE_I1_T);
            IRBlock* b_c3pack = cf_create_block(ctx, "اقرأ_حرف_حالة3_حزم");
            if (!b_c3pack) b_c3pack = b_inv;
            ir_builder_emit_br_cond(ctx->builder, ok3, b_c3pack, b_inv);
            ir_builder_set_insert_point(ctx->builder, b_c3pack);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* b1sh3 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b1u3, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* b2sh3 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b2u3, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* bytes3 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T,
                                                             ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, b0u, b1sh3), IR_TYPE_I64_T),
                                                             b2sh3),
                                           IR_TYPE_I64_T);
            IRValue* l3sh = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(3, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* packed3 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, bytes3, l3sh), IR_TYPE_I64_T);
            ir_builder_emit_store(ctx->builder, packed3, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            // حالة4: أربعة بايتات (مراجعة محافظة)
            ir_builder_set_insert_point(ctx->builder, b_c4);
            ir_lower_set_loc(ctx->builder, expr);
            // إن لم تكن len0 == 4 فعلياً، اعتبره غير صالح.
            int is4r = ir_builder_emit_cmp_eq(ctx->builder, len0, ir_value_const_int(4, IR_TYPE_I64_T));
            IRBlock* b_c4len = cf_create_block(ctx, "اقرأ_حرف_حالة4_len");
            if (!b_c4len) b_c4len = b_inv;
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is4r, IR_TYPE_I1_T), b_c4len, b_inv);
            ir_builder_set_insert_point(ctx->builder, b_c4len);
            ir_lower_set_loc(ctx->builder, expr);

            int ch1_4_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch1_4 = ir_value_reg(ch1_4_r, IR_TYPE_I32_T);
            int eof1_4 = ir_builder_emit_cmp_eq(ctx->builder, ch1_4, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b1u4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch1_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b1m4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b1u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b1ok4 = ir_builder_emit_cmp_eq(ctx->builder, b1m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b1nz4 = ir_builder_emit_cmp_ne(ctx->builder, b1u4, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb14 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b1ok4, IR_TYPE_I1_T), ir_value_reg(b1nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            int ch2_4_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch2_4 = ir_value_reg(ch2_4_r, IR_TYPE_I32_T);
            int eof2_4 = ir_builder_emit_cmp_eq(ctx->builder, ch2_4, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b2u4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch2_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b2m4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b2u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b2ok4 = ir_builder_emit_cmp_eq(ctx->builder, b2m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b2nz4 = ir_builder_emit_cmp_ne(ctx->builder, b2u4, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb24 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b2ok4, IR_TYPE_I1_T), ir_value_reg(b2nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            int ch3_4_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch3_4 = ir_value_reg(ch3_4_r, IR_TYPE_I32_T);
            int eof3_4 = ir_builder_emit_cmp_eq(ctx->builder, ch3_4, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b3u4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch3_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b3m4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b3u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b3ok4 = ir_builder_emit_cmp_eq(ctx->builder, b3m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b3nz4 = ir_builder_emit_cmp_ne(ctx->builder, b3u4, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb34 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b3ok4, IR_TYPE_I1_T), ir_value_reg(b3nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            IRValue* noeof4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                              ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof1_4, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof2_4, IR_TYPE_I1_T)), IR_TYPE_I1_T)),
                                                                           IR_TYPE_I1_T),
                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof3_4, IR_TYPE_I1_T)), IR_TYPE_I1_T)),
                                          IR_TYPE_I1_T);
            IRValue* okb = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                           ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, okb14, okb24), IR_TYPE_I1_T),
                                                           okb34),
                                       IR_TYPE_I1_T);
            IRValue* ok4v = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, noeof4, okb), IR_TYPE_I1_T);

            IRBlock* b_c4pack = cf_create_block(ctx, "اقرأ_حرف_حالة4_حزم");
            if (!b_c4pack) b_c4pack = b_inv;
            ir_builder_emit_br_cond(ctx->builder, ok4v, b_c4pack, b_inv);
            ir_builder_set_insert_point(ctx->builder, b_c4pack);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* b1sh4 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b1u4, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* b2sh4 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b2u4, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* b3sh4 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b3u4, ir_value_const_int(24, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* bytes4 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T,
                                                             ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T,
                                                                                            ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, b0u, b1sh4), IR_TYPE_I64_T),
                                                                                            b2sh4),
                                                                           IR_TYPE_I64_T),
                                                             b3sh4),
                                           IR_TYPE_I64_T);
            IRValue* l4sh = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(4, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* packed4 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, bytes4, l4sh), IR_TYPE_I64_T);
            ir_builder_emit_store(ctx->builder, packed4, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_inv);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, repl_bits, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_done);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* bitsv = ir_value_reg(ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, bits_ptr), IR_TYPE_I64_T);
            return ir_lower_i64_bits_to_char(ctx, bitsv);
        }

        if (strcmp(n, "اكتب_حرف") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اكتب_حرف' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }

            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* chv = lower_expr(ctx, a1);
            IRValue* bits = ir_lower_char_bits_to_i64(ctx, chv);

            IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
            int ret_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* ret_ptr = ir_value_reg(ret_ptr_reg, i64_ptr_t);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);

            // len = bits >> 32
            IRValue* len = ir_value_reg(ir_builder_emit_shr(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int len0 = ir_builder_emit_cmp_eq(ctx->builder, len, ir_value_const_int(0, IR_TYPE_I64_T));

            // استخراج البايتات من bits (0..255) قبل إنهاء الكتلة الحالية بالـ branch.
            IRValue* b0 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(255, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* b1 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                          ir_value_reg(ir_builder_emit_shr(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T),
                                                          ir_value_const_int(255, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
            IRValue* b2 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                          ir_value_reg(ir_builder_emit_shr(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T),
                                                          ir_value_const_int(255, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
            IRValue* b3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                          ir_value_reg(ir_builder_emit_shr(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(24, IR_TYPE_I64_T)), IR_TYPE_I64_T),
                                                          ir_value_const_int(255, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);

            IRBlock* b_len0 = cf_create_block(ctx, "اكتب_حرف_len0");
            IRBlock* b_w0 = cf_create_block(ctx, "اكتب_حرف_w0");
            IRBlock* b_a0 = cf_create_block(ctx, "اكتب_حرف_a0");
            IRBlock* b_w1 = cf_create_block(ctx, "اكتب_حرف_w1");
            IRBlock* b_a1 = cf_create_block(ctx, "اكتب_حرف_a1");
            IRBlock* b_w2 = cf_create_block(ctx, "اكتب_حرف_w2");
            IRBlock* b_a2 = cf_create_block(ctx, "اكتب_حرف_a2");
            IRBlock* b_w3 = cf_create_block(ctx, "اكتب_حرف_w3");
            IRBlock* b_a3 = cf_create_block(ctx, "اكتب_حرف_a3");
            IRBlock* b_done = cf_create_block(ctx, "اكتب_حرف_نهاية");
            if (!b_len0 || !b_w0 || !b_a0 || !b_w1 || !b_a1 || !b_w2 || !b_a2 || !b_w3 || !b_a3 || !b_done) {
                return ir_builder_const_i64(-1);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, ir_value_reg(len0, IR_TYPE_I1_T), b_len0, b_w0);
            }

            ir_builder_set_insert_point(ctx->builder, b_len0);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), ret_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_w0);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* a0v[2] = { cast_to(ctx, b0, IR_TYPE_I32_T), fv };
            int r0 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, a0v, 2);
            int fail0 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r0, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail0, IR_TYPE_I1_T), b_done, b_a0);

            ir_builder_set_insert_point(ctx->builder, b_a0);
            ir_lower_set_loc(ctx->builder, expr);
            int is1 = ir_builder_emit_cmp_eq(ctx->builder, len, ir_value_const_int(1, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is1, IR_TYPE_I1_T), b_a3, b_w1);

            ir_builder_set_insert_point(ctx->builder, b_w1);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* a1v[2] = { cast_to(ctx, b1, IR_TYPE_I32_T), fv };
            int r1 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, a1v, 2);
            int fail1 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r1, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail1, IR_TYPE_I1_T), b_done, b_a1);

            ir_builder_set_insert_point(ctx->builder, b_a1);
            ir_lower_set_loc(ctx->builder, expr);
            int is2 = ir_builder_emit_cmp_eq(ctx->builder, len, ir_value_const_int(2, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is2, IR_TYPE_I1_T), b_a3, b_w2);

            ir_builder_set_insert_point(ctx->builder, b_w2);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* a2v[2] = { cast_to(ctx, b2, IR_TYPE_I32_T), fv };
            int r2 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, a2v, 2);
            int fail2 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r2, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail2, IR_TYPE_I1_T), b_done, b_a2);

            ir_builder_set_insert_point(ctx->builder, b_a2);
            ir_lower_set_loc(ctx->builder, expr);
            int is3 = ir_builder_emit_cmp_eq(ctx->builder, len, ir_value_const_int(3, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is3, IR_TYPE_I1_T), b_a3, b_w3);

            ir_builder_set_insert_point(ctx->builder, b_w3);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* a3v[2] = { cast_to(ctx, b3, IR_TYPE_I32_T), fv };
            int r3 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, a3v, 2);
            int fail3 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r3, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail3, IR_TYPE_I1_T), b_done, b_a3);

            ir_builder_set_insert_point(ctx->builder, b_a3);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), ret_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_done);
            ir_lower_set_loc(ctx->builder, expr);
            int out_r = ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, ret_ptr);
            return ir_value_reg(out_r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "اقرأ_ملف") == 0) {
            if (!a0 || !a1 || !a2 || a2->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اقرأ_ملف' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* bv = cast_to(ctx, lower_expr(ctx, a1), i8_ptr_t);
            IRValue* nv = ensure_i64(ctx, lower_expr(ctx, a2));
            IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
            IRValue* args[4] = { bv, one, nv, fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "fread", IR_TYPE_I64_T, args, 4);
            if (r < 0) return ir_builder_const_i64(0);
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "اكتب_ملف") == 0) {
            if (!a0 || !a1 || !a2 || a2->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اكتب_ملف' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* bv = cast_to(ctx, lower_expr(ctx, a1), i8_ptr_t);
            IRValue* nv = ensure_i64(ctx, lower_expr(ctx, a2));
            IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
            IRValue* args[4] = { bv, one, nv, fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "fwrite", IR_TYPE_I64_T, args, 4);
            if (r < 0) return ir_builder_const_i64(0);
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "نهاية_ملف") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'نهاية_ملف' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_I1_T);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* args[1] = { fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "feof", IR_TYPE_I32_T, args, 1);
            if (r < 0) return ir_value_const_int(0, IR_TYPE_I1_T);
            int ne = ir_builder_emit_cmp_ne(ctx->builder, ir_value_reg(r, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            return ir_value_reg(ne, IR_TYPE_I1_T);
        }

        if (strcmp(n, "موقع_ملف") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'موقع_ملف' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* args[1] = { fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, fn_tell, IR_TYPE_I64_T, args, 1);
            if (r < 0) return ir_builder_const_i64(-1);
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "اذهب_لموقع") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اذهب_لموقع' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* pos = ensure_i64(ctx, lower_expr(ctx, a1));
            IRValue* wh = ir_value_const_int(0, IR_TYPE_I64_T); // SEEK_SET
            IRValue* args[3] = { fv, pos, wh };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, fn_seek, IR_TYPE_I32_T, args, 3);
            if (r < 0) return ir_builder_const_i64(-1);
            return cast_to(ctx, ir_value_reg(r, IR_TYPE_I32_T), IR_TYPE_I64_T);
        }

        if (strcmp(n, "اقرأ_سطر") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اقرأ_سطر' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, char_ptr_t);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            return ir_lower_builtin_read_line_from_file(ctx, expr, fv);
        }

        if (strcmp(n, "اكتب_سطر") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اكتب_سطر' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }

            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);

            IRValue* s_i8 = NULL;
            bool free_s = false;
            if (a1->type == NODE_STRING && a1->data.string_lit.value) {
                s_i8 = ir_builder_const_string(ctx->builder, a1->data.string_lit.value);
            } else {
                IRValue* sv = lower_expr(ctx, a1);
                s_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, sv);
                free_s = true;
            }

            int ret_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* ret_ptr = ir_value_reg(ret_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
            ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);

            IRBlock* ok1 = cf_create_block(ctx, "اكتب_سطر_بعد_نص");
            IRBlock* ok2 = cf_create_block(ctx, "اكتب_سطر_بعد_سطر");
            IRBlock* done = cf_create_block(ctx, "اكتب_سطر_نهاية");
            if (!ok1 || !ok2 || !done) {
                if (free_s) {
                    IRValue* fargs[1] = { s_i8 };
                    ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
                }
                return ir_builder_const_i64(-1);
            }

            IRValue* args1[2] = { s_i8, fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r1 = ir_builder_emit_call(ctx->builder, "fputs", IR_TYPE_I32_T, args1, 2);
            int fail1 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r1, IR_TYPE_I32_T),
                                              ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail1, IR_TYPE_I1_T), done, ok1);

            ir_builder_set_insert_point(ctx->builder, ok1);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* nl = ir_value_const_int(10, IR_TYPE_I32_T);
            IRValue* args2[2] = { nl, fv };
            int r2 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, args2, 2);
            int fail2 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r2, IR_TYPE_I32_T),
                                              ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail2, IR_TYPE_I1_T), done, ok2);

            ir_builder_set_insert_point(ctx->builder, ok2);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), ret_ptr);
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, done);
            ir_lower_set_loc(ctx->builder, expr);
            if (free_s) {
                IRValue* fargs[1] = { s_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            int out_r = ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, ret_ptr);
            return ir_value_reg(out_r, IR_TYPE_I64_T);
        }
    }

    // حاول العثور على توقيع الدالة داخل وحدة IR (مباشر)، أو تفسير الاسم كـ مؤشر دالة (غير مباشر).
    IRFunc* callee = NULL;
    IRValue* callee_val = NULL;
    IRType* callee_sig = NULL; // IR_TYPE_FUNC عند النداء غير المباشر

    if (expr->data.call.name) {
        const char* n = expr->data.call.name;

        // 1) متغير محلي من نوع دالة(...)->...
        IRLowerBinding* b = find_local(ctx, n);
        if (b && b->value_type && b->value_type->kind == IR_TYPE_FUNC) {
            const char* storage_name = ir_lower_binding_storage_name(b);
            IRType* ft = b->value_type;
            IRValue* ptr = NULL;
            if (b->is_static_storage) {
                ptr = ir_value_global(storage_name, ft);
            } else {
                IRType* ptr_type = ir_type_ptr(ft ? ft : IR_TYPE_I64_T);
                ptr = ir_value_reg(b->ptr_reg, ptr_type);
            }
            int loaded = ir_builder_emit_load(ctx->builder, ft, ptr);
            callee_val = ir_value_reg(loaded, ft);
            callee_sig = ft;
        }

        // 2) متغير عام من نوع دالة(...)->...
        if (!callee_sig && ctx->builder && ctx->builder->module) {
            IRGlobal* g = ir_module_find_global(ctx->builder->module, n);
            if (g && g->type && g->type->kind == IR_TYPE_FUNC) {
                IRValue* gptr = ir_value_global(n, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                callee_val = ir_value_reg(loaded, g->type);
                callee_sig = g->type;
            }
        }

        // 3) إذا لم يكن هناك مؤشر دالة في النطاق/العوام، فاعتبره نداء مباشراً للدالة.
        if (!callee_sig && m) {
            callee = ir_module_find_func(m, n);
        }
    }

    IRType* ret_type = NULL;
    if (callee && callee->ret_type) {
        ret_type = callee->ret_type;
    } else if (callee_sig && callee_sig->kind == IR_TYPE_FUNC) {
        ret_type = callee_sig->data.func.ret;
    } else {
        ret_type = ir_type_from_datatype_ex(m, expr->inferred_type, expr->inferred_func_sig);
    }
    if (!ret_type) ret_type = IR_TYPE_I64_T;
    bool callee_is_variadic = (callee && callee->is_variadic);
    int callee_fixed_params = 0;
    if (callee) {
        callee_fixed_params = callee->param_count;
        if (callee_is_variadic && callee_fixed_params > 0) {
            callee_fixed_params -= 1; // معامل خفي: __baa_va_base
        }
    }

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
        IRType* exp_t = NULL;
        if (callee && i < callee_fixed_params && callee->params) {
            exp_t = callee->params[i].type;
        } else if (callee_sig && callee_sig->kind == IR_TYPE_FUNC &&
                   i < callee_sig->data.func.param_count && callee_sig->data.func.params) {
            exp_t = callee_sig->data.func.params[i];
        }
        if (exp_t) {
            av = cast_to(ctx, av, exp_t);
        }
        args[i++] = av;
    }

    IRValue** call_args = args;
    int call_arg_count = arg_count;
    if (callee_is_variadic) {
        IRValue* va_base = ir_lower_pack_variadic_args(ctx, expr, args, arg_count);
        int fixed = callee_fixed_params;
        if (fixed < 0) fixed = 0;
        if (fixed > arg_count) fixed = arg_count;

        call_arg_count = fixed + 1;
        call_args = (IRValue**)malloc(sizeof(IRValue*) * (size_t)call_arg_count);
        if (!call_args) {
            if (args) free(args);
            ir_lower_report_error(ctx, expr, "فشل تخصيص معاملات النداء المتغير.");
            return ir_builder_const_i64(0);
        }
        for (int k = 0; k < fixed; k++) {
            call_args[k] = args[k];
        }
        call_args[fixed] = va_base;
    }

    int dest = -1;
    if (callee) {
        dest = ir_builder_emit_call(ctx->builder, expr->data.call.name, ret_type, call_args, call_arg_count);
    } else if (callee_val) {
        dest = ir_builder_emit_call_indirect(ctx->builder, callee_val, ret_type, call_args, call_arg_count);
    } else {
        ir_lower_report_error(ctx, expr, "استدعاء غير صالح: '%s' ليس دالة ولا مؤشر دالة.",
                              expr->data.call.name ? expr->data.call.name : "???");
        dest = -1;
    }

    if (call_args && call_args != args) free(call_args);
    if (args) free(args);

    if (dest < 0) {
        // Void call used as expression: return 0 (placeholder).
        return ir_builder_const_i64(0);
    }

    return ir_value_reg(dest, ret_type);
}

static IRValue* ir_lower_builtin_print_format_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* fmt_node = call_expr->data.call.args;
    Node* args_after = fmt_node ? fmt_node->next : NULL;
    if (!fmt_node || fmt_node->type != NODE_STRING || !fmt_node->data.string_lit.value) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: نص التنسيق يجب أن يكون ثابتاً (literal).");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    BaaFmtSpec specs[BAA_FMT_MAX_SPECS];
    int spec_count = 0;
    char* cfmt = NULL;
    if (!baa_fmt_translate_ar(ctx, fmt_node, fmt_node->data.string_lit.value, false, &cfmt, specs, &spec_count)) {
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* fmt_val = ir_builder_const_string(b, cfmt);
    free(cfmt);
    if (!fmt_val) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: فشل إنشاء ثابت نص التنسيق.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRValue* call_args[1 + BAA_FMT_MAX_SPECS];
    IRValue* to_free[BAA_FMT_MAX_SPECS];
    int free_count = 0;

    call_args[0] = fmt_val;

    int idx = 0;
    for (Node* an = args_after; an && idx < spec_count; an = an->next, idx++) {
        IRValue* v = lower_expr(ctx, an);
        switch (specs[idx].kind) {
            case BAA_FMT_SPEC_STR: {
                IRValue* s_alloc = ir_lower_baa_string_to_cstr_alloc(ctx, call_expr, v);
                if (free_count < BAA_FMT_MAX_SPECS) {
                    to_free[free_count++] = s_alloc;
                }
                v = ir_lower_cstr_or_literal_on_null(ctx, call_expr, s_alloc, "عدم");
                break;
            }
            case BAA_FMT_SPEC_CHAR:
                v = ir_lower_char_to_cstr_tmp(ctx, call_expr, v);
                break;
            case BAA_FMT_SPEC_F64:
            case BAA_FMT_SPEC_F64_SCI:
                v = cast_to(ctx, v, IR_TYPE_F64_T);
                break;
            case BAA_FMT_SPEC_U64:
            case BAA_FMT_SPEC_HEX:
                v = cast_to(ctx, v, IR_TYPE_U64_T);
                break;
            case BAA_FMT_SPEC_PTR:
                v = cast_to(ctx, v, i8_ptr_t);
                break;
            case BAA_FMT_SPEC_I64:
            default:
                v = ensure_i64(ctx, v);
                break;
        }
        call_args[1 + idx] = v ? v : ir_value_const_int(0, IR_TYPE_I64_T);
    }

    ir_lower_set_loc(b, call_expr);
    ir_builder_emit_call_void(b, "printf", call_args, 1 + spec_count);

    for (int i = 0; i < free_count; i++) {
        IRValue* fargs[1] = { to_free[i] };
        ir_builder_emit_call_void(b, "free", fargs, 1);
    }

    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_format_string_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_value_const_int(0, get_char_ptr_type(NULL));

    Node* fmt_node = call_expr->data.call.args;
    Node* args_after = fmt_node ? fmt_node->next : NULL;
    if (!fmt_node || fmt_node->type != NODE_STRING || !fmt_node->data.string_lit.value) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: نص التنسيق يجب أن يكون ثابتاً (literal).");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_value_const_int(0, get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL));
    }

    BaaFmtSpec specs[BAA_FMT_MAX_SPECS];
    int spec_count = 0;
    char* cfmt = NULL;
    if (!baa_fmt_translate_ar(ctx, fmt_node, fmt_node->data.string_lit.value, false, &cfmt, specs, &spec_count)) {
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_value_const_int(0, get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL));
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    IRValue* fmt_val = ir_builder_const_string(b, cfmt);
    free(cfmt);
    if (!fmt_val) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: فشل إنشاء ثابت نص التنسيق.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_value_const_int(0, char_ptr_t);
    }

    IRValue* tmp_args[3 + BAA_FMT_MAX_SPECS];
    IRValue* to_free[BAA_FMT_MAX_SPECS];
    int free_count = 0;

    tmp_args[0] = ir_value_const_int(0, i8_ptr_t);
    tmp_args[1] = ir_value_const_int(0, IR_TYPE_I64_T);
    tmp_args[2] = fmt_val;

    int idx = 0;
    for (Node* an = args_after; an && idx < spec_count; an = an->next, idx++) {
        IRValue* v = lower_expr(ctx, an);
        switch (specs[idx].kind) {
            case BAA_FMT_SPEC_STR: {
                IRValue* s_alloc = ir_lower_baa_string_to_cstr_alloc(ctx, call_expr, v);
                if (free_count < BAA_FMT_MAX_SPECS) {
                    to_free[free_count++] = s_alloc;
                }
                v = ir_lower_cstr_or_literal_on_null(ctx, call_expr, s_alloc, "عدم");
                break;
            }
            case BAA_FMT_SPEC_CHAR:
                v = ir_lower_char_to_cstr_tmp(ctx, call_expr, v);
                break;
            case BAA_FMT_SPEC_F64:
            case BAA_FMT_SPEC_F64_SCI:
                v = cast_to(ctx, v, IR_TYPE_F64_T);
                break;
            case BAA_FMT_SPEC_U64:
            case BAA_FMT_SPEC_HEX:
                v = cast_to(ctx, v, IR_TYPE_U64_T);
                break;
            case BAA_FMT_SPEC_PTR:
                v = cast_to(ctx, v, i8_ptr_t);
                break;
            case BAA_FMT_SPEC_I64:
            default:
                v = ensure_i64(ctx, v);
                break;
        }
        tmp_args[3 + idx] = v ? v : ir_value_const_int(0, IR_TYPE_I64_T);
    }

    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);

    IRBlock* b_bad = cf_create_block(ctx, "نسق_فشل");
    IRBlock* b_ok = cf_create_block(ctx, "نسق_عمل");
    IRBlock* b_ret = cf_create_block(ctx, "نسق_إرجاع");
    if (!b_bad || !b_ok || !b_ret) {
        for (int i = 0; i < free_count; i++) {
            IRValue* fargs[1] = { to_free[i] };
            ir_builder_emit_call_void(b, "free", fargs, 1);
        }
        return ir_value_const_int(0, char_ptr_t);
    }

    ir_lower_set_loc(b, call_expr);
    int n_r = ir_builder_emit_call(b, "snprintf", IR_TYPE_I32_T, tmp_args, 3 + spec_count);
    IRValue* n_i32 = ir_value_reg(n_r, IR_TYPE_I32_T);
    IRValue* n_i64 = cast_to(ctx, n_i32, IR_TYPE_I64_T);
    int neg_r = ir_builder_emit_cmp_lt(b, n_i64, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* neg = ir_value_reg(neg_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, neg, b_bad, b_ok);

    ir_builder_set_insert_point(b, b_bad);
    ir_lower_set_loc(b, call_expr);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_ok);
    ir_lower_set_loc(b, call_expr);
    IRValue* size1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, n_i64, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* margs[1] = { size1 };
    int buf_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
    IRValue* buf = ir_value_reg(buf_r, i8_ptr_t);

    tmp_args[0] = buf;
    tmp_args[1] = size1;
    tmp_args[2] = fmt_val;
    (void)ir_builder_emit_call(b, "snprintf", IR_TYPE_I32_T, tmp_args, 3 + spec_count);

    IRValue* baa = ir_lower_cstr_to_baa_string_alloc(ctx, call_expr, buf);
    IRValue* fbuf[1] = { buf };
    ir_builder_emit_call_void(b, "free", fbuf, 1);
    ir_builder_emit_store(b, baa, res_ptr);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_ret);
    ir_lower_set_loc(b, call_expr);
    int rr = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    IRValue* outv = ir_value_reg(rr, char_ptr_t);

    for (int i = 0; i < free_count; i++) {
        IRValue* fargs[1] = { to_free[i] };
        ir_builder_emit_call_void(b, "free", fargs, 1);
    }

    return outv;
}

static IRValue* ir_lower_builtin_read_num_stdin_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    if (call_expr->data.call.args) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'اقرأ_رقم' لا يقبل معاملات.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    int tmp_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* tmp_ptr = ir_value_reg(tmp_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), tmp_ptr);

    IRValue* fmt_val = ir_builder_const_string(b, "%lld");
    IRValue* args2[2] = { fmt_val, tmp_ptr };
    ir_lower_set_loc(b, call_expr);
    (void)ir_builder_emit_call(b, "scanf", IR_TYPE_I32_T, args2, 2);

    int lr = ir_builder_emit_load(b, IR_TYPE_I64_T, tmp_ptr);
    return ir_value_reg(lr, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_read_line_stdin_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_value_const_int(0, get_char_ptr_type(NULL));

    if (call_expr->data.call.args) {
        // دع مسار اقرأ_سطر(ملف) يمر إلى builtins الملفات (v0.3.12).
        return ir_value_const_int(0, get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL));
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* i32_ptr_t = ir_type_ptr(IR_TYPE_I32_T);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    // res: نص باء أو NULL (عند EOF قبل أي بايت)
    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);

    int buf_ptr_reg = ir_builder_emit_alloca(b, i8_ptr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, i8_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), buf_ptr);

    int len_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int cap_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int last_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I32_T);
    IRValue* len_ptr = ir_value_reg(len_ptr_reg, i64_ptr_t);
    IRValue* cap_ptr = ir_value_reg(cap_ptr_reg, i64_ptr_t);
    IRValue* last_ptr = ir_value_reg(last_ptr_reg, i32_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), len_ptr);
    ir_builder_emit_store(b, ir_value_const_int(256, IR_TYPE_I64_T), cap_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I32_T), last_ptr);

    IRValue* cap0 = ir_value_const_int(256, IR_TYPE_I64_T);
    IRValue* margs0[1] = { cap0 };
    ir_lower_set_loc(b, call_expr);
    int buf0_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs0, 1);
    IRValue* buf0 = ir_value_reg(buf0_r, i8_ptr_t);
    ir_builder_emit_store(b, buf0, buf_ptr);

    IRBlock* head = cf_create_block(ctx, "اقرأ_سطر_تحقق");
    IRBlock* check = cf_create_block(ctx, "اقرأ_سطر_سعة");
    IRBlock* grow = cf_create_block(ctx, "اقرأ_سطر_توسيع");
    IRBlock* storeb = cf_create_block(ctx, "اقرأ_سطر_خزن");
    IRBlock* done = cf_create_block(ctx, "اقرأ_سطر_نهاية");
    IRBlock* nullb = cf_create_block(ctx, "اقرأ_سطر_NULL");
    IRBlock* workb = cf_create_block(ctx, "اقرأ_سطر_عمل");
    IRBlock* retb = cf_create_block(ctx, "اقرأ_سطر_إرجاع");
    if (!head || !check || !grow || !storeb || !done || !nullb || !workb || !retb) {
        return ir_value_const_int(0, char_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    // head: ch = getchar(); إذا EOF أو '\n' -> done، وإلا -> check
    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, call_expr);
    int ch_r = ir_builder_emit_call(b, "getchar", IR_TYPE_I32_T, NULL, 0);
    IRValue* ch = ir_value_reg(ch_r, IR_TYPE_I32_T);
    ir_builder_emit_store(b, ch, last_ptr);
    IRValue* ch_i64 = cast_to(ctx, ch, IR_TYPE_I64_T);
    int is_eof_r = ir_builder_emit_cmp_eq(b, ch_i64, ir_value_const_int(-1, IR_TYPE_I64_T));
    IRValue* is_eof = ir_value_reg(is_eof_r, IR_TYPE_I1_T);
    int is_nl_r = ir_builder_emit_cmp_eq(b, ch_i64, ir_value_const_int(10, IR_TYPE_I64_T));
    IRValue* is_nl = ir_value_reg(is_nl_r, IR_TYPE_I1_T);
    int stop_r = ir_builder_emit_or(b, IR_TYPE_I1_T, is_eof, is_nl);
    IRValue* stop = ir_value_reg(stop_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, stop, done, check);

    // check: إن احتجنا توسعة -> grow else -> store
    ir_builder_set_insert_point(b, check);
    ir_lower_set_loc(b, call_expr);
    IRValue* lenv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* capv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, cap_ptr), IR_TYPE_I64_T);
    IRValue* need = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, lenv, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int need_grow_r = ir_builder_emit_cmp_ge(b, need, capv);
    IRValue* need_grow = ir_value_reg(need_grow_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, need_grow, grow, storeb);

    // grow: cap*=2; buf=realloc(buf,cap); ثم -> store
    ir_builder_set_insert_point(b, grow);
    ir_lower_set_loc(b, call_expr);
    IRValue* cap2 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, capv, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* bufv0 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* rargs[2] = { bufv0, cap2 };
    int nb_r = ir_builder_emit_call(b, "realloc", i8_ptr_t, rargs, 2);
    IRValue* nb = ir_value_reg(nb_r, i8_ptr_t);
    ir_builder_emit_store(b, nb, buf_ptr);
    ir_builder_emit_store(b, cap2, cap_ptr);
    ir_builder_emit_br(b, storeb);

    // store: buf[len]=byte; len++; -> head
    ir_builder_set_insert_point(b, storeb);
    ir_lower_set_loc(b, call_expr);
    IRValue* bufv1 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* len_store = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* pos = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, bufv1, len_store), i8_ptr_t);
    IRValue* last = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I32_T, last_ptr), IR_TYPE_I32_T);
    ir_builder_emit_store(b, cast_to(ctx, last, IR_TYPE_I8_T), pos);
    IRValue* len2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, len_store, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    ir_builder_emit_store(b, len2, len_ptr);
    ir_builder_emit_br(b, head);

    // done: إذا EOF و len==0 -> NULL، وإلا -> work
    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, call_expr);
    IRValue* lenf = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* last2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I32_T, last_ptr), IR_TYPE_I32_T);
    IRValue* last2_i64 = cast_to(ctx, last2, IR_TYPE_I64_T);
    int eof2_r = ir_builder_emit_cmp_eq(b, last2_i64, ir_value_const_int(-1, IR_TYPE_I64_T));
    IRValue* eof2 = ir_value_reg(eof2_r, IR_TYPE_I1_T);
    int len0_r = ir_builder_emit_cmp_eq(b, lenf, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* len0 = ir_value_reg(len0_r, IR_TYPE_I1_T);
    int ret_null_r = ir_builder_emit_and(b, IR_TYPE_I1_T, eof2, len0);
    IRValue* ret_null = ir_value_reg(ret_null_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, ret_null, nullb, workb);

    ir_builder_set_insert_point(b, nullb);
    ir_lower_set_loc(b, call_expr);
    IRValue* bufv2 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* f0[1] = { bufv2 };
    ir_builder_emit_call_void(b, "free", f0, 1);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);
    ir_builder_emit_br(b, retb);

    ir_builder_set_insert_point(b, workb);
    ir_lower_set_loc(b, call_expr);
    IRValue* bufv3 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* termp = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, bufv3, lenf), i8_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), termp);
    IRValue* baa = ir_lower_cstr_to_baa_string_alloc(ctx, call_expr, bufv3);
    IRValue* f1[1] = { bufv3 };
    ir_builder_emit_call_void(b, "free", f1, 1);
    ir_builder_emit_store(b, baa, res_ptr);
    ir_builder_emit_br(b, retb);

    ir_builder_set_insert_point(b, retb);
    ir_lower_set_loc(b, call_expr);
    int rr = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(rr, char_ptr_t);
}

static IRValue* ir_lower_builtin_scan_format_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* fmt_node = call_expr->data.call.args;
    Node* args_after = fmt_node ? fmt_node->next : NULL;
    if (!fmt_node || fmt_node->type != NODE_STRING || !fmt_node->data.string_lit.value) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: نص التنسيق في 'اقرأ_منسق' يجب أن يكون ثابتاً (literal).");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    BaaFmtSpec specs[BAA_FMT_MAX_SPECS];
    int spec_count = 0;
    char* cfmt = NULL;
    if (!baa_fmt_translate_ar(ctx, fmt_node, fmt_node->data.string_lit.value, true, &cfmt, specs, &spec_count)) {
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    IRValue* fmt_val = ir_builder_const_string(b, cfmt);
    free(cfmt);
    if (!fmt_val) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: فشل إنشاء ثابت نص التنسيق.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRValue* call_args[1 + BAA_FMT_MAX_SPECS];
    call_args[0] = fmt_val;

    IRValue* str_bufs[BAA_FMT_MAX_SPECS];
    IRValue* str_out_ptrs[BAA_FMT_MAX_SPECS];
    int str_spec_index[BAA_FMT_MAX_SPECS];
    int str_count = 0;

    int idx = 0;
    for (Node* an = args_after; an && idx < spec_count; an = an->next, idx++) {
        IRValue* v = lower_expr(ctx, an);
        switch (specs[idx].kind) {
            case BAA_FMT_SPEC_STR: {
                int w = specs[idx].has_width ? specs[idx].width : 64;
                if (w <= 0) w = 64;
                IRValue* outp = cast_to(ctx, v, char_ptr_ptr_t);
                IRValue* sz = ir_value_const_int((int64_t)w + 1, IR_TYPE_I64_T);
                IRValue* margs[1] = { sz };
                int buf_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
                IRValue* buf = ir_value_reg(buf_r, i8_ptr_t);
                call_args[1 + idx] = buf;
                if (str_count < BAA_FMT_MAX_SPECS) {
                    str_bufs[str_count] = buf;
                    str_out_ptrs[str_count] = outp;
                    str_spec_index[str_count] = idx;
                    str_count++;
                }
                break;
            }
            case BAA_FMT_SPEC_I64:
                call_args[1 + idx] = cast_to(ctx, v, ir_type_ptr(IR_TYPE_I64_T));
                break;
            case BAA_FMT_SPEC_U64:
            case BAA_FMT_SPEC_HEX:
                call_args[1 + idx] = cast_to(ctx, v, ir_type_ptr(IR_TYPE_U64_T));
                break;
            case BAA_FMT_SPEC_F64:
            case BAA_FMT_SPEC_F64_SCI:
                call_args[1 + idx] = cast_to(ctx, v, ir_type_ptr(IR_TYPE_F64_T));
                break;
            default:
                ir_lower_report_error(ctx, call_expr, "تنسيق عربي: مواصفة غير مدعومة في 'اقرأ_منسق'.");
                call_args[1 + idx] = cast_to(ctx, v, i8_ptr_t);
                break;
        }
    }

    ir_lower_set_loc(b, call_expr);
    int rc_r = ir_builder_emit_call(b, "scanf", IR_TYPE_I32_T, call_args, 1 + spec_count);
    IRValue* rc_i32 = ir_value_reg(rc_r, IR_TYPE_I32_T);
    IRValue* rc_i64 = cast_to(ctx, rc_i32, IR_TYPE_I64_T);

    // لكل %ن: إذا rc > index -> حوّل وخزّن، وإلا لا تغيّر قيمة نص المستخدم.
    for (int si = 0; si < str_count; si++) {
        int spec_i = str_spec_index[si];
        int ok_r = ir_builder_emit_cmp_gt(b, rc_i64, ir_value_const_int((int64_t)spec_i, IR_TYPE_I64_T));
        IRValue* ok = ir_value_reg(ok_r, IR_TYPE_I1_T);

        IRBlock* yesb = cf_create_block(ctx, "اقرأ_منسق_نص_نعم");
        IRBlock* nob = cf_create_block(ctx, "اقرأ_منسق_نص_لا");
        IRBlock* nextb = cf_create_block(ctx, "اقرأ_منسق_نص_تابع");
        if (!yesb || !nob || !nextb) continue;

        ir_builder_emit_br_cond(b, ok, yesb, nob);

        ir_builder_set_insert_point(b, yesb);
        ir_lower_set_loc(b, call_expr);
        IRValue* baa = ir_lower_cstr_to_baa_string_alloc(ctx, call_expr, str_bufs[si]);
        ir_builder_emit_store(b, baa, str_out_ptrs[si]);
        IRValue* f0[1] = { str_bufs[si] };
        ir_builder_emit_call_void(b, "free", f0, 1);
        ir_builder_emit_br(b, nextb);

        ir_builder_set_insert_point(b, nob);
        ir_lower_set_loc(b, call_expr);
        IRValue* f1[1] = { str_bufs[si] };
        ir_builder_emit_call_void(b, "free", f1, 1);
        ir_builder_emit_br(b, nextb);

        ir_builder_set_insert_point(b, nextb);
        ir_lower_set_loc(b, call_expr);
    }

    return rc_i64;
}

static int64_t ir_lower_pointer_elem_size_meta(DataType base, int depth)
{
    if (depth > 1) return 8;
    if (depth <= 0) return 1;

    switch (base) {
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

static IRValue* ir_lower_pointer_index_chain(IRLowerCtx* ctx,
                                             Node* site,
                                             IRValue* base_ptr_val,
                                             DataType base_type,
                                             const char* base_type_name,
                                             int depth,
                                             Node* indices,
                                             int index_count,
                                             bool return_address,
                                             IRType** out_pointee_type)
{
    (void)base_type_name;

    if (out_pointee_type) *out_pointee_type = IR_TYPE_I64_T;
    if (!ctx || !ctx->builder || !base_ptr_val || !indices) {
        return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
    }

    int effective_count = index_count > 0 ? index_count : ir_lower_index_count(indices);
    if (effective_count <= 0) {
        ir_lower_report_error(ctx, site, "فهرسة مؤشر بدون فهارس غير صالحة.");
        return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
    }

    IRModule* m = ctx->builder->module;
    IRType* void_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

    DataType cur_base = base_type;
    int cur_depth = depth;

    IRValue* cur_ptr = cast_to(ctx, base_ptr_val, void_ptr_t);
    Node* idx_node = indices;

    for (int i = 0; i < effective_count && idx_node; i++, idx_node = idx_node->next) {
        if (cur_depth <= 0) {
            ir_lower_report_error(ctx, site, "عمق المؤشر غير صالح أثناء فهرسة المؤشر.");
            return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
        }

        IRValue* idx0 = ensure_i64(ctx, lower_expr(ctx, idx_node));
        int64_t elem_size = ir_lower_pointer_elem_size_meta(cur_base, cur_depth);
        IRValue* idx_bytes = idx0;
        if (elem_size > 1) {
            int mr = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, idx0,
                                         ir_value_const_int(elem_size, IR_TYPE_I64_T));
            idx_bytes = ir_value_reg(mr, IR_TYPE_I64_T);
        }

        // نستخدم i8* حتى تكون الإزاحة "بالبايت" (delta bytes).
        int ep = ir_builder_emit_ptr_offset(ctx->builder, void_ptr_t, cur_ptr, idx_bytes);
        IRValue* elem_addr_i8 = ir_value_reg(ep, void_ptr_t);

        bool is_last = (i == effective_count - 1);

        if (is_last) {
            IRType* elem_t = (cur_depth > 1) ? void_ptr_t : ir_type_from_datatype(m, cur_base);
            IRType* ptr_to_elem_t = ir_type_ptr(elem_t);
            int cp = ir_builder_emit_cast(ctx->builder, elem_addr_i8, ptr_to_elem_t);
            IRValue* typed_addr = ir_value_reg(cp, ptr_to_elem_t);
            if (out_pointee_type) *out_pointee_type = elem_t;
            if (return_address) {
                return typed_addr;
            }
            int loaded = ir_builder_emit_load(ctx->builder, elem_t, typed_addr);
            return ir_value_reg(loaded, elem_t);
        }

        // ليست آخر فهرسة: نحتاج قيمة قابلة للفهرسة لاحقاً
        if (cur_depth > 1) {
            // العنصر هو مؤشر (نحمّله كـ void* ثم نكمل على عمق أقل)
            IRType* elem_t = void_ptr_t;
            IRType* ptr_to_elem_t = ir_type_ptr(elem_t);
            int cp = ir_builder_emit_cast(ctx->builder, elem_addr_i8, ptr_to_elem_t);
            IRValue* typed_addr = ir_value_reg(cp, ptr_to_elem_t);
            int loaded = ir_builder_emit_load(ctx->builder, elem_t, typed_addr);
            cur_ptr = ir_value_reg(loaded, elem_t);
            cur_ptr = cast_to(ctx, cur_ptr, void_ptr_t);
            cur_depth--;
            continue;
        }

        // cur_depth == 1: العنصر هو نوع الأساس مباشرة
        if (cur_base == TYPE_STRING) {
            // فهرسة ثانية على نص: str[j] → حرف
            IRType* str_t = get_char_ptr_type(m);
            IRType* ptr_to_str_t = ir_type_ptr(str_t);
            int cp0 = ir_builder_emit_cast(ctx->builder, elem_addr_i8, ptr_to_str_t);
            IRValue* str_slot = ir_value_reg(cp0, ptr_to_str_t);
            int ld0 = ir_builder_emit_load(ctx->builder, str_t, str_slot);
            IRValue* str_ptr = ir_value_reg(ld0, str_t);

            int remaining = effective_count - i - 1;
            if (remaining != 1 || !idx_node->next) {
                ir_lower_report_error(ctx, site, "فهرسة متعددة غير مدعومة بعد الوصول إلى 'نص'.");
                return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
            }

            Node* inner_node = idx_node->next;
            IRValue* inner_idx = ensure_i64(ctx, lower_expr(ctx, inner_node));
            int ep2 = ir_builder_emit_ptr_offset(ctx->builder, str_t, str_ptr, inner_idx);
            IRValue* ch_addr = ir_value_reg(ep2, str_t);
            if (out_pointee_type) *out_pointee_type = IR_TYPE_CHAR_T;
            if (return_address) {
                return ch_addr;
            }
            int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, ch_addr);
            return ir_value_reg(ch, IR_TYPE_CHAR_T);
        }

        ir_lower_report_error(ctx, site, "فهرسة متعددة تتطلب مؤشراً/نصاً كعنصر وسيط.");
        return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
    }

    ir_lower_report_error(ctx, site, "عدد الفهارس غير صالح أثناء فهرسة المؤشر.");
    return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
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

    if (expr->type == NODE_ARRAY_ACCESS) {
        const char* name = expr->data.array_op.name;
        int index_count = expr->data.array_op.index_count;
        if (index_count <= 0) index_count = ir_lower_index_count(expr->data.array_op.indices);

        IRLowerBinding* b = find_local(ctx, name);
        if (b) {
            const char* storage_name = ir_lower_binding_storage_name(b);

            // فهرسة مؤشرات محلية: p[i] و p[i][j]...
            if (b->is_pointer) {
                IRType* ptr_t = b->value_type;
                IRValue* ptr_val = NULL;
                if (b->is_static_storage) {
                    IRValue* gptr = ir_value_global(storage_name, ptr_t);
                    int loaded = ir_builder_emit_load(ctx->builder, ptr_t, gptr);
                    ptr_val = ir_value_reg(loaded, ptr_t);
                } else {
                    IRType* slot_t = ir_type_ptr(ptr_t);
                    IRValue* slot = ir_value_reg(b->ptr_reg, slot_t);
                    int loaded = ir_builder_emit_load(ctx->builder, ptr_t, slot);
                    ptr_val = ir_value_reg(loaded, ptr_t);
                }

                return ir_lower_pointer_index_chain(ctx, expr, ptr_val,
                                                    b->ptr_base_type, b->ptr_base_type_name, b->ptr_depth,
                                                    expr->data.array_op.indices, index_count,
                                                    true, out_pointee_type);
            }

            if (!b->is_array || !b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
                ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
                for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                    (void)lower_expr(ctx, idx);
                }
                return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
            }

            bool elem_is_agg = (b->array_elem_type == TYPE_STRUCT || b->array_elem_type == TYPE_UNION);
            IRType* elem_t = elem_is_agg
                ? IR_TYPE_I8_T
                : (b->value_type->data.array.element ? b->value_type->data.array.element : IR_TYPE_I64_T);
            IRType* elem_ptr_t = ir_type_ptr(elem_t);

            IRValue* base_ptr = NULL;
            if (b->is_static_storage) {
                base_ptr = ir_value_global(storage_name, elem_t);
            } else {
                IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
                base_ptr = ir_value_reg(base_reg, elem_ptr_t);
            }

            IRValue* linear = ir_lower_build_linear_index(ctx, expr,
                                                          expr->data.array_op.indices,
                                                          index_count,
                                                          b->array_dims,
                                                          b->array_rank);
            int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
            if (out_pointee_type) *out_pointee_type = elem_t;
            return ir_value_reg(ep, elem_ptr_t);
        }

        IRGlobal* g = NULL;
        if (ctx->builder && ctx->builder->module && name) {
            g = ir_module_find_global(ctx->builder->module, name);
        }
        Node* vdecl = ir_lower_find_global_var_decl(ctx, name);
        if (vdecl && vdecl->type == NODE_VAR_DECL &&
            vdecl->data.var_decl.type == TYPE_POINTER &&
            g && g->type) {
            // فهرسة مؤشرات عامة: نحمّل قيمة المؤشر أولاً من @name ثم نفهرسه بالميتا الدلالية.
            IRValue* gptr = ir_value_global(name, g->type);
            int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
            IRValue* p = ir_value_reg(loaded, g->type);
            return ir_lower_pointer_index_chain(ctx, expr, p,
                                                vdecl->data.var_decl.ptr_base_type,
                                                vdecl->data.var_decl.ptr_base_type_name,
                                                vdecl->data.var_decl.ptr_depth,
                                                expr->data.array_op.indices, index_count,
                                                true, out_pointee_type);
        }
        if (!g || !g->type || g->type->kind != IR_TYPE_ARRAY) {
            ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة.", name ? name : "???");
            for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                (void)lower_expr(ctx, idx);
            }
            return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
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
        IRValue* base_ptr = ir_value_global(name, elem_t);

        IRValue* linear = ir_lower_build_linear_index(ctx, expr,
                                                      expr->data.array_op.indices,
                                                      index_count,
                                                      dims,
                                                      rank);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
        if (out_pointee_type) *out_pointee_type = elem_t;
        return ir_value_reg(ep, elem_ptr_t);
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

            // 3) Function reference (as a value)
            IRFunc* f = NULL;
            if (ctx->builder && ctx->builder->module && name) {
                f = ir_module_find_func(ctx->builder->module, name);
            }
            if (f) {
                IRType* pts_local[64];
                IRType** pts = NULL;
                int pc = f->param_count;
                if (pc < 0) pc = 0;
                if (pc > (int)(sizeof(pts_local) / sizeof(pts_local[0]))) {
                    pc = (int)(sizeof(pts_local) / sizeof(pts_local[0]));
                }
                for (int i = 0; i < pc; i++) {
                    pts_local[i] = (f->params && f->params[i].type) ? f->params[i].type : IR_TYPE_I64_T;
                }
                if (pc > 0) pts = pts_local;
                IRType* ft = ir_type_func(f->ret_type ? f->ret_type : IR_TYPE_VOID_T, pts, pc);
                return ir_value_func_ref(name, ft);
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

                // مؤشرات عامة: p[i] و p[i][j]...
                if (b->is_pointer) {
                    if (!expr->data.array_op.indices) {
                        ir_lower_report_error(ctx, expr, "فهرس المؤشر '%s' مفقود.", name ? name : "???");
                        return ir_builder_const_i64(0);
                    }

                    IRType* ptr_t = b->value_type;
                    IRValue* ptr_val = NULL;
                    if (b->is_static_storage) {
                        IRValue* gptr = ir_value_global(storage_name, ptr_t);
                        int loaded = ir_builder_emit_load(ctx->builder, ptr_t, gptr);
                        ptr_val = ir_value_reg(loaded, ptr_t);
                    } else {
                        IRType* slot_t = ir_type_ptr(ptr_t);
                        IRValue* slot = ir_value_reg(b->ptr_reg, slot_t);
                        int loaded = ir_builder_emit_load(ctx->builder, ptr_t, slot);
                        ptr_val = ir_value_reg(loaded, ptr_t);
                    }

                    return ir_lower_pointer_index_chain(ctx, expr, ptr_val,
                                                        b->ptr_base_type, b->ptr_base_type_name, b->ptr_depth,
                                                        expr->data.array_op.indices, index_count,
                                                        false, NULL);
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

            Node* vdecl = ir_lower_find_global_var_decl(ctx, name);
            if (vdecl && vdecl->type == NODE_VAR_DECL &&
                vdecl->data.var_decl.type == TYPE_POINTER &&
                g && g->type) {
                IRValue* gptr = ir_value_global(name, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                IRValue* p = ir_value_reg(loaded, g->type);
                return ir_lower_pointer_index_chain(ctx, expr, p,
                                                    vdecl->data.var_decl.ptr_base_type,
                                                    vdecl->data.var_decl.ptr_base_type_name,
                                                    vdecl->data.var_decl.ptr_depth,
                                                    expr->data.array_op.indices, index_count,
                                                    false, NULL);
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
                if ((op == OP_EQ || op == OP_NEQ) &&
                    expr->data.bin_op.left && expr->data.bin_op.right &&
                    (expr->data.bin_op.left->inferred_type == TYPE_STRING || expr->data.bin_op.right->inferred_type == TYPE_STRING)) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                    IRType* st = get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL);
                    if (!st) st = ir_type_ptr(IR_TYPE_CHAR_T);
                    IRValue* lhs = cast_to(ctx, lhs0, st);
                    IRValue* rhs = cast_to(ctx, rhs0, st);
                    IRCmpPred pred = ir_cmp_to_pred(op, false);
                    int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs, rhs);
                    return ir_value_reg(dest, IR_TYPE_I1_T);
                }

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

                if (expr->data.bin_op.left && expr->data.bin_op.right &&
                    (expr->data.bin_op.left->inferred_type == TYPE_FUNC_PTR || expr->data.bin_op.right->inferred_type == TYPE_FUNC_PTR)) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);

                    // توحيد ثابت "عدم" (0) ليأخذ نفس نوع مؤشر الدالة للطرف الآخر.
                    // هذا يمنع فشل متحقق الـ IR عندما يحمل "عدم" توقيعاً افتراضياً مختلفاً.
                    if (lhs0 && rhs0 && lhs0->type && rhs0->type) {
                        if (lhs0->type->kind == IR_TYPE_FUNC && !ir_types_equal(lhs0->type, rhs0->type)) {
                            if (rhs0->kind == IR_VAL_CONST_INT && rhs0->data.const_int == 0) {
                                rhs0 = ir_value_const_int(0, lhs0->type);
                            }
                        } else if (rhs0->type->kind == IR_TYPE_FUNC && !ir_types_equal(lhs0->type, rhs0->type)) {
                            if (lhs0->kind == IR_VAL_CONST_INT && lhs0->data.const_int == 0) {
                                lhs0 = ir_value_const_int(0, rhs0->type);
                            }
                        }
                    }

                    IRCmpPred pred = ir_cmp_to_pred(op, false);
                    int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs0, rhs0);
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

        case NODE_POSTFIX_OP: {
            UnaryOpType op = expr->data.unary_op.op;
            if (op != UOP_INC && op != UOP_DEC) {
                ir_lower_report_error(ctx, expr, "عملية لاحقة غير مدعومة (%d).", (int)op);
                return ir_builder_const_i64(0);
            }

            IRType* pointee = IR_TYPE_I64_T;
            IRValue* addr0 = lower_lvalue_address(ctx, expr->data.unary_op.operand, &pointee);
            if (!pointee || pointee->kind == IR_TYPE_VOID) {
                ir_lower_report_error(ctx, expr, "نوع العملية اللاحقة غير صالح.");
                return ir_builder_const_i64(0);
            }
            if (pointee->kind == IR_TYPE_F64) {
                ir_lower_report_error(ctx, expr, "الزيادة/النقصان للعشري غير مدعومة حالياً.");
                return ir_builder_const_i64(0);
            }

            IRType* ptr_t = ir_type_ptr(pointee);
            IRValue* addr = cast_to(ctx, addr0, ptr_t);

            int old_r = ir_builder_emit_load(ctx->builder, pointee, addr);
            IRValue* old_v = ir_value_reg(old_r, pointee);
            IRValue* one = ir_value_const_int(1, pointee);

            int new_r = -1;
            if (op == UOP_INC) new_r = ir_builder_emit_add(ctx->builder, pointee, old_v, one);
            else new_r = ir_builder_emit_sub(ctx->builder, pointee, old_v, one);

            IRValue* new_v = ir_value_reg(new_r, pointee);
            ir_builder_emit_store(ctx->builder, new_v, addr);
            return old_v;
        }

        case NODE_CALL_EXPR:
            return lower_call_expr(ctx, expr);

        // -----------------------------------------------------------------
        // Extra nodes (not in v0.3.0.3 checklist, but useful for robustness)
        // -----------------------------------------------------------------
        case NODE_BOOL:
            return ir_value_const_int(expr->data.bool_lit.value ? 1 : 0, IR_TYPE_I1_T);

        case NODE_NULL:
            if (expr->inferred_type == TYPE_FUNC_PTR) {
                IRType* ft = ir_type_from_funcsig(ctx->builder ? ctx->builder->module : NULL,
                                                  expr->inferred_func_sig);
                return ir_value_const_int(0, ft ? ft : ir_type_func(IR_TYPE_VOID_T, NULL, 0));
            }
            if (expr->inferred_type == TYPE_STRING) {
                IRType* char_ptr_t = get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL);
                return ir_value_const_int(0, char_ptr_t ? char_ptr_t : ir_type_ptr(IR_TYPE_CHAR_T));
            }
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

        ir_lower_bind_local_static(ctx, stmt->data.array_decl.name, gname, arr_t, TYPE_INT, NULL, 0);
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

    ir_lower_bind_local(ctx, stmt->data.array_decl.name, ptr_reg, arr_t, TYPE_INT, NULL, 0);
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
        ir_lower_bind_local_static(ctx, stmt->data.var_decl.name, gname, bytes_t, TYPE_INT, NULL, 0);
        return;
    }

        IRType* value_type = ir_type_from_datatype_ex(m, stmt->data.var_decl.type, stmt->data.var_decl.func_sig);
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
        ir_lower_bind_local_static(ctx, stmt->data.var_decl.name, gname, value_type,
                                   stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_base_type : TYPE_INT,
                                   stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_base_type_name : NULL,
                                   stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_depth : 0);
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
        ir_lower_bind_local(ctx, stmt->data.var_decl.name, ptr_reg, bytes_t, TYPE_INT, NULL, 0);
        return;
    }

    IRType* value_type = ir_type_from_datatype_ex(ctx && ctx->builder ? ctx->builder->module : NULL,
                                                  stmt->data.var_decl.type,
                                                  stmt->data.var_decl.func_sig);

    // %ptr = حجز <value_type>
    int ptr_reg = ir_builder_emit_alloca(ctx->builder, value_type);

    // ربط اسم المتغير بتعليمة الحجز للتتبع.
    ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.var_decl.name);

    // Bind name -> ptr reg so NODE_VAR_REF can emit حمل.
    ir_lower_bind_local(ctx, stmt->data.var_decl.name, ptr_reg, value_type,
                        stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_base_type : TYPE_INT,
                        stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_base_type_name : NULL,
                        stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_depth : 0);

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

static bool ir_lower_inline_asm_push_arg(IRLowerCtx* ctx, const Node* site,
                                         IRValue*** args, int* count, int* cap,
                                         IRValue* value)
{
    if (!args || !count || !cap) return false;
    if (*count >= *cap) {
        int new_cap = (*cap <= 0) ? 8 : (*cap * 2);
        IRValue** grown = (IRValue**)realloc(*args, (size_t)new_cap * sizeof(IRValue*));
        if (!grown) {
            ir_lower_report_error(ctx, site, "نفدت الذاكرة أثناء خفض معاملات 'مجمع'.");
            return false;
        }
        *args = grown;
        *cap = new_cap;
    }
    (*args)[(*count)++] = value;
    return true;
}

static int ir_lower_inline_asm_count_nodes(Node* list)
{
    int n = 0;
    for (Node* it = list; it; it = it->next) n++;
    return n;
}

static void lower_inline_asm_stmt(IRLowerCtx* ctx, Node* stmt)
{
    if (!ctx || !ctx->builder || !stmt) return;

    int template_count = ir_lower_inline_asm_count_nodes(stmt->data.inline_asm.templates);
    int output_count = ir_lower_inline_asm_count_nodes(stmt->data.inline_asm.outputs);
    int input_count = ir_lower_inline_asm_count_nodes(stmt->data.inline_asm.inputs);

    IRValue** args = NULL;
    int arg_count = 0;
    int arg_cap = 0;

    if (!ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap,
                                      ir_value_const_int((int64_t)template_count, IR_TYPE_I64_T)) ||
        !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap,
                                      ir_value_const_int((int64_t)output_count, IR_TYPE_I64_T)) ||
        !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap,
                                      ir_value_const_int((int64_t)input_count, IR_TYPE_I64_T))) {
        free(args);
        return;
    }

    for (Node* t = stmt->data.inline_asm.templates; t; t = t->next) {
        const char* s = NULL;
        if (t->type == NODE_STRING && t->data.string_lit.value) {
            s = t->data.string_lit.value;
        } else {
            ir_lower_report_error(ctx, t ? t : stmt, "سطر 'مجمع' يجب أن يكون نصاً ثابتاً.");
            s = "";
        }
        IRValue* sv = ir_builder_const_string(ctx->builder, s);
        if (!sv || !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, sv)) {
            free(args);
            return;
        }
    }

    for (Node* op = stmt->data.inline_asm.outputs; op; op = op->next) {
        const char* cstr = (op->type == NODE_ASM_OPERAND && op->data.asm_operand.constraint)
                               ? op->data.asm_operand.constraint
                               : "";
        IRValue* cv = ir_builder_const_string(ctx->builder, cstr);
        if (!cv || !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, cv)) {
            free(args);
            return;
        }

        IRType* pointee = IR_TYPE_I64_T;
        IRValue* addr = NULL;
        if (op->type == NODE_ASM_OPERAND && op->data.asm_operand.expression) {
            addr = lower_lvalue_address(ctx, op->data.asm_operand.expression, &pointee);
        } else {
            ir_lower_report_error(ctx, op ? op : stmt, "خرج 'مجمع' غير صالح.");
            addr = ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
            pointee = IR_TYPE_I64_T;
        }
        if (!addr || !addr->type || addr->type->kind != IR_TYPE_PTR) {
            addr = cast_to(ctx, addr, ir_type_ptr(pointee ? pointee : IR_TYPE_I64_T));
        }
        if (!ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, addr)) {
            free(args);
            return;
        }
    }

    for (Node* op = stmt->data.inline_asm.inputs; op; op = op->next) {
        const char* cstr = (op->type == NODE_ASM_OPERAND && op->data.asm_operand.constraint)
                               ? op->data.asm_operand.constraint
                               : "";
        IRValue* cv = ir_builder_const_string(ctx->builder, cstr);
        if (!cv || !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, cv)) {
            free(args);
            return;
        }

        IRValue* iv = NULL;
        if (op->type == NODE_ASM_OPERAND && op->data.asm_operand.expression) {
            iv = lower_expr(ctx, op->data.asm_operand.expression);
        } else {
            ir_lower_report_error(ctx, op ? op : stmt, "إدخال 'مجمع' غير صالح.");
            iv = ir_builder_const_i64(0);
        }
        if (!ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, iv)) {
            free(args);
            return;
        }
    }

    ir_lower_set_loc(ctx->builder, stmt);
    ir_builder_emit_call_void(ctx->builder, BAA_INLINE_ASM_PSEUDO_CALL, args, arg_count);
    free(args);
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

        case NODE_INLINE_ASM:
            lower_inline_asm_stmt(ctx, stmt);
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

        case NODE_VAR_REF:
            // تهيئة عامة لمؤشر دالة: اسم دالة يُخزن كعنوان ثابت.
            if (expected_type && expected_type->kind == IR_TYPE_FUNC &&
                expr->data.var_ref.name) {
                return ir_value_func_ref(expr->data.var_ref.name, expected_type);
            }
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);

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

IRModule* ir_lower_program(Node* program, const char* module_name,
                           bool enable_bounds_checks, const BaaTarget* target) {
    if (!program || program->type != NODE_PROGRAM) return NULL;

    // هل لدينا `الرئيسية(صحيح، نص[])`؟ إن كان نعم سنولد غلاف ABI لـ C ونُعيد تسمية دالة المستخدم.
    bool main_with_args = false;
    const Node* main_site = NULL;
    for (Node* d = program->data.program.declarations; d; d = d->next) {
        if (d->type != NODE_FUNC_DEF) continue;
        if (!ir_lower_ast_func_is_main_with_args(d)) continue;
        main_with_args = true;
        // نفضل موقع التعريف غير النموذجي لربط التشخيصات.
        if (!main_site || (!d->data.func_def.is_prototype && main_site->data.func_def.is_prototype)) {
            main_site = d;
        }
    }

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
        int decl0_param_count = 0;
        for (Node* p = decl0->data.func_def.params; p; p = p->next) {
            if (p->type == NODE_VAR_DECL) decl0_param_count++;
        }

        const char* ir_name = decl0->data.func_def.name;
        if (main_with_args && ir_lower_ast_func_is_main_with_args(decl0)) {
            ir_name = BAA_WRAPPED_USER_MAIN_NAME;
        }

        IRFunc* existing = ir_module_find_func(module, ir_name);
        if (existing) {
            existing->is_variadic = decl0->data.func_def.is_variadic;
            if (decl0->data.func_def.is_variadic &&
                existing->param_count == decl0_param_count) {
                ir_builder_set_func(builder, existing);
                (void)ir_builder_add_param(builder, "__baa_va_base", ir_type_ptr(IR_TYPE_I8_T));
            }
            if (!decl0->data.func_def.is_prototype) {
                existing->is_prototype = false;
            }
            continue;
        }

        IRType* ret_type = ir_type_from_datatype_ex(module,
                                                    decl0->data.func_def.return_type,
                                                    decl0->data.func_def.return_func_sig);
        IRFunc* f0 = ir_builder_create_func(builder, ir_name, ret_type);
        if (!f0) continue;
        f0->is_prototype = decl0->data.func_def.is_prototype;
        f0->is_variadic = decl0->data.func_def.is_variadic;

        for (Node* p = decl0->data.func_def.params; p; p = p->next) {
            if (p->type != NODE_VAR_DECL) continue;
            IRType* ptype = ir_type_from_datatype_ex(module,
                                                     p->data.var_decl.type,
                                                     p->data.var_decl.func_sig);
            (void)ir_builder_add_param(builder, p->data.var_decl.name, ptype);
        }
        if (decl0->data.func_def.is_variadic) {
            (void)ir_builder_add_param(builder, "__baa_va_base", ir_type_ptr(IR_TYPE_I8_T));
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

            IRType* gtype = ir_type_from_datatype_ex(module,
                                                     decl->data.var_decl.type,
                                                     decl->data.var_decl.func_sig);
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
            int decl_param_count = 0;
            for (Node* p = decl->data.func_def.params; p; p = p->next) {
                if (p->type == NODE_VAR_DECL) decl_param_count++;
            }

            const char* ir_name = decl->data.func_def.name;
            if (main_with_args && ir_lower_ast_func_is_main_with_args(decl)) {
                ir_name = BAA_WRAPPED_USER_MAIN_NAME;
            }

            IRFunc* func = ir_module_find_func(module, ir_name);
            if (!func) {
                IRType* ret_type = ir_type_from_datatype_ex(module,
                                                            decl->data.func_def.return_type,
                                                            decl->data.func_def.return_func_sig);
                func = ir_builder_create_func(builder, ir_name, ret_type);
                if (!func) continue;
                func->is_prototype = decl->data.func_def.is_prototype;
                func->is_variadic = decl->data.func_def.is_variadic;
                for (Node* p = decl->data.func_def.params; p; p = p->next) {
                    if (p->type != NODE_VAR_DECL) continue;
                    IRType* ptype = ir_type_from_datatype_ex(module,
                                                             p->data.var_decl.type,
                                                             p->data.var_decl.func_sig);
                    (void)ir_builder_add_param(builder, p->data.var_decl.name, ptype);
                }
                if (decl->data.func_def.is_variadic) {
                    (void)ir_builder_add_param(builder, "__baa_va_base", ir_type_ptr(IR_TYPE_I8_T));
                }
            } else {
                func->is_variadic = decl->data.func_def.is_variadic;
                if (decl->data.func_def.is_variadic &&
                    func->param_count == decl_param_count) {
                    ir_builder_set_func(builder, func);
                    (void)ir_builder_add_param(builder, "__baa_va_base", ir_type_ptr(IR_TYPE_I8_T));
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
            // نُبقي الاسم المصدرّي لأغراض رسائل/تسميات داخلية حتى إن اختلف اسم IR.
            ctx.current_func_name = decl->data.func_def.name;
            ctx.static_local_counter = 0;
            ctx.enable_bounds_checks = enable_bounds_checks;
            ctx.program_root = program;
            ctx.target = target;
            ctx.current_func_is_variadic = decl->data.func_def.is_variadic;
            ctx.current_func_fixed_params = decl_param_count;
            ctx.current_func_va_base_reg = -1;

            if (ctx.current_func_is_variadic &&
                func->param_count > ctx.current_func_fixed_params &&
                func->params) {
                ctx.current_func_va_base_reg = func->params[ctx.current_func_fixed_params].reg;
            }

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
                ir_lower_bind_local(&ctx, pname, ptr_reg, ptype,
                                    p->data.var_decl.type == TYPE_POINTER ? p->data.var_decl.ptr_base_type : TYPE_INT,
                                    p->data.var_decl.type == TYPE_POINTER ? p->data.var_decl.ptr_base_type_name : NULL,
                                    p->data.var_decl.type == TYPE_POINTER ? p->data.var_decl.ptr_depth : 0);

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

    // إن كانت الرئيسية بوسائط: ولّد غلاف ABI لـ C باسم 'الرئيسية' ليستدعي '__baa_user_main'.
    if (main_with_args) {
        if (!ir_lower_emit_main_args_wrapper(builder, program, main_site, target)) {
            ir_builder_free(builder);
            ir_module_free(module);
            return NULL;
        }
    }

    ir_builder_free(builder);
    return module;
}
