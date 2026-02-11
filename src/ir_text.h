/**
 * @file ir_text.h
 * @brief تسلسل/إلغاء تسلسل الـIR كنص (Text Serialization)
 *
 * هذا الملف يعرّف واجهة كتابة وقراءة تمثيل Baa IR بصيغة نصية
 * قابلة للقراءة الآلية (machine-readable) من أجل الاختبارات والتبادل.
 */

#ifndef BAA_IR_TEXT_H
#define BAA_IR_TEXT_H

#include <stdio.h>

#include "ir.h"

// ============================================================================
// تسلسل IR كنص (IR Text Serialization)
// ============================================================================

/**
 * @brief كتابة وحدة IR إلى ملف نصي (صيغة قياسية)
 * @param module وحدة IR
 * @param out وجهة الكتابة
 * @return 1 عند النجاح، 0 عند الفشل
 */
int ir_text_write_module(IRModule* module, FILE* out);

/**
 * @brief كتابة وحدة IR إلى مسار ملف
 * @param module وحدة IR
 * @param filename مسار الملف
 * @return 1 عند النجاح، 0 عند الفشل
 */
int ir_text_dump_module(IRModule* module, const char* filename);

/**
 * @brief قراءة وحدة IR من ملف نصي
 * @param filename مسار الملف
 * @return وحدة IR جديدة، أو NULL عند الفشل
 */
IRModule* ir_text_read_module_file(const char* filename);

#endif // BAA_IR_TEXT_H
