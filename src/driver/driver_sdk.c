/**
 * @file driver_sdk.c
 * @brief إخراج baa-sdk-info-v1 من مسارات الطقم التي حلّها السائق.
 *
 * لا تخمن الوثيقة شيئا: كل مسار فيها هو ما سيستعمله هذا المصرّف فعلا، أو null
 * إن لم يوجد. تُطبَّع المسارات بفواصل '/' وتُطوى مقاطع '..' لفظيا.
 */

#include "driver_sdk.h"

#include "driver.h"
#include "driver_nazm.h"
#include "driver_toolchain.h"
#include "../backend/target.h"
#include "../support/file_io.h"
#include "../support/version.h"

#include <string.h>

#define BAA_SDK_METADATA_FILE "BAA-SDK.json"
#define BAA_SDK_PATH_CAP 4096

static void sdk_json_string(FILE *out, const char *text)
{
    if (!text)
    {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor; ++cursor)
    {
        switch (*cursor)
        {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (*cursor < 0x20u)
                    fprintf(out, "\\u%04x", (unsigned int)*cursor);
                else
                    fputc((int)*cursor, out);
        }
    }
    fputc('"', out);
}

/**
 * @brief توحيد الفواصل وطي '.' و'..' لفظيا؛ يرجع false إن لم يتسع المخزن.
 */
static bool sdk_normalize_path(const char *path, char *out, size_t out_cap)
{
    size_t length = strlen(path);
    if (length + 1u > out_cap) return false;
    memcpy(out, path, length + 1u);
    for (char *cursor = out; *cursor; ++cursor)
        if (*cursor == '\\') *cursor = '/';

    // يبقى البادئ (حرف القرص أو '/') ثابتا ولا يُطوى.
    size_t root = 0;
    if (out[0] == '/') root = 1;
    else if (out[0] && out[1] == ':') root = out[2] == '/' ? 3 : 2;

    size_t write = root;
    size_t read = root;
    while (out[read])
    {
        size_t end = read;
        while (out[end] && out[end] != '/') ++end;
        size_t segment = end - read;
        bool more = out[end] != '\0';
        if (segment == 0 || (segment == 1 && out[read] == '.'))
        {
            // مقطع فارغ أو '.'
        }
        else if (segment == 2 && out[read] == '.' && out[read + 1] == '.')
        {
            // المقاطع المكتوبة لا تنتهي بفاصل، فـ back بداية آخر مقطع.
            size_t back = write;
            while (back > root && out[back - 1] != '/') --back;
            bool previous_is_parent = write - back == 2 &&
                                      out[back] == '.' && out[back + 1] == '.';
            if (back == write || previous_is_parent)
            {
                if (write > root && out[write - 1] != '/') out[write++] = '/';
                out[write++] = '.';
                out[write++] = '.';
            }
            else
            {
                write = back;
                if (write > root && out[write - 1] == '/') --write;
            }
        }
        else
        {
            if (write > root && out[write - 1] != '/') out[write++] = '/';
            memmove(out + write, out + read, segment);
            write += segment;
        }
        read = more ? end + 1u : end;
    }
    if (write > root && out[write - 1] == '/') --write;
    out[write] = '\0';
    return true;
}

