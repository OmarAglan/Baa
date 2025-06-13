# Error Site Migration Report
**Generated:** D:\My Dev Life\Software Dev\Baa
**Total Error Sites:** 245

## Summary by Category
- **directive:** 79 sites
- **expression:** 15 sites
- **file:** 16 sites
- **syntax:** 135 sites

## Summary by File
- **preprocessor.c:** 18 sites
- **preprocessor_core.c:** 16 sites
- **preprocessor_directives.c:** 79 sites
- **preprocessor_expansion.c:** 44 sites
- **preprocessor_expr_eval.c:** 15 sites
- **preprocessor_line_processing.c:** 30 sites
- **preprocessor_utils.c:** 43 sites

## Detailed Breakdown
### preprocessor.c (18 sites)

**Line 32:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** وسيطات غير صالحة تم تمريرها إلى المعالج المسبق (المصدر أو اسم المصدر أو مؤشر رسالة الخطأ هو NULL)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطا...`

**Line 32:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** وسيطات غير صالحة تم تمريرها إلى المعالج المسبق (المصدر أو اسم المصدر أو مؤشر رسالة الخطأ هو NULL)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطا...`

**Line 41:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** وسيطات غير صالحة: نوع المصدر هو ملف ولكن مسار الملف هو NULL....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطا...`

**Line 41:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** وسيطات غير صالحة: نوع المصدر هو ملف ولكن مسار الملف هو NULL....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطا...`

**Line 50:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** وسيطات غير صالحة: نوع المصدر هو سلسلة ولكن مؤشر السلسلة هو NULL....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطا...`

**Line 50:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** وسيطات غير صالحة: نوع المصدر هو سلسلة ولكن مؤشر السلسلة هو NULL....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطا...`

**Line 152:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** فشل في تعريف الماكرو المدمج __التاريخ__....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تع...`

**Line 152:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** فشل في تعريف الماكرو المدمج __التاريخ__....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تع...`

**Line 161:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** فشل في تعريف الماكرو المدمج __الوقت__....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تع...`

**Line 161:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** فشل في تعريف الماكرو المدمج __الوقت__....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تع...`

**Line 170:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** فشل في تعريف الماكرو المدمج __الدالة__....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تع...`

**Line 170:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** فشل في تعريف الماكرو المدمج __الدالة__....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تع...`

**Line 181:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** فشل في تعريف الماكرو المدمج __إصدار_المعيار_باء__....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تع...`

**Line 181:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** فشل في تعريف الماكرو المدمج __إصدار_المعيار_باء__....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تع...`

**Line 204:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** فشل في دفع الموقع الأولي للملف (نفاد الذاكرة؟)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&file_start_loc, L"فشل في...`

**Line 204:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** فشل في دفع الموقع الأولي للملف (نفاد الذاكرة؟)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&file_start_loc, L"فشل في...`

**Line 229:** `legacy_error`
- **Function:** report_unterminated_conditional
- **Message:** فشل في دفع الموقع الأولي للسلسلة (نفاد الذاكرة؟)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&string_start_loc, L"فشل ...`

**Line 229:** `error_assignment`
- **Function:** report_unterminated_conditional
- **Message:** فشل في دفع الموقع الأولي للسلسلة (نفاد الذاكرة؟)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&string_start_loc, L"فشل ...`

### preprocessor_core.c (16 sites)

**Line 20:** `legacy_error`
- **Function:** unknown
- **Message:** فشل في الحصول على المسار المطلق لملف التضمين '%hs'....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في الحص...`

**Line 20:** `error_assignment`
- **Function:** unknown
- **Message:** فشل في الحصول على المسار المطلق لملف التضمين '%hs'....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في الحص...`

**Line 37:** `legacy_error`
- **Function:** unknown
- **Message:** تم اكتشاف تضمين دائري: الملف '%hs' مضمن بالفعل....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"تم اكتشاف ت...`

**Line 37:** `error_assignment`
- **Function:** unknown
- **Message:** تم اكتشاف تضمين دائري: الملف '%hs' مضمن بالفعل....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"تم اكتشاف ت...`

**Line 61:** `legacy_error`
- **Function:** unknown
- **Message:** فشل في تخصيص الذاكرة لمخزن الإخراج المؤقت....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 61:** `error_assignment`
- **Function:** unknown
- **Message:** فشل في تخصيص الذاكرة لمخزن الإخراج المؤقت....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 99:** `legacy_error`
- **Function:** unknown
- **Message:** فشل في تخصيص الذاكرة لسطر....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 99:** `error_assignment`
- **Function:** unknown
- **Message:** فشل في تخصيص الذاكرة لسطر....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 169:** `legacy_error`
- **Function:** unknown
- **Message:** فشل في إلحاق السطر بمخزن الإخراج المؤقت....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_loc, L"فشل في إل...`

