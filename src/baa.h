/**
 * @file baa.h
 * @brief ملف الرأس الرئيسي الذي يعرف هياكل البيانات لمحوسب لغة "باء" (Baa Compiler).
 * @version 0.3.3
 */

#ifndef BAA_H
#define BAA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

// معلومات الإصدار
#define BAA_VERSION "0.3.4.5"
#define BAA_BUILD_DATE __DATE__

// ============================================================================
// تعريفات المحلل اللفظي (Lexer)
// ============================================================================

/**
 * @enum BaaTokenType
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
    TOKEN_KEYWORD_STRING, // نص
    TOKEN_KEYWORD_BOOL, // منطقي
    TOKEN_CONST,        // ثابت
    TOKEN_RETURN,       // إرجع
    TOKEN_PRINT,        // اطبع
    TOKEN_READ,         // اقرأ
    TOKEN_IF,           // إذا
    TOKEN_ELSE,         // وإلا
    TOKEN_WHILE,        // طالما
    TOKEN_FOR,          // لكل
    TOKEN_BREAK,        // توقف
    TOKEN_CONTINUE,     // استمر
    TOKEN_SWITCH,       // اختر
    TOKEN_CASE,         // حالة
    TOKEN_DEFAULT,      // افتراضي
    TOKEN_TRUE,         // صواب
    TOKEN_FALSE,        // خطأ

    // أنواع مركبة (v0.3.4)
    TOKEN_ENUM,         // تعداد
    TOKEN_STRUCT,       // هيكل
    TOKEN_UNION,        // اتحاد
    
    // الرموز (Symbols)
    TOKEN_ASSIGN,       // =
    TOKEN_DOT,          // .
    TOKEN_COMMA,        // ,
    TOKEN_COLON,        // :
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
} BaaTokenType;

/**
 * @struct Token
 * @brief يمثل وحدة لفظية واحدة مستخرجة من المصدر.
 */
typedef struct {
    BaaTokenType type;     // نوع الوحدة
    char* value;        // القيمة النصية
    int line;           // رقم السطر
    int col;            // رقم العمود
    const char* filename; // اسم الملف
} Token;

/**
 * @struct Macro
 * @brief تمثيل لتعريف ماكرو (Preprocessor Macro).
 */
typedef struct {
    char* name;  // اسم الماكرو
    char* value; // القيمة الاستبدالية
} Macro;

/**
 * @struct LexerState
 * @brief يحافظ على حالة المحلل اللفظي لملف واحد (لدعم التضمين المتداخل).
 */
typedef struct {
    char* source;
    char* cur_char;
    const char* filename; // اسم الملف الحالي
    int line;
    int col;
} LexerState;

/**
 * @struct Lexer
 * @brief المحلل اللفظي والمعالج القبلي (Lexer & Preprocessor).
 */
typedef struct {
    // الحالة الحالية
    LexerState state;

    // مكدس التضمين (Include Stack)
    LexerState stack[10]; // أقصى عمق للتضمين: 10
    int stack_depth;

    // حالة المعالج القبلي (Preprocessor State)
    Macro macros[100];    // جدول الماكروهات (حد أقصى 100)
    int macro_count;      // عدد الماكروهات المعرفة
    bool skipping;        // هل نحن في وضع التخطي؟ (مُشتق من مكدس الشروط)

    // مكدس الشروط (#إذا_عرف/#وإلا/#نهاية) لدعم التعشيش بشكل صحيح
    struct {
        unsigned char parent_active;
        unsigned char cond_true;
        unsigned char in_else;
    } if_stack[32];
    int if_depth;
} Lexer;

/**
 * @brief تهيئة المحلل اللفظي بنص المصدر وتخطي الـ BOM إذا وجد.
 */
void lexer_init(Lexer* lexer, char* src, const char* filename);

// قراءة ملف بالكامل إلى ذاكرة
char* read_file(const char* path);

/**
 * @brief استخراج الوحدة اللفظية (Token) التالية من المصدر.
 */
Token lexer_next_token(Lexer* lexer);

/**
 * @brief تحويل نوع الوحدة إلى نص مقروء.
 */
const char* token_type_to_str(BaaTokenType type);

// ============================================================================
// نظام التشخيص (Diagnostic Engine) - الأخطاء والتحذيرات
// ============================================================================

/**
 * @enum WarningType
 * @brief أنواع التحذيرات المدعومة.
 */
