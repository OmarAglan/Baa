/**
 * @file ir_loop.h
 * @brief تحليل الحلقات (Loop Detection) — اكتشاف الحلقات الطبيعية عبر حواف الرجوع.
 * @version 0.3.2.7.1
 *
 * هذا التحليل يُحدد الحلقات الطبيعية (Natural Loops) في CFG الخاصة بـ IR.
 * التعريف:
 * - الحافة (tail -> head) تعتبر حافة رجوع (back edge) إذا كان head يسيطر (dominates) على tail.
 * - الحلقة الطبيعية لهذه الحافة هي أصغر مجموعة كتل تحتوي head و tail وكل أسلاف tail (preds)
 *   حتى الوصول إلى head.
 *
 * ملاحظة:
 * - يعتمد هذا التحليل على `ir_func_compute_dominators()` لبناء idom.
 * - هذا التحليل يُستخدم كأساس لـ LICM وتقليل القوة وفك الحلقات لاحقاً.
 */

#ifndef BAA_IR_LOOP_H
#define BAA_IR_LOOP_H

#include <stdbool.h>

#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IRLoop IRLoop;
typedef struct IRLoopInfo IRLoopInfo;

/**
 * @brief تشغيل تحليل الحلقات على دالة واحدة.
 *
 * @param func دالة IR الهدف.
 * @return معلومات الحلقات (يجب تحريرها عبر ir_loop_info_free)، أو NULL عند الفشل.
 */
IRLoopInfo* ir_loop_analyze_func(IRFunc* func);

/**
 * @brief تحرير معلومات الحلقات.
 */
void ir_loop_info_free(IRLoopInfo* info);

/**
 * @brief عدد الحلقات المكتشفة.
 */
int ir_loop_info_count(IRLoopInfo* info);

/**
 * @brief الحصول على حلقة بالترتيب.
 */
IRLoop* ir_loop_info_get(IRLoopInfo* info, int index);

/**
 * @brief هل الكتلة ضمن الحلقة؟
 */
bool ir_loop_contains(IRLoop* loop, IRBlock* block);

/**
 * @brief رأس الحلقة (header).
 */
IRBlock* ir_loop_header(IRLoop* loop);

/**
 * @brief كتلة ما قبل الرأس (preheader) إن وُجدت (قد تكون NULL).
 */
IRBlock* ir_loop_preheader(IRLoop* loop);

/**
 * @brief عدد كتل الحلقة.
 */
int ir_loop_block_count(IRLoop* loop);

/**
 * @brief الحصول على كتلة من داخل الحلقة.
 */
IRBlock* ir_loop_block_at(IRLoop* loop, int index);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_LOOP_H
