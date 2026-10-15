/**
 * @file driver_sdk.h
 * @brief إخراج baa-sdk-info-v1: ما اكتشفه هذا المصرّف من طقمه.
 */

#ifndef BAA_DRIVER_SDK_H
#define BAA_DRIVER_SDK_H

#include <stdbool.h>
#include <stdio.h>

/**
 * @brief كتابة وثيقة baa-sdk-info-v1 بعد حل مسارات الأدوات.
 */
bool driver_sdk_info_json_write(FILE *out);

#endif // BAA_DRIVER_SDK_H
