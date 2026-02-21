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
        error_report(parser.next, "Found invalid token.");
    }
}

/**
 * @brief تهيئة المحلل القواعدي ولبدء قراءة أول وحدتين.
 */
void init_parser(Lexer* l) {
    parser.lexer = l;
    parser.panic_mode = false;
    parser.had_error = false;
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
    
    error_report(parser.current, "Expected '%s' but found '%s'", token_type_to_str(type), found_text);
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
            type == TOKEN_KEYWORD_FLOAT ||
            type == TOKEN_ENUM || type == TOKEN_STRUCT || type == TOKEN_UNION);
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
            error_report(parser.current, "Expected identifier after type keyword.");
            return false;
        }

        char* tn = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        if (out_type) *out_type = is_enum ? TYPE_ENUM : (is_union ? TYPE_UNION : TYPE_STRUCT);
        if (out_type_name) *out_type_name = tn;
        else free(tn);

        return true;
    }

    return false;
}

/**
 * @brief محاولة التعافي من الخطأ بالقفز إلى بداية الجملة التالية.
 */
void synchronize() {
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF) {
        // إذا وجدنا فاصلة منقوطة، فغالباً انتهت الجملة السابقة
        if (parser.current.type == TOKEN_SEMICOLON || parser.current.type == TOKEN_DOT) {
            advance();
            return;
        }

        // إذا وجدنا كلمة مفتاحية تبدأ جملة جديدة
        switch (parser.current.type) {
            case TOKEN_KEYWORD_INT:
            case TOKEN_KEYWORD_I8:
            case TOKEN_KEYWORD_I16:
            case TOKEN_KEYWORD_I32:
            case TOKEN_KEYWORD_I64:
            case TOKEN_KEYWORD_U8:
            case TOKEN_KEYWORD_U16:
            case TOKEN_KEYWORD_U32:
            case TOKEN_KEYWORD_U64:
            case TOKEN_KEYWORD_STRING:
            case TOKEN_KEYWORD_BOOL:
            case TOKEN_KEYWORD_CHAR:
            case TOKEN_KEYWORD_FLOAT:
            case TOKEN_ENUM:
            case TOKEN_STRUCT:
            case TOKEN_UNION:
            case TOKEN_CONST:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_PRINT:
            case TOKEN_READ:
            case TOKEN_RETURN:
            case TOKEN_SWITCH:
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
                return;
            default:
                ;
        }

        advance();
    }
}

// --- تصريحات مسبقة ---
Node* parse_expression();
Node* parse_statement();
Node* parse_block();
Node* parse_primary();

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
            error_report(parser.current, "Invalid float literal.");
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
            error_report(parser.current, "Invalid UTF-8 char literal.");
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
                    if (!arg) break; // Error recovery
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
        // الوصول لمصفوفة: اسم[فهرس]
        else if (parser.current.type == TOKEN_LBRACKET) {
            eat(TOKEN_LBRACKET);
            Node* index = parse_expression();
            eat(TOKEN_RBRACKET);

            node = ast_node_new(NODE_ARRAY_ACCESS, tok_ident);
            if (!node) return NULL;
            node->data.array_op.name = name;
            node->data.array_op.index = index;
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
            error_report(parser.current, "Expected expression.");
            parser.panic_mode = true;
        }
        return NULL;
    }

    // الوصول للأعضاء/القيم المؤهلة: <expr>:<member> (يمكن تكراره للتعشيش)
    while (node && parser.current.type == TOKEN_COLON) {
        Token tok_colon = parser.current;
        eat(TOKEN_COLON);

        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "Expected member name after ':' .");
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

