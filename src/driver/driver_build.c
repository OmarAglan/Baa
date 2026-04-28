/**
 * @file driver_build.c
 * @brief تنفيذ بيان البناء والتخزين المؤقت الاختياري.
 */

#include "driver_internal.h"

#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define BAA_BUILD_SCHEMA 1
#define BAA_HASH_OFFSET 1469598103934665603ull
#define BAA_HASH_PRIME 1099511628211ull

typedef struct {
    char source_hash[17];
    char object_path[PATH_MAX];
    DriverBuildDep* deps;
    size_t dep_count;
} DriverCacheMeta;

static uint64_t fnv1a_update(uint64_t h, const unsigned char* data, size_t len)
{
    if (!data) return h;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint64_t)data[i];
        h *= BAA_HASH_PRIME;
    }
    return h;
}

static void hash_to_hex(uint64_t h, char out[17])
{
    static const char k_hex[] = "0123456789abcdef";
    for (int i = 15; i >= 0; --i) {
        out[i] = k_hex[h & 0xFu];
        h >>= 4;
    }
    out[16] = '\0';
}

static bool hash_file_hex(const char* path, char out[17])
{
    if (!path || !out) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint64_t h = BAA_HASH_OFFSET;
    unsigned char buf[8192];
    bool ok = true;
    while (!feof(f)) {
        size_t got = fread(buf, 1, sizeof(buf), f);
        if (got > 0) h = fnv1a_update(h, buf, got);
        if (ferror(f)) {
            ok = false;
            break;
        }
    }
    fclose(f);
    if (!ok) return false;
    hash_to_hex(h, out);
    return true;
}

static char* driver_build_strdup(const char* text)
{
    if (!text) return NULL;
    size_t n = strlen(text);
    char* out = (char*)malloc(n + 1u);
    if (!out) return NULL;
    memcpy(out, text, n + 1u);
    return out;
}

static void normalize_path_text(char* path)
{
    if (!path) return;
    for (size_t i = 0; path[i]; ++i) {
        if (path[i] == '\\') path[i] = '/';
    }
#ifdef _WIN32
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        if (path[0] >= 'A' && path[0] <= 'Z') path[0] = (char)(path[0] - 'A' + 'a');
    }
#endif
}

char* driver_build_canonical_path(const char* path)
{
    if (!path || !path[0]) return NULL;
    char resolved[PATH_MAX];
    char* out = NULL;

#ifdef _WIN32
    if (_fullpath(resolved, path, sizeof(resolved)) != NULL) {
        out = driver_build_strdup(resolved);
    }
#else
    if (realpath(path, resolved) != NULL) {
        out = driver_build_strdup(resolved);
    }
#endif
    if (!out) out = driver_build_strdup(path);
    if (out) normalize_path_text(out);
    return out;
}

char* driver_build_default_cache_dir(void)
{
    return driver_build_strdup(".baa_build/cache");
}

static bool ensure_dir_one(const char* path)
{
    if (!path || !path[0]) return false;
#ifdef _WIN32
    if (_mkdir(path) == 0) return true;
#else
    if (mkdir(path, 0777) == 0) return true;
#endif
    return errno == EEXIST;
}

static bool ensure_dir_recursive(const char* path)
{
    if (!path || !path[0]) return false;
    char* tmp = driver_build_strdup(path);
    if (!tmp) return false;
    normalize_path_text(tmp);

    for (char* p = tmp; *p; ++p) {
        if (*p != '/') continue;
        if (p == tmp) continue;
        if (p > tmp && *(p - 1) == ':') continue;
        *p = '\0';
        if (tmp[0] && !ensure_dir_one(tmp)) {
            free(tmp);
            return false;
        }
        *p = '/';
    }
    bool ok = ensure_dir_one(tmp);
    free(tmp);
    return ok;
}

static const char* cache_dir_or_default(const CompilerConfig* config)
{
    if (config && config->cache_dir && config->cache_dir[0]) return config->cache_dir;
    return ".baa_build/cache";
}

static void hash_string(uint64_t* h, const char* text)
{
    if (!h || !text) return;
    *h = fnv1a_update(*h, (const unsigned char*)text, strlen(text));
    *h = fnv1a_update(*h, (const unsigned char*)"\n", 1u);
}

