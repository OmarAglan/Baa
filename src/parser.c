/**
 * @file parser.c
 * @brief يقوم بتحليل الوحدات اللفظية وبناء شجرة الإعراب المجردة (AST) باستخدام طريقة الانحدار العودي (Recursive Descent).
 * @version 0.2.9 (Input & UX Polish)
 */

#include "baa.h"
#include <errno.h>
#include <limits.h>

// كائن المحلل القواعدي الذي يدير الحالة الحالية والوحدة القادمة
Parser parser;

#define PARSER_MAX_TYPE_ALIASES 256

typedef struct {
    char* name;
    DataType target_type;
    char* target_type_name;
} ParserTypeAlias;

static ParserTypeAlias parser_type_aliases[PARSER_MAX_TYPE_ALIASES];
static int parser_type_alias_count = 0;

static void parser_type_alias_reset(void)
{
    for (int i = 0; i < parser_type_alias_count; i++) {
        free(parser_type_aliases[i].name);
        parser_type_aliases[i].name = NULL;
        free(parser_type_aliases[i].target_type_name);
        parser_type_aliases[i].target_type_name = NULL;
        parser_type_aliases[i].target_type = TYPE_INT;
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

static bool parser_type_alias_register(Token tok, const char* alias_name,
                                       DataType target_type, const char* target_type_name)
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
    if (target_type_name && target_type_name[0]) {
        slot->target_type_name = strdup(target_type_name);
        if (!slot->target_type_name) {
            free(slot->name);
            slot->name = NULL;
            error_report(tok, "نفدت الذاكرة أثناء تسجيل مرجع النوع البديل.");
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
            type == TOKEN_ENUM || type == TOKEN_STRUCT || type == TOKEN_UNION);
}

static bool parser_current_starts_type(void)
{
    if (is_type_keyword(parser.current.type)) return true;
    if (parser.current.type == TOKEN_IDENTIFIER &&
        parser.current.value &&
        parser_type_alias_lookup(parser.current.value)) {
        return true;
    }
    return false;
}

static bool parser_current_is_type_alias_keyword(void)
{
    if (parser.current.type == TOKEN_TYPE_ALIAS) return true;
    return parser.current.type == TOKEN_IDENTIFIER &&
           parser.current.value &&
           strcmp(parser.current.value, "نوع") == 0;
}

static bool parser_is_decl_qualifier(BaaTokenType type)
{
    return type == TOKEN_CONST || type == TOKEN_STATIC;
}

typedef struct {
    bool is_const;
    bool is_static;
    Token tok_const;
    Token tok_static;
} ParserDeclQualifiers;

static void parser_parse_decl_qualifiers(ParserDeclQualifiers* out_q)
{
    if (!out_q) return;

    memset(out_q, 0, sizeof(*out_q));

    while (parser_is_decl_qualifier(parser.current.type)) {
        if (parser.current.type == TOKEN_CONST) {
            if (out_q->is_const) {
                error_report(parser.current, "تكرار 'ثابت' غير مسموح.");
            } else {
                out_q->is_const = true;
                out_q->tok_const = parser.current;
            }
            eat(TOKEN_CONST);
            continue;
        }

        if (parser.current.type == TOKEN_STATIC) {
            if (out_q->is_static) {
                error_report(parser.current, "تكرار 'ساكن' غير مسموح.");
            } else {
                out_q->is_static = true;
                out_q->tok_static = parser.current;
            }
            eat(TOKEN_STATIC);
            continue;
        }
    }
}

/**
 * @brief تحويل وحدة النوع إلى قيمة DataType.
 */
DataType token_to_datatype(BaaTokenType type) {
    if (type == TOKEN_KEYWORD_I8) return TYPE_I8;
    if (type == TOKEN_KEYWORD_I16) return TYPE_I16;
    if (type == TOKEN_KEYWORD_I32) return TYPE_I32;
    if (type == TOKEN_KEYWORD_I64) return TYPE_INT;
    if (type == TOKEN_KEYWORD_U8) return TYPE_U8;
    if (type == TOKEN_KEYWORD_U16) return TYPE_U16;
    if (type == TOKEN_KEYWORD_U32) return TYPE_U32;
    if (type == TOKEN_KEYWORD_U64) return TYPE_U64;
    if (type == TOKEN_KEYWORD_STRING) return TYPE_STRING;
    if (type == TOKEN_KEYWORD_BOOL) return TYPE_BOOL;
    if (type == TOKEN_KEYWORD_CHAR) return TYPE_CHAR;
    if (type == TOKEN_KEYWORD_FLOAT) return TYPE_FLOAT;
    if (type == TOKEN_KEYWORD_VOID) return TYPE_VOID;
    return TYPE_INT;
}

static bool parse_float_token_checked(Token tok, double* out)
{
    if (out) *out = 0.0;
    if (tok.type != TOKEN_FLOAT || !tok.value) return false;

    char* endp = NULL;
    double v = strtod(tok.value, &endp);
    if (!endp || endp == tok.value || *endp != '\0') return false;
    if (out) *out = v;
    return true;
}

static bool utf8_decode_one(const char* s, uint32_t* out_cp)
{
    if (out_cp) *out_cp = 0;
    if (!s || !*s) return false;

    const unsigned char b0 = (unsigned char)s[0];
    if ((b0 & 0x80u) == 0x00u) {
        if (out_cp) *out_cp = (uint32_t)b0;
        return true;
    }
    if ((b0 & 0xE0u) == 0xC0u) {
        const unsigned char b1 = (unsigned char)s[1];
        if ((b1 & 0xC0u) != 0x80u) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        if (cp < 0x80u) return false; // overlong
        if (out_cp) *out_cp = cp;
        return s[2] == '\0';
    }
    if ((b0 & 0xF0u) == 0xE0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        if (((b1 & 0xC0u) != 0x80u) || ((b2 & 0xC0u) != 0x80u)) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x0Fu) << 12) | ((uint32_t)(b1 & 0x3Fu) << 6) | (uint32_t)(b2 & 0x3Fu);
        if (cp < 0x800u) return false; // overlong
        if (cp >= 0xD800u && cp <= 0xDFFFu) return false; // surrogate
        if (out_cp) *out_cp = cp;
        return s[3] == '\0';
    }
    if ((b0 & 0xF8u) == 0xF0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        const unsigned char b3 = (unsigned char)s[3];
        if (((b1 & 0xC0u) != 0x80u) || ((b2 & 0xC0u) != 0x80u) || ((b3 & 0xC0u) != 0x80u)) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x07u) << 18) | ((uint32_t)(b1 & 0x3Fu) << 12) |
                      ((uint32_t)(b2 & 0x3Fu) << 6) | (uint32_t)(b3 & 0x3Fu);
        if (cp < 0x10000u) return false; // overlong
        if (cp > 0x10FFFFu) return false;
        if (out_cp) *out_cp = cp;
        return s[4] == '\0';
    }
    return false;
}

// ============================================================================
// تحليل نوع (TypeSpec) بما في ذلك تعداد/هيكل (v0.3.4)
// ============================================================================

/**
 * @brief تحليل مواصفة نوع: نوع بدائي أو تعداد/هيكل.
 *
 * الصيغ المدعومة:
 * - صحيح | نص | منطقي
 * - تعداد <اسم>
 * - هيكل <اسم>
 */
static bool parse_type_spec(DataType* out_type, char** out_type_name) {
    if (out_type) *out_type = TYPE_INT;
    if (out_type_name) *out_type_name = NULL;

    if (parser.current.type == TOKEN_KEYWORD_INT ||
        parser.current.type == TOKEN_KEYWORD_I8 ||
        parser.current.type == TOKEN_KEYWORD_I16 ||
        parser.current.type == TOKEN_KEYWORD_I32 ||
        parser.current.type == TOKEN_KEYWORD_I64 ||
        parser.current.type == TOKEN_KEYWORD_U8 ||
        parser.current.type == TOKEN_KEYWORD_U16 ||
        parser.current.type == TOKEN_KEYWORD_U32 ||
        parser.current.type == TOKEN_KEYWORD_U64 ||
        parser.current.type == TOKEN_KEYWORD_STRING ||
        parser.current.type == TOKEN_KEYWORD_BOOL ||
        parser.current.type == TOKEN_KEYWORD_CHAR ||
        parser.current.type == TOKEN_KEYWORD_VOID ||
        parser.current.type == TOKEN_KEYWORD_FLOAT) {
        DataType dt = token_to_datatype(parser.current.type);
        eat(parser.current.type);
        if (out_type) *out_type = dt;
        return true;
    }

    if (parser.current.type == TOKEN_ENUM || parser.current.type == TOKEN_STRUCT || parser.current.type == TOKEN_UNION) {
        bool is_enum = (parser.current.type == TOKEN_ENUM);
        bool is_union = (parser.current.type == TOKEN_UNION);
        eat(parser.current.type);

        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم معرّف بعد كلمة النوع.");
            return false;
        }

        char* tn = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        if (out_type) *out_type = is_enum ? TYPE_ENUM : (is_union ? TYPE_UNION : TYPE_STRUCT);
        if (out_type_name) *out_type_name = tn;
        else free(tn);

        return true;
    }

    if (parser.current.type == TOKEN_IDENTIFIER && parser.current.value) {
        const ParserTypeAlias* alias = parser_type_alias_lookup(parser.current.value);
        if (!alias) return false;

        if (out_type) *out_type = alias->target_type;
        if (out_type_name && alias->target_type_name) {
            *out_type_name = strdup(alias->target_type_name);
            if (!*out_type_name) {
                error_report(parser.current, "نفدت الذاكرة أثناء نسخ النوع البديل.");
                return false;
            }
        }

        eat(TOKEN_IDENTIFIER);
        return true;
    }

    return false;
}

