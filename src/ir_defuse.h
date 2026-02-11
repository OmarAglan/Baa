/**
 * @file ir_defuse.h
 * @brief سلاسل التعريف/الاستعمال (Def-Use) لسجلات SSA في IR.
 *
 * الهدف:
 * - بناء قوائم استعمال لكل سجل افتراضي بسرعة.
 * - تمكين التمريرات من استبدال الاستعمالات دون إعادة مسح الدالة كاملة.
 *
 * ملاحظة إدارة الذاكرة:
 * - هذه البنى تحليلية وليست جزءاً من IR نفسه.
 * - تُخصَّص على الـ heap وتُحرَّر/تُعاد بناؤها عند أي تغيير في IR.
 */

#ifndef BAA_IR_DEFUSE_H
#define BAA_IR_DEFUSE_H

#include <stdbool.h>
#include <stdint.h>

#include "ir.h"

typedef struct IRUse {
    // مؤشر إلى خانة القيمة المستعملة (operand/call_arg/phi entry).
    IRValue** slot;
    struct IRUse* next;
} IRUse;

typedef struct IRDefUse {
    // epoch الذي بُنيت عليه هذه البنية.
    uint32_t built_epoch;

    int max_reg;

    // تعريف السجل (تعليمة تنتج dest). في SSA يجب أن يكون تعريف واحد.
    IRInst** def_inst_by_reg;

    // هل تعريف السجل هو مُعامل (بارامتر) للدالة؟
    unsigned char* def_is_param;

    // رأس قائمة الاستعمالات لكل سجل.
    IRUse** uses_by_reg;

    // كتلة واحدة لتخزين جميع IRUse لتقليل عدد تخصيصات heap.
    IRUse* uses_storage;
    int uses_storage_count;

    // علم تشخيصي: هل رصدنا تعريفاً مكرراً لنفس السجل أثناء البناء؟
    bool has_duplicate_defs;
} IRDefUse;

IRDefUse* ir_defuse_build(IRFunc* func);
void ir_defuse_free(IRDefUse* du);

void ir_func_invalidate_defuse(IRFunc* func);
IRDefUse* ir_func_get_defuse(IRFunc* func, bool rebuild);

#endif // BAA_IR_DEFUSE_H