static void build_slot_hex(const CompilerConfig* config, const char* source, char out[17])
{
    uint64_t h = BAA_HASH_OFFSET;
    hash_string(&h, BAA_VERSION);
    hash_string(&h, source ? source : "");
    hash_string(&h, config && config->target ? config->target->name : "");

    char tmp[128];
    snprintf(tmp,
             sizeof(tmp),
             "O=%d;dbg=%d;pic=%d;pie=%d;sp=%d;cm=%d;unroll=%d",
             config ? (int)config->opt_level : 0,
             config ? (int)config->debug_info : 0,
             config ? (int)config->codegen_opts.pic : 0,
             config ? (int)config->codegen_opts.pie : 0,
             config ? (int)config->codegen_opts.stack_protector : 0,
             config ? (int)config->codegen_opts.code_model : 0,
             config ? (int)config->funroll_loops : 0);
    hash_string(&h, tmp);

    if (config && config->include_dirs) {
        for (size_t i = 0; i < config->include_dir_count; ++i) {
            hash_string(&h, config->include_dirs[i] ? config->include_dirs[i] : "");
        }
    }
    hash_to_hex(h, out);
}

static char* join_cache_path(const CompilerConfig* config, const char* slot, const char* ext)
{
    const char* dir = cache_dir_or_default(config);
    size_t need = strlen(dir) + 1u + strlen(slot) + strlen(ext) + 1u;
    char* out = (char*)malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "%s/%s%s", dir, slot, ext);
    normalize_path_text(out);
    return out;
}

bool driver_build_cache_is_allowed(const CompilerConfig* config)
{
    if (!config || !config->incremental) return false;
    if (config->assembly_only) return false;
    if (config->dump_ir || config->dump_ir_opt || config->emit_ir) return false;
    if (config->verify_ir || config->verify_ssa || config->verify_gate) return false;
    return true;
}

void driver_build_manifest_init(DriverBuildManifest* manifest)
{
    if (!manifest) return;
    manifest->units = NULL;
    manifest->unit_count = 0;
    manifest->unit_capacity = 0;
}

static void free_unit(DriverBuildUnitRecord* unit)
{
    if (!unit) return;
    free(unit->source);
    free(unit->output);
    free(unit->cache_slot);
    free(unit->cache_reason);
    for (size_t i = 0; i < unit->dep_count; ++i) {
        free(unit->deps[i].path);
    }
    free(unit->deps);
    memset(unit, 0, sizeof(*unit));
}

void driver_build_manifest_free(DriverBuildManifest* manifest)
{
    if (!manifest) return;
    for (size_t i = 0; i < manifest->unit_count; ++i) {
        free_unit(&manifest->units[i]);
    }
    free(manifest->units);
    manifest->units = NULL;
    manifest->unit_count = 0;
    manifest->unit_capacity = 0;
}

static DriverBuildUnitRecord* manifest_add_unit(DriverBuildManifest* manifest)
{
    if (!manifest) return NULL;
    if (manifest->unit_count >= manifest->unit_capacity) {
        size_t new_cap = manifest->unit_capacity ? manifest->unit_capacity * 2u : 8u;
        DriverBuildUnitRecord* units =
            (DriverBuildUnitRecord*)realloc(manifest->units, new_cap * sizeof(*units));
        if (!units) return NULL;
        manifest->units = units;
        manifest->unit_capacity = new_cap;
    }
    DriverBuildUnitRecord* unit = &manifest->units[manifest->unit_count++];
    memset(unit, 0, sizeof(*unit));
    return unit;
}

static int dep_cmp(const void* a, const void* b)
{
    const DriverBuildDep* da = (const DriverBuildDep*)a;
    const DriverBuildDep* db = (const DriverBuildDep*)b;
    if (!da->path && !db->path) return 0;
    if (!da->path) return -1;
    if (!db->path) return 1;
    return strcmp(da->path, db->path);
}