// المستوى 1.5: العمليات الأحادية والبادئة (!, -, ++, --)
Node* parse_unary() {
    if (parser.current.type == TOKEN_MINUS || parser.current.type == TOKEN_NOT ||
        parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {

        Token tok_op = parser.current;
        
        UnaryOpType op;
        if (parser.current.type == TOKEN_MINUS) op = UOP_NEG;
        else if (parser.current.type == TOKEN_NOT) op = UOP_NOT;
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
                        error_report(parser.current, "Division by zero.");
                        result = 0;
                    } else if (sa == INT32_MIN && sb == -1) {
                        // التفاف آمن (بدون UB)
                        result = (int64_t)INT32_MIN;
                    } else {
                        result = (int64_t)(sa / sb);
                    }
                } else if (op == OP_MOD) {
                    if (sb == 0) {
                        error_report(parser.current, "Modulo by zero.");
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
                        error_report(parser.current, "Division by zero.");
                        result = 0;
                    } else if (a == INT64_MIN && b == -1) {
                        result = INT64_MIN;
                    } else {
                        result = a / b;
                    }
                } else if (op == OP_MOD) {
                    if (b == 0) {
                        error_report(parser.current, "Modulo by zero.");
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

// المستوى 4: عمليات المقارنة العلائقية
Node* parse_relational() {
    Node* left = parse_additive();
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

// المستوى 5: عمليات التساوي
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

// المستوى 6: العملية المنطقية "و" (&&)
Node* parse_logical_and() {
    Node* left = parse_equality();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_AND) {
        Token tok_op = parser.current;
        eat(TOKEN_AND);
        Node* right = parse_equality();
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

// المستوى 7: العملية المنطقية "أو" (||)
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
                error_report(parser.current, "Invalid UTF-8 char literal.");
                cp = (uint32_t)'?';
            }
            v->data.char_lit.value = (int)cp;
            eat(TOKEN_CHAR);
            node->data.case_stmt.value = v;
        }
        else {
            error_report(parser.current, "Expected integer/char literal in 'case'.");
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
            synchronize(); // Panic recovery inside switch
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
            error_report(parser.current, "Expected 'case' or 'default' inside switch.");
            synchronize();
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
            synchronize();
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
 * @brief تحليل قائمة تهيئة مصفوفة بالشكل: = { expr, expr, ... }
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
    eat(TOKEN_LBRACE);

    Node* head = NULL;
    Node* tail = NULL;
    int count = 0;

    if (parser.current.type != TOKEN_RBRACE) {
        while (1) {
            Node* expr = parse_expression();
            if (!expr) break;

            expr->next = NULL;
            if (!head) { head = expr; tail = expr; }
            else { tail->next = expr; tail = expr; }
            count++;

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

    if (out_count) *out_count = count;
    return head;
}

Node* parse_statement() {
    if (parser.current.type == TOKEN_LBRACE) return parse_block();

    if (parser.current.type == TOKEN_SWITCH) {
        return parse_switch();
    }

    if (parser.current.type == TOKEN_RETURN) {
        Token tok = parser.current;
        eat(TOKEN_RETURN);
        Node* stmt = ast_node_new(NODE_RETURN, tok);
        if (!stmt) return NULL;
        stmt->data.return_stmt.expression = parse_expression();
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
            error_report(parser.current, "Expected variable name after 'اقرأ'.");
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
        if (is_type_keyword(parser.current.type)) {
            Token tok_type = parser.current;
            DataType dt = TYPE_INT;
            char* type_name = NULL;
            (void)tok_type;

            if (!parse_type_spec(&dt, &type_name)) {
                error_report(parser.current, "Expected type for for-loop init.");
                synchronize();
                return NULL;
            }

            if (dt == TYPE_STRUCT) {
                error_report(parser.current, "تعريف الهيكل داخل تهيئة for غير مدعوم حالياً.");
                free(type_name);
            }

            Token tok_name = parser.current;
            if (parser.current.type != TOKEN_IDENTIFIER) {
                error_report(parser.current, "Expected identifier in for-loop init.");
                free(type_name);
                synchronize();
                return NULL;
            }
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);

            if (parser.current.type != TOKEN_ASSIGN) {
                error_report(parser.current, "Expected '=' in for-loop init.");
            }
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();
            eat(TOKEN_SEMICOLON);

            init = ast_node_new(NODE_VAR_DECL, tok_name);
            if (!init) { free(type_name); return NULL; }
            init->data.var_decl.name = name;
            init->data.var_decl.type = dt;
            init->data.var_decl.type_name = type_name;
            init->data.var_decl.expression = expr;
            init->data.var_decl.is_global = false;
            init->data.var_decl.is_const = false;
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

    // التحقق من وجود كلمة ثابت (const)
    bool is_const = false;
    Token tok_const = {0};
    if (parser.current.type == TOKEN_CONST) {
        is_const = true;
        tok_const = parser.current;
        eat(TOKEN_CONST);
    }

    if (is_type_keyword(parser.current.type)) {
        Token tok_type = parser.current;
        DataType dt = TYPE_INT;
        char* type_name = NULL;
        (void)tok_type;

        if (!parse_type_spec(&dt, &type_name)) {
            error_report(parser.current, "Expected type.");
            synchronize();
            return NULL;
        }

        Token tok_name = parser.current;
        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "Expected identifier after type.");
            free(type_name);
            synchronize();
            return NULL;
        }
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        if (parser.current.type == TOKEN_LBRACKET) {
            if (dt != TYPE_INT) {
                error_report(parser.current, "Only int arrays are supported.");
                exit(1);
            }
            eat(TOKEN_LBRACKET);
            int size = 0;
            if (parser.current.type == TOKEN_INT) {
                int64_t v = 0;
                if (parse_int_token_checked(parser.current, &v) && v >= 0) {
                    if (v > INT_MAX) {
                        error_report(parser.current, "حجم المصفوفة كبير جداً.");
                    } else {
                        size = (int)v;
                    }
                } else if (v < 0) {
                    error_report(parser.current, "حجم المصفوفة يجب أن يكون عدداً غير سالب.");
                }
                eat(TOKEN_INT);
            } else {
                error_report(parser.current, "Array size must be integer.");
            }
            eat(TOKEN_RBRACKET);

            int init_count = 0;
            bool has_init = false;
            Node* init_vals = parse_array_initializer_list(&init_count, &has_init);

            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_ARRAY_DECL, tok_name);
            if (!stmt) return NULL;
            stmt->data.array_decl.name = name;
            stmt->data.array_decl.size = size;
            stmt->data.array_decl.is_global = false;
            stmt->data.array_decl.is_const = is_const;
            stmt->data.array_decl.has_init = has_init;
            stmt->data.array_decl.init_values = init_vals;
            stmt->data.array_decl.init_count = init_count;
            return stmt;
        }

        // تعريف هيكل (محلي): هيكل <T> <name>.
        if (dt == TYPE_STRUCT || dt == TYPE_UNION) {
            if (is_const) {
                error_report(tok_const, "لا يمكن تعريف نوع مركب ثابت بدون تهيئة (غير مدعوم حالياً).");
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
            return stmt;
        }

        if (is_const && parser.current.type != TOKEN_ASSIGN) {
            error_report(parser.current, "Constant must be initialized.");
        }

        eat(TOKEN_ASSIGN);
        Node* expr = parse_expression();
        eat(TOKEN_DOT);

        Node* stmt = ast_node_new(NODE_VAR_DECL, tok_name);
        if (!stmt) { free(type_name); return NULL; }
        stmt->data.var_decl.name = name;
        stmt->data.var_decl.type = dt;
        stmt->data.var_decl.type_name = type_name;
        stmt->data.var_decl.expression = expr;
        stmt->data.var_decl.is_global = false;
        stmt->data.var_decl.is_const = is_const;
        (void)tok_const;
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
            eat(TOKEN_LBRACKET);
            Node* index = parse_expression();
            eat(TOKEN_RBRACKET);

            if (parser.current.type == TOKEN_ASSIGN) {
                eat(TOKEN_ASSIGN);
                Node* value = parse_expression();
                eat(TOKEN_DOT);

                Node* stmt = ast_node_new(NODE_ARRAY_ASSIGN, tok_name);
                if (!stmt) return NULL;
                stmt->data.array_op.name = name;
                stmt->data.array_op.index = index;
                stmt->data.array_op.value = value;
                return stmt;
            } else if (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {
                // array[index]++.
                Node* access = ast_node_new(NODE_ARRAY_ACCESS, tok_name);
                if (!access) return NULL;
                access->data.array_op.name = name;
                access->data.array_op.index = index;
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

    error_report(parser.current, "Unexpected token or statement.");
    return NULL;
}

Node* parse_declaration() {
    // التحقق من وجود كلمة ثابت (const) للتصريحات العامة
    bool is_const = false;
    if (parser.current.type == TOKEN_CONST) {
        is_const = true;
        eat(TOKEN_CONST);
    }

    if (is_type_keyword(parser.current.type)) {
        Token tok_type = parser.current;
        DataType dt = TYPE_INT;
        char* type_name = NULL;
        (void)tok_type;

        if (!parse_type_spec(&dt, &type_name)) {
            error_report(parser.current, "Expected type.");
            synchronize();
            return NULL;
        }

        // تعريف تعداد/هيكل/اتحاد: تعداد <name> { ... }  |  هيكل <name> { ... } | اتحاد <name> { ... }
        if ((dt == TYPE_ENUM || dt == TYPE_STRUCT || dt == TYPE_UNION) && parser.current.type == TOKEN_LBRACE) {
            if (is_const) {
                error_report(parser.current, "Type declarations cannot be const.");
            }

            Token tok_name = tok_type;
            if (dt == TYPE_ENUM) {
                eat(TOKEN_LBRACE);

                Node* head = NULL;
                Node* tail = NULL;

                if (parser.current.type != TOKEN_RBRACE) {
                    while (1) {
                        if (parser.current.type != TOKEN_IDENTIFIER) {
                            error_report(parser.current, "Expected enum member name.");
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
                            if (parser.current.type == TOKEN_RBRACE) break; // trailing comma
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
                    error_report(parser.current, "Expected field type inside struct.");
                    free(ftype_name);
                    synchronize();
                    break;
                }

                if (parser.current.type != TOKEN_IDENTIFIER) {
                    error_report(parser.current, "Expected field name inside struct.");
                    free(ftype_name);
                    synchronize();
                    break;
                }

                Token tok_field_name = parser.current;
                char* fname = strdup(parser.current.value);
                eat(TOKEN_IDENTIFIER);

                if (parser.current.type == TOKEN_LBRACKET) {
                    error_report(parser.current, "Array fields inside structs are not supported yet.");
                    free(ftype_name);
                    synchronize();
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
            error_report(parser.current, "Expected identifier after type.");
            free(type_name);
            synchronize();
            return NULL;
        }

        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        // دالة: نسمح فقط بأنواع بدائية حالياً
        if (parser.current.type == TOKEN_LPAREN) {
                if (dt == TYPE_ENUM || dt == TYPE_STRUCT || dt == TYPE_UNION) {
                    error_report(parser.current, "User-defined return types are not supported in function signatures yet.");
                    free(type_name);
                    free(name);
                    synchronize();
                    return NULL;
                }
            if (is_const) {
                error_report(parser.current, "Functions cannot be declared as const.");
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
                        error_report(parser.current, "Expected type for parameter.");
                        synchronize();
                        return NULL;
                    }
                    if (param_dt == TYPE_ENUM || param_dt == TYPE_STRUCT || param_dt == TYPE_UNION) {
                        error_report(tok_param_type, "User-defined parameter types are not supported in function signatures yet.");
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

        // تعريف مصفوفة عامة (صحيح فقط)
        if (parser.current.type == TOKEN_LBRACKET) {
            if (dt != TYPE_INT) {
                error_report(parser.current, "Only int arrays are supported.");
                free(type_name);
                free(name);
                exit(1);
            }

            eat(TOKEN_LBRACKET);
            int size = 0;
            if (parser.current.type == TOKEN_INT) {
                int64_t v = 0;
                if (parse_int_token_checked(parser.current, &v) && v >= 0) {
                    if (v > INT_MAX) {
                        error_report(parser.current, "حجم المصفوفة كبير جداً.");
                    } else {
                        size = (int)v;
                    }
                } else if (v < 0) {
                    error_report(parser.current, "حجم المصفوفة يجب أن يكون عدداً غير سالب.");
                }
                eat(TOKEN_INT);
            } else {
                error_report(parser.current, "Array size must be integer.");
            }
            eat(TOKEN_RBRACKET);

            int init_count = 0;
            bool has_init = false;
            Node* init_vals = parse_array_initializer_list(&init_count, &has_init);

            if (is_const && !has_init) {
                error_report(parser.current, "Constant must be initialized.");
            }

            eat(TOKEN_DOT);

            Node* arr = ast_node_new(NODE_ARRAY_DECL, tok_name);
            if (!arr) return NULL;
            arr->data.array_decl.name = name;
            arr->data.array_decl.size = size;
            arr->data.array_decl.is_global = true;
            arr->data.array_decl.is_const = is_const;
            arr->data.array_decl.has_init = has_init;
            arr->data.array_decl.init_values = init_vals;
            arr->data.array_decl.init_count = init_count;
            free(type_name);
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
            return var;
        }

        // ثوابت عامة يجب أن تُهيّأ
        if (is_const && parser.current.type != TOKEN_ASSIGN) {
            error_report(parser.current, "Constant must be initialized.");
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
        return var;
    }
    
    error_report(parser.current, "Expected declaration (function or global variable).");
    synchronize();
    return NULL;
}

Node* parse(Lexer* l) {
    init_parser(l);
    
    // تهيئة نظام الأخطاء بمؤشر المصدر للطباعة
    error_init(l->state.source);

    Node* head = NULL;
    Node* tail = NULL;
    while (parser.current.type != TOKEN_EOF) {
        Node* decl = parse_declaration();
        if (decl) {
            if (head == NULL) { head = decl; tail = decl; }
            else { tail->next = decl; tail = decl; }
        }
        // If decl is NULL, synchronize() was likely called inside parse_declaration
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
