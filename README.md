<div dir="rtl">

# باء

باء لغة نظم عربية، تُصرّف إلى برامج أصلية على ويندوز ولينكس.

الهدف بسيط: أن تكون كتابة البرامج القريبة من النظام ممكنة بالعربية، من غير أن نخسر صرامة المصرّف، الذاكرة الصريحة، أو قابلية قراءة الكود على مستوى منخفض.

```baa
#تضمين "baalib.baahd"

صحيح الرئيسية() {
    اطبع "أهلاً من باء".
    إرجع ٠.
}
```

## الفكرة

باء ليست غلافاً تعليمياً فوق لغة أخرى. هي مصرّف مكتوب حالياً بـ C11/GNU11 وينتقل تدريجياً إلى باء نفسها. المسار الداخلي واضح:

```text
المصدر -> lexer -> parser -> semantic analysis -> IR -> optimizer -> backend -> assembler/linker
```

اللغة عربية أولاً: الكلمات المفتاحية، الأرقام العربية، التشخيصات، والتوثيق العام. وفي الوقت نفسه تبقى لغة نظم: مؤشرات، أحجام أعداد صريحة، ملفات ترويسة، تصريف متعدد الملفات، وخرج أصلي.

## جرّبها

ويندوز:

```powershell
cmake -B build -G "MinGW Makefiles"
cmake --build build
build\baa.exe examples\hello_world.baa
.\out.exe
```

لينكس:

```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j
./build-linux/baa examples/hello_world.baa
./out
```

## ما الذي تدعمه الآن؟

- أهداف x86-64: ويندوز `COFF` ولينكس `ELF`.
- توليد تجميع عابر للأهداف عبر `-S`، أما التجميع والربط العابر فيعتمدان على أدوات المضيف.
- دوال، شروط، حلقات، `اختر`/`حالة`، مصفوفات، مؤشرات، تحويلات صريحة، مؤشرات دوال، وهياكل/اتحادات.
- أعداد صحيحة بعروض مختلفة، `عشري`، `حرف` UTF-8، `نص`، وثوابت خام `خام"..."` من نوع `ط٨*`.
- ملفات ترويسة `.baahd` عبر `#تضمين`.
- مكتبة قياسية صغيرة في `stdlib/baalib.baahd`.
- تشخيصات عربية واختبارات سلبية تثبت مواقع الأخطاء.

## أين وصل المشروع؟

المسار الحالي هو نقل المصرّف نفسه إلى باء خطوة خطوة. العمل النشط الآن في `v0.9.1.5` هو نقل المحلل اللفظي:

- يوجد المحلل اللفظي الإنتاجي في `src/frontend/lexer.baa`.
- يوجد عقد ترويسة باء له في `src/frontend/lexer.baahd`.
- تبقى `src/frontend/lexer.h` واجهة توافق C مؤقتة للـ parser إلى أن يبدأ نقل parser.
- حُذف lexer المكتوب بـ C وحاضنات الهجرة الخاصة به؛ بناء نسخة جديدة من المصدر يتطلب مصرّف باء موجوداً عبر `-DBAA_BOOTSTRAP_COMPILER=...`.

بعد lexer، تُطبّق نفس القاعدة على parser ثم IR ثم بقية الطبقات: عقد باء، حاضنة توافق، غلاف إنتاجي، حذف C، ثم تنظيف الحاضنة المؤقتة.

## أوامر مفيدة

```powershell
cmake -B build -G "MinGW Makefiles" -DBAA_BOOTSTRAP_COMPILER="path\to\baa.exe"
cmake --build build
build\baa.exe --help
build\baa.exe --version
build\baa.exe --dump-ast examples\hello_world.baa
build\baa.exe --dump-ir examples\hello_world.baa
python scripts\qa_run.py --mode quick
python scripts\qa_run.py --mode full
python scripts\qa_run.py --mode release
```

## اقرأ أكثر

- اللغة: `docs/LANGUAGE.md`
- دليل المستخدم: `docs/USER_GUIDE.md`
- الدروس العربية: `docs/TUTORIALS_AR.md`
- البنية الداخلية: `docs/INTERNALS.md`
- خطة التطوير: `ROADMAP.md`
- سجل التغييرات: `CHANGELOG.md`

## ملاحظة نضج

باء مشروع مصرّف قيد البناء، وليس وعداً باستقرار كامل لكل واجهات الداخل. إن أردت تجربة اللغة، ابدأ بالأمثلة والدروس. إن أردت المساهمة في المصرّف، ابدأ من `ROADMAP.md` وبوابات QA.

</div>
