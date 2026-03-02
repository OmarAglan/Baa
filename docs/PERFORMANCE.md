# دليل الأداء والقياس (v0.4.4)

الهدف: قياس وقت التصريف بشكل متكرر وقابل للمقارنة، ثم تحسين النقاط الأعلى كلفة.

## 1) قياس سريع لكل مرحلة

استخدم `--time-phases` مع `-S` لعزل وقت المصرّف:

- ويندوز:
  - `build\baa.exe -O2 --verify --time-phases -S tests\integration\backend\backend_test.baa -o build\perf_test.s`
- لينكس:
  - `./build-linux/baa -O2 --verify --time-phases -S tests/integration/backend/backend_test.baa -o build-linux/perf_test.s`

## 2) تشغيل مجموعة القياس

- `python scripts/bench.py --mode compile_s --opt O2 --verify --time-phases`
- `python scripts/bench.py --mode all`

## 3) قواعد التحسين

- ابدأ بالملفات الأعلى كلفة (زمنياً) حسب خرج `[TIME]`.
- لا تغيّر السلوك اللغوي أثناء التحسين.
- بعد كل تحسين:
  - شغّل `python scripts/qa_run.py --mode quick`
  - ثم `python scripts/qa_run.py --mode full`

## 4) بوابة القبول

لا يُقبل تحسين أداء إذا:

- كسر `--verify-ir` أو `--verify-ssa`.
- غيّر التشخيصات المطلوبة في الاختبارات السلبية.
- أدى لتراجع ملحوظ في سرعة الاختبارات المعتادة.
