/**
 * @file language_profile.c
 * @brief مفردات باء الثابتة وقوالبها الأساسية في مصدر واحد.
 */

#include "language_profile.h"

#include <string.h>

static const BaaLanguageKeyword g_keywords[] = {
    {"إرجع", TOKEN_RETURN, "keyword", "إرجاع قيمة من الدالة", true},
    {"اطبع", TOKEN_PRINT, "keyword", "طباعة قيمة", true},
    {"اقرأ", TOKEN_READ, "keyword", "قراءة قيمة", true},
    {"مجمع", TOKEN_ASM, "keyword", "كتلة تعليمات منخفضة المستوى", true},
    {"صحيح", TOKEN_KEYWORD_INT, "type", "نوع عدد صحيح", true},
    {"ص٨", TOKEN_KEYWORD_I8, "type", "نوع صحيح موقّع بعرض ٨ بت", true},
    {"ص١٦", TOKEN_KEYWORD_I16, "type", "نوع صحيح موقّع بعرض ١٦ بت", true},
    {"ص٣٢", TOKEN_KEYWORD_I32, "type", "نوع صحيح موقّع بعرض ٣٢ بت", true},
    {"ص٦٤", TOKEN_KEYWORD_I64, "type", "نوع صحيح موقّع بعرض ٦٤ بت", true},
    {"ط٨", TOKEN_KEYWORD_U8, "type", "نوع صحيح غير موقّع بعرض ٨ بت", true},
    {"ط١٦", TOKEN_KEYWORD_U16, "type", "نوع صحيح غير موقّع بعرض ١٦ بت", true},
    {"ط٣٢", TOKEN_KEYWORD_U32, "type", "نوع صحيح غير موقّع بعرض ٣٢ بت", true},
    {"ط٦٤", TOKEN_KEYWORD_U64, "type", "نوع صحيح غير موقّع بعرض ٦٤ بت", true},
    {"نص", TOKEN_KEYWORD_STRING, "type", "نوع نص", true},
    {"منطقي", TOKEN_KEYWORD_BOOL, "type", "نوع منطقي", true},
    {"حرف", TOKEN_KEYWORD_CHAR, "type", "نوع حرف", true},
    {"عشري", TOKEN_KEYWORD_FLOAT, "type", "نوع عدد عشري", true},
    {"عشري٣٢", TOKEN_KEYWORD_FLOAT32, "type", "نوع عدد عشري بعرض ٣٢ بت", true},
    {"عدم", TOKEN_KEYWORD_VOID, "type", "نوع بلا قيمة", true},
    {"كـ", TOKEN_CAST, "keyword", "تحويل صريح بين الأنواع", true},
    {"حجم", TOKEN_SIZEOF, "keyword", "حساب حجم النوع أو القيمة", true},
    {"نوع", TOKEN_TYPE_ALIAS, "keyword", "تعريف اسم بديل لنوع", false},
    {"ثابت", TOKEN_CONST, "keyword", "واصف قيمة ثابتة", true},
    {"ساكن", TOKEN_STATIC, "keyword", "واصف تخزين ساكن", true},
    {"خارجي", TOKEN_EXTERN, "keyword", "واصف تصريح خارجي", true},
    {"إذا", TOKEN_IF, "keyword", "بدء جملة شرطية", true},
    {"وإلا", TOKEN_ELSE, "keyword", "فرع شرطي بديل", true},
    {"طالما", TOKEN_WHILE, "keyword", "حلقة تكرار شرطية", true},
    {"لكل", TOKEN_FOR, "keyword", "حلقة تكرار محددة", true},
    {"توقف", TOKEN_BREAK, "keyword", "إنهاء الحلقة أو حالة الاختيار", true},
    {"استمر", TOKEN_CONTINUE, "keyword", "الانتقال إلى دورة الحلقة التالية", true},
    {"اختر", TOKEN_SWITCH, "keyword", "بدء جملة اختيار متعدد", true},
    {"حالة", TOKEN_CASE, "keyword", "فرع في جملة الاختيار", true},
    {"افتراضي", TOKEN_DEFAULT, "keyword", "الفرع الافتراضي في جملة الاختيار", true},
    {"صواب", TOKEN_TRUE, "value", "قيمة منطقية صائبة", true},
    {"خطأ", TOKEN_FALSE, "value", "قيمة منطقية خاطئة", true},
    {"تعداد", TOKEN_ENUM, "type", "تعريف نوع تعدادي", true},
    {"هيكل", TOKEN_STRUCT, "type", "تعريف نوع هيكلي", true},
    {"اتحاد", TOKEN_UNION, "type", "تعريف نوع اتحادي", true},
};

