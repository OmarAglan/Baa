# سلسلة دروس باء (v0.4.4)

هذه السلسلة تقدم مساراً عملياً قصيراً من "أول برنامج" حتى الميزات المتقدمة.

## الدرس 1: أول برنامج

- افتح `examples/hello_world.baa`.
- ابنِ المصرّف:
  - ويندوز: `cmake -B build -G "MinGW Makefiles" && cmake --build build`
  - لينكس: `cmake -B build-linux -DCMAKE_BUILD_TYPE=Release && cmake --build build-linux -j`
- صرّف وشغّل:
  - ويندوز: `build\baa.exe examples\hello_world.baa -o build\hello.exe`
  - لينكس: `./build-linux/baa examples/hello_world.baa -o build-linux/hello`

## الدرس 2: المكتبة القياسية + التنسيق

- افتح `examples/math_and_format.baa`.
- جرّب `نسق` مع `%ع` و`%أ`.
- تأكد من تحرير النص الناتج عبر `حرر_نص`.

## الدرس 3: معالجة الأخطاء (v0.4.3)

- افتح `examples/error_handling_demo.baa`.
- لاحظ استخدام:
  - `تأكد(شرط، رسالة)`
  - `كود_خطأ_النظام()` و`ضبط_كود_خطأ_النظام(...)`
  - رموز الخطأ القياسية من `stdlib/baalib.baahd`.

## الدرس 4: الملفات

- افتح `examples/file_copy_small.baa`.
- شغّل المثال مع مسارات ملفات مناسبة.
- راقب مسار الفشل وإرجاع رموز الخطأ القياسية.

## الدرس 5: أدوات الجودة

- تشغيل سريع: `python scripts/qa_run.py --mode quick`
- تشغيل شامل: `python scripts/qa_run.py --mode full`
- فحص IR: أضف `--dump-ir` أو `--verify`.

## الخطوة التالية

- راجع `docs/PERFORMANCE.md` لضبط الأداء والقياس.
- راجع `ROADMAP.md` لمرحلة `v0.5.x`.
