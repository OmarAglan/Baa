// ============================================================================
// تسلسل IR النصي (IR Text Serialization)
// ============================================================================

#include "middleend_internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// ----------------------------------------------------------------------------
// أدوات صغيرة (Utilities)
// ----------------------------------------------------------------------------

static void ir_text_skip_ws(const char** p) {
    while (p && *p && (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n')) {
        (*p)++;
    }
}

static int ir_text_is_eol(const char* p) {
    if (!p) return 1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return *p == '\0';
}

static int ir_text_match(const char** p, const char* lit) {
    if (!p || !*p || !lit) return 0;
    size_t n = strlen(lit);
    if (strncmp(*p, lit, n) != 0) return 0;
    *p += n;
    return 1;
}

static char* ir_text_parse_token(const char** p) {
    if (!p || !*p) return NULL;
    ir_text_skip_ws(p);
    const char* s = *p;
    if (*s == '\0') return NULL;

    // التوكن: حتى مسافة أو رمز فصل
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c <= 0x20) break;
        if (c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
            c == ',' || c == ':' || c == '=' || c == ';') {
            break;
        }
        s++;
    }

    size_t len = (size_t)(s - *p);
    if (len == 0) return NULL;

    char* tok = (char*)malloc(len + 1);
    if (!tok) return NULL;
    memcpy(tok, *p, len);
    tok[len] = '\0';
    *p = s;
    return tok;
}

static int ir_text_parse_int64(const char** p, int64_t* out) {
    if (!p || !*p || !out) return 0;
    ir_text_skip_ws(p);
    errno = 0;
    char* end = NULL;
    long long v = strtoll(*p, &end, 10);
    if (end == *p) return 0;
    if (errno != 0) return 0;
    *out = (int64_t)v;
    *p = end;
    return 1;
}

static int ir_text_parse_int32(const char** p, int* out) {
    int64_t v = 0;
    if (!ir_text_parse_int64(p, &v)) return 0;
    if (v < -2147483648LL || v > 2147483647LL) return 0;
    *out = (int)v;
    return 1;
}

static void ir_text_write_escaped(FILE* out, const char* s) {
    if (!out) return;
    fputc('"', out);
    if (s) {
        for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
            unsigned char c = *p;
            switch (c) {
                case '\\': fputs("\\\\", out); break;
                case '"':  fputs("\\\"", out); break;
                case '\n': fputs("\\n", out); break;
                case '\r': fputs("\\r", out); break;
                case '\t': fputs("\\t", out); break;
                case '\0': fputs("\\0", out); break;
                default:
                    // نترك UTF-8 كما هو؛ ونحول المحارف غير القابلة للطباعة إلى \xHH.
                    if (c < 0x20) {
                        fprintf(out, "\\x%02X", (unsigned)c);
                    } else {
                        fputc((int)c, out);
                    }
                    break;
            }
        }
    }
    fputc('"', out);
}

static int ir_text_parse_string_lit(const char** p, char** out) {
    if (!p || !*p || !out) return 0;
    ir_text_skip_ws(p);
    if (**p != '"') return 0;
    (*p)++;

    size_t cap = 64;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return 0;

    while (**p && **p != '"') {
        unsigned char c = (unsigned char)**p;
        (*p)++;

        if (c == '\\') {
            unsigned char esc = (unsigned char)**p;
            if (!esc) { free(buf); return 0; }
            (*p)++;
            switch (esc) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case '0': c = '\0'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case 'x': {
                    int hi = (unsigned char)**p;
                    int lo = (unsigned char)*(*p + 1);
                    if (!isxdigit(hi) || !isxdigit(lo)) { free(buf); return 0; }
                    int hv = isdigit(hi) ? (hi - '0') : (tolower(hi) - 'a' + 10);
                    int lv = isdigit(lo) ? (lo - '0') : (tolower(lo) - 'a' + 10);
                    c = (unsigned char)((hv << 4) | lv);
                    *p += 2;
                    break;
                }
                default:
                    // هروب غير معروف: نعتبره خطأ للحفاظ على الصرامة.
                    free(buf);
                    return 0;
            }
        }

        if (len + 1 >= cap) {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return 0; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }

    if (**p != '"') { free(buf); return 0; }
    (*p)++;

    buf[len] = '\0';
    *out = buf;
    return 1;
}

