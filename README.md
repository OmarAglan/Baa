<div dir="rtl">

<div align="center">
<img width="260" height="260" alt="شعار باء" src="resources/Logo.png" />


![Version](https://img.shields.io/badge/version-0.3.2.8.4-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

**أول لغة برمجة نُظُم مُصرَّفة (Compiled) بصياغة عربية**

*اكتب تطبيقات أصلية على Windows و Linux باستخدام كلمات مفتاحية وأرقام وعلامات ترقيم عربية*

</div>

---

## ✨ المزايا

| الميزة | الوصف |
|---------|-------------|
| 🖥️ **تصريف/ترجمة أصلية (Native Compilation)** | تُصرِّف الشيفرة إلى Assembly لمعمارية x86-64 ← ثم تُنتج ملفات تنفيذية أصلية (Windows: PE/COFF، Linux: ELF) |
| 🌐 **متعدد الأهداف (Multi-Target)** | دعم أولي لهدفين: `x86_64-windows` (COFF) و `x86_64-linux` (ELF) مع خيار `--target` |
| 🌍 **صياغة عربية كاملة** | كلمات مفتاحية عربية، وأرقام (٠-٩)، وعلامات ترقيم (`.` `؛`) |
| 🧩 **شيفرة معيارية** | `#تضمين` (Include)، تصريف متعدد الملفات، وملفات ترويسة `.baahd` |
| 🔧 **المعالج القبلي (Preprocessor)** | `#تعريف` (Define)، `#إذا_عرف` (Ifdef)، `#الغاء_تعريف` (Undefine) |
| ⚡ **الدوال** | تعريف الدوال واستدعاؤها مع معاملات (Parameters) وقيم إرجاع (Return Values) |
| 📦 **المصفوفات** | مصفوفات ثابتة الحجم على المكدّس (Stack) مثل: (`صحيح قائمة[١٠]`) |
| 🔄 **تدفّق التحكّم** | `إذا`/`وإلا` (If/Else)، `طالما` (While)، `لكل` (For) |
| 🎯 **تحكّم متقدّم** | `اختر` (Switch)، `حالة` (Case)، `افتراضي` (Default)، `توقف` (Break)، `استمر` (Continue) |
| ➕ **المعاملات كاملة** | معاملات حسابية ومقارنة ومنطقية مع تقييم قصير (Short-circuit) |
| 📝 **دعم النصوص** | ثوابت السلاسل النصية (`"..."`) وثوابت المحارف (`'...'`) |
| ❓ **منطق بولياني** | النوع `منطقي` مع `صواب` (True) و`خطأ` (False) |
| ⌨️ **إدخال المستخدم** | العبارة `اقرأ` (Read) لقراءة عدد صحيح من المستخدم |
| ✅ **سلامة الأنواع (Type Safety)** | تدقيق ثابت للأنواع (ابتداءً من v0.2.4+) مع تحليل دلالي (Semantic Analysis) |
| 🔄 **تحديث ذاتي** | مُحدِّث مدمج (`baa update`) — *Windows فقط حالياً* |

---

## 🧩 التوافقية

| العنصر | مدعوم | ملاحظات |
|------|-----------|------|
| نظام التشغيل | Windows (x86-64) + Linux (x86-64) | على Linux يتم إنتاج ELF والربط باستخدام GCC المضيف حالياً |
| سلسلة الأدوات | Windows: CMake 3.10+ + MinGW-w64 GCC  /  Linux: CMake 3.10+ + GCC/Clang | على Windows نستخدم MinGW، وعلى Linux نستخدم GCC/Clang المضيف |
| ترميز ملفات المصدر | UTF-8 | يتطلب النص العربي ملفات بترميز UTF-8 |
| الطرفية | Windows Terminal / PowerShell | فعِّل UTF-8 إذا ظهر الإخراج بصورة غير سليمة |

## 🚀 البدء السريع

### 1) بناء المُصرِّف

**Windows (MinGW):**

```powershell
git clone https://github.com/OmarAglan/Baa.git
cd Baa
mkdir build && cd build
cmake ..
cmake --build .
```

**Linux (GCC/Clang):**

```bash
git clone https://github.com/OmarAglan/Baa.git
cd Baa
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/baa --version
```

### 2) اكتب برنامجك الأول

أنشئ ملف `hello.baa` (⚠️ **مهم:** احفظ الملف بترميز **UTF-8**):

```baa
صحيح الرئيسية() {
    اطبع "مرحباً بالعالم!".
    إرجع ٠.
}
```

### 3) صرِّف وشغِّل

```powershell
# التصريف (Compile)
.\baa.exe ..\hello.baa

# التشغيل (Run)
.\out.exe
```

```bash
# التصريف (Compile)
./baa ../hello.baa

# التشغيل (Run)
./out
```

### 4) اختيار الهدف (Multi-Target)

افتراضياً يختار المُصرِّف هدف النظام المضيف. يمكنك تحديده صراحةً:

```bash
baa --target=x86_64-windows البرنامج.baa
baa --target=x86_64-linux   البرنامج.baa
```

ملاحظة (v0.3.2.8.4):

- عند اختلاف الهدف عن نظام المضيف، يدعم المُصرّف حالياً **-S فقط** (توليد Assembly). التجميع/الربط العابر للأهداف مؤجل.

### القيود الحالية (v0.3.2.8.2)

- **نداءات الدوال:** مدعومة حالياً فقط ضمن معاملات السجلات (Windows: حتى ٤ معاملات، Linux/SysV: حتى ٦ معاملات). معاملات المكدس مؤجلة إلى v0.3.2.8.5.

### خيارات نموذج الكود (v0.3.2.8.3)

- `-fPIC` / `-fPIE` (Linux/ELF): خيارات أولية لـ PIC/PIE (حالياً التأثير الأساسي هو وضع الربط PIE عند `-fPIE`).
- `-fstack-protector` / `-fstack-protector-all` (Linux/ELF): إضافة كناري حماية المكدس.

**المخرجات:** `مرحباً بالعالم!`

---

## 📖 مثال: جمع عناصر مصفوفة

```baa
// حساب مجموع مصفوفة
صحيح الرئيسية() {
    // التصريح بمصفوفة من ٥ أعداد صحيحة
    صحيح قائمة[٥].
    صحيح مجموع = ٠.

    // ملء المصفوفة بالقيم: 0، 10، 20، 30، 40
    لكل (صحيح س = ٠؛ س < ٥؛ س++) {
        قائمة[س] = س * ١٠.
    }

    // جمع جميع القيم
    لكل (صحيح س = ٠؛ س < ٥؛ س++) {
        مجموع = مجموع + قائمة[س].
    }

    اطبع "المجموع هو: ".
    اطبع مجموع.
    
    إرجع ٠.
}
```

**المخرجات:** `المجموع هو: 100` (0 + 10 + 20 + 30 + 40)

---

## 📚 التوثيق

| المستند | الوصف |
|----------|-------------|
| [دليل المستخدم](docs/USER_GUIDE.md) | البدء والاستخدام الأساسي |
| [الكتاب العربي](docs/BAA_BOOK_AR.md) | تعلّم/مرجع عربي شامل (مسودة) |
| [مواصفة اللغة](docs/LANGUAGE.md) | مرجع كامل للصياغة والمزايا |
| [البنية الداخلية للمُصرِّف](docs/INTERNALS.md) | المعمارية وتفاصيل التنفيذ |
| [مرجع واجهة API](docs/API_REFERENCE.md) | توثيق واجهة C الداخلية |
| [خارطة الطريق](ROADMAP.md) | خطط التطوير المستقبلية |
| [سجل التغييرات](CHANGELOG.md) | تاريخ الإصدارات |

---

## 🛠️ البناء من الشيفرة المصدرية

### المتطلبات

- **CMake** 3.10+
- **MinGW-w64** مع GCC
- **PowerShell** (Windows)
- **Git** (لاستنساخ المستودع)

### خطوات البناء

```powershell
# استنساخ المستودع
git clone https://github.com/OmarAglan/Baa.git
cd Baa

# إنشاء مجلد البناء
mkdir build
cd build

# التوليد ثم البناء
cmake ..
cmake --build .

# أصبح المُصرّف الآن في: build/baa.exe
```

### تشغيل الاختبارات

```powershell
# توليد ملف test.baa
gcc ..\make_test.c -o make_test.exe
.\make_test.exe

# تصريف وتشغيل
.\baa.exe .\test.baa -o test.exe
.\test.exe
```

**المخرجات المتوقعة:**
```
1
0
0
```

---

## ✅ التحقّق (مقترح)

للتحقق من أمثلة التوثيق مقابل التنفيذ:

```powershell
cmake -S . -B build
cmake --build build
.\build\baa.exe --help
.\build\baa.exe --version
```

ولفحص سريع طرف-إلى-طرف (End-to-End):

```powershell
gcc .\make_test.c -o .\build\make_test.exe
.\build\make_test.exe
.\build\baa.exe .\build\test.baa -o .\build\test_suite.exe
.\build\test_suite.exe
```
</div>
