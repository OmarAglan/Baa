/**
 * @file updater.c
 * @brief نظام التحديث التلقائي (Self-Updater).
 * @version 0.2.3
 */

#include "baa.h"
#include <windows.h>
#include <urlmon.h>
#include <wininet.h>

// ---------------------------------------------------------
// ⚠️ TODO: Change these URLs to your GitHub Pages path
// Example: "https://username.github.io/baa/version.txt"
// ---------------------------------------------------------
#define UPDATE_URL_VERSION "https://omardev.engineer/baaInstaller/version.txt"
#define UPDATE_URL_SETUP   "https://omardev.engineer/baaInstaller/baa_setup.exe"

// ربط مكتبة urlmon و wininet
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")

/**
 * @brief قراءة الإصدار من الخادم.
 * @return 1 إذا وجد تحديث، 0 إذا لم يوجد.
 */
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

    // 4. مقارنة الإصدارات (مقارنة دلالية بسيطة)
    // نفترض الصيغة: X.Y.Z
    int v1_maj, v1_min, v1_patch;
    int v2_maj, v2_min, v2_patch;

    sscanf(buffer, "%d.%d.%d", &v1_maj, &v1_min, &v1_patch); // Remote
    sscanf(BAA_VERSION, "%d.%d.%d", &v2_maj, &v2_min, &v2_patch); // Local
    
    bool update_needed = false;
    if (v1_maj > v2_maj) update_needed = true;
    else if (v1_maj == v2_maj && v1_min > v2_min) update_needed = true;
    else if (v1_maj == v2_maj && v1_min == v2_min && v1_patch > v2_patch) update_needed = true;

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
    sprintf(setupPath, "%sbaa_setup.exe", tempPath);

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