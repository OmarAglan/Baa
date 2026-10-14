/**
 * @file baa_frontend_internal.h
 * @brief ما تحتاجه baa_frontend.c من المصرّف حين يُبنى مكتبة داخل العملية.
 *
 * دوال "النسيان" تصفّر الحالة العامة لكل وحدة دون تحرير: بعد كل نداء
 * يحرر المتتبع كل ما خصصه المصرّف دفعة واحدة، فلا يجوز أن يبقى مؤشر عام
 * يشير إلى تلك الذاكرة أو عدّاد يقود النداء التالي إلى قراءتها.
 */

#ifndef BAA_FRONTEND_INTERNAL_H
#define BAA_FRONTEND_INTERNAL_H

#ifdef BAA_FRONTEND_LIBRARY

int baa_driver_main(int argc, char **argv);
void baa_driver_on_frontend_exit(void);
void driver_toolchain_set_frontend_executable_dir(const char *directory);

void analysis_forget_state(void);
void parser_forget_state(void);
void error_forget_state(void);
void ir_forget_state(void);

#endif /* BAA_FRONTEND_LIBRARY */

#endif /* BAA_FRONTEND_INTERNAL_H */
