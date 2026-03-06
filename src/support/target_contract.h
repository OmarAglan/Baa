/**
 * @file target_contract.h
 * @brief عقد مشترك ومصغّر لوصف الهدف بين المكوّنات.
 */

#ifndef BAA_SUPPORT_TARGET_CONTRACT_H
#define BAA_SUPPORT_TARGET_CONTRACT_H

#include <stdbool.h>

/**
 * @enum BaaTargetKind
 * @brief يحدد الهدف المنطقي للمترجم دون كشف بقية تفاصيل الخلفية.
 */
typedef enum {
    BAA_TARGET_X86_64_WINDOWS = 0,
    BAA_TARGET_X86_64_LINUX   = 1,
} BaaTargetKind;

typedef struct BaaTarget BaaTarget;

/**
 * @brief إرجاع نوع الهدف المنطقي من واصف هدف معتم.
 */
BaaTargetKind baa_target_kind(const BaaTarget* target);

/**
 * @brief هل الهدف الحالي هو Windows x64.
 */
bool baa_target_is_windows(const BaaTarget* target);

/**
 * @brief هل الهدف الحالي هو Linux x64.
 */
bool baa_target_is_linux(const BaaTarget* target);

#endif
