# أمثلة باء الجاهزة (v0.4.4)

## الملفات

- `examples/hello_world.baa` — أبسط برنامج.
- `examples/math_and_format.baa` — تنسيق عربي + فواصل عائمة (`%ع/%أ`) + دوال رياضيات.
- `examples/error_handling_demo.baa` — `تأكد/توقف_فوري` وجسر `errno`.
- `examples/file_copy_small.baa` — نسخ ملف بايت-بايت مع رموز خطأ قياسية.

## التشغيل

- ويندوز:
  - `build\baa.exe examples\hello_world.baa -o build\hello.exe`
  - `build\hello.exe`
- لينكس:
  - `./build-linux/baa examples/hello_world.baa -o build-linux/hello`
  - `./build-linux/hello`
