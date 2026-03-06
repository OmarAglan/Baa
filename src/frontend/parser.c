/**
 * @file parser.c
 * @brief يقوم بتحليل الوحدات اللفظية وبناء شجرة الإعراب المجردة (AST) باستخدام طريقة الانحدار العودي (Recursive Descent).
 * @version 0.2.9 (Input & UX Polish)
 */

#include "frontend_internal.h"
#include <errno.h>
#include <limits.h>

// كائن المحلل القواعدي الذي يدير الحالة الحالية والوحدة القادمة
Parser parser;

#define PARSER_MAX_TYPE_ALIASES 256

typedef struct {
    char* name;
    DataType target_type;
    char* target_type_name;
    DataType target_ptr_base_type;
    char* target_ptr_base_type_name;
    int target_ptr_depth;
    FuncPtrSig* target_func_sig; // مملوك (قد يكون NULL)
} ParserTypeAlias;

static ParserTypeAlias parser_type_aliases[PARSER_MAX_TYPE_ALIASES];
static int parser_type_alias_count = 0;

static void parser_funcsig_free(FuncPtrSig* s);

static void parser_type_alias_reset(void)
{
    for (int i = 0; i < parser_type_alias_count; i++) {
        free(parser_type_aliases[i].name);
        parser_type_aliases[i].name = NULL;
        free(parser_type_aliases[i].target_type_name);
        parser_type_aliases[i].target_type_name = NULL;
        free(parser_type_aliases[i].target_ptr_base_type_name);
        parser_type_aliases[i].target_ptr_base_type_name = NULL;
        if (parser_type_aliases[i].target_func_sig) {
            parser_funcsig_free(parser_type_aliases[i].target_func_sig);
            parser_type_aliases[i].target_func_sig = NULL;
        }
        parser_type_aliases[i].target_type = TYPE_INT;
        parser_type_aliases[i].target_ptr_base_type = TYPE_INT;
        parser_type_aliases[i].target_ptr_depth = 0;
    }
    parser_type_alias_count = 0;
}

static const ParserTypeAlias* parser_type_alias_lookup(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < parser_type_alias_count; i++) {
        if (parser_type_aliases[i].name && strcmp(parser_type_aliases[i].name, name) == 0) {
            return &parser_type_aliases[i];
        }
    }
    return NULL;
}

typedef enum {
    PARSER_SYNC_STATEMENT = 0,
    PARSER_SYNC_DECLARATION = 1,
    PARSER_SYNC_SWITCH = 2
} ParserSyncMode;

static void synchronize_mode(ParserSyncMode mode);

static void parser_funcsig_free(FuncPtrSig* s)
{
    if (!s) return;
    if (s->param_ptr_base_type_names) {
        for (int i = 0; i < s->param_count; i++) {
            free(s->param_ptr_base_type_names[i]);
        }
    }
    free(s->return_ptr_base_type_name);
    free(s->param_types);
    free(s->param_ptr_base_types);
    free(s->param_ptr_base_type_names);
    free(s->param_ptr_depths);
    free(s);
}

static FuncPtrSig* parser_funcsig_clone(const FuncPtrSig* s)
{
    if (!s) return NULL;

    FuncPtrSig* out = (FuncPtrSig*)calloc(1, sizeof(FuncPtrSig));
    if (!out) return NULL;

    out->return_type = s->return_type;
    out->return_ptr_base_type = s->return_ptr_base_type;
    out->return_ptr_depth = s->return_ptr_depth;
    out->is_variadic = s->is_variadic;
    if (s->return_ptr_base_type_name) {
        out->return_ptr_base_type_name = strdup(s->return_ptr_base_type_name);
        if (!out->return_ptr_base_type_name) {
            free(out);
            return NULL;
        }
    }

    out->param_count = s->param_count;
    if (out->param_count > 0) {
        size_t n = (size_t)out->param_count;
        out->param_types = (DataType*)malloc(n * sizeof(DataType));
        out->param_ptr_base_types = (DataType*)malloc(n * sizeof(DataType));
        out->param_ptr_base_type_names = (char**)calloc(n, sizeof(char*));
        out->param_ptr_depths = (int*)malloc(n * sizeof(int));
        if (!out->param_types || !out->param_ptr_base_types || !out->param_ptr_base_type_names || !out->param_ptr_depths) {
            parser_funcsig_free(out);
            return NULL;
        }

        for (int i = 0; i < out->param_count; i++) {
            out->param_types[i] = s->param_types ? s->param_types[i] : TYPE_INT;
            out->param_ptr_base_types[i] = s->param_ptr_base_types ? s->param_ptr_base_types[i] : TYPE_INT;
            out->param_ptr_depths[i] = s->param_ptr_depths ? s->param_ptr_depths[i] : 0;
            if (s->param_ptr_base_type_names && s->param_ptr_base_type_names[i]) {
                out->param_ptr_base_type_names[i] = strdup(s->param_ptr_base_type_names[i]);
                if (!out->param_ptr_base_type_names[i]) {
                    parser_funcsig_free(out);
                    return NULL;
                }
            }
        }
    }

    return out;
}