static bool fill_deps(DriverBuildUnitRecord* unit, const char* const* dep_paths, size_t dep_count)
{
    if (!unit) return false;
    if (!dep_paths || dep_count == 0) return true;

    unit->deps = (DriverBuildDep*)calloc(dep_count, sizeof(*unit->deps));
    if (!unit->deps) return false;

    for (size_t i = 0; i < dep_count; ++i) {
        char* canonical = driver_build_canonical_path(dep_paths[i]);
        if (!canonical) return false;
        unit->deps[unit->dep_count].path = canonical;
        if (!hash_file_hex(canonical, unit->deps[unit->dep_count].hash)) {
            strcpy(unit->deps[unit->dep_count].hash, "0000000000000000");
        }
        unit->dep_count++;
    }

    qsort(unit->deps, unit->dep_count, sizeof(*unit->deps), dep_cmp);
    return true;
}

static bool add_record(DriverBuildManifest* manifest,
                       const CompilerConfig* config,
                       const char* input_file,
                       const char* output_file,
                       const char* const* dep_paths,
                       size_t dep_count,
                       bool cache_enabled,
                       bool cache_hit,
                       const char* reason)
{
    if (!manifest) return true;
    DriverBuildUnitRecord* unit = manifest_add_unit(manifest);
    if (!unit) return false;

    unit->source = driver_build_canonical_path(input_file);
    unit->output = driver_build_canonical_path(output_file);
    unit->cache_enabled = cache_enabled;
    unit->cache_hit = cache_hit;
    unit->cache_reason = driver_build_strdup(reason ? reason : "");

    if (unit->source) {
        char slot[17];
        build_slot_hex(config, unit->source, slot);
        unit->cache_slot = driver_build_strdup(slot);
    }

    return fill_deps(unit, dep_paths, dep_count);
}

static void cache_meta_free(DriverCacheMeta* meta)
{
    if (!meta) return;
    for (size_t i = 0; i < meta->dep_count; ++i) {
        free(meta->deps[i].path);
    }
    free(meta->deps);
    memset(meta, 0, sizeof(*meta));
}

static char* extract_json_string(const char* text, const char* key)
{
    if (!text || !key) return NULL;
    const char* p = strstr(text, key);
    if (!p) return NULL;
    p += strlen(key);
    const char* e = strchr(p, '"');
    if (!e) return NULL;
    size_t n = (size_t)(e - p);
    char* out = (char*)malloc(n + 1u);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

static bool parse_cache_meta(const char* meta_path, DriverCacheMeta* meta)
{
    if (!meta_path || !meta) return false;
    memset(meta, 0, sizeof(*meta));
    FILE* probe = fopen(meta_path, "rb");
    if (!probe) return false;
    fclose(probe);
    char* text = read_file(meta_path);
    if (!text) return false;

    char* source_hash = extract_json_string(text, "\"source_hash\":\"");
    char* object_path = extract_json_string(text, "\"object\":\"");
    if (!source_hash || !object_path) {
        free(source_hash);
        free(object_path);
        free(text);
        return false;
    }
    snprintf(meta->source_hash, sizeof(meta->source_hash), "%s", source_hash);
    snprintf(meta->object_path, sizeof(meta->object_path), "%s", object_path);
    free(source_hash);
    free(object_path);

    const char* p = text;
    while ((p = strstr(p, "{\"path\":\"")) != NULL) {
        p += strlen("{\"path\":\"");
        const char* path_end = strchr(p, '"');
        if (!path_end) break;
        const char* hkey = strstr(path_end, "\"hash\":\"");
        if (!hkey) break;
        hkey += strlen("\"hash\":\"");
        const char* hash_end = strchr(hkey, '"');
        if (!hash_end) break;

        DriverBuildDep* deps =
            (DriverBuildDep*)realloc(meta->deps, (meta->dep_count + 1u) * sizeof(*deps));
        if (!deps) {
            free(text);
            return false;
        }
        meta->deps = deps;
        DriverBuildDep* dep = &meta->deps[meta->dep_count];
        memset(dep, 0, sizeof(*dep));

        size_t path_len = (size_t)(path_end - p);
        dep->path = (char*)malloc(path_len + 1u);
        if (!dep->path) {
            free(text);
            return false;
        }
        memcpy(dep->path, p, path_len);
        dep->path[path_len] = '\0';

        size_t hash_len = (size_t)(hash_end - hkey);
        if (hash_len > 16u) hash_len = 16u;
        memcpy(dep->hash, hkey, hash_len);
        dep->hash[hash_len] = '\0';
        meta->dep_count++;
        p = hash_end;
    }

    free(text);
    return true;
}

static bool cache_meta_valid(const DriverCacheMeta* meta, const char* source_hash)
{
    if (!meta || !source_hash || strcmp(meta->source_hash, source_hash) != 0) return false;
    if (!meta->object_path[0]) return false;

    FILE* obj = fopen(meta->object_path, "rb");
    if (!obj) return false;
    fclose(obj);

    for (size_t i = 0; i < meta->dep_count; ++i) {
        char current_hash[17];
        if (!hash_file_hex(meta->deps[i].path, current_hash)) return false;
        if (strcmp(current_hash, meta->deps[i].hash) != 0) return false;
    }
    return true;
}

bool driver_build_try_reuse_object(const CompilerConfig* config,
                                   const char* input_file,
                                   const char* obj_file,
                                   DriverBuildManifest* manifest)
{
    if (!driver_build_cache_is_allowed(config)) return false;
    char* source = driver_build_canonical_path(input_file);
    if (!source) return false;

    char source_hash[17];
    if (!hash_file_hex(source, source_hash)) {
        free(source);
        return false;
    }

    char slot[17];
    build_slot_hex(config, source, slot);
    char* meta_path = join_cache_path(config, slot, ".json");
    DriverCacheMeta meta;
    bool hit = false;

    if (meta_path && parse_cache_meta(meta_path, &meta)) {
        if (cache_meta_valid(&meta, source_hash)) {
            hit = driver_toolchain_copy_file_utf8(meta.object_path, obj_file);
            if (hit && manifest) {
                const char** dep_paths = (const char**)calloc(meta.dep_count, sizeof(char*));
                if (dep_paths) {
                    for (size_t i = 0; i < meta.dep_count; ++i) dep_paths[i] = meta.deps[i].path;
                    (void)add_record(manifest, config, input_file, obj_file, dep_paths, meta.dep_count, true, true, "hit");
                    free(dep_paths);
                }
            }
        }
        cache_meta_free(&meta);
    }

    free(meta_path);
    free(source);
    return hit;
}

static void json_escape(FILE* out, const char* text)
{
    if (!out || !text) return;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', out);
            fputc((int)*p, out);
        } else if (*p == '\n') {
            fputs("\\n", out);
        } else if (*p == '\r') {
            fputs("\\r", out);
        } else if (*p == '\t') {
            fputs("\\t", out);
        } else {
            fputc((int)*p, out);
        }
    }
}

