/**
 * @file updater.c
 * @brief نظام التحديث التلقائي (Self-Updater).
 * @version 0.2.3
 */

#include "baa.h"
#include <windows.h>
#include <urlmon.h>
#include <wininet.h>
#include <ctype.h>

// -----------------------------------------------------------------------------
// ملاحظة: روابط التحديث (يمكن تعديلها لتناسب مسار النشر الخاص بك)
// مثال: "https://username.github.io/baa/version.txt"
// -----------------------------------------------------------------------------
#define UPDATE_URL_VERSION "https://omardev.engineer/baaInstaller/version.txt"
#define UPDATE_URL_SETUP   "https://omardev.engineer/baaInstaller/baa_setup.exe"

// ربط مكتبة urlmon و wininet
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")

/**
 * @brief قراءة الإصدار من الخادم.
 * @return 1 إذا وجد تحديث، 0 إذا لم يوجد.
 */
static int parse_version_parts_5(const char* s, int out_parts[5]) {
    if (!s || !out_parts) return 0;

    for (int i = 0; i < 5; i++) out_parts[i] = 0;

    const char* p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;

    for (int part = 0; part < 5; part++) {
        if (!isdigit((unsigned char)*p)) {
            // لا نقبل نصاً بلا أرقام كبداية، ونرفض أي محرف غير رقمي داخل الإصدار.
            if (part == 0) return 0;
            return (*p == '\0') ? 1 : 0;
        }

        char* end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p) return 0;
        if (v < 0) return 0;
        if (v > 1000000L) return 0;

        out_parts[part] = (int)v;
        p = end;

        if (*p == '.') {
            p++;
            if (!*p) return 0;
            continue;
        }
        break;
    }

    while (*p && isspace((unsigned char)*p)) p++;
    return *p == '\0';
}

static int compare_version_parts_5(const int a[5], const int b[5]) {
    for (int i = 0; i < 5; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

int check_for_updates() {
    IStream* stream;
    char buffer[128];
    unsigned long bytesRead;

    printf("[Update] Checking for updates...\n");
    printf("[Update] Connecting to: %s\n", UPDATE_URL_VERSION);

    // 1. تنزيل ملف الإصدار إلى الذاكرة
    // مسح التخزين المؤقت (Cache) لضمان الحصول على أحدث نسخة
    DeleteUrlCacheEntryA(UPDATE_URL_VERSION);

    HRESULT hr = URLOpenBlockingStreamA(0, UPDATE_URL_VERSION, &stream, 0, 0);
    if (hr != S_OK) {
        printf("[Error] Failed to connect to update server (Code: 0x%x).\n", (unsigned int)hr);
        return 0;
    }

    // 2. قراءة البيانات
    stream->lpVtbl->Read(stream, buffer, sizeof(buffer) - 1, &bytesRead);
    buffer[bytesRead] = '\0';
    stream->lpVtbl->Release(stream);

    // 3. تنظيف النص (إزالة الأسطر الجديدة والمسافات)
    char* newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buffer, '\r');
    if (newline) *newline = '\0';

    printf("[Update] Latest version: %s\n", buffer);
    printf("[Update] Current version: %s\n", BAA_VERSION);

    // 4. مقارنة الإصدارات (مقارنة قوية تدعم حتى 5 أجزاء: X.Y.Z.A.B)
    int remote_v[5];
    int local_v[5];

    if (!parse_version_parts_5(buffer, remote_v)) {
        printf("[Error] Failed to parse remote version string.\n");
        return 0; // افتراضي آمن
    }
    if (!parse_version_parts_5(BAA_VERSION, local_v)) {
        printf("[Error] Failed to parse local version string.\n");
        return 0; // افتراضي آمن
    }

    bool update_needed = compare_version_parts_5(remote_v, local_v) > 0;

    if (update_needed) {
        printf("[Update] New version available!\n");
        return 1;
    }

    printf("[Update] You are up to date.\n");
    return 0;
}

/**
 * @brief تنزيل وتشغيل المثبت.
 */
void perform_update() {
    char tempPath[MAX_PATH];
    char setupPath[MAX_PATH];

    // الحصول على مسار المجلد المؤقت
    GetTempPathA(MAX_PATH, tempPath);
    int n = snprintf(setupPath, MAX_PATH, "%sbaa_setup.exe", tempPath);
    if (n < 0 || n >= MAX_PATH) {
        printf("[Error] Failed to build installer path.\n");
        return;
    }

    printf("[Update] Downloading installer to: %s\n", setupPath);

    // تنزيل الملف
    DeleteUrlCacheEntryA(UPDATE_URL_SETUP); // مسح الكاش
    HRESULT hr = URLDownloadToFileA(NULL, UPDATE_URL_SETUP, setupPath, 0, NULL);
    
    if (hr == S_OK) {
        printf("[Update] Download complete. Starting installer...\n");
        
        // تشغيل المثبت
        // SW_SHOW يعرض نافذة التثبيت
        ShellExecuteA(NULL, "open", setupPath, NULL, NULL, SW_SHOW);
        
        // إغلاق البرنامج الحالي ليتمكن المثبت من استبدال الملفات
        exit(0);
    } else {
        printf("[Error] Download failed (Error code: 0x%x)\n", (unsigned int)hr);
    }
}

/**
 * @brief نقطة الدخول العامة للتحديث.
 */
void run_updater() {
    if (check_for_updates()) {
        printf("Do you want to update? (y/n): ");
        char response = getchar();
        // تنظيف مدخلات لوحة المفاتيح
        while ((getchar()) != '\n'); 

        if (response == 'y' || response == 'Y') {
            perform_update();
        }
    }
}
