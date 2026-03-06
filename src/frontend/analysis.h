/**
 * @file analysis.h
 * @brief ????? ??????? ???????.
 */

#ifndef BAA_FRONTEND_ANALYSIS_H
#define BAA_FRONTEND_ANALYSIS_H

#include <stdbool.h>
#include <stdint.h>
#include "ast.h"

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
    DataType ptr_base_type;      // نوع أساس المؤشر عندما type == TYPE_POINTER
    char ptr_base_type_name[32]; // اسم النوع المركب لأساس المؤشر
    int ptr_depth;               // عمق المؤشر عندما type == TYPE_POINTER
    FuncPtrSig* func_sig;        // توقيع مؤشر الدالة عندما type == TYPE_FUNC_PTR
    bool is_array;     // هل الرمز مصفوفة؟
    int array_rank;    // عدد الأبعاد
    int64_t array_total_elems; // حاصل ضرب الأبعاد
    int* array_dims;   // أبعاد المصفوفة (مملوك لجدول الرموز)
    int offset;        // الإزاحة في المكدس أو العنوان
    bool is_const;     // هل هو ثابت (immutable)؟
    bool is_static;    // هل التخزين ساكن؟
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

#endif