// ----------------------------------------------------------------------------
// كتابة الأنواع/القيم (Type/Value Printing)
// ----------------------------------------------------------------------------

static void ir_text_write_type(FILE* out, IRType* type) {
    if (!out) return;
    if (!type) {
        fputs("void", out);
        return;
    }

    switch (type->kind) {
        case IR_TYPE_VOID: fputs("void", out); return;
        case IR_TYPE_I1:   fputs("i1", out); return;
        case IR_TYPE_I8:   fputs("i8", out); return;
        case IR_TYPE_I16:  fputs("i16", out); return;
        case IR_TYPE_I32:  fputs("i32", out); return;
        case IR_TYPE_I64:  fputs("i64", out); return;
        case IR_TYPE_U8:   fputs("u8", out); return;
        case IR_TYPE_U16:  fputs("u16", out); return;
        case IR_TYPE_U32:  fputs("u32", out); return;
        case IR_TYPE_U64:  fputs("u64", out); return;
        case IR_TYPE_CHAR: fputs("char", out); return;
        case IR_TYPE_F64:  fputs("f64", out); return;
        case IR_TYPE_PTR:
            fputs("ptr<", out);
            ir_text_write_type(out, type->data.pointee);
            fputc('>', out);
            return;
        case IR_TYPE_ARRAY:
            fputs("array<", out);
            ir_text_write_type(out, type->data.array.element);
            fprintf(out, ", %d>", type->data.array.count);
            return;
        case IR_TYPE_FUNC:
            fputs("func(", out);
            for (int i = 0; i < type->data.func.param_count; i++) {
                if (i > 0) fputs(", ", out);
                ir_text_write_type(out, type->data.func.params[i]);
            }
            fputs(") -> ", out);
            ir_text_write_type(out, type->data.func.ret);
            return;
        default:
            fputs("unknown", out);
            return;
    }
}

static void ir_text_write_value(FILE* out, IRValue* val) {
    if (!out) return;
    if (!val) {
        fputs("null", out);
        return;
    }

    switch (val->kind) {
        case IR_VAL_NONE:
            fputs("void", out);
            return;
        case IR_VAL_REG:
            fprintf(out, "%%r%d", val->data.reg_num);
            return;
        case IR_VAL_CONST_INT:
            fprintf(out, "%lld", (long long)val->data.const_int);
            return;
        case IR_VAL_CONST_STR:
            fprintf(out, "@.str%d", val->data.const_str.id);
            return;
        case IR_VAL_BAA_STR:
            fprintf(out, "@.bs%d", val->data.const_str.id);
            return;
        case IR_VAL_BLOCK:
            if (val->data.block) fprintf(out, "%%block%d", val->data.block->id);
            else fputs("%block?", out);
            return;
        case IR_VAL_GLOBAL:
        case IR_VAL_FUNC:
            fputc('@', out);
            fputs(val->data.global_name ? val->data.global_name : "???", out);
            return;
        default:
            fputs("???", out);
            return;
    }
}

static const char* ir_text_cmp_pred_name(IRCmpPred pred) {
    switch (pred) {
        case IR_CMP_EQ: return "eq";
        case IR_CMP_NE: return "ne";
        case IR_CMP_GT: return "sgt";
        case IR_CMP_LT: return "slt";
        case IR_CMP_GE: return "sge";
        case IR_CMP_LE: return "sle";
        case IR_CMP_UGT: return "ugt";
        case IR_CMP_ULT: return "ult";
        case IR_CMP_UGE: return "uge";
        case IR_CMP_ULE: return "ule";
        default: return "unknown";
    }
}

// ----------------------------------------------------------------------------
// كتابة التعليمات/الكتل/الدوال (Inst/Block/Func Printing)
// ----------------------------------------------------------------------------

