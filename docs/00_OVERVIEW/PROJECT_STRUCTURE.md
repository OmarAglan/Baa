# هيكل مشروع لغة باء (Baa Project Structure)

## نظرة عامة (Overview)

يوضح هذا المستند الهيكل التنظيمي لمجلدات وملفات مشروع مترجم لغة باء. يهدف هذا الهيكل إلى تحقيق الوضوح، الفصل بين الاهتمامات، وتسهيل عملية التطوير والصيانة.

## المجلدات الرئيسية (Main Directories)

```
baa/
├── .editorconfig            # إعدادات المحرر لتوحيد نمط الشيفرة
├── .gitignore               # الملفات والمجلدات التي يتجاهلها Git
├── CHANGELOG.md             # سجل التغييرات والإصدارات
├── CMakeLists.txt           # ملف بناء المشروع الرئيسي (CMake)
├── LICENSE                  # رخصة المشروع (MIT)
├── README.md                # معلومات عامة عن المشروع ومقدمة
│
├── cmake/                   # وحدات CMake المخصصة (Custom CMake modules)
│   └── BaaCompilerSettings.cmake # إعدادات المترجم المشتركة (مثال)
│
├── docs/                    # مجلد التوثيق (Documentation)
│   ├── AST.md               # (تصميم جديد) وثيقة تصميم شجرة النحو المجردة
│   ├── AST_ROADMAP.md       # (تصميم جديد) خارطة طريق تنفيذ شجرة النحو المجردة
│   ├── CMAKE_ROADMAP.md     # خارطة طريق تطوير نظام البناء CMake
│   ├── LEXER_ROADMAP.md     # خارطة طريق تطوير المحلل اللفظي
│   ├── LLVM_CODEGEN.md      # وثيقة تصميم توليد الشيفرة باستخدام LLVM
│   ├── LLVM_CODEGEN_ROADMAP.md # خارطة طريق تنفيذ توليد الشيفرة LLVM
│   ├── PARSER.md            # (تصميم جديد) وثيقة تصميم المحلل النحوي
│   ├── PARSER_ROADMAP.md    # (تصميم جديد) خارطة طريق تنفيذ المحلل النحوي
│   ├── PREPROCESSOR_ROADMAP.md # خارطة طريق تطوير المعالج المسبق
│   ├── SEMANTIC_ANALYSIS.md # (مخطط) وثيقة تصميم التحليل الدلالي
│   ├── SEMANTIC_ANALYSIS_ROADMAP.md # (مخطط) خارطة طريق التحليل الدلالي
│   ├── arabic_support.md    # تفاصيل دعم اللغة العربية
│   ├── architecture.md      # نظرة عامة على معمارية المترجم
│   ├── c_comparison.md      # مقارنة مع لغة C
│   ├── development.md       # دليل تطوير المشروع
│   ├── language.md          # مواصفات لغة باء
│   ├── lexer.md             # وثائق المحلل اللفظي
│   ├── preprocessor.md      # وثائق المعالج المسبق
│   ├── project_structure.md # هذا الملف (هيكل المشروع)
│   └── roadmap.md           # خارطة الطريق العامة للمشروع
│
├── include/                 # ملفات الرأس العامة للمكتبات (Public headers for libraries)
│   └── baa/                 # مساحة الاسم الرئيسية لمكتبات باء
│       ├── analysis/        # واجهات التحليل الدلالي وتحليل التدفق
│       │   ├── flow_analysis.h
│       │   └── flow_errors.h
│       ├── codegen/         # واجهات توليد الشيفرة
│       │   ├── codegen.h
│       │   └── llvm_codegen.h
│       ├── compiler.h       # واجهة المترجم الرئيسية (الدالة compile_baa_file)
│       ├── diagnostics/     # (مخطط، يعتمد على AST) واجهات نظام التشخيصات والأخطاء
│       │   └── diagnostics.h
│       ├── lexer/           # واجهات المحلل اللفظي
│       │   ├── lexer.h              # الواجهة العامة للمحلل اللفظي
│       │   ├── lexer_char_utils.h   # أدوات مساعدة للحروف (عامة إذا احتاجتها مكونات أخرى)
│       │   ├── lexer_internal.h     # (عادة لا يكون هنا، بل في src/lexer) تعريفات داخلية إذا كانت مشتركة بشكل استثنائي
│       │   └── token_scanners.h   # (عادة لا يكون هنا، بل في src/lexer)
│       ├── operators/       # واجهة نظام العمليات
│       │   └── operators.h
│       ├── preprocessor/    # واجهة المعالج المسبق
│       │   └── preprocessor.h
│       ├── types/           # واجهة نظام الأنواع
│       │   └── types.h
│       └── utils/           # واجهات الأدوات المساعدة
│           ├── errors.h
│           └── utils.h
│
├── src/                     # الشيفرة المصدرية للمكتبات والمترجم (Source code)
│   ├── CMakeLists.txt       # يضيف المجلدات الفرعية للمكونات كمكتبات
│   │
│   ├── analysis/            # تنفيذ التحليل الدلالي وتحليل التدفق
│   │   ├── CMakeLists.txt
│   │   ├── flow_analysis.c
│   │   └── flow_errors.c
│   │
│   ├── codegen/             # تنفيذ توليد الشيفرة
│   │   ├── CMakeLists.txt
│   │   ├── codegen.c        # المنطق العام لتوليد الشيفرة
│   │   ├── llvm_codegen.c   # تنفيذ توليد الشيفرة باستخدام LLVM (إذا كان LLVM مفعلاً)
│   │   └── llvm_stub.c      # تنفيذ بديل (stub) عند تعطيل LLVM
│   │
│   ├── compiler.c           # منطق الترجمة الرئيسي (جزء من مكتبة baa_compiler_lib)
│   │
│   ├── lexer/               # تنفيذ المحلل اللفظي
│   │   ├── CMakeLists.txt
│   │   ├── lexer.c              # المنطق الرئيسي للمحلل اللفظي
│   │   ├── lexer_char_utils.c   # تنفيذ أدوات الحروف
│   │   ├── number_parser.c      # محلل الأرقام (تحويل النص إلى قيمة)
│   │   └── token_scanners.c     # دوال مسح أنواع الرموز المختلفة
│   │
│   ├── main.c               # نقطة الدخول الرئيسية لبرنامج المترجم `baa`
│   │
│   ├── operators/           # تنفيذ نظام العمليات
│   │   ├── CMakeLists.txt
│   │   └── operators.c
│   │
│   ├── preprocessor/        # تنفيذ المعالج المسبق
│   │   ├── CMakeLists.txt
│   │   ├── preprocessor.c
│   │   ├── preprocessor_conditionals.c
│   │   ├── preprocessor_core.c
│   │   ├── preprocessor_directives.c
│   │   ├── preprocessor_expansion.c
│   │   ├── preprocessor_expr_eval.c
│   │   ├── preprocessor_internal.h    # تعريفات داخلية مشتركة للمعالج المسبق
│   │   ├── preprocessor_line_processing.c
│   │   ├── preprocessor_macros.c
│   │   └── preprocessor_utils.c
│   │
│   ├── types/               # تنفيذ نظام الأنواع
│   │   ├── CMakeLists.txt
│   │   └── types.c
│   │
│   └── utils/               # تنفيذ الأدوات المساعدة
│       ├── CMakeLists.txt
│       └── utils.c
│
├── tests/                   # الاختبارات (Unit and Integration Tests)
│   ├── CMakeLists.txt       # إعداد بيئة الاختبارات
│   │
│   ├── framework/           # إطار عمل بسيط للاختبارات
│   │   ├── test_framework.c
│   │   └── test_framework.h
│   │
│   ├── codegen_tests/       # اختبارات خاصة بتوليد الشيفرة (حالياً قد تعتمد على AST القديم)
│   │   ├── CMakeLists.txt
│   │   ├── llvm_codegen_test.c # (قد يحتاج لتحديث أو تعطيل مؤقت)
│   │   ├── llvm_test.c         # (قد يحتاج لتحديث أو تعطيل مؤقت)
│   │   └── test_codegen.c      # (قد يحتاج لتحديث أو تعطيل مؤقت)
│   │
│   ├── integration/         # اختبارات التكامل (مخطط لها)
│   │   └── CMakeLists.txt
│   │
│   ├── resources/           # ملفات الإدخال للاختبارات
│   │   ├── lexer_test_cases/
│   │   │   └── lexer_test_suite.baa
│   │   └── preprocessor_test_cases/
│   │       ├── include_test_header.baa
│   │       ├── nested_include.baa
│   │       └── preprocessor_test_all.baa
│   │
│   └── unit/                # اختبارات الوحدات لكل مكون
│       ├── CMakeLists.txt     # يضيف مجلدات اختبارات الوحدات الفرعية
│       ├── core/              # اختبارات المكونات الأساسية (الأنواع، العمليات)
│       │   ├── CMakeLists.txt
│       │   ├── test_operators.c # (قد يعتمد جزئيًا على AST/Parser القديم)
│       │   └── test_types.c
│       ├── lexer/
│       │   ├── CMakeLists.txt
│       │   └── ... (ملفات اختبار المحلل اللفظي)
│       ├── preprocessor/
│       │   ├── CMakeLists.txt
│       │   └── test_preprocessor.c
│       └── utils/
│           ├── CMakeLists.txt
│           └── test_utils.c
│
└── tools/                   # أدوات مساعدة للمطورين أو المستخدمين
    ├── baa_lexer_tester.c       # أداة اختبار المحلل اللفظي
    ├── baa_parser_tester.c      # (مخطط) أداة اختبار المحلل النحوي (تعتمد على المحلل الجديد)
    └── baa_preprocessor_tester.c # أداة اختبار المعالج المسبق
```

