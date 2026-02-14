/**
 * @file ir_data_layout.c
 * @brief تنفيذ تخطيط البيانات للـ IR — v0.3.2.6.6
 *
 * يوفر استعلامات حجم ومحاذاة لكل نوع IR بناءً على
 * وصف الهدف (IRDataLayout). الافتراضي: Windows x86-64.
 *
 * دلالات الحساب (Arithmetic Semantics):
 * ─────────────────────────────────────
 * • الطفحان (Overflow): التفاف مكمل الاثنين (two's-complement wrap).
 * • القسمة/الباقي: اقتطاع نحو الصفر (truncation toward zero).
 *   - حالة خاصة: INT64_MIN / -1 = INT64_MIN (التفاف آمن).
 *   - حالة خاصة: INT64_MIN % -1 = 0.
 * • المقارنات: دائماً بإشارة (signed) للأنواع الصحيحة.
 * • ص١ (i1): أي قيمة غير صفرية → 1، صفر → 0.
 *
 * عقد الذاكرة (Memory Model Contract):
 * ─────────────────────────────────────
 * • حجز (alloca) ينتج مؤشراً مكتوباً: ptr(T).
 * • حمل (load) يجب أن يُطابق نوع العنصر المشار إليه.
 * • خزن (store) نوع القيمة يجب أن يُطابق نوع المؤشر الوجهة.
 * • التسمية (Aliasing): محافظ — أي حمل قد يتداخل مع أي خزن
 *   ما لم يُثبت العكس. لا يوجد TBAA حالياً.
 * • التحسينات لا تحذف/تعيد ترتيب خزن/حمل تعسفياً.
 */

#include "ir_data_layout.h"

#include <stddef.h>

// ============================================================================
// التخطيط الافتراضي: Windows x86-64
// ============================================================================

const IRDataLayout IR_DATA_LAYOUT_WIN_X64 = {
    .pointer_size_bytes  = 8,
    .pointer_align_bytes = 8,
    .i1_store_size_bytes = 1,
    .i8_align_bytes      = 1,
    .i16_align_bytes     = 2,
    .i32_align_bytes     = 4,
    .i64_align_bytes     = 8,
};

// ============================================================================
// مساعد: اختيار التخطيط الافتراضي إذا كان NULL
// ============================================================================

static const IRDataLayout* dl_or_default(const IRDataLayout* dl) {
    return dl ? dl : &IR_DATA_LAYOUT_WIN_X64;
}

// ============================================================================
// حجم النوع بالبايت
// ============================================================================

/**
 * @brief حجم النوع بالبايت.
 *
 * يُرجع الحجم الفعلي بالبايت لنوع IR معيّن.
 * المصفوفات: عدد_العناصر × حجم_العنصر.
 * الدوال: حجم المؤشر (لأنها تُمرر كعنوان).
 *
 * @param dl تخطيط البيانات (NULL = الافتراضي Win x64).
 * @param type نوع الـ IR.
 * @return الحجم بالبايت؛ 0 لـ void أو NULL.
 */
int ir_type_size_bytes(const IRDataLayout* dl, const IRType* type) {
    if (!type) return 0;
    const IRDataLayout* d = dl_or_default(dl);

    switch (type->kind) {
        case IR_TYPE_VOID:
            return 0;
        case IR_TYPE_I1:
            return d->i1_store_size_bytes;  // 1 بايت للتخزين
        case IR_TYPE_I8:
            return 1;
        case IR_TYPE_I16:
            return 2;
        case IR_TYPE_I32:
            return 4;
        case IR_TYPE_I64:
            return 8;
        case IR_TYPE_PTR:
            return d->pointer_size_bytes;
        case IR_TYPE_ARRAY:
            // حجم المصفوفة = عدد العناصر × حجم العنصر الواحد
            return type->data.array.count *
                   ir_type_size_bytes(d, type->data.array.element);
        case IR_TYPE_FUNC:
            // مؤشر دالة
            return d->pointer_size_bytes;
        default:
            return 0;
    }
}

// ============================================================================
// محاذاة النوع بالبايت
// ============================================================================

/**
 * @brief محاذاة النوع بالبايت.
 *
 * تُرجع محاذاة النوع الطبيعية بالبايت.
 * المصفوفات: محاذاة العنصر.
 *
 * @param dl تخطيط البيانات (NULL = الافتراضي Win x64).
 * @param type نوع الـ IR.
 * @return المحاذاة بالبايت؛ 1 كحد أدنى.
 */
int ir_type_alignment(const IRDataLayout* dl, const IRType* type) {
    if (!type) return 1;
    const IRDataLayout* d = dl_or_default(dl);

    switch (type->kind) {
        case IR_TYPE_VOID:
            return 1;
        case IR_TYPE_I1:
            return 1;
        case IR_TYPE_I8:
            return d->i8_align_bytes;
        case IR_TYPE_I16:
            return d->i16_align_bytes;
        case IR_TYPE_I32:
            return d->i32_align_bytes;
        case IR_TYPE_I64:
            return d->i64_align_bytes;
        case IR_TYPE_PTR:
            return d->pointer_align_bytes;
        case IR_TYPE_ARRAY:
            // محاذاة المصفوفة = محاذاة عنصرها
            return ir_type_alignment(d, type->data.array.element);
        case IR_TYPE_FUNC:
            return d->pointer_align_bytes;
        default:
            return 1;
    }
}

// ============================================================================
// حجم التخزين (مع الحشو)
// ============================================================================

/**
 * @brief حجم التخزين مع الحشو (تقريب لأعلى للمحاذاة).
 *
 * @param dl تخطيط البيانات (NULL = الافتراضي Win x64).
 * @param type نوع الـ IR.
 * @return حجم التخزين بالبايت.
 */
int ir_type_store_size(const IRDataLayout* dl, const IRType* type) {
    if (!type) return 0;
    const IRDataLayout* d = dl_or_default(dl);

    int size  = ir_type_size_bytes(d, type);
    int align = ir_type_alignment(d, type);

    if (align <= 0) align = 1;

    // تقريب لأعلى: (size + align - 1) / align * align
    return ((size + align - 1) / align) * align;
}

// ============================================================================
// محددات الأنواع (Type Predicates)
// ============================================================================

/**
 * @brief هل النوع عدد صحيح (ص١/ص٨/ص١٦/ص٣٢/ص٦٤)؟
 */
int ir_type_is_integer(const IRType* type) {
    if (!type) return 0;
    switch (type->kind) {
        case IR_TYPE_I1:
        case IR_TYPE_I8:
        case IR_TYPE_I16:
        case IR_TYPE_I32:
        case IR_TYPE_I64:
            return 1;
        default:
            return 0;
    }
}

/**
 * @brief هل النوع مؤشر؟
 */
int ir_type_is_pointer(const IRType* type) {
    if (!type) return 0;
    return type->kind == IR_TYPE_PTR;
}
