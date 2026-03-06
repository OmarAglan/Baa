/**
 * @file ir_mutate.h
 * @brief مساعدات موحّدة لتعديل IR (إدراج/حذف التعليمات) بشكل آمن.
 */

#ifndef BAA_IR_MUTATE_H
#define BAA_IR_MUTATE_H

#include "ir.h"

// ============================================================================
// تعديل قائمة التعليمات داخل الكتلة
// ============================================================================

void ir_block_insert_before(IRBlock* block, IRInst* before, IRInst* inst);
void ir_block_remove_inst(IRBlock* block, IRInst* inst);
void ir_block_insert_phi(IRBlock* block, IRInst* phi);

// ============================================================================
// إدارة حقول الكتل التي تُدار بالـ heap (تحليلات CFG/السيطرة)
// ============================================================================

void ir_block_free_analysis_caches(IRBlock* block);

#endif // BAA_IR_MUTATE_H