## ملاحظات على الهيكل (Notes on Structure)

* **`include/baa/`**: يحتوي على ملفات الرأس العامة (`.h`) التي تشكل الواجهة البرمجية (API) لكل مكتبة من مكونات المترجم. يجب أن تكون هذه الملفات مستقلة قدر الإمكان ولا تكشف التفاصيل الداخلية للتنفيذ.
* **`src/<component>/`**: كل مكون رئيسي للمترجم (مثل `lexer`, `preprocessor`, `types`) له مجلده الخاص داخل `src/`.
  * **`src/<component>/CMakeLists.txt`**: يعرف كيفية بناء المكتبة الثابتة (static library) الخاصة بالمكون (مثلاً `baa_lexer`, `baa_utils`).
  * **`src/<component>/*.c`**: ملفات الشيفرة المصدرية لتنفيذ المكون.
  * **`src/<component>/*_internal.h` (مثال: `preprocessor_internal.h`)**: ملفات رأس داخلية تستخدم لمشاركة التعريفات والدوال بين ملفات `.c` المختلفة *داخل نفس المكون فقط*. لا يجب تضمينها من خارج مجلد المكون.
* **`src/compiler.c`**: يحتوي على المنطق الرئيسي لتنسيق عملية الترجمة (استدعاء المعالج المسبق، المحلل اللفظي، إلخ). يتم بناؤه الآن كجزء من مكتبة `baa_compiler_lib`.
* **`src/main.c`**: نقطة الدخول لبرنامج `baa` القابل للتنفيذ. يستدعي بشكل أساسي الدوال من `baa_compiler_lib`.
* **Parser & AST**: نظرًا لأنهما قيد إعادة التصميم، فإن ملفاتهما المحددة في `include/baa/ast`, `include/baa/parser`, `src/ast`, `src/parser` قد تكون بسيطة جدًا أو غير موجودة حاليًا حتى يكتمل التصميم الجديد.
* **CMake Build System**:
  * **Root `CMakeLists.txt`**: يدير المشروع بشكل عام، يحدد الخيارات، يجد الحزم (مثل LLVM)، ويضيف المجلدات الفرعية الرئيسية (`src`, `tests`, `tools`).
  * **`cmake/BaaCompilerSettings.cmake`**: وحدة CMake مخصصة لتحديد إعدادات المترجم المشتركة (مثل تعريفات `UNICODE`) عبر مكتبة واجهة `BaaCommonSettings`.
  * **Target-Centric Approach**: يتم تطبيق خصائص البناء (تعريفات، مسارات تضمين، روابط) على الأهداف المحددة (المكتبات والبرامج التنفيذية) بدلاً من استخدام أوامر عامة.

يهدف هذا الهيكل إلى توفير أساس منظم وقابل للتطوير لمشروع لغة باء.
