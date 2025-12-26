/**
 * @file baa.h
 * @brief ملف الرأس الرئيسي الذي يعرف هياكل البيانات لمحوسب لغة "باء" (Baa Compiler).
 * @version 0.1.2 (Recursion Support - String Variables)
 */

#ifndef BAA_H
#define BAA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// تعريفات المحلل اللفظي (Lexer)
// ============================================================================

/**
 * @enum TokenType
 * @brief تصنيف الوحدات الذرية (Atomic Units) للكود المصدري.
 */
typedef enum {
    TOKEN_EOF,          // نهاية الملف
    TOKEN_INT,          // رقم صحيح: 123
    TOKEN_STRING,       // نص: "مرحباً"
    TOKEN_CHAR,         // حرف: 'أ'
    TOKEN_IDENTIFIER,   // معرف: اسم متغير أو دالة (س، ص، الرئيسية)
    
    // الكلمات المفتاحية (Keywords)
    TOKEN_KEYWORD_INT,  // صحيح
    TOKEN_KEYWORD_STRING, // نص (جديد)
    TOKEN_RETURN,       // إرجع
    TOKEN_PRINT,        // اطبع
    TOKEN_IF,           // إذا
    TOKEN_WHILE,        // طالما
    TOKEN_FOR,          // لكل
    
    // الرموز (Symbols)
    TOKEN_ASSIGN,       // =
    TOKEN_DOT,          // .
    TOKEN_COMMA,        // ,
    TOKEN_SEMICOLON,    // ؛
    
    // العمليات الحسابية (Math)
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_INC,          // ++
    TOKEN_DEC,          // --
    
    // العمليات المنطقية والعلائقية (Logic / Relational)
    TOKEN_EQ,           // ==
    TOKEN_NEQ,          // !=
    TOKEN_LT,           // <
    TOKEN_GT,           // >
    TOKEN_LTE,          // <=
    TOKEN_GTE,          // >=
    TOKEN_AND,          // &&
    TOKEN_OR,           // ||
    TOKEN_NOT,          // !
    
    // التجميع (Grouping)
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_LBRACKET,     // [
    TOKEN_RBRACKET,     // ]
    
    TOKEN_INVALID       // وحدة غير صالحة
} TokenType;

/**
 * @struct Token
 * @brief يمثل وحدة لفظية واحدة مستخرجة من المصدر.
 */
typedef struct {
    TokenType type;     // نوع الوحدة
    char* value;        // القيمة النصية (للأرقام والنصوص والمعرفات)
    int line;           // رقم السطر في الكود المصدري
} Token;

/**
 * @struct Lexer
 * @brief يحافظ على حالة عملية مسح النص وتفكيكه.
 */
typedef struct {
    const char* src;    // نص المصدر
    size_t pos;         // الموقع الحالي للمسح
    size_t len;         // طول نص المصدر
    int line;           // السطر الحالي
} Lexer;

/**
 * @brief تهيئة المحلل اللفظي بنص المصدر.
 */
void lexer_init(Lexer* lexer, const char* src);

/**
 * @brief استخراج الوحدة اللفظية (Token) التالية من المصدر.
 */
Token lexer_next_token(Lexer* lexer);

// ============================================================================
// تعريفات المحلل القواعدي وشجرة الإعراب (Parser & AST)
// ============================================================================

/**
 * @enum NodeType
 * @brief مميز لنوع العقدة في شجرة الإعراب المجردة (AST).
 */
typedef enum {
    // المستوى الأعلى
    NODE_PROGRAM,       // البرنامج: يحتوي على قائمة التصريحات
    NODE_FUNC_DEF,      // تعريف دالة
    NODE_VAR_DECL,      // تعريف متغير (عام أو محلي)
    
    // الجمل البرمجية (Statements)
    NODE_BLOCK,         // نطاق { ... } وكتلة الدالة
    NODE_RETURN,        // جملة الإرجاع (إرجع)
    NODE_PRINT,         // جملة الطباعة (اطبع)
    NODE_IF,            // جملة الشرط (إذا)
    NODE_WHILE,         // جملة التكرار (طالما)
    NODE_FOR,           // جملة التكرار المحدد (لكل)
    NODE_ASSIGN,        // جملة التعيين (س = 5)
    NODE_CALL_STMT,     // استدعاء دالة كجملة
    
    // المصفوفات
    NODE_ARRAY_DECL,    // تعريف مصفوفة: صحيح س[5].
    NODE_ARRAY_ASSIGN,  // تعيين عنصر مصفوفة: س[0] = 1.
    NODE_ARRAY_ACCESS,  // قراءة عنصر مصفوفة: س[0]
    
    // التعبيرات (Expressions)
    NODE_BIN_OP,        // العمليات الثنائية (+، -، *، /)
    NODE_UNARY_OP,      // العمليات الأحادية (!، -)
    NODE_POSTFIX_OP,    // العمليات اللاحقة (++، --)
    NODE_INT,           // قيمة عددية صحيحة
    NODE_STRING,        // قيمة نصية
    NODE_CHAR,          // قيمة حرفية
    NODE_VAR_REF,       // إشارة لمتغير
    NODE_CALL_EXPR      // تعبير استدعاء دالة
} NodeType;