**Line 169:** `error_assignment`
- **Function:** unknown
- **Message:** فشل في إلحاق السطر بمخزن الإخراج المؤقت....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_loc, L"فشل في إل...`

**Line 249:** `legacy_error`
- **Function:** unknown
- **Message:** فشل في تخصيص الذاكرة لمخزن الإخراج المؤقت للسلسلة....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 249:** `error_assignment`
- **Function:** unknown
- **Message:** فشل في تخصيص الذاكرة لمخزن الإخراج المؤقت للسلسلة....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 282:** `legacy_error`
- **Function:** unknown
- **Message:** فشل في تخصيص الذاكرة لسطر من السلسلة....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 282:** `error_assignment`
- **Function:** unknown
- **Message:** فشل في تخصيص الذاكرة لسطر من السلسلة....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 344:** `legacy_error`
- **Function:** unknown
- **Message:** فشل في إلحاق السطر بمخزن الإخراج المؤقت....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_loc, L"فشل في إل...`

**Line 344:** `error_assignment`
- **Function:** unknown
- **Message:** فشل في إلحاق السطر بمخزن الإخراج المؤقت....
- **Category:** file
- **Suggested Code:** PP_ERROR_FILE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_loc, L"فشل في إل...`

### preprocessor_directives.c (79 sites)

**Line 82:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #إذا غير صالح: التعبير مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 82:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #إذا غير صالح: التعبير مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 98:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** خطأ في تقييم تعبير #إذا....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"خطأ في ...`

**Line 98:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** خطأ في تقييم تعبير #إذا....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"خطأ في ...`

**Line 105:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في دفع الحالة الشرطية لـ #إذا (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 105:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في دفع الحالة الشرطية لـ #إذا (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 126:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #إذا_عرف غير صالح: اسم الماكرو مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 126:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #إذا_عرف غير صالح: اسم الماكرو مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 135:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم الماكرو في #إذا_عرف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 135:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم الماكرو في #إذا_عرف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 143:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في دفع الحالة الشرطية لـ #إذا_عرف (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 143:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في دفع الحالة الشرطية لـ #إذا_عرف (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 163:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #إذا_لم_يعرف غير صالح: اسم الماكرو مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 163:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #إذا_لم_يعرف غير صالح: اسم الماكرو مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 172:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم الماكرو في #إذا_لم_يعرف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 172:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم الماكرو في #إذا_لم_يعرف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 180:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في دفع الحالة الشرطية لـ #إذا_لم_يعرف (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 180:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في دفع الحالة الشرطية لـ #إذا_لم_يعرف (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 193:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** #نهاية_إذا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"#نهاية_...`

**Line 193:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** #نهاية_إذا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"#نهاية_...`

**Line 203:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** #إلا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"#إلا بد...`

**Line 203:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** #إلا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"#إلا بد...`

**Line 227:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** #وإلا_إذا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"#وإلا_إ...`

**Line 227:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** #وإلا_إذا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"#وإلا_إ...`

**Line 275:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #وإلا_إذا غير صالح: التعبير مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 275:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #وإلا_إذا غير صالح: التعبير مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 286:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** خطأ في تقييم تعبير #وإلا_إذا....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"خطأ في ...`

**Line 286:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** خطأ في تقييم تعبير #وإلا_إذا....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"خطأ في ...`

**Line 347:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تضمين غير صالح: يجب أن يتبع اسم الملف بـ \...
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 347:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تضمين غير صالح: يجب أن يتبع اسم الملف بـ \...
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 356:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تضمين غير صالح: مسار الملف فارغ....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 356:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تضمين غير صالح: مسار الملف فارغ....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 364:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لمسار التضمين....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 364:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لمسار التضمين....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 378:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لمسار التضمين (MB)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 378:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لمسار التضمين (MB)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 384:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تحويل مسار التضمين إلى UTF-8....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 384:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تحويل مسار التضمين إلى UTF-8....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 415:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تعذر العثور على ملف التضمين '<%hs>' في مسارات التضمين....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تعذر ال...`

**Line 415:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تعذر العثور على ملف التضمين '<%hs>' في مسارات التضمين....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تعذر ال...`