typedef enum {
    PARSER_SYNC_STATEMENT = 0,
    PARSER_SYNC_DECLARATION = 1,
    PARSER_SYNC_SWITCH = 2
} ParserSyncMode;

static bool parser_current_starts_declaration_anchor(void)
{
    if (parser_current_is_type_alias_keyword() && parser.next.type == TOKEN_IDENTIFIER) return true;
    if (parser_current_starts_type()) return true;
    return parser.current.type == TOKEN_CONST || parser.current.type == TOKEN_STATIC;
}

static bool parser_current_starts_statement_anchor(void)
{
    if (parser_current_starts_type()) return true;
    if (parser_current_is_type_alias_keyword()) return true;
    if (parser.current.type == TOKEN_CONST || parser.current.type == TOKEN_STATIC) return true;
    if (parser.current.type == TOKEN_IDENTIFIER) return true;

    switch (parser.current.type) {
        case TOKEN_LBRACE:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_FOR:
        case TOKEN_PRINT:
        case TOKEN_READ:
        case TOKEN_RETURN:
        case TOKEN_SWITCH:
        case TOKEN_BREAK:
        case TOKEN_CONTINUE:
            return true;
        default:
            return false;
    }
}

/**
 * @brief محاولة التعافي من الخطأ حسب سياق التحليل.
 */
static void synchronize_mode(ParserSyncMode mode)
{
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF) {
        // إذا وجدنا فاصلة منقوطة، فغالباً انتهت الجملة السابقة
        if (parser.current.type == TOKEN_SEMICOLON || parser.current.type == TOKEN_DOT) {
            advance();
            return;
        }

        if (mode == PARSER_SYNC_SWITCH) {
            if (parser.current.type == TOKEN_CASE ||
                parser.current.type == TOKEN_DEFAULT ||
                parser.current.type == TOKEN_RBRACE) {
                return;
            }
        } else if (mode == PARSER_SYNC_DECLARATION) {
            if (parser_current_starts_declaration_anchor() || parser.current.type == TOKEN_RBRACE) {
                return;
            }
        } else {
            if (parser_current_starts_statement_anchor() || parser.current.type == TOKEN_RBRACE) {
                return;
            }
        }

        advance();
    }
}

/**
 * @brief محاولة التعافي من الخطأ بالقفز إلى بداية الجملة التالية.
 */
void synchronize() {
    synchronize_mode(PARSER_SYNC_STATEMENT);
}

// --- تصريحات مسبقة ---
Node* parse_expression();
Node* parse_statement();
Node* parse_block();
Node* parse_primary();
Node* parse_shift();
Node* parse_bitwise_and();
Node* parse_bitwise_xor();
Node* parse_bitwise_or();

/**
 * @brief إضافة عنصر إلى نهاية قائمة عقد أحادية الربط.
 */
static void parser_list_append(Node** head, Node** tail, Node* item)
{
    if (!item) return;
    item->next = NULL;
    if (!*head) {
        *head = item;
        *tail = item;
    } else {
        (*tail)->next = item;
        *tail = item;
    }
}

/**
 * @brief تحليل سلسلة فهارس بالشكل: [expr][expr]...
 */
static Node* parse_array_indices(int* out_count)
{
    if (out_count) *out_count = 0;

    Node* head = NULL;
    Node* tail = NULL;
    int count = 0;

    while (parser.current.type == TOKEN_LBRACKET) {
        eat(TOKEN_LBRACKET);
        Node* index = parse_expression();
        eat(TOKEN_RBRACKET);

        if (index) {
            parser_list_append(&head, &tail, index);
            count++;
        }
    }

    if (out_count) *out_count = count;
    return head;
}

static bool parser_mul_i64_checked(int64_t a, int64_t b, int64_t* out)
{
    if (!out) return false;
    *out = 0;
    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }
    if (a > INT64_MAX / b) return false;
    *out = a * b;
    return true;
}

/**
 * @brief تحليل أبعاد مصفوفة ثابتة: [N][M]...
 */
static bool parse_array_dimensions(int** out_dims, int* out_rank, int64_t* out_total)
{
    if (out_dims) *out_dims = NULL;
    if (out_rank) *out_rank = 0;
    if (out_total) *out_total = 0;

    int cap = 4;
    int rank = 0;
    int* dims = NULL;
    int64_t total = 1;

    while (parser.current.type == TOKEN_LBRACKET) {
        eat(TOKEN_LBRACKET);

        int dim = 0;
        if (parser.current.type == TOKEN_INT) {
            int64_t v = 0;
            if (parse_int_token_checked(parser.current, &v)) {
                if (v <= 0) {
                    error_report(parser.current, "حجم البعد يجب أن يكون عدداً موجباً.");
                } else if (v > INT_MAX) {
                    error_report(parser.current, "حجم البعد كبير جداً.");
                } else {
                    dim = (int)v;
                }
            } else {
                error_report(parser.current, "حجم البعد يجب أن يكون عدداً صحيحاً.");
            }
            eat(TOKEN_INT);
        } else {
            error_report(parser.current, "متوقع حجم بعد مصفوفة ثابت.");
        }

        eat(TOKEN_RBRACKET);

        if (rank >= cap) {
            int new_cap = cap * 2;
            int* new_dims = (int*)realloc(dims, (size_t)new_cap * sizeof(int));
            if (!new_dims) {
                free(dims);
                error_report(parser.current, "نفدت الذاكرة أثناء تحليل أبعاد المصفوفة.");
                return false;
            }
            dims = new_dims;
            cap = new_cap;
        }
        if (!dims) {
            dims = (int*)malloc((size_t)cap * sizeof(int));
            if (!dims) {
                error_report(parser.current, "نفدت الذاكرة أثناء تحليل أبعاد المصفوفة.");
                return false;
            }
        }

        dims[rank++] = dim;

        if (dim > 0) {
            int64_t nt = 0;
            if (!parser_mul_i64_checked(total, (int64_t)dim, &nt)) {
                error_report(parser.current, "حجم المصفوفة الكلي كبير جداً.");
                total = 0;
            } else {
                total = nt;
            }
        } else {
            total = 0;
        }
    }

    if (rank == 0) {
        free(dims);
        return false;
    }

    if (out_dims) *out_dims = dims;
    else free(dims);
    if (out_rank) *out_rank = rank;
    if (out_total) *out_total = total;
    return true;
}

// --- تحليل التعبيرات (حسب مستويات الأولوية) ---

