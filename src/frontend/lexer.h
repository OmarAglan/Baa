/**
 * @file lexer.h
 * @brief ????? ?????? ?????? ???????? ??????.
 */

#ifndef BAA_FRONTEND_LEXER_H
#define BAA_FRONTEND_LEXER_H

#include <stdbool.h>
#include <stddef.h>

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
    TOKEN_FLOAT,        // رقم عشري: 3.14
    TOKEN_STRING,       // نص: "مرحباً"
    TOKEN_CHAR,         // حرف: 'أ'
    TOKEN_IDENTIFIER,   // معرف: اسم متغير أو دالة (س، ص، الرئيسية)
    
    // الكلمات المفتاحية (Keywords)
    TOKEN_KEYWORD_INT,  // صحيح
    TOKEN_KEYWORD_I8,   // ص٨
    TOKEN_KEYWORD_I16,  // ص١٦
    TOKEN_KEYWORD_I32,  // ص٣٢
    TOKEN_KEYWORD_I64,  // ص٦٤
    TOKEN_KEYWORD_U8,   // ط٨
    TOKEN_KEYWORD_U16,  // ط١٦
    TOKEN_KEYWORD_U32,  // ط٣٢
    TOKEN_KEYWORD_U64,  // ط٦٤
    TOKEN_KEYWORD_STRING, // نص
    TOKEN_KEYWORD_BOOL, // منطقي
    TOKEN_KEYWORD_CHAR, // حرف
    TOKEN_KEYWORD_FLOAT, // عشري
    TOKEN_KEYWORD_FLOAT32, // عشري٣٢
    TOKEN_KEYWORD_VOID, // عدم
    TOKEN_CAST,         // كـ
    TOKEN_SIZEOF,       // حجم
    TOKEN_TYPE_ALIAS,   // نوع
    TOKEN_CONST,        // ثابت
    TOKEN_STATIC,       // ساكن
    TOKEN_RETURN,       // إرجع
    TOKEN_PRINT,        // اطبع
    TOKEN_READ,         // اقرأ
    TOKEN_ASM,          // مجمع
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
    TOKEN_ELLIPSIS,     // ...
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
    TOKEN_AMP,          // &
    TOKEN_PIPE,         // |
    TOKEN_CARET,        // ^
    TOKEN_TILDE,        // ~
    TOKEN_SHL,          // <<
    TOKEN_SHR,          // >>
    
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
    int length;         // طول الوحدة بالبايتات داخل السطر
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

    // مسارات تضمين إضافية من سطر الأوامر (-I)
    const char* const* include_dirs;
    size_t include_dir_count;

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

    // تبعيات البناء المكتشفة أثناء المعالجة القبلية (مسارات مطبعة ومملوكة)
    char** dependency_paths;
    size_t dependency_count;
    size_t dependency_capacity;
} Lexer;

/**
 * @brief تهيئة المحلل اللفظي بنص المصدر وتخطي الـ BOM إذا وجد.
 */
void lexer_init(Lexer* lexer,
                char* src,
                const char* filename,
                const char* const* include_dirs,
                size_t include_dir_count);

// قراءة ملف بالكامل إلى ذاكرة

/**
 * @brief استخراج الوحدة اللفظية (Token) التالية من المصدر.
 */
Token lexer_next_token(Lexer* lexer);

/**
 * @brief تحويل نوع الوحدة إلى نص مقروء.
 */
const char* token_type_to_str(BaaTokenType type);

/**
 * @brief إرجاع قائمة تبعيات البناء المكتشفة للمصدر الحالي.
 */
const char* const* lexer_get_dependencies(const Lexer* lexer, size_t* out_count);

/**
 * @brief تحرير قائمة تبعيات البناء المملوكة للمحلل اللفظي.
 */
void lexer_free_dependencies(Lexer* lexer);

#endif