static const BaaLanguageCompletionEntry g_directives[] = {
    {"#تضمين", "directive", "تضمين ملف رأس باء", "#تضمين", "#تضمين \"${1:ملف.رأسباء}\"", true},
    {"#تعريف", "directive", "تعريف ثابت للمعالجة القبلية", "#تعريف", "#تعريف ${1:الاسم} ${0:القيمة}", true},
    {"#إذا_عرف", "directive", "شرط معالجة قبلية", "#إذا_عرف", "#إذا_عرف ${1:الاسم}\n\t${0}\n#نهاية", true},
    {"#وإلا", "directive", "فرع بديل في شرط المعالجة القبلية", "#وإلا", "#وإلا", false},
    {"#نهاية", "directive", "إنهاء شرط المعالجة القبلية", "#نهاية", "#نهاية", false},
    {"#الغاء_تعريف", "directive", "إلغاء تعريف معالجة قبلية", "#الغاء_تعريف", "#الغاء_تعريف ${0:الاسم}", true},
};

static const BaaLanguageCompletionEntry g_snippets[] = {
    {"الرئيسية (دالة)", "snippet", "قالب نقطة بداية البرنامج", "الرئيسية", "صحيح الرئيسية() {\n\t${0}\n\tإرجع ٠.\n}", true},
    {"دالة جديدة", "snippet", "قالب تعريف دالة", "دالة", "صحيح ${1:اسم_الدالة}(صحيح ${2:معامل}) {\n\t${0}\n\tإرجع ٠.\n}", true},
    {"إذا (شرط)", "snippet", "قالب جملة شرطية", "إذا", "إذا (${1:الشرط}) {\n\t${0}\n}", true},
    {"إذا وإلا", "snippet", "قالب جملة شرطية بفرع بديل", "إذا_وإلا", "إذا (${1:الشرط}) {\n\t${2}\n} وإلا {\n\t${0}\n}", true},
    {"وإلا", "snippet", "قالب فرع شرطي بديل", "وإلا", "وإلا {\n\t${0}\n}", true},
    {"وإلا إذا", "snippet", "قالب شرط إضافي", "وإلا_إذا", "وإلا إذا (${1:الشرط}) {\n\t${0}\n}", true},
    {"لكل (حلقة)", "snippet", "قالب حلقة تكرار محددة", "لكل", "لكل (صحيح ${1:س} = ٠؛ ${1:س} < ${2:١٠}؛ ${1:س}++) {\n\t${0}\n}", true},
    {"طالما (حلقة)", "snippet", "قالب حلقة تكرار شرطية", "طالما", "طالما (${1:الشرط}) {\n\t${0}\n}", true},
    {"اختر (حالات)", "snippet", "قالب اختيار متعدد", "اختر", "اختر (${1:القيمة}) {\n\tحالة ${2:١}:\n\t\t${0}\n\t\tتوقف.\n\tافتراضي:\n\t\tتوقف.\n}", true},
    {"مصفوفة", "snippet", "قالب تعريف مصفوفة", "مصفوفة", "صحيح ${1:المصفوفة}[${2:١٠}].", true},
    {"ثابت (قيمة)", "snippet", "قالب تعريف قيمة ثابتة", "ثابت", "ثابت صحيح ${1:الاسم} = ${0:القيمة}.", true},
};

const BaaLanguageKeyword *baa_language_keywords(size_t *count)
{
    if (count) *count = sizeof(g_keywords) / sizeof(g_keywords[0]);
    return g_keywords;
}

bool baa_language_keyword_token(const char *word, BaaTokenType *token_type)
{
    if (!word || !token_type) return false;
    size_t count = 0;
    const BaaLanguageKeyword *keywords = baa_language_keywords(&count);
    for (size_t i = 0; i < count; ++i)
    {
        if (keywords[i].lexical_keyword && strcmp(word, keywords[i].label) == 0)
        {
            *token_type = keywords[i].token_type;
            return true;
        }
    }
    return false;
}

const BaaLanguageCompletionEntry *baa_language_directives(size_t *count)
{
    if (count) *count = sizeof(g_directives) / sizeof(g_directives[0]);
    return g_directives;
}

const BaaLanguageCompletionEntry *baa_language_snippets(size_t *count)
{
    if (count) *count = sizeof(g_snippets) / sizeof(g_snippets[0]);
    return g_snippets;
}