static bool sdk_file_exists(const char *path)
{
    FILE *file = baa_fopen_utf8(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static void sdk_json_path(FILE *out, const char *path)
{
    char normalized[BAA_SDK_PATH_CAP];
    if (!path || !sdk_normalize_path(path, normalized, sizeof(normalized)))
    {
        sdk_json_string(out, path);
        return;
    }
    sdk_json_string(out, normalized);
}

/**
 * @brief جذر baa-sdk-v1 هو أب مجلد bin/ إن وُجد فيه BAA-SDK.json.
 */
static bool sdk_find_root(const char *executable_dir, char *root, size_t root_cap)
{
    if (!executable_dir) return false;
    char joined[BAA_SDK_PATH_CAP];
    int count = snprintf(joined, sizeof(joined), "%s/..", executable_dir);
    if (count <= 0 || (size_t)count >= sizeof(joined)) return false;
    if (!sdk_normalize_path(joined, root, root_cap)) return false;

    char metadata[BAA_SDK_PATH_CAP];
    count = snprintf(metadata, sizeof(metadata), "%s/%s", root, BAA_SDK_METADATA_FILE);
    return count > 0 && (size_t)count < sizeof(metadata) && sdk_file_exists(metadata);
}

static void sdk_json_child_path(FILE *out, const char *parent, const char *child,
                                const char *probe)
{
    char path[BAA_SDK_PATH_CAP];
    char probe_path[BAA_SDK_PATH_CAP];
    int count = parent ? snprintf(path, sizeof(path), "%s/%s", parent, child) : -1;
    int probe_count = parent ? snprintf(probe_path, sizeof(probe_path), "%s/%s/%s",
                                        parent, child, probe)
                             : -1;
    if (count <= 0 || (size_t)count >= sizeof(path) ||
        probe_count <= 0 || (size_t)probe_count >= sizeof(probe_path) ||
        !sdk_file_exists(probe_path))
    {
        fputs("null", out);
        return;
    }
    sdk_json_path(out, path);
}

bool driver_sdk_info_json_write(FILE *out)
{
    const BaaTarget *host = baa_target_host_default();
    const char *executable_dir = driver_toolchain_get_executable_dir();
    const char *install_home = driver_toolchain_get_install_home();
    const char *bundled_linker = driver_toolchain_get_bundled_linker();
    const char *nazm_version = driver_nazm_embedded_version();

    char sdk_root[BAA_SDK_PATH_CAP];
    bool in_sdk = sdk_find_root(executable_dir, sdk_root, sizeof(sdk_root));

    fputs("{\n", out);
    fputs("  \"schema_version\": \"baa-sdk-info-v1\",\n", out);
    fputs("  \"compiler_version\": ", out);
    sdk_json_string(out, BAA_VERSION);
    fputs(",\n  \"host_target\": ", out);
    sdk_json_string(out, host ? host->name : NULL);
    fputs(",\n  \"executable_dir\": ", out);
    sdk_json_path(out, executable_dir);
    fputs(",\n  \"sdk_root\": ", out);
    if (in_sdk) sdk_json_string(out, sdk_root);
    else fputs("null", out);
    fputs(",\n  \"install_home\": ", out);
    sdk_json_path(out, install_home);
    fputs(",\n  \"stdlib_dir\": ", out);
    sdk_json_child_path(out, install_home, "stdlib", "baalib.baahd");
    fputs(",\n  \"targets_dir\": ", out);
    sdk_json_child_path(out, install_home, "targets", "x86_64-windows.json");
    fputs(",\n  \"runtime_library\": ", out);
    sdk_json_path(out, driver_toolchain_get_runtime_library());

    fputs(",\n  \"assembler\": {\n    \"kind\": ", out);
    sdk_json_string(out, nazm_version ? "nazm-embedded" : "external");
    fputs(",\n    \"default\": ", out);
    fputs(driver_nazm_embedded_is_default() ? "true" : "false", out);
    fputs(",\n    \"version\": ", out);
    sdk_json_string(out, nazm_version);
    fputs(",\n    \"revision\": ", out);
    sdk_json_string(out, nazm_version ? driver_nazm_embedded_revision() : NULL);
    fputs("\n  }", out);

    // أداة الربط مكوّن قابل للاستبدال: kind يسمّيها ولا تفترض الصيغة GCC.
    fputs(",\n  \"linker\": {\n    \"kind\": \"gcc\",\n    \"bundled\": ", out);
    fputs(bundled_linker ? "true" : "false", out);
    fputs(",\n    \"path\": ", out);
    if (bundled_linker) sdk_json_path(out, bundled_linker);
    else sdk_json_string(out, driver_toolchain_get_gcc_command());
    fputs(",\n    \"version\": ", out);
    sdk_json_string(out, driver_toolchain_get_bundled_linker_version());
    fputs("\n  }\n}\n", out);
    return !ferror(out);
}
