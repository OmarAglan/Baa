# أمثلة باء الجاهزة (v0.6.5)

## الملفات

- `examples/hello_world.باء` — أبسط برنامج.
- `examples/math_and_format.باء` — تنسيق عربي + فواصل عائمة (`%ع/%أ`) + دوال رياضيات.
- `examples/error_handling_demo.باء` — `تأكد/توقف_فوري` وجسر `errno`.
- `examples/file_copy_small.باء` — نسخ ملف بايت-بايت مع رموز خطأ قياسية.

تتحقق بوابة QA من تجميع كل مثال عام عبر `tests/test_examples.py` باستخدام `-O2 --verify`.

## التشغيل

- ويندوز:
  - `build\baa.exe examples\hello_world.باء -o build\hello.exe`
  - `build\hello.exe`
- لينكس:
  - `./build-linux/baa examples/hello_world.باء -o build-linux/hello`
  - `./build-linux/hello`
