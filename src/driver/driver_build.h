/**
 * @file driver_build.h
 * @brief بيانات البناء والتخزين المؤقت لتسريع البناءات المتكررة.
 */

#ifndef BAA_DRIVER_BUILD_H
#define BAA_DRIVER_BUILD_H

#include "driver.h"

typedef struct {
    char* path;
    char hash[17];
} DriverBuildDep;

typedef struct {
    char* source;
    char* output;
    char* cache_slot;
    char* cache_reason;
    bool cache_enabled;
    bool cache_hit;
    DriverBuildDep* deps;
    size_t dep_count;
} DriverBuildUnitRecord;

typedef struct DriverBuildManifest {
    DriverBuildUnitRecord* units;
    size_t unit_count;
    size_t unit_capacity;
} DriverBuildManifest;

void driver_build_manifest_init(DriverBuildManifest* manifest);
void driver_build_manifest_free(DriverBuildManifest* manifest);

bool driver_build_cache_is_allowed(const CompilerConfig* config);

char* driver_build_default_cache_dir(void);
char* driver_build_canonical_path(const char* path);

bool driver_build_try_reuse_object(const CompilerConfig* config,
                                   const char* input_file,
                                   const char* obj_file,
                                   DriverBuildManifest* manifest);

bool driver_build_update_cache(const CompilerConfig* config,
                               const char* input_file,
                               const char* obj_file,
                               const char* const* dep_paths,
                               size_t dep_count,
                               DriverBuildManifest* manifest);

bool driver_build_record_uncached(const CompilerConfig* config,
                                  const char* input_file,
                                  const char* output_file,
                                  const char* const* dep_paths,
                                  size_t dep_count,
                                  const char* reason,
                                  DriverBuildManifest* manifest);

bool driver_build_write_manifest(const CompilerConfig* config,
                                 const DriverBuildManifest* manifest,
                                 const char* manifest_path);

#endif // BAA_DRIVER_BUILD_H