static bool write_cache_meta(const char* meta_path,
                             const char* object_path,
                             const DriverBuildUnitRecord* unit)
{
    if (!meta_path || !object_path || !unit) return false;
    FILE* out = fopen(meta_path, "wb");
    if (!out) return false;
    char source_hash[17];
    if (!unit->source || !hash_file_hex(unit->source, source_hash)) {
        strcpy(source_hash, "0000000000000000");
    }
    fprintf(out, "{\"schema\":%d,\"source_hash\":\"%s\",\"object\":\"", BAA_BUILD_SCHEMA, source_hash);
    json_escape(out, object_path);
    fputs("\",\"dependencies\":[", out);
    for (size_t i = 0; i < unit->dep_count; ++i) {
        if (i) fputc(',', out);
        fputs("{\"path\":\"", out);
        json_escape(out, unit->deps[i].path);
        fprintf(out, "\",\"hash\":\"%s\"}", unit->deps[i].hash);
    }
    fputs("]}\n", out);
    fclose(out);
    return true;
}

bool driver_build_update_cache(const CompilerConfig* config,
                               const char* input_file,
                               const char* obj_file,
                               const char* const* dep_paths,
                               size_t dep_count,
                               DriverBuildManifest* manifest)
{
    bool cache_allowed = driver_build_cache_is_allowed(config);
    DriverBuildManifest local_manifest;
    DriverBuildManifest* target_manifest = manifest;
    if (!target_manifest) {
        driver_build_manifest_init(&local_manifest);
        target_manifest = &local_manifest;
    }

    if (!add_record(target_manifest,
                    config,
                    input_file,
                    obj_file,
                    dep_paths,
                    dep_count,
                    cache_allowed,
                    false,
                    cache_allowed ? "miss" : "bypass")) {
        if (!manifest) driver_build_manifest_free(&local_manifest);
        return false;
    }

    if (!cache_allowed || target_manifest->unit_count == 0) {
        if (!manifest) driver_build_manifest_free(&local_manifest);
        return true;
    }
    DriverBuildUnitRecord* unit = &target_manifest->units[target_manifest->unit_count - 1u];
    if (!unit->cache_slot) {
        if (!manifest) driver_build_manifest_free(&local_manifest);
        return false;
    }

    const char* cache_dir = cache_dir_or_default(config);
    if (!ensure_dir_recursive(cache_dir)) {
        if (!manifest) driver_build_manifest_free(&local_manifest);
        return false;
    }

    char* cache_obj = join_cache_path(config, unit->cache_slot, ".o");
    char* cache_meta = join_cache_path(config, unit->cache_slot, ".json");
    if (!cache_obj || !cache_meta) {
        free(cache_obj);
        free(cache_meta);
        if (!manifest) driver_build_manifest_free(&local_manifest);
        return false;
    }

    bool ok = driver_toolchain_copy_file_utf8(obj_file, cache_obj);
    if (ok) ok = write_cache_meta(cache_meta, cache_obj, unit);
    free(cache_obj);
    free(cache_meta);
    if (!manifest) driver_build_manifest_free(&local_manifest);
    return ok;
}