static bool parser_type_alias_register(Token tok, const char* alias_name,
                                       DataType target_type, const char* target_type_name,
                                       DataType target_ptr_base_type,
                                       const char* target_ptr_base_type_name,
                                       int target_ptr_depth,
                                       const FuncPtrSig* target_func_sig)
{
    if (!alias_name) return false;

    if (parser_type_alias_lookup(alias_name)) {
        return false;
    }

    if (parser_type_alias_count >= PARSER_MAX_TYPE_ALIASES) {
        error_report(tok, "عدد أسماء الأنواع البديلة كبير جداً (الحد %d).", PARSER_MAX_TYPE_ALIASES);
        return false;
    }

    ParserTypeAlias* slot = &parser_type_aliases[parser_type_alias_count];
    memset(slot, 0, sizeof(*slot));

    slot->name = strdup(alias_name);
    if (!slot->name) {
        error_report(tok, "نفدت الذاكرة أثناء تسجيل اسم النوع البديل.");
        return false;
    }

    slot->target_type = target_type;
    slot->target_ptr_base_type = target_ptr_base_type;
    slot->target_ptr_depth = target_ptr_depth;
    slot->target_func_sig = NULL;

    if (target_type_name && target_type_name[0]) {
        slot->target_type_name = strdup(target_type_name);
        if (!slot->target_type_name) {
            free(slot->name);
            slot->name = NULL;
            error_report(tok, "نفدت الذاكرة أثناء تسجيل مرجع النوع البديل.");
            return false;
        }
    }

    if (target_ptr_base_type_name && target_ptr_base_type_name[0]) {
        slot->target_ptr_base_type_name = strdup(target_ptr_base_type_name);
        if (!slot->target_ptr_base_type_name) {
            free(slot->target_type_name);
            slot->target_type_name = NULL;
            free(slot->name);
            slot->name = NULL;
            error_report(tok, "نفدت الذاكرة أثناء تسجيل أساس مؤشر النوع البديل.");
            return false;
        }
    }

    if (target_type == TYPE_FUNC_PTR) {
        slot->target_func_sig = parser_funcsig_clone(target_func_sig);
        if (!slot->target_func_sig) {
            free(slot->target_ptr_base_type_name);
            slot->target_ptr_base_type_name = NULL;
            free(slot->target_type_name);
            slot->target_type_name = NULL;
            free(slot->name);
            slot->name = NULL;
            error_report(tok, "نفدت الذاكرة أثناء نسخ توقيع مؤشر الدالة للاسم البديل.");
            return false;
        }
    }

    parser_type_alias_count++;
    return true;
}

static bool parser_int_literal_is_i32(int64_t v)
{
    return v >= INT32_MIN && v <= INT32_MAX;
}

static int64_t parser_sign_extend_u32(uint32_t u)
{
    int64_t v = (int64_t)u;
    if (u & 0x80000000u)
        v -= 0x100000000LL;
    return v;
}

// --- دالات مساعدة ---

/**
 * @brief التقدم للوحدة اللفظية التالية.
 */
void advance() {
    parser.current = parser.next;
    
    while (true) {
        parser.next = lexer_next_token(parser.lexer);
        if (parser.next.type != TOKEN_INVALID) break;
        // إذا وجدنا وحدة غير صالحة، نبلغ عن الخطأ ونستمر في البحث
        error_report(parser.next, "وحدة لفظية غير صالحة.");
    }
}

/**
 * @brief تهيئة المحلل القواعدي ولبدء قراءة أول وحدتين.
 */
