// ============================================================================
// تسلسل IR النصي (IR Text Serialization)
// ============================================================================

#include "ir_text.h"

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
            // صيغة التسلسل: call <ret_type> @name(args)
            ir_text_write_type(out, inst->type);
            fputc(' ', out);
            fputc('@', out);
            fputs(inst->call_target ? inst->call_target : "???", out);
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

    if (g->is_const) fputs("const ", out);
    fputs("global @", out);
    fputs(g->name ? g->name : "???", out);
    fputs(" = ", out);
    ir_text_write_type(out, g->type);
    fputc(' ', out);
    if (g->init) {
        ir_text_write_value(out, g->init);
    } else {
        fputs("0", out);
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
} IRTextBlockMap;

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
    if (strcmp(op, "not") == 0) return IR_OP_NOT;
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

static IRValue* ir_text_parse_value(IRModule* module, IRFunc* func, IRTextBlockMap* bmap,
                                   const char** p, IRType* expected_type) {
    (void)func;
    ir_text_skip_ws(p);

    // رقم ثابت
    if (**p == '-' || isdigit((unsigned char)**p)) {
        int64_t v = 0;
        if (!ir_text_parse_int64(p, &v)) return NULL;
        return ir_value_const_int(v, expected_type);
    }

    // سلسلة: @.strN
    if (ir_text_match(p, "@.str")) {
        int id = 0;
        if (!ir_text_parse_int32(p, &id)) return NULL;
        const char* s = module ? ir_module_get_string(module, id) : NULL;
        return ir_value_const_str(s, id);
    }

    // مرجع global/function: @name
    if (**p == '@') {
        (*p)++;
        char* name = ir_text_parse_token(p);
        if (!name) return NULL;

        IRType* base = NULL;
        if (expected_type && expected_type->kind == IR_TYPE_PTR) base = expected_type->data.pointee;
        IRValue* v = ir_value_global(name, base);
        free(name);
        return v;
    }

    // سجل أو كتلة
    char* tok = ir_text_parse_token(p);
    if (!tok) return NULL;

    int reg = -1;
    if (ir_text_parse_reg_token(tok, &reg)) {
        free(tok);
        return ir_value_reg(reg, expected_type);
    }

    int bid = -1;
    if (ir_text_parse_blockref_token(tok, &bid)) {
        free(tok);
        if (!func || !bmap) return NULL;
        IRBlock* b = ir_text_get_or_create_block(func, bmap, bid);
        return b ? ir_value_block(b) : NULL;
    }

    free(tok);
    return NULL;
}

static int ir_text_consume_char(const char** p, char c) {
    ir_text_skip_ws(p);
    if (**p != c) return 0;
    (*p)++;
    return 1;
}

static int ir_text_parse_inst_attrs(const char** p, IRInst* inst, IRFunc* func) {
    if (!p || !*p || !inst || !func) return 0;
    while (!ir_text_is_eol(*p)) {
        ir_text_skip_ws(p);
        if (**p == '\0') break;

        if (ir_text_match(p, "@id")) {
            int id = -1;
            if (!ir_text_parse_int32(p, &id)) return 0;
            inst->id = id;
            if (id + 1 > func->next_inst_id) func->next_inst_id = id + 1;
            continue;
        }

        // سمات غير معروفة: نوقف بشكل صارم.
        return 0;
    }
    return 1;
}