// المستوى 1: التعبيرات الأساسية واللاحقة (Postfix)
Node* parse_primary() {
    Node* node = NULL;

    if (parser.current.type == TOKEN_INT) {
        Token tok = parser.current;
        node = ast_node_new(NODE_INT, tok);
        if (!node) return NULL;
        int64_t v = 0;
        (void)parse_int_token_checked(parser.current, &v);
        node->data.integer.value = v;
        eat(TOKEN_INT);
    }
    else if (parser.current.type == TOKEN_FLOAT) {
        Token tok = parser.current;
        node = ast_node_new(NODE_FLOAT, tok);
        if (!node) return NULL;
        double v = 0.0;
        if (!parse_float_token_checked(parser.current, &v)) {
            error_report(parser.current, "صيغة العدد العشري غير صالحة.");
        }
        node->data.float_lit.value = v;
        uint64_t bits = 0;
        memcpy(&bits, &v, sizeof(bits));
        node->data.float_lit.bits = bits;
        eat(TOKEN_FLOAT);
    }
    else if (parser.current.type == TOKEN_STRING) {
        Token tok = parser.current;
        node = ast_node_new(NODE_STRING, tok);
        if (!node) return NULL;
        node->data.string_lit.value = strdup(parser.current.value);
        node->data.string_lit.id = -1;
        eat(TOKEN_STRING);
    }
    else if (parser.current.type == TOKEN_CHAR) {
        Token tok = parser.current;
        node = ast_node_new(NODE_CHAR, tok);
        if (!node) return NULL;
        uint32_t cp = 0;
        if (!utf8_decode_one(parser.current.value, &cp)) {
            error_report(parser.current, "حرف UTF-8 غير صالح.");
            cp = (uint32_t)'?';
        }
        node->data.char_lit.value = (int)cp;
        eat(TOKEN_CHAR);
    }
    else if (parser.current.type == TOKEN_TRUE) {
        Token tok = parser.current;
        node = ast_node_new(NODE_BOOL, tok);
        if (!node) return NULL;
        node->data.bool_lit.value = true;
        eat(TOKEN_TRUE);
    }
    else if (parser.current.type == TOKEN_FALSE) {
        Token tok = parser.current;
        node = ast_node_new(NODE_BOOL, tok);
        if (!node) return NULL;
        node->data.bool_lit.value = false;
        eat(TOKEN_FALSE);
    }
    else if (parser.current.type == TOKEN_SIZEOF) {
        Token tok = parser.current;
        eat(TOKEN_SIZEOF);
        eat(TOKEN_LPAREN);

        node = ast_node_new(NODE_SIZEOF, tok);
        if (!node) return NULL;
        node->data.sizeof_expr.has_type_form = false;
        node->data.sizeof_expr.target_type = TYPE_INT;
        node->data.sizeof_expr.target_type_name = NULL;
        node->data.sizeof_expr.expression = NULL;
        node->data.sizeof_expr.size_bytes = 0;
        node->data.sizeof_expr.size_known = false;

        if (parser_current_starts_type()) {
            DataType dt = TYPE_INT;
            char* tn = NULL;
            if (!parse_type_spec(&dt, &tn)) {
                error_report(parser.current, "متوقع نوع أو تعبير داخل 'حجم(...)'.");
                free(tn);
            } else {
                node->data.sizeof_expr.has_type_form = true;
                node->data.sizeof_expr.target_type = dt;
                node->data.sizeof_expr.target_type_name = tn;
            }
        } else {
            Node* expr = parse_expression();
            node->data.sizeof_expr.has_type_form = false;
            node->data.sizeof_expr.expression = expr;
        }

        eat(TOKEN_RPAREN);
    }
    else if (parser.current.type == TOKEN_IDENTIFIER) {
        Token tok_ident = parser.current;
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        // استدعاء دالة: اسم(...)
        if (parser.current.type == TOKEN_LPAREN) {
            eat(TOKEN_LPAREN);
            Node* head_arg = NULL;
            Node* tail_arg = NULL;

            // تحليل الوسائط (مفصولة بفاصلة)
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    Node* arg = parse_expression();
                    if (!arg) break; // تعافٍ من الخطأ
                    arg->next = NULL;
                    if (head_arg == NULL) { head_arg = arg; tail_arg = arg; }
                    else { tail_arg->next = arg; tail_arg = arg; }

                    if (parser.current.type == TOKEN_COMMA) eat(TOKEN_COMMA);
                    else break;
                }
            }
            eat(TOKEN_RPAREN);

            node = ast_node_new(NODE_CALL_EXPR, tok_ident);
            if (!node) return NULL;
            node->data.call.name = name;
            node->data.call.args = head_arg;
        }
        // الوصول لمصفوفة: اسم[فهرس][فهرس]...
        else if (parser.current.type == TOKEN_LBRACKET) {
            int index_count = 0;
            Node* indices = parse_array_indices(&index_count);

            node = ast_node_new(NODE_ARRAY_ACCESS, tok_ident);
            if (!node) return NULL;
            node->data.array_op.name = name;
            node->data.array_op.indices = indices;
            node->data.array_op.index_count = index_count;
            node->data.array_op.value = NULL;
        }
        // متغير عادي
        else {
            node = ast_node_new(NODE_VAR_REF, tok_ident);
            if (!node) return NULL;
            node->data.var_ref.name = name;
        }
    }
    else if (parser.current.type == TOKEN_LPAREN) {
        eat(TOKEN_LPAREN);
        node = parse_expression();
        eat(TOKEN_RPAREN);
    }
    else {
        if (!parser.panic_mode) {
            error_report(parser.current, "متوقع تعبير.");
            parser.panic_mode = true;
        }
        return NULL;
    }

    // الوصول للأعضاء/القيم المؤهلة: <expr>:<member> (يمكن تكراره للتعشيش)
    while (node && parser.current.type == TOKEN_COLON) {
        Token tok_colon = parser.current;
        eat(TOKEN_COLON);

        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم عضو بعد ':'.");
            break;
        }

        char* member = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        Node* ma = ast_node_new(NODE_MEMBER_ACCESS, tok_colon);
        if (!ma) return NULL;
        ma->data.member_access.base = node;
        ma->data.member_access.member = member;
        node = ma;
    }

    // التحقق من العمليات اللاحقة (Postfix Operators: ++, --)
    if (node && (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC)) {
        Token tok_op = parser.current;
        UnaryOpType op = (parser.current.type == TOKEN_INC) ? UOP_INC : UOP_DEC;
        eat(parser.current.type);

        Node* postfix = ast_node_new(NODE_POSTFIX_OP, tok_op);
        if (!postfix) return NULL;
        postfix->data.unary_op.operand = node;
        postfix->data.unary_op.op = op;
        return postfix;
    }

    return node;
}

// المستوى 1.5: العمليات الأحادية والبادئة (!, ~, -, ++, --)
Node* parse_unary() {
    if (parser.current.type == TOKEN_MINUS || parser.current.type == TOKEN_NOT ||
        parser.current.type == TOKEN_TILDE ||
        parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {

        Token tok_op = parser.current;
        
        UnaryOpType op;
        if (parser.current.type == TOKEN_MINUS) op = UOP_NEG;
        else if (parser.current.type == TOKEN_NOT) op = UOP_NOT;
        else if (parser.current.type == TOKEN_TILDE) op = UOP_BIT_NOT;
        else if (parser.current.type == TOKEN_INC) op = UOP_INC;
        else op = UOP_DEC;

        eat(parser.current.type);
        Node* operand = parse_unary();
        
        if (!operand) return NULL; // Error propagation

        Node* node = ast_node_new(NODE_UNARY_OP, tok_op);
        if (!node) return NULL;
        node->data.unary_op.operand = operand;
        node->data.unary_op.op = op;
        return node;
    }
    return parse_primary();
}

// المستوى 2: عمليات الضرب والقسمة وباقي القسمة (*، /، %)
Node* parse_multiplicative() {
    Node* left = parse_unary();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_STAR || parser.current.type == TOKEN_SLASH || parser.current.type == TOKEN_PERCENT) {
        Token tok_op = parser.current;
        OpType op;
        if (parser.current.type == TOKEN_STAR) op = OP_MUL;
        else if (parser.current.type == TOKEN_SLASH) op = OP_DIV;
        else op = OP_MOD;
        
        eat(parser.current.type);
        Node* right = parse_unary();
        if (!right) return NULL;

        // **Constant Folding Optimization**
        // ملاحظة: يجب أن تتطابق مع قواعد ترقيم الثوابت (ص٣٢ إذا كانت ضمن المدى، وإلا ص٦٤).
        // لذلك نُنفّذ الطي على 32-بت عندما يكون الطرفان ضمن مدى ص٣٢، وإلا على 64-بت.
        if (left->type == NODE_INT && right->type == NODE_INT) {
            int64_t a = left->data.integer.value;
            int64_t b = right->data.integer.value;
            int64_t result = 0;
            bool i32_mode = parser_int_literal_is_i32(a) && parser_int_literal_is_i32(b);

            if (i32_mode) {
                int32_t sa = (int32_t)a;
                int32_t sb = (int32_t)b;
                uint32_t ua = (uint32_t)sa;
                uint32_t ub = (uint32_t)sb;

                if (op == OP_MUL) {
                    uint32_t ur = (uint32_t)((uint64_t)ua * (uint64_t)ub);
                    result = parser_sign_extend_u32(ur);
                } else if (op == OP_DIV) {
                    if (sb == 0) {
                        error_report(parser.current, "قسمة على صفر.");
                        result = 0;
                    } else if (sa == INT32_MIN && sb == -1) {
                        // التفاف آمن (بدون UB)
                        result = (int64_t)INT32_MIN;
                    } else {
                        result = (int64_t)(sa / sb);
                    }
                } else if (op == OP_MOD) {
                    if (sb == 0) {
                        error_report(parser.current, "باقي قسمة على صفر.");
                        result = 0;
                    } else if (sa == INT32_MIN && sb == -1) {
                        result = 0;
                    } else {
                        result = (int64_t)(sa % sb);
                    }
                }
            } else {
                // 64-بت مع التفاف مكمل الاثنين (تجنب UB في C عند تجاوز signed).
                uint64_t ua = (uint64_t)a;
                uint64_t ub = (uint64_t)b;

                if (op == OP_MUL) {
                    result = (int64_t)(ua * ub);
                } else if (op == OP_DIV) {
                    if (b == 0) {
                        error_report(parser.current, "قسمة على صفر.");
                        result = 0;
                    } else if (a == INT64_MIN && b == -1) {
                        result = INT64_MIN;
                    } else {
                        result = a / b;
                    }
                } else if (op == OP_MOD) {
                    if (b == 0) {
                        error_report(parser.current, "باقي قسمة على صفر.");
                        result = 0;
                    } else if (a == INT64_MIN && b == -1) {
                        result = 0;
                    } else {
                        result = a % b;
                    }
                }
            }

            left->data.integer.value = result;
            free(right);
            continue;
        }

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }
    return left;
}