void init_parser(Lexer* l) {
    parser.lexer = l;
    parser.panic_mode = false;
    parser.had_error = false;
    parser_type_alias_reset();
    advance(); // Load first token into parser.next (current is garbage initially)
    parser.current = parser.next; // Sync
    advance(); // Load next token
}

/**
 * @brief تحليل رقم صحيح من وحدة TOKEN_INT مع التحقق من النطاق.
 * @return true عند النجاح، false عند الفشل (مع تسجيل خطأ).
 */
static bool parse_int_token_checked(Token tok, int64_t* out_value) {
    if (!out_value) return false;
    *out_value = 0;

    if (!tok.value) {
        error_report(tok, "رقم غير صالح.");
        return false;
    }

    errno = 0;
    char* end = NULL;
    long long v = strtoll(tok.value, &end, 10);

    if (errno != 0 || end == tok.value || (end && *end != '\0')) {
        error_report(tok, "رقم غير صالح.");
        return false;
    }

    *out_value = (int64_t)v;
    return true;
}

// ============================================================================
// مساعدات إنشاء عقد AST مع معلومات الموقع (Debug Locations)
// ============================================================================

static Node* ast_node_new(NodeType type, Token tok) {
    Node* n = (Node*)malloc(sizeof(Node));
    if (!n) {
        error_report(tok, "فشل تخصيص عقدة AST.");
        return NULL;
    }
    memset(n, 0, sizeof(Node));
    n->type = type;
    n->next = NULL;
    n->filename = tok.filename;
    n->line = tok.line;
    n->col = tok.col;
    return n;
}

/**
 * @brief التأكد من أن الوحدة الحالية من نوع معين واستهلاكها، وإلا يتم إظهار خطأ.
 */
void eat(BaaTokenType type) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    if (parser.panic_mode) return;
    
    parser.panic_mode = true;
    
    // تحسين رسالة الخطأ
    // إذا كانت الوحدة الحالية لها قيمة نصية (مثل اسم متغير)، اعرضها
    // وإلا اعرض نوعها كنص
    const char* found_text = parser.current.value ? parser.current.value : token_type_to_str(parser.current.type);
    
    error_report(parser.current, "متوقع '%s' لكن وُجد '%s'.", token_type_to_str(type), found_text);
}

/**
 * @brief التحقق مما إذا كانت الوحدة الحالية تمثل نوع بيانات (صحيح أو نص أو منطقي).
 */
bool is_type_keyword(BaaTokenType type) {
    return (type == TOKEN_KEYWORD_INT ||
            type == TOKEN_KEYWORD_I8 || type == TOKEN_KEYWORD_I16 || type == TOKEN_KEYWORD_I32 || type == TOKEN_KEYWORD_I64 ||
            type == TOKEN_KEYWORD_U8 || type == TOKEN_KEYWORD_U16 || type == TOKEN_KEYWORD_U32 || type == TOKEN_KEYWORD_U64 ||
            type == TOKEN_KEYWORD_STRING || type == TOKEN_KEYWORD_BOOL ||
            type == TOKEN_KEYWORD_CHAR ||
            type == TOKEN_KEYWORD_VOID ||
            type == TOKEN_KEYWORD_FLOAT ||
            type == TOKEN_KEYWORD_FLOAT32 ||
            type == TOKEN_ENUM || type == TOKEN_STRUCT || type == TOKEN_UNION);
}

#include "parser_types.c"
#include "parser_expr.c"
#include "parser_stmt.c"
#include "parser_decl.c"

Node* parse(Lexer* l) {
    // تهيئة نظام الأخطاء بمؤشر المصدر للطباعة
    error_init(l->state.source);
    error_register_source(l->state.filename, l->state.source);
    init_parser(l);

    Node* head = NULL;
    Node* tail = NULL;
    while (parser.current.type != TOKEN_EOF) {
        Node* decl = parse_declaration();
        if (decl) {
            if (head == NULL) { head = decl; tail = decl; }
            else { tail->next = decl; tail = decl; }
        }
        // عندما تكون decl فارغة، فالتعافي غالباً تم داخل parse_declaration
    }
    Token tok_program = {0};
    tok_program.filename = l ? l->state.filename : NULL;
    tok_program.line = 1;
    tok_program.col = 1;

    Node* program = ast_node_new(NODE_PROGRAM, tok_program);
    if (!program) return NULL;
    program->data.program.declarations = head;
    return program;
}