**Line 424:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في الحصول على دليل الملف الحالي....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 424:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في الحصول على دليل الملف الحالي....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 435:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة للمسار النسبي المدمج....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 435:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة للمسار النسبي المدمج....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 450:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في دفع موقع التضمين (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&include_loc, L"فشل في دف...`

**Line 450:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في دفع موقع التضمين (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&include_loc, L"فشل في دف...`

**Line 468:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في إلحاق المحتوى المضمن من '%hs'....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_loc, L"فشل في إل...`

**Line 468:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في إلحاق المحتوى المضمن من '%hs'....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_loc, L"فشل في إل...`

**Line 488:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تضمين غير صالح: علامة الاقتباس أو القوس الختامي مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 488:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تضمين غير صالح: علامة الاقتباس أو القوس الختامي مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 515:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: اسم الماكرو مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&name_error_loc, L"تنسيق ...`

**Line 515:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: اسم الماكرو مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&name_error_loc, L"تنسيق ...`

**Line 524:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم الماكرو في #تعريف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&name_error_loc, L"فشل في...`

**Line 524:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم الماكرو في #تعريف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&name_error_loc, L"فشل في...`

**Line 564:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: لا يمكن أن يتبع 'وسائط_إضافية' معاملات أخرى....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 564:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: لا يمكن أن يتبع 'وسائط_إضافية' معاملات أخرى....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 576:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: متوقع ',' أو ')' بين معاملات الماكرو الوظيفي....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 576:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: متوقع ',' أو ')' بين معاملات الماكرو الوظيفي....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 593:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: 'وسائط_إضافية' يجب أن تكون المعامل الأخير....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 593:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: 'وسائط_إضافية' يجب أن تكون المعامل الأخير....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 604:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: متوقع اسم معامل أو ')' أو 'وسائط_إضافية' بعد '('....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 604:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: متوقع اسم معامل أو ')' أو 'وسائط_إضافية' بعد '('....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 615:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: اسم معامل فارغ....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 615:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #تعريف غير صالح: اسم معامل فارغ....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"تنسيق...`

**Line 622:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم المعامل في #تعريف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"فشل ف...`

**Line 622:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم المعامل في #تعريف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"فشل ف...`

**Line 633:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في إعادة تخصيص الذاكرة لمعاملات الماكرو في #تعريف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"فشل ف...`

**Line 633:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في إعادة تخصيص الذاكرة لمعاملات الماكرو في #تعريف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&current_arg_loc, L"فشل ف...`

**Line 692:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في إضافة تعريف الماكرو '%ls' (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 692:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في إضافة تعريف الماكرو '%ls' (نفاد الذاكرة؟)....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 722:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #الغاء_تعريف غير صالح: اسم الماكرو مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 722:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** تنسيق #الغاء_تعريف غير صالح: اسم الماكرو مفقود....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"تنسيق #...`

**Line 731:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم الماكرو في #الغاء_تعريف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 731:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** فشل في تخصيص ذاكرة لاسم الماكرو في #الغاء_تعريف....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"فشل في ...`

**Line 767:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** %ls...
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"%ls", a...`

**Line 767:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** %ls...
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"%ls", a...`

**Line 796:** `legacy_warning`
- **Function:** handle_preprocessor_directive
- **Message:** %ls...
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + ?
- **Context:** `wchar_t *formatted_warning = format_preprocessor_warning_at_location(&directive_...`

**Line 807:** `legacy_error`
- **Function:** handle_preprocessor_directive
- **Message:** توجيه معالج مسبق غير معروف يبدأ بـ '#'....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 1
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"توجيه م...`

**Line 807:** `error_assignment`
- **Function:** handle_preprocessor_directive
- **Message:** توجيه معالج مسبق غير معروف يبدأ بـ '#'....
- **Category:** directive
- **Suggested Code:** PP_ERROR_DIRECTIVE_BASE + 1
- **Context:** `*error_message = format_preprocessor_error_at_location(&directive_loc, L"توجيه م...`

### preprocessor_expansion.c (44 sites)

**Line 58:** `legacy_error`
- **Function:** stringify_argument
- **Message:** فشل في تخصيص ذاكرة مؤقتة لتسلسل الوسيطة....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 58:** `error_assignment`
- **Function:** stringify_argument
- **Message:** فشل في تخصيص ذاكرة مؤقتة لتسلسل الوسيطة....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 98:** `legacy_error`
- **Function:** stringify_argument
- **Message:** فشل في إلحاق الوسيطة المتسلسلة للمخرج....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في إلحا...`

**Line 98:** `error_assignment`
- **Function:** stringify_argument
- **Message:** فشل في إلحاق الوسيطة المتسلسلة للمخرج....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في إلحا...`

**Line 182:** `legacy_error`
- **Function:** stringify_argument
- **Message:** تنسيق استدعاء الماكرو غير صالح: متوقع ',' أو ')' بين الوسيطات....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&comma_loc, L"تنسيق استدع...`

**Line 182:** `error_assignment`
- **Function:** stringify_argument
- **Message:** تنسيق استدعاء الماكرو غير صالح: متوقع ',' أو ')' بين الوسيطات....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&comma_loc, L"تنسيق استدع...`

**Line 280:** `legacy_error`
- **Function:** stringify_argument
- **Message:** أقواس غير متطابقة في وسيطات الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&arg_start_loc, L"أقواس غ...`

**Line 280:** `error_assignment`
- **Function:** stringify_argument
- **Message:** أقواس غير متطابقة في وسيطات الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&arg_start_loc, L"أقواس غ...`

**Line 286:** `legacy_error`
- **Function:** stringify_argument
- **Message:** علامة اقتباس غير منتهية في وسيطات الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&arg_start_loc, L"علامة ا...`

**Line 286:** `error_assignment`
- **Function:** stringify_argument
- **Message:** علامة اقتباس غير منتهية في وسيطات الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&arg_start_loc, L"علامة ا...`

**Line 322:** `legacy_error`
- **Function:** stringify_argument
- **Message:** فشل في تخصيص ذاكرة لوسيطة الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&arg_start_loc, L"فشل في ...`

**Line 322:** `error_assignment`
- **Function:** stringify_argument
- **Message:** فشل في تخصيص ذاكرة لوسيطة الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&arg_start_loc, L"فشل في ...`

**Line 335:** `legacy_error`
- **Function:** stringify_argument
- **Message:** فشل إعادة تخصيص وسيطات الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&arg_start_loc, L"فشل إعا...`

**Line 335:** `error_assignment`
- **Function:** stringify_argument
- **Message:** فشل إعادة تخصيص وسيطات الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&arg_start_loc, L"فشل إعا...`

**Line 348:** `legacy_error`
- **Function:** stringify_argument
- **Message:** قوس إغلاق ')' مفقود في استدعاء الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&call_loc, L"قوس إغلاق ')...`

**Line 348:** `error_assignment`
- **Function:** stringify_argument
- **Message:** قوس إغلاق ')' مفقود في استدعاء الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&call_loc, L"قوس إغلاق ')...`

**Line 368:** `legacy_error`
- **Function:** stringify_argument
- **Message:** تنسيق استدعاء الماكرو غير صالح: قوس الإغلاق ')' مفقود بعد الوسيطات....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"تنسيق استدع...`

**Line 368:** `error_assignment`
- **Function:** stringify_argument
- **Message:** تنسيق استدعاء الماكرو غير صالح: قوس الإغلاق ')' مفقود بعد الوسيطات....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + 2
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"تنسيق استدع...`

**Line 383:** `legacy_error`
- **Function:** stringify_argument
- **Message:** فشل تخصيص __VA_ARGS__ فارغ....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&va_alloc_loc, L"فشل تخصي...`

**Line 383:** `error_assignment`
- **Function:** stringify_argument
- **Message:** فشل تخصيص __VA_ARGS__ فارغ....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&va_alloc_loc, L"فشل تخصي...`

**Line 394:** `legacy_error`
- **Function:** stringify_argument
- **Message:** فشل نسخ __VA_ARGS__ فارغ....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&va_dup_loc, L"فشل نسخ __...`

**Line 394:** `error_assignment`
- **Function:** stringify_argument
- **Message:** فشل نسخ __VA_ARGS__ فارغ....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&va_dup_loc, L"فشل نسخ __...`

**Line 405:** `legacy_error`
- **Function:** stringify_argument
- **Message:** عدد وسيطات غير صحيح للماكرو '%ls' (متوقع %zu، تم الحصول على %zu)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + 3
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"عدد وسيطات ...`

**Line 405:** `error_assignment`
- **Function:** stringify_argument
- **Message:** عدد وسيطات غير صحيح للماكرو '%ls' (متوقع %zu، تم الحصول على %zu)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + 3
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"عدد وسيطات ...`

**Line 419:** `legacy_error`
- **Function:** stringify_argument
- **Message:** فشل في تخصيص مصفوفة فارغة للماكرو بدون معاملات '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 419:** `error_assignment`
- **Function:** stringify_argument
- **Message:** فشل في تخصيص مصفوفة فارغة للماكرو بدون معاملات '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 452:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** فشل تهيئة مخزن الرمز المعلق....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تهيئة مخزن الر...`

**Line 452:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** فشل تهيئة مخزن الرمز المعلق....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تهيئة مخزن الر...`

**Line 489:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** ## في موقع غير صالح بـ '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"## في موقع غير صال...`

**Line 489:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** ## في موقع غير صالح بـ '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"## في موقع غير صال...`

**Line 517:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** ## يجب أن يتبعه معرف أو رقم أو __وسائط_متغيرة__ في '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"## يجب أن يتبعه مع...`

**Line 517:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** ## يجب أن يتبعه معرف أو رقم أو __وسائط_متغيرة__ في '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"## يجب أن يتبعه مع...`

**Line 526:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** فشل تخصيص RHS لـ ##....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص RHS لـ #...`

**Line 526:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** فشل تخصيص RHS لـ ##....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص RHS لـ #...`

**Line 562:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** فشل تخصيص قيمة RHS لـ ##....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص قيمة RHS...`

**Line 562:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** فشل تخصيص قيمة RHS لـ ##....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص قيمة RHS...`

**Line 585:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** فشل إلحاق قيمة RHS لـ ##....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل إلحاق قيمة RHS...`

**Line 585:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** فشل إلحاق قيمة RHS لـ ##....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل إلحاق قيمة RHS...`

**Line 638:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** فشل تخصيص معرف لـ #....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص معرف لـ ...`

**Line 638:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** فشل تخصيص معرف لـ #....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص معرف لـ ...`

**Line 687:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** فشل تخصيص معرف بنص ماكرو '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص معرف بنص...`

**Line 687:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** فشل تخصيص معرف بنص ماكرو '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص معرف بنص...`

**Line 748:** `legacy_error`
- **Function:** substitute_macro_body
- **Message:** فشل إلحاق الرمز المعلق الأخير في '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل إلحاق الرمز ال...`

**Line 748:** `error_assignment`
- **Function:** substitute_macro_body
- **Message:** فشل إلحاق الرمز المعلق الأخير في '%ls'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&el, L"فشل إلحاق الرمز ال...`

### preprocessor_expr_eval.c (15 sites)

**Line 32:** `legacy_error`
- **Function:** make_error_token
- **Message:** خطأ في مقيم التعبير (لا يوجد سياق): %ls...
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*tz->error_message = format_preprocessor_error_at_location(&generic_loc, L"خطأ ف...`

**Line 318:** `legacy_error`
- **Function:** get_token_precedence
- **Message:** فشل تهيئة مخزن الإدخال لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل تهيئة م...`

**Line 318:** `error_assignment`
- **Function:** get_token_precedence
- **Message:** فشل تهيئة مخزن الإدخال لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل تهيئة م...`

**Line 326:** `legacy_error`
- **Function:** get_token_precedence
- **Message:** فشل نسخ التعبير إلى مخزن الإدخال لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل نسخ الت...`

**Line 326:** `error_assignment`
- **Function:** get_token_precedence
- **Message:** فشل نسخ التعبير إلى مخزن الإدخال لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل نسخ الت...`

**Line 342:** `legacy_error`
- **Function:** get_token_precedence
- **Message:** فشل تهيئة مخزن الإخراج لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل تهيئة م...`

**Line 342:** `error_assignment`
- **Function:** get_token_precedence
- **Message:** فشل تهيئة مخزن الإخراج لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل تهيئة م...`

**Line 359:** `legacy_error`
- **Function:** get_token_precedence
- **Message:** فشل إعادة تهيئة مخزن الإدخال لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل إعادة ت...`

**Line 359:** `error_assignment`
- **Function:** get_token_precedence
- **Message:** فشل إعادة تهيئة مخزن الإدخال لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل إعادة ت...`

**Line 371:** `legacy_error`
- **Function:** get_token_precedence
- **Message:** فشل نسخ إلى مخزن الإدخال لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل نسخ إلى...`

**Line 371:** `error_assignment`
- **Function:** get_token_precedence
- **Message:** فشل نسخ إلى مخزن الإدخال لتوسيع تعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل نسخ إلى...`

**Line 385:** `legacy_error`
- **Function:** get_token_precedence
- **Message:** تم تجاوز الحد الأقصى لمرات إعادة فحص الماكرو لتعبير #إذا (%d)....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"تم تجاوز ال...`

**Line 385:** `error_assignment`
- **Function:** get_token_precedence
- **Message:** تم تجاوز الحد الأقصى لمرات إعادة فحص الماكرو لتعبير #إذا (%d)....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"تم تجاوز ال...`

**Line 398:** `legacy_error`
- **Function:** get_token_precedence
- **Message:** فشل في تخصيص الذاكرة للسلسلة النهائية لتعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 398:** `error_assignment`
- **Function:** get_token_precedence
- **Message:** فشل في تخصيص الذاكرة للسلسلة النهائية لتعبير #إذا....
- **Category:** expression
- **Suggested Code:** PP_ERROR_EXPRESSION_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

### preprocessor_line_processing.c (30 sites)

**Line 39:** `legacy_error`
- **Function:** scan_and_expand_macros_for_expressions
- **Message:** فشل في تخصيص ذاكرة للمعرف للاستبدال....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في...`

**Line 39:** `error_assignment`
- **Function:** scan_and_expand_macros_for_expressions
- **Message:** فشل في تخصيص ذاكرة للمعرف للاستبدال....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في...`

**Line 236:** `legacy_error`
- **Function:** scan_and_expand_macros_for_expressions
- **Message:** فشل في دفع موقع استدعاء الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"ف...`

**Line 236:** `error_assignment`
- **Function:** scan_and_expand_macros_for_expressions
- **Message:** فشل في دفع موقع استدعاء الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"ف...`

**Line 243:** `legacy_error`
- **Function:** scan_and_expand_macros_for_expressions
- **Message:** فشل في دفع الماكرو '%ls' إلى مكدس التوسيع....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"ف...`

**Line 243:** `error_assignment`
- **Function:** scan_and_expand_macros_for_expressions
- **Message:** فشل في دفع الماكرو '%ls' إلى مكدس التوسيع....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"ف...`

**Line 254:** `legacy_error`
- **Function:** scan_and_expand_macros_for_expressions
- **Message:** فشل تهيئة مخزن التوسيع....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&err_loc_init, L"فشل تهيئ...`

**Line 254:** `error_assignment`
- **Function:** scan_and_expand_macros_for_expressions
- **Message:** فشل تهيئة مخزن التوسيع....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&err_loc_init, L"فشل تهيئ...`

**Line 417:** `legacy_error`
- **Function:** scan_and_substitute_macros_one_pass
- **Message:** فشل في تخصيص ذاكرة للمعرف للاستبدال....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في...`

**Line 417:** `error_assignment`
- **Function:** scan_and_substitute_macros_one_pass
- **Message:** فشل في تخصيص ذاكرة للمعرف للاستبدال....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في...`

**Line 615:** `legacy_error`
- **Function:** scan_and_substitute_macros_one_pass
- **Message:** فشل في دفع موقع استدعاء الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"ف...`

**Line 615:** `error_assignment`
- **Function:** scan_and_substitute_macros_one_pass
- **Message:** فشل في دفع موقع استدعاء الماكرو....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"ف...`

**Line 622:** `legacy_error`
- **Function:** scan_and_substitute_macros_one_pass
- **Message:** فشل في دفع الماكرو '%ls' إلى مكدس التوسيع....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"ف...`

**Line 622:** `error_assignment`
- **Function:** scan_and_substitute_macros_one_pass
- **Message:** فشل في دفع الماكرو '%ls' إلى مكدس التوسيع....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"ف...`

**Line 633:** `legacy_error`
- **Function:** scan_and_substitute_macros_one_pass
- **Message:** فشل تهيئة مخزن التوسيع....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&err_loc_init, L"فشل تهيئ...`

**Line 633:** `error_assignment`
- **Function:** scan_and_substitute_macros_one_pass
- **Message:** فشل تهيئة مخزن التوسيع....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&err_loc_init, L"فشل تهيئ...`

**Line 773:** `legacy_error`
- **Function:** process_code_line_for_macros
- **Message:** فشل في تهيئة المخزن المؤقت لسطر المعالجة (الإدخال)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في تهيئة...`

**Line 773:** `error_assignment`
- **Function:** process_code_line_for_macros
- **Message:** فشل في تهيئة المخزن المؤقت لسطر المعالجة (الإدخال)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في تهيئة...`

**Line 779:** `legacy_error`
- **Function:** process_code_line_for_macros
- **Message:** فشل في نسخ السطر الأولي إلى مخزن المعالجة (الإدخال)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في نسخ ا...`

**Line 779:** `error_assignment`
- **Function:** process_code_line_for_macros
- **Message:** فشل في نسخ السطر الأولي إلى مخزن المعالجة (الإدخال)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في نسخ ا...`

**Line 796:** `legacy_error`
- **Function:** process_code_line_for_macros
- **Message:** فشل في تهيئة المخزن المؤقت لجولة الفحص (الإخراج)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في تهيئة...`

**Line 796:** `error_assignment`
- **Function:** process_code_line_for_macros
- **Message:** فشل في تهيئة المخزن المؤقت لجولة الفحص (الإخراج)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في تهيئة...`

**Line 815:** `legacy_error`
- **Function:** process_code_line_for_macros
- **Message:** فشل في إعادة تهيئة مخزن الإدخال للجولة التالية....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في إعادة...`

**Line 815:** `error_assignment`
- **Function:** process_code_line_for_macros
- **Message:** فشل في إعادة تهيئة مخزن الإدخال للجولة التالية....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في إعادة...`

**Line 825:** `legacy_error`
- **Function:** process_code_line_for_macros
- **Message:** فشل في نسخ محتوى الجولة إلى مخزن الإدخال التالي....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في نسخ م...`

**Line 825:** `error_assignment`
- **Function:** process_code_line_for_macros
- **Message:** فشل في نسخ محتوى الجولة إلى مخزن الإدخال التالي....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في نسخ م...`

**Line 841:** `legacy_error`
- **Function:** process_code_line_for_macros
- **Message:** تم تجاوز الحد الأقصى لمرات إعادة فحص الماكرو لسطر واحد (%d)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&err_loc_data, L"تم تجاوز...`

**Line 841:** `error_assignment`
- **Function:** process_code_line_for_macros
- **Message:** تم تجاوز الحد الأقصى لمرات إعادة فحص الماكرو لسطر واحد (%d)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&err_loc_data, L"تم تجاوز...`

**Line 855:** `legacy_error`
- **Function:** process_code_line_for_macros
- **Message:** فشل في إلحاق السطر النهائي بمخزن الإخراج المؤقت....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في إلحاق...`

**Line 855:** `error_assignment`
- **Function:** process_code_line_for_macros
- **Message:** فشل في إلحاق السطر النهائي بمخزن الإخراج المؤقت....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في إلحاق...`

### preprocessor_utils.c (43 sites)

**Line 443:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تحويل مسار الملف '%hs' إلى UTF-16....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحوي...`

**Line 443:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تحويل مسار الملف '%hs' إلى UTF-16....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحوي...`

**Line 450:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة لمسار الملف (UTF-16) '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 450:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة لمسار الملف (UTF-16) '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 464:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في فتح الملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في فتح ...`

**Line 464:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في فتح الملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في فتح ...`

**Line 506:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** خطأ في حساب حجم الملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"خطأ في حساب...`

**Line 506:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** خطأ في حساب حجم الملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"خطأ في حساب...`

**Line 517:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة لملف فارغ '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 517:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة لملف فارغ '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 531:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** حجم محتوى الملف '%hs' (UTF-16LE) ليس من مضاعفات حجم wchar_t....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"حجم محتوى ا...`

**Line 531:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** حجم محتوى الملف '%hs' (UTF-16LE) ليس من مضاعفات حجم wchar_t....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"حجم محتوى ا...`

**Line 540:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة لمحتوى الملف (UTF-16LE) '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 540:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة لمحتوى الملف (UTF-16LE) '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 548:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في قراءة محتوى الملف (UTF-16LE) بالكامل من '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في قراء...`

**Line 548:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في قراءة محتوى الملف (UTF-16LE) بالكامل من '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في قراء...`

**Line 562:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة لمحتوى الملف (UTF-8) '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 562:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة لمحتوى الملف (UTF-8) '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 570:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في قراءة محتوى الملف (UTF-8) بالكامل من '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في قراء...`

**Line 570:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في قراءة محتوى الملف (UTF-8) بالكامل من '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في قراء...`

**Line 583:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في حساب حجم التحويل من UTF-8 للملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في حساب...`

**Line 583:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في حساب حجم التحويل من UTF-8 للملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في حساب...`

**Line 592:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة للمخزن المؤقت wchar_t للملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 592:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة للمخزن المؤقت wchar_t للملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 601:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تحويل محتوى الملف '%hs' من UTF-8 إلى wchar_t....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحوي...`

**Line 601:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تحويل محتوى الملف '%hs' من UTF-8 إلى wchar_t....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحوي...`

**Line 614:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** تسلسل بايت UTF-8 غير صالح في الملف '%hs' أو فشل في تحديد حجم التحويل....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"تسلسل بايت ...`

**Line 614:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** تسلسل بايت UTF-8 غير صالح في الملف '%hs' أو فشل في تحديد حجم التحويل....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"تسلسل بايت ...`

**Line 624:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة للمخزن المؤقت wchar_t للملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 624:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تخصيص الذاكرة للمخزن المؤقت wchar_t للملف '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصي...`

**Line 633:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تحويل محتوى الملف '%hs' من UTF-8 إلى wchar_t (تسلسل غير صالح؟)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحوي...`

**Line 633:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** فشل في تحويل محتوى الملف '%hs' من UTF-8 إلى wchar_t (تسلسل غير صالح؟)....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحوي...`

**Line 647:** `legacy_error`
- **Function:** free_dynamic_buffer
- **Message:** خطأ داخلي: ترميز ملف غير معروف تم اكتشافه لـ '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + 1
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"خطأ داخلي: ...`

**Line 647:** `error_assignment`
- **Function:** free_dynamic_buffer
- **Message:** خطأ داخلي: ترميز ملف غير معروف تم اكتشافه لـ '%hs'....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + 1
- **Context:** `*error_message = format_preprocessor_error_at_location(&error_loc, L"خطأ داخلي: ...`

**Line 802:** `legacy_error`
- **Function:** add_preprocessor_diagnostic
- **Message:** فشل داخلي: خطأ في تنسيق رسالة التشخيص الأصلية....
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `wchar_t *fallback_msg = format_preprocessor_error_at_location(loc, L"فشل داخلي: ...`

**Line 810:** `legacy_diagnostic`
- **Function:** add_preprocessor_diagnostic
- **Message:** %ls...
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `// add_preprocessor_diagnostic(pp_state, loc, true, L"%ls", fallback_msg); // Th...`

**Line 832:** `legacy_error`
- **Function:** add_preprocessor_diagnostic
- **Message:** %ls...
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `wchar_t *full_diagnostic_message = is_error ? format_preprocessor_error_at_locat...`

**Line 832:** `legacy_warning`
- **Function:** add_preprocessor_diagnostic
- **Message:** %ls...
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `wchar_t *full_diagnostic_message = is_error ? format_preprocessor_error_at_locat...`

**Line 960:** `legacy_error`
- **Function:** add_preprocessor_diagnostic_ex
- **Message:** خطأ فادح: %ls...
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `full_message = format_preprocessor_error_at_location(loc, L"خطأ فادح: %ls", form...`

**Line 963:** `legacy_error`
- **Function:** add_preprocessor_diagnostic_ex
- **Message:** %ls...
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `full_message = format_preprocessor_error_at_location(loc, L"%ls", formatted_mess...`

**Line 966:** `legacy_warning`
- **Function:** add_preprocessor_diagnostic_ex
- **Message:** %ls...
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `full_message = format_preprocessor_warning_at_location(loc, L"%ls", formatted_me...`

**Line 969:** `legacy_warning`
- **Function:** add_preprocessor_diagnostic_ex
- **Message:** ملاحظة: %ls...
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `full_message = format_preprocessor_warning_at_location(loc, L"ملاحظة: %ls", form...`

**Line 972:** `legacy_error`
- **Function:** add_preprocessor_diagnostic_ex
- **Message:** %ls...
- **Category:** syntax
- **Suggested Code:** PP_ERROR_SYNTAX_BASE + ?
- **Context:** `full_message = format_preprocessor_error_at_location(loc, L"%ls", formatted_mess...`

## Migration Recommendations

### High Priority Files (Week 2-3)
- **preprocessor_directives.c:** 79 sites - Core functionality
- **preprocessor_expansion.c:** 44 sites - Core functionality
- **preprocessor_expr_eval.c:** 15 sites - Core functionality

### Medium Priority Files (Week 4)
- **preprocessor_line_processing.c:** 30 sites - Support functionality
- **preprocessor_core.c:** 16 sites - Support functionality