// المستوى 3: عمليات الجمع والطرح (+، -)
Node* parse_additive() {
    Node* left = parse_multiplicative();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS) {
        Token tok_op = parser.current;
        OpType op = (parser.current.type == TOKEN_PLUS) ? OP_ADD : OP_SUB;
        eat(parser.current.type);
        Node* right = parse_multiplicative();
        if (!right) return NULL;

        // **Constant Folding Optimization**
        // مطابق لقواعد ترقيم الثوابت: i32 داخل المدى، وإلا i64.
        if (left->type == NODE_INT && right->type == NODE_INT) {
            int64_t a = left->data.integer.value;
            int64_t b = right->data.integer.value;
            int64_t result = 0;
            bool i32_mode = parser_int_literal_is_i32(a) && parser_int_literal_is_i32(b);

            if (i32_mode) {
                uint32_t ua = (uint32_t)(int32_t)a;
                uint32_t ub = (uint32_t)(int32_t)b;
                uint32_t ur = (op == OP_ADD) ? (ua + ub) : (ua - ub);
                result = parser_sign_extend_u32(ur);
            } else {
                uint64_t ua = (uint64_t)a;
                uint64_t ub = (uint64_t)b;
                result = (op == OP_ADD) ? (int64_t)(ua + ub) : (int64_t)(ua - ub);
            }

            left->data.integer.value = result;
            free(right);
            continue;
        }

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }
    return left;
}

// المستوى 4: عمليات الإزاحة البتية (<<، >>)
Node* parse_shift() {
    Node* left = parse_additive();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_SHL || parser.current.type == TOKEN_SHR) {
        Token tok_op = parser.current;
        OpType op = (parser.current.type == TOKEN_SHL) ? OP_SHL : OP_SHR;
        eat(parser.current.type);
        Node* right = parse_additive();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }

    return left;
}

// المستوى 5: عمليات المقارنة العلائقية
Node* parse_relational() {
    Node* left = parse_shift();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_LT || parser.current.type == TOKEN_GT ||
           parser.current.type == TOKEN_LTE || parser.current.type == TOKEN_GTE) {
        Token tok_op = parser.current;
        OpType op;
        if (parser.current.type == TOKEN_LT) op = OP_LT;
        else if (parser.current.type == TOKEN_GT) op = OP_GT;
        else if (parser.current.type == TOKEN_LTE) op = OP_LTE;
        else op = OP_GTE;
        eat(parser.current.type);
        Node* right = parse_shift();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }
    return left;
}

// المستوى 6: عمليات التساوي
Node* parse_equality() {
    Node* left = parse_relational();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_EQ || parser.current.type == TOKEN_NEQ) {
        Token tok_op = parser.current;
        OpType op = (parser.current.type == TOKEN_EQ) ? OP_EQ : OP_NEQ;
        eat(parser.current.type);
        Node* right = parse_relational();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }
    return left;
}

// المستوى 7: العملية البتية AND (&)
Node* parse_bitwise_and() {
    Node* left = parse_equality();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_AMP) {
        Token tok_op = parser.current;
        eat(TOKEN_AMP);
        Node* right = parse_equality();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_BIT_AND;
        left = new_node;
    }
    return left;
}

// المستوى 8: العملية البتية XOR (^)
Node* parse_bitwise_xor() {
    Node* left = parse_bitwise_and();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_CARET) {
        Token tok_op = parser.current;
        eat(TOKEN_CARET);
        Node* right = parse_bitwise_and();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_BIT_XOR;
        left = new_node;
    }
    return left;
}

// المستوى 9: العملية البتية OR (|)
Node* parse_bitwise_or() {
    Node* left = parse_bitwise_xor();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_PIPE) {
        Token tok_op = parser.current;
        eat(TOKEN_PIPE);
        Node* right = parse_bitwise_xor();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_BIT_OR;
        left = new_node;
    }
    return left;
}

// المستوى 10: العملية المنطقية "و" (&&)
Node* parse_logical_and() {
    Node* left = parse_bitwise_or();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_AND) {
        Token tok_op = parser.current;
        eat(TOKEN_AND);
        Node* right = parse_bitwise_or();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_AND;
        left = new_node;
    }
    return left;
}

// المستوى 11: العملية المنطقية "أو" (||)
Node* parse_logical_or() {
    Node* left = parse_logical_and();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_OR) {
        Token tok_op = parser.current;
        eat(TOKEN_OR);
        Node* right = parse_logical_and();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_OR;
        left = new_node;
    }
    return left;
}

/**
 * @brief نقطة الدخول الرئيسية لتحليل التعبيرات.
 */
Node* parse_expression() {
    return parse_logical_or();
}

// --- تحليل الجمل البرمجية (Statements) ---

Node* parse_case() {
    Token tok_case = parser.current;
    Node* node = ast_node_new(NODE_CASE, tok_case);
    if (!node) return NULL;

    if (parser.current.type == TOKEN_DEFAULT) {
        eat(TOKEN_DEFAULT);
        eat(TOKEN_COLON);
        node->data.case_stmt.is_default = true;
        node->data.case_stmt.value = NULL;
    } else {
        eat(TOKEN_CASE);
        // قيمة case يجب أن تكون ثابتاً بسيطاً (رقم/حرف) ولا يجب أن تستهلك ':' كعامل وصول للأعضاء.
        if (parser.current.type == TOKEN_INT) {
            Token tok = parser.current;
            Node* v = ast_node_new(NODE_INT, tok);
            if (!v) return NULL;
            int64_t n = 0;
            (void)parse_int_token_checked(parser.current, &n);
            v->data.integer.value = n;
            eat(TOKEN_INT);
            node->data.case_stmt.value = v;
        }
        else if (parser.current.type == TOKEN_CHAR) {
            Token tok = parser.current;
            Node* v = ast_node_new(NODE_CHAR, tok);
            if (!v) return NULL;
            uint32_t cp = 0;
            if (!utf8_decode_one(parser.current.value, &cp)) {
                error_report(parser.current, "حرف UTF-8 غير صالح.");
                cp = (uint32_t)'?';
            }
            v->data.char_lit.value = (int)cp;
            eat(TOKEN_CHAR);
            node->data.case_stmt.value = v;
        }
        else {
            error_report(parser.current, "متوقع ثابت عددي أو حرفي بعد 'حالة'.");
            node->data.case_stmt.value = NULL;
        }
        eat(TOKEN_COLON);
        node->data.case_stmt.is_default = false;
    }

    Node* head_stmt = NULL;
    Node* tail_stmt = NULL;

    while (parser.current.type != TOKEN_CASE && 
           parser.current.type != TOKEN_DEFAULT && 
           parser.current.type != TOKEN_RBRACE && 
           parser.current.type != TOKEN_EOF) {
        
        Node* stmt = parse_statement();
        if (stmt) {
            if (head_stmt == NULL) { head_stmt = stmt; tail_stmt = stmt; }
            else { tail_stmt->next = stmt; tail_stmt = stmt; }
        } else {
            synchronize_mode(PARSER_SYNC_SWITCH); // تعافٍ داخل جملة اختر
        }
    }

    node->data.case_stmt.body = head_stmt;
    return node;
}

Node* parse_switch() {
    Token tok_switch = parser.current;
    eat(TOKEN_SWITCH);
    eat(TOKEN_LPAREN);
    Node* expr = parse_expression();
    eat(TOKEN_RPAREN);
    eat(TOKEN_LBRACE);

    Node* head_case = NULL;
    Node* tail_case = NULL;

    while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
        if (parser.current.type == TOKEN_CASE || parser.current.type == TOKEN_DEFAULT) {
            Node* case_node = parse_case();
            if (head_case == NULL) { head_case = case_node; tail_case = case_node; }
            else { tail_case->next = case_node; tail_case = case_node; }
        } else {
            error_report(parser.current, "متوقع 'حالة' أو 'افتراضي' داخل 'اختر'.");
            synchronize_mode(PARSER_SYNC_SWITCH);
        }
    }
    eat(TOKEN_RBRACE);

    Node* node = ast_node_new(NODE_SWITCH, tok_switch);
    if (!node) return NULL;
    node->data.switch_stmt.expression = expr;
    node->data.switch_stmt.cases = head_case;
    return node;
}

