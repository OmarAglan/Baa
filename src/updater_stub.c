/**
 * @file updater_stub.c
 * @brief محدّث بديل للأنظمة غير Windows.
 *
 * حالياً، ميزة `update` تعتمد على واجهات Windows (UrlMon/WinINet).
 * هذا الملف يوفر تطبيقاً بسيطاً حتى يتم نقل المحدّث إلى حل متعدد الأهداف.
 */

#include <stdio.h>

void run_updater(void)
{
    fprintf(stderr, "ملاحظة: أمر update غير مدعوم على هذا النظام حالياً.\n");
}