bool driver_build_record_uncached(const CompilerConfig* config,
                                  const char* input_file,
                                  const char* output_file,
                                  const char* const* dep_paths,
                                  size_t dep_count,
                                  const char* reason,
                                  DriverBuildManifest* manifest)
{
    bool cache_allowed = driver_build_cache_is_allowed(config);
    return add_record(manifest,
                      config,
                      input_file,
                      output_file,
                      dep_paths,
                      dep_count,
                      cache_allowed,
                      false,
                      reason ? reason : (cache_allowed ? "miss" : "bypass"));
}

bool driver_build_write_manifest(const CompilerConfig* config,
                                 const DriverBuildManifest* manifest,
                                 const char* manifest_path)
{
    if (!manifest_path || !manifest) return true;
    FILE* out = fopen(manifest_path, "wb");
    if (!out) return false;

    fprintf(out, "{\n  \"schema\": %d,\n", BAA_BUILD_SCHEMA);
    fprintf(out, "  \"compiler_version\": \"%s\",\n", BAA_VERSION);
    fprintf(out, "  \"target\": \"");
    json_escape(out, config && config->target ? config->target->name : "");
    fprintf(out, "\",\n  \"mode\": \"%s\",\n",
            config && config->assembly_only ? "assembly" :
            (config && config->compile_only ? "compile" : "link"));
    fprintf(out, "  \"opt_level\": %d,\n", config ? (int)config->opt_level : 0);
    fprintf(out, "  \"incremental\": %s,\n", config && config->incremental ? "true" : "false");
    fputs("  \"units\": [\n", out);

    for (size_t i = 0; i < manifest->unit_count; ++i) {
        const DriverBuildUnitRecord* unit = &manifest->units[i];
        fprintf(out, "    {\n      \"source\": \"");
        json_escape(out, unit->source ? unit->source : "");
        fprintf(out, "\",\n      \"output\": \"");
        json_escape(out, unit->output ? unit->output : "");
        fprintf(out, "\",\n      \"cache\": {\"enabled\": %s, \"hit\": %s, \"slot\": \"",
                unit->cache_enabled ? "true" : "false",
                unit->cache_hit ? "true" : "false");
        json_escape(out, unit->cache_slot ? unit->cache_slot : "");
        fprintf(out, "\", \"reason\": \"");
        json_escape(out, unit->cache_reason ? unit->cache_reason : "");
        fputs("\"},\n      \"dependencies\": [\n", out);
        for (size_t j = 0; j < unit->dep_count; ++j) {
            fprintf(out, "        {\"path\": \"");
            json_escape(out, unit->deps[j].path ? unit->deps[j].path : "");
            fprintf(out, "\", \"hash\": \"%s\"}%s\n",
                    unit->deps[j].hash,
                    (j + 1u < unit->dep_count) ? "," : "");
        }
        fprintf(out, "      ]\n    }%s\n", (i + 1u < manifest->unit_count) ? "," : "");
    }

    fputs("  ]\n}\n", out);
    fclose(out);
    return true;
}
