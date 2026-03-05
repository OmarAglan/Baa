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

#include "ir_lower_ctx.c"
#include "ir_lower_helpers_core.c"
#include "ir_lower_cf_helpers.c"
#include "ir_lower_string_mem.c"
#include "ir_lower_format.c"

static IRValue* lower_call_expr(IRLowerCtx* ctx, Node* expr) {
#include "ir_lower_call_expr_body_a.inc"
#include "ir_lower_call_expr_body_b.inc"
#include "ir_lower_call_expr_body_c.inc"
#include "ir_lower_call_expr_body_d.inc"
#include "ir_lower_call_expr_body_e.inc"
#include "ir_lower_call_expr_body_f.inc"
#include "ir_lower_call_expr_body_g.inc"
#include "ir_lower_call_expr_body_h.inc"
}

#include "ir_lower_format_builtins.c"
#include "ir_lower_expr_impl.c"
#include "ir_lower_stmt_impl.c"
#include "ir_lower_cf_stmt.c"
#include "ir_lower_program_impl.c"