static int ir_text_parse_instruction_line(IRModule* module, IRFunc* func, IRBlock* block,
                                         IRTextBlockMap* bmap, const char* line) {
    if (!module || !func || !block || !bmap || !line) return 0;

    const char* p = line;
    ir_text_skip_ws(&p);
    if (*p == '\0') return 1;

    int dest = -1;
    if (*p == '%') {
        char* lhs = ir_text_parse_token(&p);
        if (!lhs) return 0;
        if (!ir_text_parse_reg_token(lhs, &dest)) { free(lhs); return 0; }
        free(lhs);
        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, "=")) return 0;
    }

    char* op_tok = ir_text_parse_token(&p);
    if (!op_tok) return 0;
    IROp op = ir_text_parse_op(op_tok);
    free(op_tok);

    IRInst* inst = NULL;

    if (op == IR_OP_NOP) {
        inst = ir_inst_new(IR_OP_NOP, IR_TYPE_VOID_T, -1);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        return 1;
    }

    if (op == IR_OP_BR) {
        IRType* bt = NULL;
        IRValue* tgtv = ir_text_parse_value(module, func, bmap, &p, bt);
        if (!tgtv || tgtv->kind != IR_VAL_BLOCK) return 0;
        inst = ir_inst_br(tgtv->data.block);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        ir_block_add_succ(block, tgtv->data.block);
        return 1;
    }

    if (op == IR_OP_BR_COND) {
        IRValue* cond = ir_text_parse_value(module, func, bmap, &p, IR_TYPE_I1_T);
        if (!cond) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRValue* t = ir_text_parse_value(module, func, bmap, &p, NULL);
        if (!t || t->kind != IR_VAL_BLOCK) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRValue* f = ir_text_parse_value(module, func, bmap, &p, NULL);
        if (!f || f->kind != IR_VAL_BLOCK) return 0;

        inst = ir_inst_br_cond(cond, t->data.block, f->data.block);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        ir_block_add_succ(block, t->data.block);
        ir_block_add_succ(block, f->data.block);
        return 1;
    }

    if (op == IR_OP_RET) {
        IRType* rt = ir_text_parse_type_rec(&p);
        if (!rt) return 0;
        if (rt->kind == IR_TYPE_VOID) {
            inst = ir_inst_ret(NULL);
            if (!inst) return 0;
            if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
            ir_block_append(block, inst);
            return 1;
        }
        IRValue* v = ir_text_parse_value(module, func, bmap, &p, rt);
        if (!v) return 0;
        inst = ir_inst_ret(v);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        return 1;
    }

    if (op == IR_OP_CALL) {
        IRType* ret_t = ir_text_parse_type_rec(&p);
        if (!ret_t) return 0;
        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, "@")) return 0;
        char* fn = ir_text_parse_token(&p);
        if (!fn) return 0;

        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, "(")) { free(fn); return 0; }

        // جمع الوسائط
        IRValue* args_local[128];
        int ac = 0;
        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, ")")) {
            while (1) {
                if (ac >= 128) { free(fn); return 0; }
                IRValue* a = ir_text_parse_value(module, func, bmap, &p, NULL);
                if (!a) { free(fn); return 0; }
                args_local[ac++] = a;

                ir_text_skip_ws(&p);
                if (ir_text_match(&p, ")")) break;
                if (!ir_text_match(&p, ",")) { free(fn); return 0; }
            }
        }

        IRValue** args = NULL;
        if (ac > 0) {
            args = (IRValue**)malloc((size_t)ac * sizeof(IRValue*));
            if (!args) { free(fn); return 0; }
            for (int i = 0; i < ac; i++) args[i] = args_local[i];
        }

        inst = ir_inst_call(fn, ret_t, dest, args, ac);
        free(fn);
        free(args);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        return 1;
    }

    if (op == IR_OP_ALLOCA) {
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        inst = ir_inst_alloca(t, dest);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        return 1;
    }

    if (op == IR_OP_LOAD) {
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRType* ptr_t = ir_type_ptr(t);
        IRValue* ptr = ir_text_parse_value(module, func, bmap, &p, ptr_t);
        if (!ptr) return 0;
        inst = ir_inst_load(t, dest, ptr);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        return 1;
    }

    if (op == IR_OP_STORE) {
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        IRValue* v = ir_text_parse_value(module, func, bmap, &p, t);
        if (!v) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRType* ptr_t = ir_type_ptr(t);
        IRValue* ptr = ir_text_parse_value(module, func, bmap, &p, ptr_t);
        if (!ptr) return 0;
        inst = ir_inst_store(v, ptr);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        return 1;
    }

    if (op == IR_OP_CMP) {
        char* pred_tok = ir_text_parse_token(&p);
        if (!pred_tok) return 0;
        IRCmpPred pred = ir_text_parse_pred(pred_tok);
        free(pred_tok);
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        IRValue* lhs = ir_text_parse_value(module, func, bmap, &p, t);
        if (!lhs) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRValue* rhs = ir_text_parse_value(module, func, bmap, &p, t);
        if (!rhs) return 0;
        inst = ir_inst_cmp(pred, dest, lhs, rhs);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        return 1;
    }

    if (op == IR_OP_PHI) {
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        inst = ir_inst_phi(t, dest);
        if (!inst) return 0;

        // phi entries: [v, %blockN], ...
        while (1) {
            ir_text_skip_ws(&p);
            if (ir_text_is_eol(p)) break;
            if (!ir_text_match(&p, "[")) break;
            IRValue* v = ir_text_parse_value(module, func, bmap, &p, t);
            if (!v) return 0;
            if (!ir_text_consume_char(&p, ',')) return 0;
            char* br = ir_text_parse_token(&p);
            if (!br) return 0;
            int bid = -1;
            if (!ir_text_parse_blockref_token(br, &bid)) { free(br); return 0; }
            free(br);
            IRBlock* pb = ir_text_get_or_create_block(func, bmap, bid);
            if (!pb) return 0;
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "]")) return 0;
            ir_inst_phi_add(inst, v, pb);

            ir_text_skip_ws(&p);
            if (ir_text_match(&p, ",")) continue;
            break;
        }

        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        return 1;
    }

    if (op == IR_OP_CAST) {
        IRType* from_t = ir_text_parse_type_rec(&p);
        if (!from_t) return 0;
        IRValue* v = ir_text_parse_value(module, func, bmap, &p, from_t);
        if (!v) return 0;
        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, "to")) return 0;
        IRType* to_t = ir_text_parse_type_rec(&p);
        if (!to_t) return 0;

        inst = ir_inst_new(IR_OP_CAST, to_t, dest);
        if (!inst) return 0;
        ir_inst_add_operand(inst, v);
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        return 1;
    }

    // العمليات القياسية ذات النوع
    IRType* t = ir_text_parse_type_rec(&p);
    if (!t) return 0;

    inst = ir_inst_new(op, t, dest);
    if (!inst) return 0;

    // تحميل/نسخ/حساب...: معاملات مفصولة بفواصل
    int first = 1;
    while (!ir_text_is_eol(p)) {
        if (!first) {
            if (!ir_text_consume_char(&p, ',')) break;
        }
        first = 0;
        IRValue* v = ir_text_parse_value(module, func, bmap, &p, t);
        if (!v) break;
        ir_inst_add_operand(inst, v);
        ir_text_skip_ws(&p);
        if (*p != ',') break;
    }

    if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
    ir_block_append(block, inst);
    if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
    return 1;
}

