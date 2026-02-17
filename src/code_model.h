/**
 * @file code_model.h
 * @brief خيارات نموذج الكود (Code Model Options) — v0.3.2.8.3
 */

#ifndef BAA_CODE_MODEL_H
#define BAA_CODE_MODEL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// نموذج الكود (Code Model)
// ============================================================================

typedef enum {
    BAA_CODEMODEL_SMALL = 0,
} BaaCodeModel;

// ============================================================================
// حماية المكدس (Stack Protector)
// ============================================================================

typedef enum {
    BAA_STACKPROT_OFF = 0,
    BAA_STACKPROT_ON  = 1,
    BAA_STACKPROT_ALL = 2,
} BaaStackProtectorMode;

// ============================================================================
// خيارات الخلفية (Backend Options)
// ============================================================================

typedef struct {
    BaaCodeModel code_model;
    bool pic; // -fPIC
    bool pie; // -fPIE / link -pie
    BaaStackProtectorMode stack_protector;
} BaaCodegenOptions;

static inline BaaCodegenOptions baa_codegen_options_default(void)
{
    BaaCodegenOptions o;
    o.code_model = BAA_CODEMODEL_SMALL;
    o.pic = false;
    o.pie = false;
    o.stack_protector = BAA_STACKPROT_OFF;
    return o;
}

#ifdef __cplusplus
}
#endif

#endif // BAA_CODE_MODEL_H
