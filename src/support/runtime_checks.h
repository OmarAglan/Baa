/**
 * @file runtime_checks.h
 * @brief أقنعة فحوص وقت التشغيل الاختيارية.
 */

#ifndef BAA_RUNTIME_CHECKS_H
#define BAA_RUNTIME_CHECKS_H

#define BAA_RUNTIME_CHECK_BOUNDS   (1u << 0)
#define BAA_RUNTIME_CHECK_NULL     (1u << 1)
#define BAA_RUNTIME_CHECK_DIV_ZERO (1u << 2)
#define BAA_RUNTIME_CHECK_SHIFT    (1u << 3)

#define BAA_RUNTIME_CHECK_ALL \
    (BAA_RUNTIME_CHECK_BOUNDS | BAA_RUNTIME_CHECK_NULL | \
     BAA_RUNTIME_CHECK_DIV_ZERO | BAA_RUNTIME_CHECK_SHIFT)

#endif // BAA_RUNTIME_CHECKS_H