static void ir_text_write_inst(FILE* out, IRInst* inst) {
    if (!out || !inst) return;

    fputs("    ", out);
    if (inst->dest >= 0) {
        fprintf(out, "%%r%d = ", inst->dest);
    }

    // opcode
    fputs(ir_op_to_english(inst->op), out);

    switch (inst->op) {
        case IR_OP_NOP:
            break;

        case IR_OP_BR:
            fputc(' ', out);
            if (inst->operand_count >= 1) ir_text_write_value(out, inst->operands[0]);
            break;

        case IR_OP_BR_COND:
            fputc(' ', out);
            if (inst->operand_count >= 1) ir_text_write_value(out, inst->operands[0]);
            if (inst->operand_count >= 2) { fputs(", ", out); ir_text_write_value(out, inst->operands[1]); }
            if (inst->operand_count >= 3) { fputs(", ", out); ir_text_write_value(out, inst->operands[2]); }
            break;

        case IR_OP_RET:
            fputc(' ', out);
            if (inst->type && inst->type->kind == IR_TYPE_VOID) {
                fputs("void", out);
                break;
            }
            ir_text_write_type(out, inst->type);
            if (inst->operand_count >= 1) {
                fputc(' ', out);
                ir_text_write_value(out, inst->operands[0]);
            }
            break;

        case IR_OP_CALL: {
            fputc(' ', out);
            // صيغة التسلسل:
            // - مباشر: call <ret_type> @name(args)
            // - غير مباشر: call <ret_type> <callee>(args)
            ir_text_write_type(out, inst->type);
            fputc(' ', out);
            if (inst->call_target) {
                fputc('@', out);
                fputs(inst->call_target, out);
            } else if (inst->call_callee) {
                fputc('<', out);
                ir_text_write_value(out, inst->call_callee);
                fputc('>', out);
            } else {
                fputs("???", out);
            }
            fputc('(', out);
            for (int i = 0; i < inst->call_arg_count; i++) {
                if (i > 0) fputs(", ", out);
                ir_text_write_value(out, inst->call_args[i]);
            }
            fputc(')', out);
            break;
        }

        case IR_OP_PHI: {
            fputc(' ', out);
            ir_text_write_type(out, inst->type);
            fputc(' ', out);

            // طباعة فاي بشكل قياسي: ترتيب المدخلات حسب رقم الكتلة.
            int count = 0;
            for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) count++;
            if (count > 0) {
                IRPhiEntry** arr = (IRPhiEntry**)malloc((size_t)count * sizeof(IRPhiEntry*));
                if (!arr) break;
                int i = 0;
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) arr[i++] = e;

                // فرز بسيط (عدد المدخلات صغير عادةً)
                for (int a = 0; a < count; a++) {
                    for (int b = a + 1; b < count; b++) {
                        int ida = (arr[a]->block ? arr[a]->block->id : 2147483647);
                        int idb = (arr[b]->block ? arr[b]->block->id : 2147483647);
                        if (idb < ida) {
                            IRPhiEntry* tmp = arr[a];
                            arr[a] = arr[b];
                            arr[b] = tmp;
                        }
                    }
                }

                for (int idx = 0; idx < count; idx++) {
                    if (idx > 0) fputs(", ", out);
                    fputc('[', out);
                    ir_text_write_value(out, arr[idx]->value);
                    fputs(", ", out);
                    if (arr[idx]->block) fprintf(out, "%%block%d", arr[idx]->block->id);
                    else fputs("%block?", out);
                    fputc(']', out);
                }
                free(arr);
            }
            break;
        }

        case IR_OP_CAST: {
            fputc(' ', out);
            IRType* from_t = (inst->operand_count >= 1 && inst->operands[0]) ? inst->operands[0]->type : NULL;
            ir_text_write_type(out, from_t);
            fputc(' ', out);
            if (inst->operand_count >= 1) ir_text_write_value(out, inst->operands[0]);
            fputs(" to ", out);
            ir_text_write_type(out, inst->type);
            break;
        }

        case IR_OP_CMP: {
            fputc(' ', out);
            fputs(ir_text_cmp_pred_name(inst->cmp_pred), out);
            fputc(' ', out);
            IRType* cmp_t = (inst->operand_count >= 1 && inst->operands[0]) ? inst->operands[0]->type : IR_TYPE_I64_T;
            ir_text_write_type(out, cmp_t);
            fputc(' ', out);
            if (inst->operand_count >= 1) ir_text_write_value(out, inst->operands[0]);
            if (inst->operand_count >= 2) { fputs(", ", out); ir_text_write_value(out, inst->operands[1]); }
            break;
        }

        case IR_OP_ALLOCA: {
            fputc(' ', out);
            IRType* t = inst->type;
            if (t && t->kind == IR_TYPE_PTR) t = t->data.pointee;
            ir_text_write_type(out, t);
            break;
        }

        case IR_OP_LOAD:
            fputc(' ', out);
            ir_text_write_type(out, inst->type);
            fputs(", ", out);
            if (inst->operand_count >= 1) ir_text_write_value(out, inst->operands[0]);
            break;

        case IR_OP_STORE: {
            fputc(' ', out);
            IRType* t = (inst->operand_count >= 1 && inst->operands[0]) ? inst->operands[0]->type : IR_TYPE_I64_T;
            ir_text_write_type(out, t);
            fputc(' ', out);
            if (inst->operand_count >= 1) ir_text_write_value(out, inst->operands[0]);
            if (inst->operand_count >= 2) { fputs(", ", out); ir_text_write_value(out, inst->operands[1]); }
            break;
        }

        default: {
            // الصيغة الافتراضية: <op> <type> <op0>, <op1>, ...
            fputc(' ', out);
            if (inst->type && inst->type->kind != IR_TYPE_VOID) {
                ir_text_write_type(out, inst->type);
                if (inst->operand_count > 0) fputc(' ', out);
            }
            for (int i = 0; i < inst->operand_count; i++) {
                if (i > 0) fputs(", ", out);
                ir_text_write_value(out, inst->operands[i]);
            }
            break;
        }
    }

    // سمات اختيارية للثبات في الاختبارات
    if (inst->id >= 0) {
        fprintf(out, " @id %d", inst->id);
    }

    fputc('\n', out);
}

