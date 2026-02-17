/**
 * @file driver_toolchain.h
 * @brief تشغيل أدوات التجميع/الربط واكتشاف GCC المضمّن.
 * @version 0.3.2.9.4
 */

#ifndef BAA_DRIVER_TOOLCHAIN_H
#define BAA_DRIVER_TOOLCHAIN_H

#include "driver.h"

#include <stdbool.h>

/**
 * @brief البحث عن GCC المضمّن بجانب baa.exe (Windows) أو تركه افتراضياً.
 */
void driver_toolchain_resolve_gcc_path(void);

/**
 * @brief إرجاع أمر GCC المناسب ("gcc" أو مسار كامل على Windows).
 */
const char *driver_toolchain_get_gcc_command(void);

/**
 * @brief تنسيق الكائنات للمضيف (COFF على Windows، ELF على Linux).
 */
BaaObjectFormat driver_toolchain_host_object_format(void);

/**
 * @brief تجميع ملف .s إلى .o عبر GCC.
 */
int driver_toolchain_assemble_one(const CompilerConfig *config,
                                 CompilerPhaseTimes *times,
                                 const char *asm_file,
                                 const char *obj_file);

/**
 * @brief ربط ملفات الكائنات إلى ملف تنفيذ.
 */
int driver_toolchain_link(const CompilerConfig *config,
                          CompilerPhaseTimes *times,
                          const char **obj_files,
                          int obj_count);

#endif // BAA_DRIVER_TOOLCHAIN_H