static char* ir_text_read_line(FILE* in) {
    if (!in) return NULL;
    size_t cap = 256;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    int ch;
    while ((ch = fgetc(in)) != EOF) {
        if (len + 2 >= cap) {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
        if (ch == '\n') break;
    }

    if (len == 0 && ch == EOF) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

IRModule* ir_text_read_module_file(const char* filename) {
    if (!filename) return NULL;
    FILE* in = fopen(filename, "r");
    if (!in) return NULL;

    IRModule* module = ir_module_new(filename);
    if (!module) { fclose(in); return NULL; }

    IRFunc* cur_func = NULL;
    IRBlock* cur_block = NULL;
    IRTextBlockMap bmap = {0};

    int ok = 1;

    for (;;) {
        char* line = ir_text_read_line(in);
        if (!line) break;

        const char* p = line;
        ir_text_skip_ws(&p);

        // تجاهل التعليقات بأسلوب ";;" أو ";".
        if (ir_text_match(&p, ";;") || ir_text_match(&p, ";")) {
            free(line);
            continue;
        }

        // سطر فارغ
        if (ir_text_is_eol(p)) {
            free(line);
            continue;
        }

        // جدول النصوص: @.strN = "..."
        if (ir_text_match(&p, "@.str")) {
            int id = 0;
            if (!ir_text_parse_int32(&p, &id)) { ok = 0; free(line); break; }
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "=")) { ok = 0; free(line); break; }
            char* s = NULL;
            if (!ir_text_parse_string_lit(&p, &s)) { ok = 0; free(line); break; }

            int got = ir_module_add_string(module, s);
            free(s);
            if (got != id) { ok = 0; free(line); break; }

            free(line);
            continue;
        }

        // global
        if (strncmp(p, "const global ", 13) == 0 || strncmp(p, "global ", 7) == 0) {
            int is_const = 0;
            if (ir_text_match(&p, "const")) {
                is_const = 1;
                ir_text_skip_ws(&p);
            }
            if (!ir_text_match(&p, "global")) { ok = 0; free(line); break; }
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "@")) { ok = 0; free(line); break; }
            char* name = ir_text_parse_token(&p);
            if (!name) { ok = 0; free(line); break; }
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "=")) { free(name); ok = 0; free(line); break; }
            IRType* t = ir_text_parse_type_rec(&p);
            if (!t) { free(name); ok = 0; free(line); break; }
            IRValue* init = ir_text_parse_value(module, NULL, NULL, &p, t);
            if (!init) { free(name); ok = 0; free(line); break; }

            IRGlobal* g = ir_global_new(name, t, is_const);
            free(name);
            if (!g) { ok = 0; free(line); break; }
            ir_global_set_init(g, init);
            ir_module_add_global(module, g);

            free(line);
            continue;
        }

        // func header
        if (ir_text_match(&p, "func")) {
            // إعادة تهيئة سياق الكتل
            free(bmap.blocks);
            memset(&bmap, 0, sizeof(bmap));
            cur_block = NULL;

            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "@")) { ok = 0; free(line); break; }
            char* fname = ir_text_parse_token(&p);
            if (!fname) { ok = 0; free(line); break; }
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "(")) { free(fname); ok = 0; free(line); break; }

            IRFunc* fn = ir_func_new(fname, IR_TYPE_VOID_T);
            free(fname);
            if (!fn) { ok = 0; free(line); break; }

            // params
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, ")")) {
                while (1) {
                    IRType* pt = ir_text_parse_type_rec(&p);
                    if (!pt) { ok = 0; free(line); break; }
                    char* reg_tok = ir_text_parse_token(&p);
                    if (!reg_tok) { ok = 0; free(line); break; }
                    int preg = -1;
                    if (!ir_text_parse_reg_token(reg_tok, &preg)) { free(reg_tok); ok = 0; free(line); break; }
                    free(reg_tok);

                    ir_func_add_param(fn, NULL, pt);
                    // ضبط رقم السجل كما ورد في النص (إن اختلف)
                    fn->params[fn->param_count - 1].reg = preg;
                    if (preg + 1 > fn->next_reg) fn->next_reg = preg + 1;

                    ir_text_skip_ws(&p);
                    if (ir_text_match(&p, ")")) break;
                    if (!ir_text_match(&p, ",")) { ok = 0; free(line); break; }
                }
                if (!ok) { free(line); break; }
            }

            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "->")) { ok = 0; free(line); break; }
            IRType* rt = ir_text_parse_type_rec(&p);
            if (!rt) { ok = 0; free(line); break; }
            fn->ret_type = rt;

            ir_text_skip_ws(&p);
            if (ir_text_match(&p, ";")) {
                fn->is_prototype = true;
                ir_module_add_func(module, fn);
                cur_func = NULL;
                free(line);
                continue;
            }

            if (!ir_text_match(&p, "{")) { ok = 0; free(line); break; }
            fn->is_prototype = false;
            ir_module_add_func(module, fn);
            cur_func = fn;

            free(line);
            continue;
        }

        // نهاية دالة
        if (*p == '}' && cur_func) {
            cur_func = NULL;
            cur_block = NULL;
            free(line);
            continue;
        }

        // block header: blockN:
        if (strncmp(p, "block", 5) == 0) {
            const char* q = p + 5;
            int id = 0;
            errno = 0;
            char* end = NULL;
            long v = strtol(q, &end, 10);
            if (end == q || errno != 0) { ok = 0; free(line); break; }
            if (!end || *end != ':') { ok = 0; free(line); break; }
            id = (int)v;
            if (!cur_func) { ok = 0; free(line); break; }
            cur_block = ir_text_get_or_create_block(cur_func, &bmap, id);
            if (!cur_block) { ok = 0; free(line); break; }
            free(line);
            continue;
        }

        // instruction
        if (cur_func && cur_block) {
            if (!ir_text_parse_instruction_line(module, cur_func, cur_block, &bmap, p)) {
                ok = 0;
                free(line);
                break;
            }
            free(line);
            continue;
        }

        // سطر غير معروف
        ok = 0;
        free(line);
        break;
    }

    free(bmap.blocks);
    fclose(in);

    if (!ok) {
        ir_module_free(module);
        return NULL;
    }
    return module;
}
