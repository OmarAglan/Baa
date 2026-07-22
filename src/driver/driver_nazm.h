/**
 * @file driver_nazm.h
 * @brief ربط مُصدّر نظم بعقد الخروج والتشخيص في سائق باء.
 */

#ifndef BAA_DRIVER_NAZM_H
#define BAA_DRIVER_NAZM_H

#include "driver.h"
#include "../backend/isel.h"

typedef struct DriverBuildManifest DriverBuildManifest;

/**
 * @brief هل يحمل المسار امتداد مصدر نظم العربي `.نظم`؟
 */
bool driver_nazm_is_source_path(const char *path);

/**
 * @brief التحقق المسبق من توافق ملفات `.نظم` المباشرة مع وضع السائق.
 */
BaaCompilerExitCode driver_validate_nazm_inputs(
    const CompilerConfig *config,
    char **input_files,
    int input_count);

BaaCompilerExitCode driver_emit_nazm_source(const CompilerConfig *config,
                                            MachineModule *module,
                                            const char *output_path);

/**
 * @brief إرجاع ملف نظم التنفيذي للمسار الطبيعي.
 *
 * الأولوية: --nazm-path ثم BAA_NAZM ثم الاسم `nazm` من PATH.
 */
const char *driver_nazm_get_executable(const CompilerConfig *config);

/**
 * @brief تثبيت بصمة إصدار/قدرات نظم الدقيقة في إعداد البناء.
 */
BaaCompilerExitCode driver_nazm_resolve_fingerprint(CompilerConfig *config);

/**
 * @brief إصدار وحدة الآلة كمصدر نظم وتجميعها إلى كائن للمسار الطبيعي.
 */
BaaCompilerExitCode driver_assemble_nazm_module(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    MachineModule *module,
    const char *source_path,
    const char *object_path,
    bool keep_source);

/**
 * @brief تجميع نقطة البدء المستضافة العربية لهدف ELF عبر نظم.
 */
BaaCompilerExitCode driver_assemble_nazm_startup(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    const char *source_path,
    const char *object_path,
    bool keep_source);

/**
 * @brief تجميع ملف `.نظم` قدمه المستخدم مباشرة إلى كائن.
 *
 * لا يحذف ملف المصدر، ويصنف أخطاء مصدر نظم برمز المصدر `1`.
 */
BaaCompilerExitCode driver_compile_nazm_input(
    const CompilerConfig *config,
    int input_count,
    const char *source_path,
    CompilerPhaseTimes *times,
    DriverBuildManifest *build_manifest,
    char **out_object_path);

/**
 * @brief إصدار مصدر الظل وتجميعه عبر ملف نظم التنفيذي المحدد صراحة.
 * @param out_object_path مسار مملوك للمستدعي عند النجاح.
 */
BaaCompilerExitCode driver_emit_nazm_shadow_object(const CompilerConfig *config,
                                                   MachineModule *module,
                                                   char **out_object_path);

#endif
