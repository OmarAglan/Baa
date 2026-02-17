/**
 * @file ir_verify_common.h
 * @brief أدوات مشتركة للتحقق من الـ IR/SSA (تجنب تكرار منطق التحقق).
 * @version 0.3.2.9.4
 */

#ifndef BAA_IR_VERIFY_COMMON_H
#define BAA_IR_VERIFY_COMMON_H

#include "ir.h"

typedef void (*IRVerifyReportFn)(void* ctx,
                                 const IRFunc* func,
                                 const IRBlock* block,
                                 const IRInst* inst,
                                 const char* msg);

/**
 * @brief التحقق من أن كل مراجع الكتل داخل الدالة لا تشير إلى كتل خارجها.
 *
 * هذا الفحص يُستخدم قبل rebuild_preds لتفادي تلويث CFG لدوال أخرى.
 *
 * @return 1 عند النجاح، 0 عند الفشل.
 */
int ir_verify_func_block_refs_safe(const IRFunc* func, IRVerifyReportFn report, void* ctx);

#endif // BAA_IR_VERIFY_COMMON_H