typedef enum {
    WARN_UNUSED_VARIABLE,    // متغير معرف لكن غير مستخدم
    WARN_DEAD_CODE,          // كود بعد إرجع أو توقف
    WARN_IMPLICIT_RETURN,    // دالة بدون إرجع صريح
    WARN_SHADOW_VARIABLE,    // متغير محلي يحجب متغير عام
    WARN_COUNT               // عدد التحذيرات (للتكرار)
} WarningType;

/**
 * @struct WarningConfig
 * @brief إعدادات التحذيرات.
 */
typedef struct {
    bool enabled[WARN_COUNT];   // تفعيل/تعطيل كل نوع
    bool warnings_as_errors;    // -Werror: معاملة التحذيرات كأخطاء
    bool all_warnings;          // -Wall: تفعيل جميع التحذيرات
    bool colored_output;        // ألوان ANSI في الخرج
} WarningConfig;

/**
 * @brief إعدادات التحذيرات العامة (تُستخدم في جميع الوحدات).
 */
extern WarningConfig g_warning_config;

/**
 * @brief تهيئة نظام الأخطاء بمؤشر للكود المصدري (للطباعة).
 */
void error_init(const char* source);

/**
 * @brief تهيئة إعدادات التحذيرات الافتراضية.
 */
void warning_init(void);

/**
 * @brief الإبلاغ عن خطأ مع تحديد الموقع والرسالة.
 * @param token الوحدة التي حدث عندها الخطأ (للحصول على السطر والعمود)
 * @param message رسالة الخطأ (صيغة printf)
 */
void error_report(Token token, const char* message, ...);

/**
 * @brief الإبلاغ عن تحذير مع تحديد الموقع والرسالة.
 * @param type نوع التحذير
 * @param filename اسم الملف
 * @param line رقم السطر
 * @param col رقم العمود
 * @param message رسالة التحذير (صيغة printf)
 */
void warning_report(WarningType type, const char* filename, int line, int col, const char* message, ...);

/**
 * @brief التحقق مما إذا حدثت أخطاء أثناء الترجمة.
 */
bool error_has_occurred();

/**
 * @brief التحقق مما إذا حدثت تحذيرات أثناء الترجمة.
 */
bool warning_has_occurred();

/**
 * @brief الحصول على عدد التحذيرات.
 */
int warning_get_count();

/**
 * @brief إعادة تعيين حالة الأخطاء (اختياري).
 */
void error_reset();

/**
 * @brief إعادة تعيين حالة التحذيرات.
 */
void warning_reset();

// ============================================================================
// نظام التحديث (Updater)
// ============================================================================

/**
 * @brief التحقق من وجود تحديثات وتنزيلها.
 */
void run_updater(void);

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

    // تعريفات الأنواع المركبة (v0.3.4)
    NODE_ENUM_DECL,     // تعريف تعداد: تعداد لون { ... }
    NODE_STRUCT_DECL,   // تعريف هيكل: هيكل سيارة { ... }
    NODE_ENUM_MEMBER,   // عنصر تعداد داخل تعريفه
    NODE_UNION_DECL,    // تعريف اتحاد: اتحاد قيمة { ... }
    
    // الجمل البرمجية (Statements)
    NODE_BLOCK,         // نطاق { ... } وكتلة الدالة
    NODE_RETURN,        // جملة الإرجاع (إرجع)
    NODE_PRINT,         // جملة الطباعة (اطبع)
    NODE_IF,            // جملة الشرط (إذا)
    NODE_WHILE,         // جملة التكرار (طالما)
    NODE_FOR,           // جملة التكرار المحدد (لكل)
    NODE_SWITCH,        // جملة الاختيار (اختر)
    NODE_CASE,          // حالة الاختيار (حالة/افتراضي)
    NODE_BREAK,         // جملة التوقف (توقف)
    NODE_CONTINUE,      // جملة الاستمرار (استمر)
    NODE_ASSIGN,        // جملة التعيين (س = 5)
    NODE_CALL_STMT,     // استدعاء دالة كجملة
    NODE_READ,          // جملة الإدخال (اقرأ)
    
    // المصفوفات
    NODE_ARRAY_DECL,    // تعريف مصفوفة: صحيح س[5].
    NODE_ARRAY_ASSIGN,  // تعيين عنصر مصفوفة: س[0] = 1.
    NODE_ARRAY_ACCESS,  // قراءة عنصر مصفوفة: س[0]

    // الوصول للأعضاء/المؤهلات (v0.3.4)
    NODE_MEMBER_ACCESS, // <expr>:<member> (عضو هيكل أو قيمة تعداد)
    NODE_MEMBER_ASSIGN, // <member_access> = <expr>.
    
    // التعبيرات (Expressions)
    NODE_BIN_OP,        // العمليات الثنائية (+، -، *، /)
    NODE_UNARY_OP,      // العمليات الأحادية (!، -)
    NODE_POSTFIX_OP,    // العمليات اللاحقة (++، --)
    NODE_INT,           // قيمة عددية صحيحة
    NODE_STRING,        // قيمة نصية
    NODE_CHAR,          // قيمة حرفية
    NODE_BOOL,          // قيمة منطقية (صواب/خطأ)
    NODE_VAR_REF,       // إشارة لمتغير
    NODE_CALL_EXPR      // تعبير استدعاء دالة
} NodeType;