/**
 * @enum DataType
 * @brief أنواع البيانات المدعومة في اللغة.
 */
typedef enum {
    TYPE_INT,           // صحيح (int64)
    TYPE_STRING         // نص (char*)
} DataType;

/**
 * @enum OpType
 * @brief أنواع العمليات الثنائية المدعومة.
 */
typedef enum { 
    // عمليات حسابية
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    // عمليات مقارنة
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LTE, OP_GTE,
    // عمليات منطقية
    OP_AND, OP_OR
} OpType;

/**
 * @enum UnaryOpType
 * @brief أنواع العمليات الأحادية المدعومة.
 */
typedef enum {
    UOP_NEG, // السالب (-)
    UOP_NOT, // النفي (!)
    UOP_INC, // الزيادة (++)
    UOP_DEC  // النقصان (--)
} UnaryOpType;

struct Node;

/**
 * @struct Node
 * @brief اللبنة الأساسية لشجرة الإعراب المجردة.
 *        تستخدم اتحاداً مسمى (Tagged Union) لتخزين البيانات الخاصة بنوع العقدة.
 */
typedef struct Node {
    NodeType type;      // نوع العقدة
    struct Node* next;  // مؤشر للعقدة التالية (في القوائم المرتبطة للجمل/التصريحات)

    union {
        // البرنامج: قائمة الدوال والمتغيرات العامة
        struct { struct Node* declarations; } program;
        
        // الكتلة: قائمة من الجمل البرمجية
        struct { struct Node* statements; } block;
        
        // تعريف دالة
        struct { 
            char* name;          // اسم الدالة
            DataType return_type; // نوع الإرجاع (جديد)
            struct Node* params; // قائمة المعاملات (متغيرات)
            struct Node* body;   // جسم الدالة (كتلة)
        } func_def;

        // تعريف متغير (عادي)
        struct { 
            char* name;              // اسم المتغير
            DataType type;           // نوع البيانات (جديد: صحيح أو نص)
            struct Node* expression; // القيمة الابتدائية (اختياري)
            bool is_global;          // هل هو متغير عام؟
        } var_decl;

        // تعريف مصفوفة
        struct {
            char* name;     // اسم المصفوفة
            int size;       // حجم المصفوفة (ثابت حالياً)
            bool is_global; // هل هي عامة؟ (المحلي فقط مدعوم حالياً)
        } array_decl;

        // عمليات المصفوفات - للتعيين والقراءة
        struct {
            char* name;          // اسم المصفوفة
            struct Node* index;  // الفهرس (Index)
            struct Node* value;  // القيمة (للتعيين فقط، NULL للقراءة)
        } array_op;

        // استدعاء دالة
        struct {
            char* name;         // اسم الدالة المستدعاة
            struct Node* args;  // قائمة الوسائط (تعبيرات)
        } call;

        // جمل التحكم
        struct { struct Node* condition; struct Node* then_branch; } if_stmt;
        struct { struct Node* condition; struct Node* body; } while_stmt;
        struct { struct Node* expression; } return_stmt;
        struct { struct Node* expression; } print_stmt;
        struct { char* name; struct Node* expression; } assign_stmt;

        // جملة التكرار المحددة (لكل)
        struct {
            struct Node* init;      // التهيئة (مثل: صحيح س = 0)
            struct Node* condition; // الشرط (مثل: س < 10)
            struct Node* increment; // الزيادة (مثل: س = س + 1 أو س++)
            struct Node* body;      // جسم الحلقة
        } for_stmt;

        // التعبيرات الأساسية والعمليات
        struct { int value; } integer;
        struct { char* value; int id; } string_lit;
        struct { int value; } char_lit; 

        struct { char* name; } var_ref;
        struct { struct Node* left; struct Node* right; OpType op; } bin_op;
        
        // العمليات الأحادية واللاحقة
        struct { 
            struct Node* operand; // المعامل (مثل اسم المتغير)
            UnaryOpType op;       // نوع العملية (++، --، !، -)
        } unary_op; 
    } data;
} Node;

/**
 * @struct Parser
 * @brief يمثل حالة عملية التحليل القواعدي مع تخزين مؤقت للوحدة الحالية والتالية (Lookahead).
 */
typedef struct {
    Lexer* lexer;
    Token current;
    Token next;     
} Parser;

/**
 * @brief بدء عملية التحليل القواعدي وبناء الشجرة (AST).
 */
Node* parse(Lexer* lexer);

// ============================================================================
// تعريفات مولد الكود (Codegen)
// ============================================================================

/**
 * @brief توليد كود التجميع (Assembly) للتركيب المعماري x86_64 من الشجرة.
 */
void codegen(Node* node, FILE* file);

#endif // BAA_H