Node* parse_block() {
    Token tok_lbrace = parser.current;
    eat(TOKEN_LBRACE);
    Node* head = NULL;
    Node* tail = NULL;
    while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
        Node* stmt = parse_statement();
        if (stmt) {
            if (head == NULL) { head = stmt; tail = stmt; } 
            else { tail->next = stmt; tail = stmt; }
        } else {
            synchronize_mode(PARSER_SYNC_STATEMENT);
        }
    }
    eat(TOKEN_RBRACE);
    Node* block = ast_node_new(NODE_BLOCK, tok_lbrace);
    if (!block) return NULL;
    block->data.block.statements = head;
    return block;
}

// ============================================================================
// تهيئة المصفوفات (Array Initializer Lists)
// ============================================================================

/**
 * @brief تحليل عنصر واحد داخل تهيئة المصفوفة (قيمة أو كتلة متداخلة).
 */
static void parse_array_initializer_item(Node** head, Node** tail, int* count);

/**
 * @brief تحليل كتلة تهيئة مصفوفة بالشكل: { ... } مع دعم التعشيش.
 */
static void parse_array_initializer_group(Node** head, Node** tail, int* count)
{
    eat(TOKEN_LBRACE);

    if (parser.current.type != TOKEN_RBRACE) {
        while (1) {
            parse_array_initializer_item(head, tail, count);

            if (parser.current.type == TOKEN_COMMA) {
                eat(TOKEN_COMMA);
                // السماح بفاصلة أخيرة قبل '}' كما في C.
                if (parser.current.type == TOKEN_RBRACE) break;
                continue;
            }
            break;
        }
    }

    eat(TOKEN_RBRACE);
}

/**
 * @brief تحليل عنصر واحد داخل تهيئة المصفوفة (قيمة أو كتلة متداخلة).
 */
static void parse_array_initializer_item(Node** head, Node** tail, int* count)
{
    if (parser.current.type == TOKEN_LBRACE) {
        parse_array_initializer_group(head, tail, count);
        return;
    }

    Node* expr = parse_expression();
    if (!expr) return;

    parser_list_append(head, tail, expr);
    if (count) (*count)++;
}

/**
 * @brief تحليل قائمة تهيئة مصفوفة بالشكل: = { expr, { ... }, ... }
 *
 * عند وجود '=' نعتبر أن التهيئة موجودة حتى لو كانت القائمة فارغة `{}`.
 *
 * @param out_count عدد عناصر التهيئة الفعلية
 * @param out_has_init هل تم العثور على '=' (تهيئة صريحة)
 * @return رأس قائمة العقد (قد تكون NULL عند `{}` أو عند عدم وجود تهيئة)
 */
static Node* parse_array_initializer_list(int* out_count, bool* out_has_init) {
    if (out_count) *out_count = 0;
    if (out_has_init) *out_has_init = false;

    if (parser.current.type != TOKEN_ASSIGN) {
        return NULL;
    }

    if (out_has_init) *out_has_init = true;
    eat(TOKEN_ASSIGN);

    Node* head = NULL;
    Node* tail = NULL;
    int count = 0;

    if (parser.current.type != TOKEN_LBRACE) {
        error_report(parser.current, "متوقع '{' بعد '=' في تهيئة المصفوفة.");
        return NULL;
    }

    parse_array_initializer_group(&head, &tail, &count);

    if (out_count) *out_count = count;
    return head;
}

/**
 * @brief تحليل تعريف اسم نوع بديل على المستوى العام.
 *
 * الصيغة:
 * نوع <اسم> = <نوع_موجود>.
 */
static Node* parse_type_alias_declaration(bool register_alias)
{
    if (!parser_current_is_type_alias_keyword()) {
        error_report(parser.current, "متوقع 'نوع' في بداية تعريف الاسم البديل.");
        synchronize_mode(PARSER_SYNC_DECLARATION);
        return NULL;
    }

    Token tok_kw = parser.current;
    eat(parser.current.type);

    if (parser.current.type != TOKEN_IDENTIFIER) {
        error_report(parser.current, "متوقع اسم بعد 'نوع'.");
        synchronize_mode(PARSER_SYNC_DECLARATION);
        return NULL;
    }

    Token tok_name = parser.current;
    char* alias_name = strdup(parser.current.value);
    eat(TOKEN_IDENTIFIER);

    if (!alias_name) {
        error_report(tok_name, "نفدت الذاكرة أثناء نسخ اسم النوع البديل.");
        synchronize_mode(PARSER_SYNC_DECLARATION);
        return NULL;
    }

    eat(TOKEN_ASSIGN);

    DataType target_type = TYPE_INT;
    char* target_type_name = NULL;
    if (!parse_type_spec(&target_type, &target_type_name)) {
        error_report(parser.current, "متوقع نوع معروف بعد '=' في تعريف الاسم البديل.");
        free(alias_name);
        free(target_type_name);
        synchronize_mode(PARSER_SYNC_DECLARATION);
        return NULL;
    }

    eat(TOKEN_DOT);

    Node* alias = ast_node_new(NODE_TYPE_ALIAS, tok_name);
    if (!alias) {
        free(alias_name);
        free(target_type_name);
        return NULL;
    }

    alias->data.type_alias.name = alias_name;
    alias->data.type_alias.target_type = target_type;
    alias->data.type_alias.target_type_name = target_type_name;

    if (register_alias) {
        (void)parser_type_alias_register(tok_kw, alias_name, target_type, target_type_name);
    }
    return alias;
}