static void ir_text_write_block(FILE* out, IRBlock* block) {
    if (!out || !block) return;
    fprintf(out, "block%d:\n", block->id);
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        ir_text_write_inst(out, inst);
    }
}

static void ir_text_write_func(FILE* out, IRFunc* func) {
    if (!out || !func) return;

    fprintf(out, "func @%s(", func->name ? func->name : "???");
    for (int i = 0; i < func->param_count; i++) {
        if (i > 0) fputs(", ", out);
        ir_text_write_type(out, func->params[i].type);
        fprintf(out, " %%r%d", func->params[i].reg);
    }
    if (func->is_variadic) {
        if (func->param_count > 0) fputs(", ", out);
        fputs("...", out);
    }
    fputs(") -> ", out);
    ir_text_write_type(out, func->ret_type);

    if (func->is_prototype) {
        fputs(";\n\n", out);
        return;
    }

    fputs(" {\n", out);

    // طباعة الكتل بترتيب المعرّف لضمان ثبات التسلسل.
    int max_id = -1;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id > max_id) max_id = b->id;
    }
    if (max_id >= 0) {
        IRBlock** by_id = (IRBlock**)calloc((size_t)(max_id + 1), sizeof(IRBlock*));
        if (by_id) {
            for (IRBlock* b = func->blocks; b; b = b->next) {
                if (b->id >= 0 && b->id <= max_id) by_id[b->id] = b;
            }
            for (int i = 0; i <= max_id; i++) {
                if (by_id[i]) ir_text_write_block(out, by_id[i]);
            }
            free(by_id);
        } else {
            for (IRBlock* b = func->blocks; b; b = b->next) {
                ir_text_write_block(out, b);
            }
        }
    }
    fputs("}\n\n", out);
}

