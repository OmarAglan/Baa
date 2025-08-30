# Baa Language Quick Start Guide
# دليل البداية السريعة للغة باء

**Get started with Baa in 5 minutes!** | **ابدأ مع باء في 5 دقائق!**

**Available in:** [English](#english-quick-start) | [العربية](#arabic-quick-start---دليل-البداية-السريعة-باللغة-العربية)

---

## English Quick Start

### 1. Prerequisites

- **CMake** (3.20 or higher)
- **C11 compliant compiler** (GCC, Clang, or MSVC)
- **Git**
- **(Optional)** LLVM development libraries

### 2. Clone and Build

```bash
# Clone the repository
git clone https://github.com/OmarAglan/baa.git
cd baa

# Create build directory
mkdir build
cd build

# Configure (Linux/macOS)
cmake -G "Ninja" ..

# Configure (Windows with LLVM/Clang)
cmake -G "Ninja" -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" ..

# Build
cmake --build .
```

### 3. Test the Installation

```bash
# Test the lexer
./tools/baa_lexer_tester ../tests/resources/lexer_test_cases/lexer_test_suite.baa

# Test the preprocessor
./tools/baa_preprocessor_tester ../tests/resources/preprocessor_test_cases/preprocessor_test_all.baa

# Test the parser (if available)
./tools/baa_parser_tester ../tests/resources/parser_tests/valid_simple.baa
```

### 4. Your First Baa Program

Create a file called `hello.ب`:

```baa
// hello.ب - Your first Baa program
#تضمين <stdio> // Standard I/O (hypothetical)

عدد_صحيح رئيسية() {
    اطبع("مرحباً بالعالم!").
    إرجع 0.
}
```

### 5. Language Features Overview

#### Arabic Keywords
```baa
// Variables and constants
عدد_صحيح العدد = ١٠.
ثابت عدد_حقيقي PI = ٣٫١٤١٥٩.
منطقي النتيجة = صحيح.

// Functions
عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب.
}

// Control flow
إذا (العدد > ٥) {
    اطبع("العدد أكبر من خمسة").
} وإلا {
    اطبع("العدد خمسة أو أقل").
}

// Loops
طالما (العدد < ٢٠) {
    العدد = العدد + ١.
}

لكل (عدد_صحيح ي = ٠؛ ي < ١٠؛ ي++) {
    اطبع(ي).
}
```

#### Arabic Numerals and Strings
```baa
// Arabic-Indic numerals
عدد_صحيح عدد_عربي = ١٢٣٤٥.
عدد_حقيقي رقم_عشري = ٣٫١٤.

// Scientific notation with Arabic exponent marker
عدد_حقيقي كبير = ١٫٢٣أ٦.  // 1.23 × 10^6

// Arabic escape sequences
حرف سطر_جديد = '\س'.     // Newline
حرف تاب = '\م'.           // Tab
نص رسالة = "مرحبا\سبالعالم".  // "Hello\nWorld"

// Unicode escape
حرف همزة = '\ي0623'.      // Arabic letter Alef with Hamza above
```

### 6. Next Steps

1. **Read the documentation**: Start with [Language Specification](language.md)
2. **Explore examples**: Check out test files in `tests/resources/`
3. **Learn the architecture**: Read [Architecture Overview](architecture.md)
4. **Contribute**: See [Development Guide](development.md)

### 7. Getting Help

- **Documentation**: [Complete Documentation Index](NAVIGATION.md)
- **Issues**: Create an issue on the project repository
- **Community**: Join discussions about Arabic programming languages

---

## Arabic Quick Start - دليل البداية السريعة باللغة العربية

### 1. المتطلبات المسبقة

- **CMake** (3.20 أو أحدث)
- **مترجم C11 متوافق** (GCC, Clang, أو MSVC)
- **Git**
- **(اختياري)** مكتبات تطوير LLVM

### 2. الاستنساخ والبناء

```bash
# استنساخ المستودع
git clone https://github.com/OmarAglan/baa.git
cd baa

# إنشاء دليل البناء
mkdir build
cd build

# التكوين (Linux/macOS)
cmake -G "Ninja" ..

# التكوين (Windows مع LLVM/Clang)
cmake -G "Ninja" -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" ..

# البناء
cmake --build .
```

### 3. اختبار التثبيت

```bash
# اختبار المحلل اللفظي
./tools/baa_lexer_tester ../tests/resources/lexer_test_cases/lexer_test_suite.baa

# اختبار المعالج المسبق
./tools/baa_preprocessor_tester ../tests/resources/preprocessor_test_cases/preprocessor_test_all.baa

# اختبار المحلل النحوي (إذا كان متوفراً)
./tools/baa_parser_tester ../tests/resources/parser_tests/valid_simple.baa
```

### 4. برنامجك الأول بلغة باء

أنشئ ملف يسمى `مرحبا.ب`:

```baa
// مرحبا.ب - برنامجك الأول بلغة باء
#تضمين <stdio> // إدخال/إخراج قياسي (افتراضي)

عدد_صحيح رئيسية() {
    اطبع("مرحباً بالعالم!").
    إرجع 0.
}
```

### 5. نظرة عامة على ميزات اللغة

#### الكلمات المفتاحية العربية
```baa
// المتغيرات والثوابت
عدد_صحيح العدد = ١٠.
ثابت عدد_حقيقي PI = ٣٫١٤١٥٩.
منطقي النتيجة = صحيح.

// الدوال
عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب.
}

// التحكم في التدفق
إذا (العدد > ٥) {
    اطبع("العدد أكبر من خمسة").
} وإلا {
    اطبع("العدد خمسة أو أقل").
}

// الحلقات
طالما (العدد < ٢٠) {
    العدد = العدد + ١.
}

لكل (عدد_صحيح ي = ٠؛ ي < ١٠؛ ي++) {
    اطبع(ي).
}
```

#### الأرقام والسلاسل النصية العربية
```baa
// الأرقام العربية-الهندية
عدد_صحيح عدد_عربي = ١٢٣٤٥.
عدد_حقيقي رقم_عشري = ٣٫١٤.

// الترميز العلمي مع علامة الأس العربية
عدد_حقيقي كبير = ١٫٢٣أ٦.  // 1.23 × 10^6

// تسلسلات الهروب العربية
حرف سطر_جديد = '\س'.     // سطر جديد
حرف تاب = '\م'.           // تاب
نص رسالة = "مرحبا\سبالعالم".  // "Hello\nWorld"

// هروب Unicode
حرف همزة = '\ي0623'.      // حرف الألف مع الهمزة فوق
```

### 6. الخطوات التالية

1. **اقرأ التوثيق**: ابدأ بـ [مواصفات اللغة](ar/مواصفات_اللغة.md)
2. **استكشف الأمثلة**: تحقق من ملفات الاختبار في `tests/resources/`
3. **تعلم المعمارية**: اقرأ [نظرة عامة على المعمارية](ar/نظرة_عامة_على_المعمارية.md) *(قريباً)*
4. **ساهم**: انظر [دليل التطوير](ar/دليل_التطوير.md) *(قريباً)*

### 7. الحصول على المساعدة

- **التوثيق**: [فهرس التوثيق الكامل](NAVIGATION.md)
- **المشاكل**: أنشئ مشكلة في مستودع المشروع
- **المجتمع**: انضم للنقاشات حول لغات البرمجة العربية

---

## Common Issues and Solutions / المشاكل الشائعة والحلول

### Build Issues / مشاكل البناء

**Problem**: `CMake Error: CMAKE_C_COMPILER not set`  
**Solution**: Install a C compiler or specify the compiler path:
```bash
cmake -DCMAKE_C_COMPILER=gcc ..
# or
cmake -DCMAKE_C_COMPILER=clang ..
```

**المشكلة**: `خطأ CMake: CMAKE_C_COMPILER غير محدد`  
**الحل**: ثبت مترجم C أو حدد مسار المترجم:
```bash
cmake -DCMAKE_C_COMPILER=gcc ..
# أو
cmake -DCMAKE_C_COMPILER=clang ..
```

### Character Encoding / ترميز الحروف

**Problem**: Arabic text appears garbled  
**Solution**: Ensure your source files are saved as UTF-8 or UTF-16LE with BOM

**المشكلة**: النص العربي يظهر مشوهاً  
**الحل**: تأكد أن ملفات المصدر محفوظة بـ UTF-8 أو UTF-16LE مع BOM

### Tool Output / إخراج الأدوات

**Problem**: Lexer test output not visible  
**Solution**: Check the generated `lexer_test_output.txt` file in your current directory

**المشكلة**: إخراج اختبار المحلل اللفظي غير مرئي  
**الحل**: تحقق من ملف `lexer_test_output.txt` المولد في دليلك الحالي

---

## Feature Checklist / قائمة فحص الميزات

Use this checklist to verify your Baa installation is working correctly:

- [ ] ✅ **Preprocessor**: Can handle `#تضمين` and `#تعريف` directives
- [ ] ✅ **Lexer**: Tokenizes Arabic keywords and numerals correctly  
- [ ] ✅ **Parser**: Parses function definitions and calls
- [ ] ✅ **AST**: Builds complete abstract syntax trees
- [ ] 📋 **Semantic Analysis**: Symbol resolution *(Next Phase)*
- [ ] 📋 **Code Generation**: LLVM IR output *(Next Phase)*

استخدم هذه القائمة للتحقق من أن تثبيت باء يعمل بشكل صحيح:

- [ ] ✅ **المعالج المسبق**: يمكنه التعامل مع توجيهات `#تضمين` و `#تعريف`
- [ ] ✅ **المحلل اللفظي**: يرمز الكلمات المفتاحية والأرقام العربية بشكل صحيح
- [ ] ✅ **المحلل النحوي**: يحلل تعريفات واستدعاءات الدوال
- [ ] ✅ **شجرة النحو المجردة**: يبني أشجار نحو مجردة كاملة
- [ ] 📋 **التحليل الدلالي**: حل الرموز *(المرحلة التالية)*
- [ ] 📋 **توليد الكود**: إخراج LLVM IR *(المرحلة التالية)*

---

**Ready to start programming in Arabic? Welcome to Baa!** 🎉  
**مستعد لبدء البرمجة بالعربية؟ مرحباً بك في باء!** 🎉