Node* parse_statement() {
    if (parser.current.type == TOKEN_LBRACE) return parse_block();

    if (parser_current_is_type_alias_keyword() && parser.next.type == TOKEN_IDENTIFIER) {
        error_report(parser.current, "تعريف 'نوع' مسموح فقط على المستوى العام.");
        return parse_type_alias_declaration(false);
    }

    if (parser.current.type == TOKEN_SWITCH) {
        return parse_switch();
    }

    if (parser.current.type == TOKEN_RETURN) {
        Token tok = parser.current;
        eat(TOKEN_RETURN);
        Node* stmt = ast_node_new(NODE_RETURN, tok);
        if (!stmt) return NULL;
        if (parser.current.type == TOKEN_DOT) {
            stmt->data.return_stmt.expression = NULL;
        } else {
            stmt->data.return_stmt.expression = parse_expression();
        }
        eat(TOKEN_DOT);
        return stmt;
    }

    if (parser.current.type == TOKEN_BREAK) {
        Token tok = parser.current;
        eat(TOKEN_BREAK);
        eat(TOKEN_DOT);
        Node* stmt = ast_node_new(NODE_BREAK, tok);
        return stmt;
    }

    if (parser.current.type == TOKEN_CONTINUE) {
        Token tok = parser.current;
        eat(TOKEN_CONTINUE);
        eat(TOKEN_DOT);
        Node* stmt = ast_node_new(NODE_CONTINUE, tok);
        return stmt;
    }

    if (parser.current.type == TOKEN_PRINT) {
        Token tok = parser.current;
        eat(TOKEN_PRINT);
        Node* stmt = ast_node_new(NODE_PRINT, tok);
        if (!stmt) return NULL;
        stmt->data.print_stmt.expression = parse_expression();
        eat(TOKEN_DOT);
        return stmt;
    }

    if (parser.current.type == TOKEN_READ) {
        Token tok = parser.current;
        eat(TOKEN_READ);

        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم متغير بعد 'اقرأ'.");
            return NULL;
        }

        Node* stmt = ast_node_new(NODE_READ, tok);
        if (!stmt) return NULL;

        stmt->data.read_stmt.var_name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);
        eat(TOKEN_DOT);
        return stmt;
    }

    if (parser.current.type == TOKEN_IF) {
        Token tok = parser.current;
        eat(TOKEN_IF);
        eat(TOKEN_LPAREN);
        Node* cond = parse_expression();
        eat(TOKEN_RPAREN);
        Node* then_branch = parse_statement();

        Node* else_branch = NULL;
        if (parser.current.type == TOKEN_ELSE) {
            eat(TOKEN_ELSE);
            else_branch = parse_statement();
        }

        Node* stmt = ast_node_new(NODE_IF, tok);
        if (!stmt) return NULL;
        stmt->data.if_stmt.condition = cond;
        stmt->data.if_stmt.then_branch = then_branch;
        stmt->data.if_stmt.else_branch = else_branch;
        return stmt;
    }

    if (parser.current.type == TOKEN_WHILE) {
        Token tok = parser.current;
        eat(TOKEN_WHILE);
        eat(TOKEN_LPAREN);
        Node* cond = parse_expression();
        eat(TOKEN_RPAREN);
        Node* body = parse_statement();

        Node* stmt = ast_node_new(NODE_WHILE, tok);
        if (!stmt) return NULL;
        stmt->data.while_stmt.condition = cond;
        stmt->data.while_stmt.body = body;
        return stmt;
    }

    if (parser.current.type == TOKEN_FOR) {
        Token tok_for = parser.current;
        eat(TOKEN_FOR);
        eat(TOKEN_LPAREN);

        Node* init = NULL;
        if (parser_is_decl_qualifier(parser.current.type) || parser_current_starts_type()) {
            ParserDeclQualifiers init_q;
            parser_parse_decl_qualifiers(&init_q);

            if (!parser_current_starts_type()) {
                error_report(parser.current, "متوقع نوع في تهيئة حلقة 'لكل'.");
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }

            Token tok_type = parser.current;
            DataType dt = TYPE_INT;
            char* type_name = NULL;
            (void)tok_type;

            if (!parse_type_spec(&dt, &type_name)) {
                error_report(parser.current, "متوقع نوع في تهيئة حلقة 'لكل'.");
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }

            if (dt == TYPE_STRUCT || dt == TYPE_UNION) {
                error_report(parser.current, "تعريف الهيكل داخل تهيئة 'لكل' غير مدعوم حالياً.");
            }

            Token tok_name = parser.current;
            if (parser.current.type != TOKEN_IDENTIFIER) {
                error_report(parser.current, "متوقع اسم معرّف في تهيئة حلقة 'لكل'.");
                free(type_name);
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);

            if (init_q.is_const && !init_q.is_static && parser.current.type != TOKEN_ASSIGN) {
                error_report(parser.current, "الثابت يجب تهيئته.");
            }

            Node* expr = NULL;
            if (parser.current.type == TOKEN_ASSIGN) {
                eat(TOKEN_ASSIGN);
                expr = parse_expression();
            } else if (!init_q.is_static) {
                error_report(parser.current, "متوقع '=' في تهيئة حلقة 'لكل'.");
                eat(TOKEN_ASSIGN);
                expr = parse_expression();
            }
            eat(TOKEN_SEMICOLON);

            init = ast_node_new(NODE_VAR_DECL, tok_name);
            if (!init) { free(type_name); return NULL; }
            init->data.var_decl.name = name;
            init->data.var_decl.type = dt;
            init->data.var_decl.type_name = type_name;
            init->data.var_decl.expression = expr;
            init->data.var_decl.is_global = false;
            init->data.var_decl.is_const = init_q.is_const;
            init->data.var_decl.is_static = init_q.is_static;
        } else {
            if (parser.current.type == TOKEN_IDENTIFIER && parser.next.type == TOKEN_ASSIGN) {
                Token tok_name = parser.current;
                char* name = strdup(parser.current.value);
                eat(TOKEN_IDENTIFIER);
                eat(TOKEN_ASSIGN);
                Node* expr = parse_expression();
                eat(TOKEN_SEMICOLON);

                init = ast_node_new(NODE_ASSIGN, tok_name);
                if (!init) return NULL;
                init->data.assign_stmt.name = name;
                init->data.assign_stmt.expression = expr;
            } else {
                if (parser.current.type != TOKEN_SEMICOLON) {
                    init = parse_expression();
                }
                eat(TOKEN_SEMICOLON);
            }
        }

        Node* cond = NULL;
        if (parser.current.type != TOKEN_SEMICOLON) {
            cond = parse_expression();
        }
        eat(TOKEN_SEMICOLON);

        Node* incr = NULL;
        if (parser.current.type == TOKEN_IDENTIFIER && parser.next.type == TOKEN_ASSIGN) {
            Token tok_name = parser.current;
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();

            incr = ast_node_new(NODE_ASSIGN, tok_name);
            if (!incr) return NULL;
            incr->data.assign_stmt.name = name;
            incr->data.assign_stmt.expression = expr;
        } else {
            incr = parse_expression();
        }
        eat(TOKEN_RPAREN);

        Node* body = parse_statement();

        Node* stmt = ast_node_new(NODE_FOR, tok_for);
        if (!stmt) return NULL;
        stmt->data.for_stmt.init = init;
        stmt->data.for_stmt.condition = cond;
        stmt->data.for_stmt.increment = incr;
        stmt->data.for_stmt.body = body;
        return stmt;
    }

    ParserDeclQualifiers decl_q;
    bool has_decl_qual = parser_is_decl_qualifier(parser.current.type);
    if (has_decl_qual) {
        parser_parse_decl_qualifiers(&decl_q);
    } else {
        memset(&decl_q, 0, sizeof(decl_q));
    }

    if (parser_current_is_type_alias_keyword() && parser.next.type == TOKEN_IDENTIFIER) {
        if (decl_q.is_const || decl_q.is_static) {
            error_report(parser.current, "تعريف اسم النوع البديل لا يقبل 'ثابت' أو 'ساكن'.");
        }
        error_report(parser.current, "تعريف 'نوع' مسموح فقط على المستوى العام.");
        return parse_type_alias_declaration(false);
    }

    if (has_decl_qual || parser_current_starts_type()) {
        if (!parser_current_starts_type()) {
            error_report(parser.current, "متوقع نوع بعد واصفات التعريف.");
            synchronize_mode(PARSER_SYNC_STATEMENT);
            return NULL;
        }

        bool is_const = decl_q.is_const;
        bool is_static = decl_q.is_static;
        Token tok_type = parser.current;
        DataType dt = TYPE_INT;
        char* type_name = NULL;
        (void)tok_type;

        if (!parse_type_spec(&dt, &type_name)) {
            error_report(parser.current, "متوقع نوع.");
            synchronize_mode(PARSER_SYNC_STATEMENT);
            return NULL;
        }

        Token tok_name = parser.current;
        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم معرّف بعد النوع.");
            free(type_name);
            synchronize_mode(PARSER_SYNC_STATEMENT);
            return NULL;
        }
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        if (dt == TYPE_VOID) {
            error_report(tok_type, "لا يمكن تعريف متغير من نوع 'عدم'.");
        }

        if (parser.current.type == TOKEN_LBRACKET) {
            int* dims = NULL;
            int dim_count = 0;
            int64_t total_elems = 0;
            if (!parse_array_dimensions(&dims, &dim_count, &total_elems)) {
                free(type_name);
                free(name);
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }

            int init_count = 0;
            bool has_init = false;
            Node* init_vals = parse_array_initializer_list(&init_count, &has_init);

            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_ARRAY_DECL, tok_name);
            if (!stmt) {
                free(dims);
                free(type_name);
                free(name);
                return NULL;
            }
            stmt->data.array_decl.name = name;
            stmt->data.array_decl.element_type = dt;
            stmt->data.array_decl.element_type_name = type_name;
            stmt->data.array_decl.element_struct_size = 0;
            stmt->data.array_decl.element_struct_align = 0;
            stmt->data.array_decl.dims = dims;
            stmt->data.array_decl.dim_count = dim_count;
            stmt->data.array_decl.total_elems = total_elems;
            stmt->data.array_decl.is_global = false;
            stmt->data.array_decl.is_const = is_const;
            stmt->data.array_decl.is_static = is_static;
            stmt->data.array_decl.has_init = has_init;
            stmt->data.array_decl.init_values = init_vals;
            stmt->data.array_decl.init_count = init_count;
            return stmt;
        }

        // تعريف هيكل (محلي): هيكل <T> <name>.
        if (dt == TYPE_STRUCT || dt == TYPE_UNION) {
            if (is_const) {
                error_report(decl_q.tok_const, "لا يمكن تعريف نوع مركب ثابت بدون تهيئة (غير مدعوم حالياً).");
            }
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_VAR_DECL, tok_name);
            if (!stmt) { free(type_name); return NULL; }
            stmt->data.var_decl.name = name;
            stmt->data.var_decl.type = dt;
            stmt->data.var_decl.type_name = type_name;
            stmt->data.var_decl.expression = NULL;
            stmt->data.var_decl.is_global = false;
            stmt->data.var_decl.is_const = is_const;
            stmt->data.var_decl.is_static = is_static;
            return stmt;
        }

        if (is_const && !is_static && parser.current.type != TOKEN_ASSIGN) {
            error_report(parser.current, "الثابت يجب تهيئته.");
        }

        Node* expr = NULL;
        if (parser.current.type == TOKEN_ASSIGN) {
            eat(TOKEN_ASSIGN);
            expr = parse_expression();
        } else if (!is_static) {
            eat(TOKEN_ASSIGN);
            expr = parse_expression();
        }
        eat(TOKEN_DOT);

        Node* stmt = ast_node_new(NODE_VAR_DECL, tok_name);
        if (!stmt) { free(type_name); return NULL; }
        stmt->data.var_decl.name = name;
        stmt->data.var_decl.type = dt;
        stmt->data.var_decl.type_name = type_name;
        stmt->data.var_decl.expression = expr;
        stmt->data.var_decl.is_global = false;
        stmt->data.var_decl.is_const = is_const;
        stmt->data.var_decl.is_static = is_static;
        return stmt;
    }

    if (parser.current.type == TOKEN_IDENTIFIER) {
        Token tok_name = parser.current;

        // إسناد إلى عضو: س:حقل = ... .
        if (parser.next.type == TOKEN_COLON) {
            Node* target = parse_expression();
            eat(TOKEN_ASSIGN);
            Node* value = parse_expression();
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_MEMBER_ASSIGN, tok_name);
            if (!stmt) return NULL;
            stmt->data.member_assign.target = target;
            stmt->data.member_assign.value = value;
            return stmt;
        }

        if (parser.next.type == TOKEN_LBRACKET) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            int index_count = 0;
            Node* indices = parse_array_indices(&index_count);

            // إسناد إلى عضو عنصر مصفوفة: س[0]:حقل = ...
            if (parser.current.type == TOKEN_COLON) {
                Node* target = ast_node_new(NODE_ARRAY_ACCESS, tok_name);
                if (!target) return NULL;
                target->data.array_op.name = name;
                target->data.array_op.indices = indices;
                target->data.array_op.index_count = index_count;
                target->data.array_op.value = NULL;

                while (parser.current.type == TOKEN_COLON) {
                    Token tok_colon = parser.current;
                    eat(TOKEN_COLON);
                    if (parser.current.type != TOKEN_IDENTIFIER) {
                        error_report(parser.current, "متوقع اسم عضو بعد ':'.");
                        break;
                    }
                    char* member = strdup(parser.current.value);
                    eat(TOKEN_IDENTIFIER);

                    Node* ma = ast_node_new(NODE_MEMBER_ACCESS, tok_colon);
                    if (!ma) return NULL;
                    ma->data.member_access.base = target;
                    ma->data.member_access.member = member;
                    target = ma;
                }

                eat(TOKEN_ASSIGN);
                Node* value = parse_expression();
                eat(TOKEN_DOT);

                Node* stmt = ast_node_new(NODE_MEMBER_ASSIGN, tok_name);
                if (!stmt) return NULL;
                stmt->data.member_assign.target = target;
                stmt->data.member_assign.value = value;
                return stmt;
            }

            if (parser.current.type == TOKEN_ASSIGN) {
                eat(TOKEN_ASSIGN);
                Node* value = parse_expression();
                eat(TOKEN_DOT);

                Node* stmt = ast_node_new(NODE_ARRAY_ASSIGN, tok_name);
                if (!stmt) return NULL;
                stmt->data.array_op.name = name;
                stmt->data.array_op.indices = indices;
                stmt->data.array_op.index_count = index_count;
                stmt->data.array_op.value = value;
                return stmt;
            } else if (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {
                // array[index]++.
                Node* access = ast_node_new(NODE_ARRAY_ACCESS, tok_name);
                if (!access) return NULL;
                access->data.array_op.name = name;
                access->data.array_op.indices = indices;
                access->data.array_op.index_count = index_count;
                access->data.array_op.value = NULL;

                Token tok_op = parser.current;
                UnaryOpType op = (parser.current.type == TOKEN_INC) ? UOP_INC : UOP_DEC;
                eat(parser.current.type);
                eat(TOKEN_DOT);

                Node* stmt = ast_node_new(NODE_POSTFIX_OP, tok_op);
                if (!stmt) return NULL;
                stmt->data.unary_op.operand = access;
                stmt->data.unary_op.op = op;
                return stmt;
            }
        } else if (parser.next.type == TOKEN_ASSIGN) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_ASSIGN, tok_name);
            if (!stmt) return NULL;
            stmt->data.assign_stmt.name = name;
            stmt->data.assign_stmt.expression = expr;
            return stmt;
        } else if (parser.next.type == TOKEN_LPAREN) {
            Node* expr = parse_expression();
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_CALL_STMT, tok_name);
            if (!stmt) return NULL;
            stmt->data.call.name = expr->data.call.name;
            stmt->data.call.args = expr->data.call.args;
            free(expr);
            return stmt;
        } else if (parser.next.type == TOKEN_INC || parser.next.type == TOKEN_DEC) {
            Node* expr = parse_expression();
            eat(TOKEN_DOT);
            return expr;
        }
    }

    if (!parser.panic_mode) {
        error_report(parser.current, "وحدة أو جملة غير متوقعة.");
        parser.panic_mode = true;
    }
    return NULL;
}