static void ir_text_write_global(FILE* out, IRGlobal* g) {
    if (!out || !g) return;

    if (g->is_internal) fputs("internal ", out);
    if (g->is_const) fputs("const ", out);
    fputs("global @", out);
    fputs(g->name ? g->name : "???", out);
    fputs(" = ", out);
    ir_text_write_type(out, g->type);
    fputc(' ', out);

    if (g->type && g->type->kind == IR_TYPE_ARRAY) {
        // للمصفوفات: قائمة تهيئة جزئية {v0, v1, ...} أو zeroinit.
        if (g->init_elem_count <= 0) {
            fputs("zeroinit", out);
        } else {
            fputc('{', out);
            for (int i = 0; i < g->init_elem_count; i++) {
                if (i > 0) fputs(", ", out);
                IRValue* v = g->init_elems ? g->init_elems[i] : NULL;
                if (v) ir_text_write_value(out, v);
                else fputs("0", out);
            }
            fputc('}', out);
        }
    } else {
        if (g->init) {
            ir_text_write_value(out, g->init);
        } else {
            fputs("0", out);
        }
    }
    fputc('\n', out);
}

// ----------------------------------------------------------------------------
// واجهة الكاتب (Writer API)
// ----------------------------------------------------------------------------

int ir_text_write_module(IRModule* module, FILE* out) {
    if (!module || !out) return 0;

    // 1) جدول النصوص (String Table)
    // نطبع بالترتيب حسب المعرّف لضمان ثبات النص.
    int max_id = -1;
    for (IRStringEntry* s = module->strings; s; s = s->next) {
        if (s->id > max_id) max_id = s->id;
    }

    if (max_id >= 0) {
        IRStringEntry** by_id = (IRStringEntry**)calloc((size_t)(max_id + 1), sizeof(IRStringEntry*));
        if (!by_id) return 0;
        for (IRStringEntry* s = module->strings; s; s = s->next) {
            if (s->id >= 0 && s->id <= max_id) by_id[s->id] = s;
        }
        for (int i = 0; i <= max_id; i++) {
            IRStringEntry* s = by_id[i];
            if (!s) continue;
            fprintf(out, "@.str%d = ", s->id);
            ir_text_write_escaped(out, s->content ? s->content : "");
            fputc('\n', out);
        }
        free(by_id);
        fputc('\n', out);
    }

    // 2) المتغيرات العامة
    for (IRGlobal* g = module->globals; g; g = g->next) {
        ir_text_write_global(out, g);
    }
    if (module->globals) fputc('\n', out);

    // 3) الدوال
    for (IRFunc* f = module->funcs; f; f = f->next) {
        ir_text_write_func(out, f);
    }

    return 1;
}

int ir_text_dump_module(IRModule* module, const char* filename) {
    if (!module || !filename) return 0;
    FILE* out = fopen(filename, "w");
    if (!out) return 0;
    int ok = ir_text_write_module(module, out);
    fclose(out);
    return ok;
}

// ----------------------------------------------------------------------------
// القارئ (Reader)
// ----------------------------------------------------------------------------

typedef struct {
    IRBlock** blocks;
    int count;
    int cap;

    // خريطة أنواع السجلات (%rN) داخل الدالة، لتفسير القيم التي لا تحمل نوعاً صريحاً في نص IR.
    IRType** reg_types;
    int reg_cap;
} IRTextBlockMap;

static IRType* ir_text_reg_type_get(IRTextBlockMap* map, int reg)
{
    if (!map || reg < 0 || reg >= map->reg_cap) return NULL;
    return map->reg_types ? map->reg_types[reg] : NULL;
}

static int ir_text_reg_type_set(IRTextBlockMap* map, int reg, IRType* t)
{
    if (!map || reg < 0) return 0;
    if (reg >= map->reg_cap) {
        int new_cap = map->reg_cap == 0 ? 32 : map->reg_cap;
        while (new_cap <= reg) new_cap *= 2;
        IRType** nt = (IRType**)realloc(map->reg_types, (size_t)new_cap * sizeof(IRType*));
        if (!nt) return 0;
        for (int i = map->reg_cap; i < new_cap; i++) nt[i] = NULL;
        map->reg_types = nt;
        map->reg_cap = new_cap;
    }
    map->reg_types[reg] = t;
    return 1;
}

