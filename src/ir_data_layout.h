/**
 * @file ir_data_layout.h
 * @brief تخطيط البيانات للـ IR (أحجام ومحاذاة الأنواع) — v0.3.2.6.6
 *
 * يوفر هذا الملف واجهة للاستعلام عن أحجام ومحاذاة أنواع الـ IR
 * بناءً على معلمات الهدف (Windows x86-64 حالياً).
 *
 * تُعدّ هذه البنية أساساً لتجريد Target مستقبلي ولخفض
 * الأنواع المركبة (مصفوفات، هياكل) بشكل صحيح.
 */

#ifndef BAA_IR_DATA_LAYOUT_H
#define BAA_IR_DATA_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ir.h"

// ============================================================================
// بنية تخطيط البيانات (Data Layout Descriptor)
// ============================================================================

/**
 * @struct IRDataLayout
 * @brief وصف تخطيط البيانات للهدف المحدد.
 *
 * يجمع أحجام ومحاذاة الأنواع الأساسية في مكان واحد.
 * الأحجام والمحاذاة بالبايت.
 */
typedef struct IRDataLayout {
    int pointer_size_bytes;     /**< حجم المؤشر (مثلاً 8 على x86-64) */
    int pointer_align_bytes;    /**< محاذاة المؤشر */
    int i1_store_size_bytes;    /**< حجم تخزين ص١ (دائماً 1) */
    int i8_align_bytes;         /**< محاذاة ص٨ */
    int i16_align_bytes;        /**< محاذاة ص١٦ */
    int i32_align_bytes;        /**< محاذاة ص٣٢ */
    int i64_align_bytes;        /**< محاذاة ص٦٤ */
} IRDataLayout;

// ============================================================================
// تخطيط البيانات الافتراضي (Windows x86-64)
// ============================================================================

/**
 * @brief تخطيط البيانات الافتراضي لـ Windows x86-64.
 *
 * المؤشرات 8 بايت، المحاذاة = حجم النوع الطبيعي.
 */
extern const IRDataLayout IR_DATA_LAYOUT_WIN_X64;

// ============================================================================
// استعلامات الحجم والمحاذاة (Size & Alignment Queries)
// ============================================================================

/**
 * @brief حجم النوع بالبايت.
 * @param dl تخطيط البيانات (أو NULL للافتراضي).
 * @param type نوع الـ IR.
 * @return الحجم بالبايت (0 لـ void).
 */
int ir_type_size_bytes(const IRDataLayout* dl, const IRType* type);

/**
 * @brief محاذاة النوع بالبايت.
 * @param dl تخطيط البيانات (أو NULL للافتراضي).
 * @param type نوع الـ IR.
 * @return المحاذاة بالبايت (1 كحد أدنى).
 */
int ir_type_alignment(const IRDataLayout* dl, const IRType* type);

/**
 * @brief حجم التخزين (مع الحشو) بالبايت.
 *
 * يُقرّب حجم النوع لأعلى ليكون مضاعفاً للمحاذاة.
 * مثلاً: ص١ → حجم=1 محاذاة=1 → تخزين=1.
 *
 * @param dl تخطيط البيانات (أو NULL للافتراضي).
 * @param type نوع الـ IR.
 * @return حجم التخزين بالبايت.
 */
int ir_type_store_size(const IRDataLayout* dl, const IRType* type);

// ============================================================================
// محددات الأنواع (Type Predicates)
// ============================================================================

/**
 * @brief هل النوع عدد صحيح (ص١/ص٨/ص١٦/ص٣٢/ص٦٤)؟
 * @param type نوع الـ IR.
 * @return 1 إذا كان عدداً صحيحاً، 0 غير ذلك.
 */
int ir_type_is_integer(const IRType* type);

/**
 * @brief هل النوع مؤشر؟
 * @param type نوع الـ IR.
 * @return 1 إذا كان مؤشراً، 0 غير ذلك.
 */
int ir_type_is_pointer(const IRType* type);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_DATA_LAYOUT_H
