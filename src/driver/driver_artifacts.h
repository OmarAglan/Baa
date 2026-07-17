#ifndef BAA_DRIVER_ARTIFACTS_H
#define BAA_DRIVER_ARTIFACTS_H

/**
 * @brief إنشاء مسار أثر مؤقت فريد بجانب وجهة البناء الحقيقية.
 *
 * يملك المستدعي النص المعاد ويحرره عبر free().
 */
char* driver_make_temp_artifact_path(const char* base,
                                     const char* prefix,
                                     const char* extension);

#endif