/**
 * @enum DataType
 * @brief أنواع البيانات المدعومة في اللغة.
 */
typedef enum {
    TYPE_INT,           // صحيح (int64)
    TYPE_STRING,        // نص (char*)
    TYPE_BOOL,          // منطقي (bool - stored as byte)
    TYPE_ENUM,          // تعداد (يُخزن كـ int64)
    TYPE_STRUCT,        // هيكل (ليس قيمة من الدرجة الأولى)
    TYPE_UNION          // اتحاد (ليس قيمة من الدرجة الأولى)
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

    // معلومات الموقع في المصدر (للديبغ)
    const char* filename;
    int line;
    int col;

    union {
        // البرنامج: قائمة الدوال والمتغيرات العامة
        struct { struct Node* declarations; } program;
        
        // الكتلة: قائمة من الجمل البرمجية
        struct { struct Node* statements; } block;
        
        // تعريف دالة
        struct { 
            char* name;          // اسم الدالة
            DataType return_type; // نوع الإرجاع
            struct Node* params; // قائمة المعاملات (متغيرات)
            struct Node* body;   // جسم الدالة (كتلة) - NULL if prototype
            bool is_prototype;   // هل هو نموذج أولي؟ (بدون جسم)
        } func_def;

        // تعريف متغير (عادي)
        struct {
            char* name;              // اسم المتغير
            DataType type;           // نوع البيانات (صحيح أو نص)
            char* type_name;          // اسم النوع عند TYPE_ENUM/TYPE_STRUCT
            int struct_size;          // حجم الهيكل بالبايت عند TYPE_STRUCT (يُملأ دلالياً)
            int struct_align;         // محاذاة الهيكل عند TYPE_STRUCT (يُملأ دلالياً)
            struct Node* expression; // القيمة الابتدائية (اختياري)
            bool is_global;          // هل هو متغير عام؟
            bool is_const;           // هل هو ثابت (immutable)؟
        } var_decl;

        // تعريف تعداد
        struct {
            char* name;              // اسم التعداد
            struct Node* members;    // قائمة NODE_ENUM_MEMBER
        } enum_decl;

        // عنصر تعداد
        struct {
            char* name;              // اسم العضو
            int64_t value;           // القيمة (يُملأ في التحليل الدلالي)
            bool has_value;          // هل تم ضبط value؟
        } enum_member;

        // تعريف هيكل
        struct {
            char* name;              // اسم الهيكل
            struct Node* fields;     // قائمة حقول (NODE_VAR_DECL) بدون تهيئة
        } struct_decl;

        // تعريف اتحاد
        struct {
            char* name;              // اسم الاتحاد
            struct Node* fields;     // قائمة حقول (NODE_VAR_DECL) بدون تهيئة
        } union_decl;

        // الوصول إلى عضو هيكل أو قيمة تعداد
        struct {
            struct Node* base;       // التعبير الأساسي
            char* member;            // اسم العضو

            // نتائج التحليل الدلالي:
            bool is_enum_value;      // هل هذا تعبير قيمة تعداد؟ (Enum:Member)
            char* enum_name;         // اسم التعداد عند is_enum_value
            int64_t enum_value;      // قيمة العضو

            bool is_struct_member;   // هل هذا عضو هيكل؟
            char* root_var;          // اسم المتغير الجذري للهيكل
            bool root_is_global;     // هل الجذر عام؟
            char* root_struct;       // اسم نوع الهيكل للجذر
            int member_offset;       // الإزاحة بالبايت من بداية الهيكل
            DataType member_type;    // نوع الحقل النهائي
            char* member_type_name;  // اسم النوع عند TYPE_ENUM/TYPE_STRUCT
            bool member_is_const;    // هل المسار/الحقل النهائي ثابت (لا يمكن الكتابة)؟
        } member_access;

        // إسناد إلى عضو: <base>:<member> = <expr>.
        struct {
            struct Node* target;     // NODE_MEMBER_ACCESS
            struct Node* value;      // RHS expression
        } member_assign;

        // تعريف مصفوفة
        struct {
            char* name;     // اسم المصفوفة
            int size;       // حجم المصفوفة (ثابت حالياً)
            bool is_global; // هل هي عامة؟ (المحلي فقط مدعوم حالياً)
            bool is_const;  // هل هي ثابتة (immutable)؟
            // تهيئة المصفوفة: عند وجود '=' نعتبر التهيئة موجودة حتى لو كانت القائمة فارغة `{}`.
            bool has_init;
            struct Node* init_values; // قائمة قيم التهيئة (قد تكون NULL عند `{}`)
            int init_count;           // عدد قيم التهيئة الفعلية (قد يكون أقل من size)
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
        struct { 
            struct Node* condition; 
            struct Node* then_branch; 
            struct Node* else_branch;
        } if_stmt;
        
        struct { struct Node* condition; struct Node* body; } while_stmt;
        struct { struct Node* expression; } return_stmt;
        struct { struct Node* expression; } print_stmt;
        struct { char* var_name; } read_stmt;        // جملة الإدخال
        struct { char* name; struct Node* expression; } assign_stmt;

        // جملة التكرار المحددة (لكل)
        struct {
            struct Node* init;      // التهيئة
            struct Node* condition; // الشرط
            struct Node* increment; // الزيادة
            struct Node* body;      // جسم الحلقة
        } for_stmt;

        // جملة الاختيار (اختر)
        struct {
            struct Node* expression; // القيمة التي يتم الاختيار بناء عليها
            struct Node* cases;      // قائمة الحالات
        } switch_stmt;

        // حالة الاختيار (حالة / افتراضي)
        struct {
            struct Node* value;      // القيمة الثابتة للحالة
            struct Node* body;       // قائمة الجمل
            bool is_default;         // هل هي الحالة الافتراضية؟
        } case_stmt;

        // التعبيرات الأساسية والعمليات
        struct { int value; } integer;
        struct { char* value; int id; } string_lit;
        struct { int value; } char_lit;
        struct { bool value; } bool_lit;             // قيمة منطقية

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
    bool panic_mode; // وضع الذعر للتعافي من الأخطاء
    bool had_error;  // هل حدث خطأ أثناء التحليل؟
} Parser;

/**
 * @brief بدء عملية التحليل القواعدي وبناء الشجرة (AST).
 */
Node* parse(Lexer* lexer);

// ============================================================================
// التحليل الدلالي وجدول الرموز (Semantic Analysis & Symbol Table)
// ============================================================================

/**
 * @enum ScopeType
 * @brief يحدد نطاق المتغير (عام أو محلي).
 */
typedef enum { 
    SCOPE_GLOBAL, 
    SCOPE_LOCAL 
} ScopeType;

/**
 * @struct Symbol
 * @brief يمثل رمزاً (متغيراً) في جدول الرموز.
 */
typedef struct {
    char name[32];     // اسم الرمز
    ScopeType scope;   // النطاق (عام أو محلي)
    DataType type;     // نوع البيانات (للمتغير: نوعه، للمصفوفة: نوع العنصر)
    char type_name[32]; // اسم النوع عند TYPE_ENUM/TYPE_STRUCT (فارغ لغير ذلك)
    bool is_array;     // هل الرمز مصفوفة؟
    int array_size;    // حجم المصفوفة (ثابت حالياً) إذا كانت is_array=true
    int offset;        // الإزاحة في المكدس أو العنوان
    bool is_const;     // هل هو ثابت (immutable)؟
    bool is_used;      // هل تم استخدام هذا المتغير؟ (للتحذيرات)
    int decl_line;     // سطر التعريف (للتحذيرات)
    int decl_col;      // عمود التعريف (للتحذيرات)
    const char* decl_file; // ملف التعريف (للتحذيرات)
} Symbol;

/**
 * @brief تنفيذ مرحلة التحليل الدلالي للتحقق من الأنواع والرموز.
 * @param program عقدة البرنامج الرئيسية (AST Root)
 * @return true إذا كان البرنامج سليماً، false في حال وجود أخطاء.
 */
bool analyze(Node* program);

#endif // BAA_H