static IRBlock* ir_text_get_or_create_block(IRFunc* func, IRTextBlockMap* map, int id) {
    if (!func || !map || id < 0) return NULL;

    if (id >= map->cap) {
        int new_cap = map->cap == 0 ? 16 : map->cap;
        while (new_cap <= id) new_cap *= 2;
        IRBlock** nb = (IRBlock**)realloc(map->blocks, (size_t)new_cap * sizeof(IRBlock*));
        if (!nb) return NULL;
        for (int i = map->cap; i < new_cap; i++) nb[i] = NULL;
        map->blocks = nb;
        map->cap = new_cap;
    }

    if (map->blocks[id]) return map->blocks[id];

    char label[64];
    snprintf(label, sizeof(label), "block%d", id);
    IRBlock* b = ir_block_new(label, id);
    if (!b) return NULL;
    ir_func_add_block(func, b);
    map->blocks[id] = b;
    if (id + 1 > map->count) map->count = id + 1;
    if (id + 1 > func->next_block_id) func->next_block_id = id + 1;
    return b;
}

static IROp ir_text_parse_op(const char* op) {
    if (!op) return IR_OP_NOP;
    // نستخدم جدولاً بسيطاً يطابق ما يكتبه الكاتب.
    if (strcmp(op, "add") == 0) return IR_OP_ADD;
    if (strcmp(op, "sub") == 0) return IR_OP_SUB;
    if (strcmp(op, "mul") == 0) return IR_OP_MUL;
    if (strcmp(op, "div") == 0) return IR_OP_DIV;
    if (strcmp(op, "mod") == 0) return IR_OP_MOD;
    if (strcmp(op, "neg") == 0) return IR_OP_NEG;
    if (strcmp(op, "alloca") == 0) return IR_OP_ALLOCA;
    if (strcmp(op, "load") == 0) return IR_OP_LOAD;
    if (strcmp(op, "store") == 0) return IR_OP_STORE;
    if (strcmp(op, "cmp") == 0) return IR_OP_CMP;
    if (strcmp(op, "and") == 0) return IR_OP_AND;
    if (strcmp(op, "or") == 0) return IR_OP_OR;
    if (strcmp(op, "xor") == 0) return IR_OP_XOR;
    if (strcmp(op, "not") == 0) return IR_OP_NOT;
    if (strcmp(op, "shl") == 0) return IR_OP_SHL;
    if (strcmp(op, "shr") == 0) return IR_OP_SHR;
    if (strcmp(op, "br") == 0) return IR_OP_BR;
    if (strcmp(op, "br.cond") == 0) return IR_OP_BR_COND;
    if (strcmp(op, "ret") == 0) return IR_OP_RET;
    if (strcmp(op, "call") == 0) return IR_OP_CALL;
    if (strcmp(op, "phi") == 0) return IR_OP_PHI;
    if (strcmp(op, "copy") == 0) return IR_OP_COPY;
    if (strcmp(op, "cast") == 0) return IR_OP_CAST;
    if (strcmp(op, "nop") == 0) return IR_OP_NOP;
    return IR_OP_NOP;
}

static IRCmpPred ir_text_parse_pred(const char* s) {
    if (!s) return IR_CMP_EQ;
    if (strcmp(s, "eq") == 0) return IR_CMP_EQ;
    if (strcmp(s, "ne") == 0) return IR_CMP_NE;
    if (strcmp(s, "sgt") == 0) return IR_CMP_GT;
    if (strcmp(s, "slt") == 0) return IR_CMP_LT;
    if (strcmp(s, "sge") == 0) return IR_CMP_GE;
    if (strcmp(s, "sle") == 0) return IR_CMP_LE;
    if (strcmp(s, "ugt") == 0) return IR_CMP_UGT;
    if (strcmp(s, "ult") == 0) return IR_CMP_ULT;
    if (strcmp(s, "uge") == 0) return IR_CMP_UGE;
    if (strcmp(s, "ule") == 0) return IR_CMP_ULE;
    return IR_CMP_EQ;
}

static IRType* ir_text_parse_type_rec(const char** p);