Node* parse_declaration() {
    ParserDeclQualifiers decl_q;
    parser_parse_decl_qualifiers(&decl_q);

    if (parser_current_is_type_alias_keyword() && parser.next.type == TOKEN_IDENTIFIER) {
        if (decl_q.is_const || decl_q.is_static) {
            error_report(parser.current, "تعريف اسم النوع البديل لا يقبل 'ثابت' أو 'ساكن'.");
            return parse_type_alias_declaration(false);
        }
        return parse_type_alias_declaration(true);
    }

    bool is_const = decl_q.is_const;
    bool is_static = decl_q.is_static;

    if (parser_current_starts_type()) {
        Token tok_type = parser.current;
        DataType dt = TYPE_INT;
        char* type_name = NULL;
        (void)tok_type;

        if (!parse_type_spec(&dt, &type_name)) {
            error_report(parser.current, "متوقع نوع.");
            synchronize_mode(PARSER_SYNC_DECLARATION);
            return NULL;
        }

        // تعريف تعداد/هيكل/اتحاد: تعداد <name> { ... }  |  هيكل <name> { ... } | اتحاد <name> { ... }
        if ((dt == TYPE_ENUM || dt == TYPE_STRUCT || dt == TYPE_UNION) && parser.current.type == TOKEN_LBRACE) {
            if (is_const || is_static) {
                error_report(parser.current, "لا يمكن وسم تعريف نوع بـ 'ثابت' أو 'ساكن'.");
            }

            Token tok_name = tok_type;
            if (dt == TYPE_ENUM) {
                eat(TOKEN_LBRACE);

                Node* head = NULL;
                Node* tail = NULL;

                if (parser.current.type != TOKEN_RBRACE) {
                    while (1) {
                        if (parser.current.type != TOKEN_IDENTIFIER) {
                            error_report(parser.current, "متوقع اسم عنصر تعداد.");
                            break;
                        }
                        Token tok_mem = parser.current;
                        char* mem_name = strdup(parser.current.value);
                        eat(TOKEN_IDENTIFIER);

                        Node* mem = ast_node_new(NODE_ENUM_MEMBER, tok_mem);
                        if (!mem) return NULL;
                        mem->data.enum_member.name = mem_name;
                        mem->data.enum_member.value = 0;
                        mem->data.enum_member.has_value = false;

                        if (!head) { head = mem; tail = mem; }
                        else { tail->next = mem; tail = mem; }

                        if (parser.current.type == TOKEN_COMMA) {
                            eat(TOKEN_COMMA);
                            if (parser.current.type == TOKEN_RBRACE) break; // السماح بفاصلة أخيرة
                            continue;
                        }
                        break;
                    }
                }

                eat(TOKEN_RBRACE);

                Node* en = ast_node_new(NODE_ENUM_DECL, tok_name);
                if (!en) return NULL;
                en->data.enum_decl.name = type_name;
                en->data.enum_decl.members = head;
                return en;
            }

            // struct / union
            eat(TOKEN_LBRACE);
            Node* head_f = NULL;
            Node* tail_f = NULL;
            while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
                bool field_const = false;
                Token tok_field_const = {0};
                if (parser.current.type == TOKEN_CONST) {
                    field_const = true;
                    tok_field_const = parser.current;
                    eat(TOKEN_CONST);
                }

                DataType fdt = TYPE_INT;
                char* ftype_name = NULL;
                if (!parse_type_spec(&fdt, &ftype_name)) {
                    error_report(parser.current, "متوقع نوع حقل داخل الهيكل/الاتحاد.");
                    free(ftype_name);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    break;
                }

                if (parser.current.type != TOKEN_IDENTIFIER) {
                    error_report(parser.current, "متوقع اسم حقل داخل الهيكل/الاتحاد.");
                    free(ftype_name);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    break;
                }

                Token tok_field_name = parser.current;
                char* fname = strdup(parser.current.value);
                eat(TOKEN_IDENTIFIER);

                if (parser.current.type == TOKEN_LBRACKET) {
                    error_report(parser.current, "حقول المصفوفات داخل الهيكل/الاتحاد غير مدعومة بعد.");
                    free(ftype_name);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    break;
                }

                eat(TOKEN_DOT);

                Node* field = ast_node_new(NODE_VAR_DECL, tok_field_name);
                if (!field) return NULL;
                field->data.var_decl.name = fname;
                field->data.var_decl.type = fdt;
                field->data.var_decl.type_name = ftype_name;
                field->data.var_decl.struct_size = 0;
                field->data.var_decl.struct_align = 0;
                field->data.var_decl.expression = NULL;
                field->data.var_decl.is_global = false;
                field->data.var_decl.is_const = field_const;
                (void)tok_field_const;

                if (!head_f) { head_f = field; tail_f = field; }
                else { tail_f->next = field; tail_f = field; }
            }
            eat(TOKEN_RBRACE);

            if (dt == TYPE_STRUCT) {
                Node* st = ast_node_new(NODE_STRUCT_DECL, tok_name);
                if (!st) return NULL;
                st->data.struct_decl.name = type_name;
                st->data.struct_decl.fields = head_f;
                return st;
            }

            Node* un = ast_node_new(NODE_UNION_DECL, tok_name);
            if (!un) return NULL;
            un->data.union_decl.name = type_name;
            un->data.union_decl.fields = head_f;
            return un;
        }

        // متغير/دالة: نحتاج اسم الرمز بعد نوعه
        Token tok_name = parser.current;
        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم معرّف بعد النوع.");
            free(type_name);
            synchronize_mode(PARSER_SYNC_DECLARATION);
            return NULL;
        }

        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        if (dt == TYPE_VOID && parser.current.type != TOKEN_LPAREN) {
            error_report(tok_type, "لا يمكن تعريف متغير عام من نوع 'عدم'.");
        }

        // دالة: نسمح فقط بأنواع بدائية حالياً
        if (parser.current.type == TOKEN_LPAREN) {
                if (dt == TYPE_ENUM || dt == TYPE_STRUCT || dt == TYPE_UNION) {
                    error_report(parser.current, "أنواع الإرجاع المعرفة من المستخدم غير مدعومة بعد في تواقيع الدوال.");
                    free(type_name);
                    free(name);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    return NULL;
                }
            if (is_const || is_static) {
                error_report(parser.current, "لا يمكن تعريف الدوال بوسم 'ثابت' أو 'ساكن'.");
            }

            eat(TOKEN_LPAREN);
            Node* head_param = NULL;
            Node* tail_param = NULL;
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    // معاملات الدالة: نسمح بالأنواع البدائية العددية/النص/منطقي/حرف.
                    Token tok_param_type = parser.current;
                    DataType param_dt = TYPE_INT;
                    char* param_tn = NULL;
                    if (!parse_type_spec(&param_dt, &param_tn)) {
                        error_report(parser.current, "متوقع نوع للمعامل.");
                        synchronize_mode(PARSER_SYNC_DECLARATION);
                        return NULL;
                    }
                    if (param_dt == TYPE_ENUM || param_dt == TYPE_STRUCT || param_dt == TYPE_UNION) {
                        error_report(tok_param_type, "أنواع المعاملات المعرفة من المستخدم غير مدعومة بعد في تواقيع الدوال.");
                        if (param_tn) free(param_tn);
                        param_tn = NULL;
                        param_dt = TYPE_INT;
                    } else if (param_dt == TYPE_VOID) {
                        error_report(tok_param_type, "لا يمكن استخدام 'عدم' كنوع معامل.");
                        if (param_tn) free(param_tn);
                        param_tn = NULL;
                        param_dt = TYPE_INT;
                    }

                    char* pname = NULL;
                    Token tok_param_name = tok_param_type;
                    if (parser.current.type == TOKEN_IDENTIFIER) {
                        tok_param_name = parser.current;
                        pname = strdup(parser.current.value);
                        eat(TOKEN_IDENTIFIER);
                    }

                    Node* param = ast_node_new(NODE_VAR_DECL, tok_param_name);
                    if (!param) return NULL;
                    param->data.var_decl.name = pname;
                    param->data.var_decl.type = param_dt;
                    param->data.var_decl.type_name = param_tn;
                    param->data.var_decl.expression = NULL;
                    param->data.var_decl.is_global = false;
                    param->data.var_decl.is_const = false;
                    if (head_param == NULL) { head_param = param; tail_param = param; }
                    else { tail_param->next = param; tail_param = param; }

                    if (parser.current.type == TOKEN_COMMA) eat(TOKEN_COMMA);
                    else break;
                }
            }
            eat(TOKEN_RPAREN);

            Node* body = NULL;
            bool is_proto = false;

            if (parser.current.type == TOKEN_DOT) {
                eat(TOKEN_DOT);
                is_proto = true;
            } else {
                body = parse_block();
                is_proto = false;
            }

            Node* func = ast_node_new(NODE_FUNC_DEF, tok_name);
            if (!func) return NULL;
            func->data.func_def.name = name;
            func->data.func_def.return_type = dt;
            func->data.func_def.params = head_param;
            func->data.func_def.body = body;
            func->data.func_def.is_prototype = is_proto;
            free(type_name);
            return func;
        }

        // تعريف مصفوفة عامة ثابتة الأبعاد
        if (parser.current.type == TOKEN_LBRACKET) {
            int* dims = NULL;
            int dim_count = 0;
            int64_t total_elems = 0;
            if (!parse_array_dimensions(&dims, &dim_count, &total_elems)) {
                free(type_name);
                free(name);
                synchronize_mode(PARSER_SYNC_DECLARATION);
                return NULL;
            }

            int init_count = 0;
            bool has_init = false;
            Node* init_vals = parse_array_initializer_list(&init_count, &has_init);

            eat(TOKEN_DOT);

            Node* arr = ast_node_new(NODE_ARRAY_DECL, tok_name);
            if (!arr) {
                free(dims);
                free(type_name);
                free(name);
                return NULL;
            }
            arr->data.array_decl.name = name;
            arr->data.array_decl.element_type = dt;
            arr->data.array_decl.element_type_name = type_name;
            arr->data.array_decl.element_struct_size = 0;
            arr->data.array_decl.element_struct_align = 0;
            arr->data.array_decl.dims = dims;
            arr->data.array_decl.dim_count = dim_count;
            arr->data.array_decl.total_elems = total_elems;
            arr->data.array_decl.is_global = true;
            arr->data.array_decl.is_const = is_const;
            arr->data.array_decl.is_static = is_static;
            arr->data.array_decl.has_init = has_init;
            arr->data.array_decl.init_values = init_vals;
            arr->data.array_decl.init_count = init_count;
            return arr;
        }

        // متغير عام
        if (dt == TYPE_STRUCT || dt == TYPE_UNION) {
            if (is_const) {
                error_report(parser.current, "لا يمكن تعريف نوع مركب ثابت بدون تهيئة (غير مدعوم حالياً).");
            }
            if (parser.current.type == TOKEN_ASSIGN) {
                error_report(parser.current, "تهيئة الأنواع المركبة العامة بالصيغة '=' غير مدعومة حالياً.");
                eat(TOKEN_ASSIGN);
                (void)parse_expression();
            }
            eat(TOKEN_DOT);

            Node* var = ast_node_new(NODE_VAR_DECL, tok_name);
            if (!var) return NULL;
            var->data.var_decl.name = name;
            var->data.var_decl.type = dt;
            var->data.var_decl.type_name = type_name;
            var->data.var_decl.expression = NULL;
            var->data.var_decl.is_global = true;
            var->data.var_decl.is_const = is_const;
            var->data.var_decl.is_static = is_static;
            return var;
        }

        Node* expr = NULL;
        if (parser.current.type == TOKEN_ASSIGN) {
            eat(TOKEN_ASSIGN);
            expr = parse_expression();
        }
        eat(TOKEN_DOT);

        Node* var = ast_node_new(NODE_VAR_DECL, tok_name);
        if (!var) { free(type_name); return NULL; }
        var->data.var_decl.name = name;
        var->data.var_decl.type = dt;
        var->data.var_decl.type_name = type_name;
        var->data.var_decl.expression = expr;
        var->data.var_decl.is_global = true;
        var->data.var_decl.is_const = is_const;
        var->data.var_decl.is_static = is_static;
        return var;
    }
    
    if (!parser.panic_mode) {
        error_report(parser.current, "متوقع تصريح (دالة أو متغير عام).");
        parser.panic_mode = true;
    }
    synchronize_mode(PARSER_SYNC_DECLARATION);
    return NULL;
}

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