static IRType* ir_text_parse_primitive_type(const char* tok) {
    if (!tok) return NULL;
    if (strcmp(tok, "void") == 0) return IR_TYPE_VOID_T;
    if (strcmp(tok, "i1") == 0) return IR_TYPE_I1_T;
    if (strcmp(tok, "i8") == 0) return IR_TYPE_I8_T;
    if (strcmp(tok, "i16") == 0) return IR_TYPE_I16_T;
    if (strcmp(tok, "i32") == 0) return IR_TYPE_I32_T;
    if (strcmp(tok, "i64") == 0) return IR_TYPE_I64_T;
    if (strcmp(tok, "u8") == 0) return IR_TYPE_U8_T;
    if (strcmp(tok, "u16") == 0) return IR_TYPE_U16_T;
    if (strcmp(tok, "u32") == 0) return IR_TYPE_U32_T;
    if (strcmp(tok, "u64") == 0) return IR_TYPE_U64_T;
    if (strcmp(tok, "char") == 0) return IR_TYPE_CHAR_T;
    if (strcmp(tok, "f64") == 0) return IR_TYPE_F64_T;
    return NULL;
}

static IRType* ir_text_parse_type_rec(const char** p) {
    ir_text_skip_ws(p);

    // ptr<...>
    if (ir_text_match(p, "ptr<")) {
        IRType* inner = ir_text_parse_type_rec(p);
        ir_text_skip_ws(p);
        if (!ir_text_match(p, ">")) return NULL;
        return ir_type_ptr(inner);
    }

    // array<type, n>
    if (ir_text_match(p, "array<")) {
        IRType* elem = ir_text_parse_type_rec(p);
        ir_text_skip_ws(p);
        if (!ir_text_match(p, ",")) return NULL;
        int count = 0;
        if (!ir_text_parse_int32(p, &count)) return NULL;
        ir_text_skip_ws(p);
        if (!ir_text_match(p, ">")) return NULL;
        return ir_type_array(elem, count);
    }

    // func(t1, t2) -> tret
    if (ir_text_match(p, "func(")) {
        IRType* params_local[64];
        int pc = 0;

        ir_text_skip_ws(p);
        if (!ir_text_match(p, ")")) {
            while (1) {
                if (pc >= 64) return NULL;
                IRType* pt = ir_text_parse_type_rec(p);
                if (!pt) return NULL;
                params_local[pc++] = pt;

                ir_text_skip_ws(p);
                if (ir_text_match(p, ")")) break;
                if (!ir_text_match(p, ",")) return NULL;
            }
        }

        ir_text_skip_ws(p);
        if (!ir_text_match(p, "->")) {
            // السماح بنسخة الطباعة: ") ->"
            if (!ir_text_match(p, " ->")) return NULL;
        }

        IRType* ret = ir_text_parse_type_rec(p);
        if (!ret) return NULL;

        IRType* param_ptrs[64];
        for (int i = 0; i < pc; i++) param_ptrs[i] = params_local[i];
        return ir_type_func(ret, param_ptrs, pc);
    }

    char* tok = ir_text_parse_token(p);
    if (!tok) return NULL;
    IRType* prim = ir_text_parse_primitive_type(tok);
    free(tok);
    return prim;
}

static int ir_text_parse_reg_token(const char* tok, int* out_reg) {
    if (!tok || !out_reg) return 0;
    if (tok[0] != '%') return 0;
    if (strncmp(tok, "%r", 2) != 0) return 0;
    char* end = NULL;
    long v = strtol(tok + 2, &end, 10);
    if (!end || *end != '\0') return 0;
    if (v < 0 || v > 1000000000L) return 0;
    *out_reg = (int)v;
    return 1;
}

static int ir_text_parse_blockref_token(const char* tok, int* out_id) {
    if (!tok || !out_id) return 0;
    if (tok[0] != '%') return 0;
    if (strncmp(tok, "%block", 6) != 0) return 0;
    char* end = NULL;
    long v = strtol(tok + 6, &end, 10);
    if (!end || *end != '\0') return 0;
    if (v < 0 || v > 1000000000L) return 0;
    *out_id = (int)v;
    return 1;
}

#include "ir_text_reader.c"
