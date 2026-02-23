# Baa Language Specification

> **Version:** 0.3.5.5 | [← User Guide](USER_GUIDE.md) | [Compiler Internals →](INTERNALS.md)

Baa (باء) is a compiled systems programming language using Arabic syntax. It compiles to native code via Assembly + host GCC/Clang on Windows and Linux.

---

## Table of Contents

- [Program Structure](#1-program-structure)
- [Preprocessor](#2-preprocessor)
- [Variables & Types](#3-variables--types)
- [Constants](#4-constants)
- [Functions](#5-functions)
- [Input / Output](#6-input--output)
- [Control Flow](#7-control-flow)
- [Operators](#8-operators)
- [Inline Assembly](#9-inline-assembly)
- [Complete Example](#10-complete-example)

---

## 1. Program Structure

A Baa program is a collection of **Global Variables** and **Functions**.

| Aspect | Description |
|--------|-------------|
| **File Format** | UTF-8 encoded, `.baa` extension |
| **Entry Point** | `الرئيسية` (Main) function |
| **Statements** | End with period (`.`) |
| **Comments** | Single-line with `//` |

### Minimal Program

```baa
صحيح الرئيسية() {
    إرجع ٠.
}
```

---

## 2. Preprocessor (المعالج القبلي)

The preprocessor handles directives before the code is compiled. All directives start with `#`.

### 2.1. Include Directive (`#تضمين`)

Include other files (headers) into the current file. This works like C's `#include`.

**Syntax:** `#تضمين "file.baahd"`

**Example:**

```baa
#تضمين "math.baahd"
// This includes the header file "math.baahd"
```

### 2.2. Definitions (`#تعريف`)

Define compile-time constants (macros). The compiler replaces the identifier with the specified value wherever it appears in the code.

**Syntax:** `#تعريف <name> <value>`

**Example:**

```baa
#تعريف حد_أقصى ١٠٠
#تعريف رسالة "مرحباً"

صحيح الرئيسية() {
    // سيتم استبدال 'حد_أقصى' بـ ١٠٠
    صحيح س = حد_أقصى.
    اطبع رسالة.
    إرجع ٠.
}
```

### 2.3. Conditional Compilation (`#إذا_عرف`)

Include or exclude blocks of code based on whether a symbol is defined.

**Syntax:**

```baa
#إذا_عرف <name>
    // Compiled if <name> is defined
#وإلا
    // Compiled if <name> is NOT defined
#نهاية
```

**Example:**

```baa
#تعريف تصحيح 1

#إذا_عرف تصحيح
    اطبع "Debug mode enabled".
#وإلا
    اطبع "Release mode".
#نهاية
```

### 2.4. Undefine Directive (`#الغاء_تعريف`)

Remove a previously defined macro.

**Syntax:** `#الغاء_تعريف <name>`

**Example:**

```baa
// Use تصحيح macro...
#تعريف تصحيح ١
#الغاء_تعريف تصحيح
// Now 'تصحيح' is undefined
```

---

## 3. Variables & Types

Baa is statically typed. All variables must be declared with their type.

### 3.1. Supported Types

| Baa Type | C Equivalent | Description | Example |
|----------|--------------|-------------|---------|
| `صحيح` | `int64_t` | Signed 64-bit integer (alias for `ص٦٤`) | `صحيح س = ٥.` |
| `ص٨` | `int8_t` | Signed 8-bit integer | `ص٨ س = -٥.` |
| `ص١٦` | `int16_t` | Signed 16-bit integer | `ص١٦ س = -٣٠٠.` |
| `ص٣٢` | `int32_t` | Signed 32-bit integer | `ص٣٢ س = -٢٠٠٠٠٠٠٠٠٠.` |
| `ص٦٤` | `int64_t` | Signed 64-bit integer | `ص٦٤ س = ٩٠٠٠٠٠٠٠٠٠٠٠٠.` |
| `ط٨` | `uint8_t` | Unsigned 8-bit integer | `ط٨ س = ٢٥٠.` |
| `ط١٦` | `uint16_t` | Unsigned 16-bit integer | `ط١٦ س = ٦٠٠٠٠.` |
| `ط٣٢` | `uint32_t` | Unsigned 32-bit integer | `ط٣٢ س = ٤٠٠٠٠٠٠٠٠٠.` |
| `ط٦٤` | `uint64_t` | Unsigned 64-bit integer | `ط٦٤ س = ١.` |
| `نص` | `حرف*` (logical) | Null-terminated string of `حرف` (`حرف[]`) | `نص اسم = "باء".` |
| `منطقي` | `bool` (stored as byte) | Boolean value (`صواب`/`خطأ`) | `منطقي ب = صواب.` |
| `حرف` | `uint64_t` (packed) | One UTF-8 character (1..4 bytes) | `حرف ح = 'أ'.` |
| `عشري` | `double` | 64-bit floating point (f64) | `عشري باي = ٣.١٤.` |

### 3.2. Scalar Variables

**Syntax:** `<type> <identifier> = <expression>.`

```baa
// Integer
صحيح س = ٥٠.
س = ١٠٠.

// String (حرف[])
نص رسالة = "مرحباً".
رسالة = "وداعاً".
```

### 3.2.1. Characters (`حرف`) and Strings (`نص`)

- `حرف` يمثل **محرف UTF-8 واحد** (1..4 بايت) مثل `'أ'`.
- `نص` يمثل **سلسلة محارف** من نوع `حرف[]` منتهية بمحرف صفر (`'\0'`).
- فهرسة النص تُعيد `حرف`:

```baa
صحيح الرئيسية() {
    نص س = "أب".
    حرف ح٠ = س[٠].
    حرف ح١ = س[١].

    اطبع س.
    اطبع ح٠.
    اطبع ح١.
    إرجع ٠.
}
```

**قيود حالية:**

- لا يوجد تحقق حدود تلقائي لفهرسة النص.
- تعديل عناصر `نص` غير مدعوم حالياً.

### 3.2.2. Integer Sizes (أحجام الأعداد الصحيحة)

إضافة إلى `صحيح` (الافتراضي)، تدعم باء أحجاماً محددة للأعداد الصحيحة:

| النوع | الوصف |
|------|-------|
| `ص٨/ص١٦/ص٣٢/ص٦٤` | أعداد موقعة |
| `ط٨/ط١٦/ط٣٢/ط٦٤` | أعداد غير موقعة |

**ملاحظات دلالية:**

- الترقيات والتحويلات الحسابية تتبع نمط C (integer promotions + usual arithmetic conversions).
- الثوابت العشرية بدون لاحقة تُعامل كـ `ص٣٢` إذا كانت ضمن المدى، وإلا كـ `صحيح/ص٦٤`.

### 3.2.3. Floating Point (`عشري`)

النوع `عشري` هو `f64` ويدعم:

- العمليات `+ - * /`
- المقارنات `== != < > <= >=`
- الطباعة عبر `اطبع`

```baa
صحيح الرئيسية() {
    عشري أ = ١.٢٥.
    عشري ب = ٢.٥.
    عشري ج = أ + ب.
    اطبع ج.
    إذا (ج >= ٣.٧٥) { اطبع "صحيح". }
    إرجع ٠.
}
```

### 3.3. Arrays (المصفوفات)

مصفوفات ثابتة الحجم. المصفوفات المحلية تُحجز على المكدس، والمصفوفات العامة تُصدر في قسم البيانات.

**قيود حالية:**
- مدعومة محلياً وعمومياً (مصفوفات عامة ثابتة الحجم).
- مدعومة لنوع `صحيح` فقط حالياً.
- `نص` ليس مصفوفة عامة؛ لكنه يُفهرس كـ `حرف[]` (انظر قسم النصوص).
- المصفوفة ليست قيمة من الدرجة الأولى: لا يمكن إسنادها إلى متغير، ولا تمريرها كمعامل مباشرة.
- إن لم تُهيّأ مصفوفة محلية، عناصرها غير مهيّأة افتراضياً؛ لا تقرأ عنصراً قبل كتابته.
- عند استخدام تهيئة `{...}`، يسمح بالتهيئة الجزئية وتُملأ بقية العناصر بأصفار (مثل C).
- المصفوفات العامة تُعامل كـ zero-initialized عند عدم وجود تهيئة.

**Syntax:**

- بدون تهيئة: `صحيح <identifier>[<size>].`
- مع تهيئة: `صحيح <identifier>[<size>] = { <values> }.`

الفواصل داخل `{}` يمكن أن تكون الفاصلة العربية `،` أو الفاصلة العادية `,`.

```baa
// تعريف مصفوفة من ٥ عناصر
صحيح قائمة[٥].

// تهيئة جزئية (الباقي أصفار)
صحيح أرقام[٥] = {١، ٢}.

// تهيئة فارغة: كل العناصر أصفار
صحيح أصفار[٣] = {}.

// تعيين قيمة (الفهرس يبدأ من ٠)
قائمة[٠] = ١٠.
قائمة[١] = ٢٠.

// قراءة قيمة
صحيح أول = قائمة[٠].

// استخدام متغير كفهرس
صحيح س = ٢.
قائمة[س] = ٣٠.
```

---

### 3.4. Enumerations (التعداد)

التعداد (`تعداد`) يعرّف مجموعة أسماء ثابتة تُخزَّن كأعداد صحيحة، مع **سلامة نوع** تمنع خلط تعدادين مختلفين.

**Syntax:**

```baa
تعداد لون {
    أحمر،
    أزرق،
    أسود،
}
```

**الوصول لقيمة تعداد:** `<اسم_التعداد>:<اسم_العنصر>`

```baa
صحيح الرئيسية() {
    تعداد لون ل = لون:أحمر.
    إذا (ل == لون:أحمر) { اطبع ١. }
    إرجع ٠.
}
```

**ملاحظات:**

- قيم العناصر تُعيَّن تلقائياً: 0, 1, 2...
- لا يمكن مقارنة/إسناد قيم تعداد من أنواع تعداد مختلفة.

---

### 3.5. Structures (الهياكل)

الهيكل (`هيكل`) يعرّف نوعاً مركّباً من حقول (Fields). التخزين يتم كبايتات داخل المكدس (محلي) أو قسم البيانات (عام).

**تعريف هيكل:**

```baa
هيكل نقطة {
    صحيح س.
    صحيح ص.
}
```

**تعريف متغير هيكل:**

```baa
صحيح الرئيسية() {
    هيكل نقطة ن.
    ن:س = ١.
    ن:ص = ٢.
    اطبع ن:س.
    إرجع ٠.
}
```

**تعشيش الهياكل (Nested Structs):**

```baa
هيكل محرك { صحيح قوة. }
هيكل سيارة {
    هيكل محرك م.
}

صحيح الرئيسية() {
    هيكل سيارة س.
    س:م:قوة = ١٥٠.
    اطبع س:م:قوة.
    إرجع ٠.
}
```

**قيود حالية:**

- لا يمكن استخدام الهيكل كقيمة من الدرجة الأولى (لا يوجد إسناد/تمرير/إرجاع بالنسخ).
- تهيئة الهيكل عند التعريف (`=`) غير مدعومة حالياً؛ يتم تعيين الحقول عبر `:`.

---

### 3.6. Union Types (الاتحادات)

الاتحاد (`اتحاد`) نوع مركب يُشارك فيه جميع الأعضاء نفس الذاكرة. جميع الأعضاء تبدأ من إزاحة 0.

**تعريف اتحاد:**

```baa
اتحاد قيمة {
    صحيح رقم.
    نص نص_قيمة.
    منطقي منطق.
}
```

**تعريف متغير اتحاد واستخدامه:**

```baa
صحيح الرئيسية() {
    اتحاد قيمة ق.
    ق:رقم = ٤٢.
    اطبع ق:رقم.

    // الكتابة إلى عضو آخر تكتب فوق نفس الذاكرة.
    ق:نص_قيمة = "مرحبا".
    اطبع ق:نص_قيمة.

    إرجع ٠.
}
```

**نمط اتحاد موسوم (Tagged Union) يدوياً:**

```baa
تعداد نوع_قيمة { رقم، نص_ق }

اتحاد قيمة {
    صحيح رقم.
    نص نص_قيمة.
}

هيكل قيمة_موسومة {
    تعداد نوع_قيمة نوع.
    اتحاد قيمة بيانات.
}
```

---

### 3.7. Additional Types & Planned Features

هذا القسم يجمع ميزات إضافية: المنفّذ حالياً والمخطط لاحقاً.

#### 3.7.1. Type Aliases (أسماء الأنواع البديلة) [Implemented v0.3.6.5]

**Syntax:** `نوع <new_name> = <existing_type>.`

```baa
نوع معرف = ط٦٤.
نوع كود = ص٣٢.

معرف ر = ١٠٠.
كود خ = -١.
```

**Current rules (v0.3.6.5):**

- `نوع` declarations are **global-only** (top-level).
- Alias must be declared **before** usage (C-like behavior).
- Alias targets in this milestone are existing implemented types (primitive + `تعداد`/`هيكل`/`اتحاد`).
- `نوع` is parsed contextually in declarations, so existing member/identifier usages مثل `س:نوع` remain valid.
- Pointer/function-type aliases are deferred to later pointer/function-type milestones.

#### 3.7.2. Static Local Variables (متغيرات ساكنة) [Scheduled v0.3.7.5]

**Syntax:** `ساكن <type> <name> = <value>.`

```baa
صحيح عداد() {
    ساكن صحيح ع = ٠.
    ع = ع + ١.
    إرجع ع.
}
```

#### 3.7.3. Type Casting (تحويل الأنواع) [Scheduled v0.3.10.5]

**Syntax:** `كـ<type>(expression)`

```baa
صحيح س = ٦٥.
حرف ح = كـ<حرف>(س).
```

ملاحظة: نوع `حرف` متاح، لكن صيغة التحويل `كـ<...>(...)` ما زالت مخططاً ولم تُنفّذ بعد.


## 4. Constants (الثوابت)

Constants are immutable variables that cannot be reassigned after initialization.

### 4.1. Constant Declaration

Use the `ثابت` keyword before the type to declare a constant.

**Syntax:** `ثابت <type> <identifier> = <expression>.`

```baa
// Global constant
ثابت صحيح الحد_الأقصى = ١٠٠.

صحيح الرئيسية() {
    // Local constant
    ثابت صحيح المعامل = ٥.
    
    اطبع الحد_الأقصى.  // ✓ OK: Reading constant
    اطبع المعامل.      // ✓ OK: Reading constant
    
    // الحد_الأقصى = ٢٠٠.  // ✗ Error: Cannot reassign constant
    
    إرجع ٠.
}
```

### 4.2. Constant Arrays (مصفوفات ثابتة)

يمكن التصريح بمصفوفة على أنها `ثابت` لمنع تعديل عناصرها.

**ملاحظة:** المصفوفات الثابتة يجب أن تكون مُهيّأة (مثل ثوابت C).

**Syntax:** `ثابت صحيح <identifier>[<size>] = { <values> }.`

```baa
صحيح الرئيسية() {
    ثابت صحيح أرقام[٣] = {١، ٢، ٣}.
    
    // أرقام[٠] = ١٠.  // ✗ Error: Cannot modify constant array
    
    إرجع ٠.
}
```

### 4.3. Rules for Constants

| Rule | Description |
|------|-------------|
| **Must be initialized (scalars)** | Scalar constants require an initial value at declaration |
| **Cannot be reassigned** | Attempting to reassign produces a semantic error |
| **Array elements immutable** | Elements of constant arrays cannot be modified |
| **Functions cannot be const** | The `ثابت` keyword applies only to variables |

---

## 5. Functions

Functions enable code reuse and modularity.

### 5.1. Definition

**Syntax:** `<type> <name>(<parameters>) { <body> }`

```baa
// Function that adds two integers
صحيح جمع(صحيح أ, صحيح ب) {
    إرجع أ + ب.
}

// Function that squares an integer
صحيح مربع(صحيح س) {
    إرجع س * س.
}
```

### 5.2. Function Prototypes (النماذج الأولية)

To use a function defined in another file (or later in the same file), you can declare its prototype without a body.

**Syntax:** `<type> <name>(<parameters>).` ← Note the dot at the end!

```baa
// Prototype declaration (notice the dot at the end)
صحيح جمع(صحيح أ, صحيح ب).

صحيح الرئيسية() {
    // Calls 'جمع' defined in another file
    اطبع جمع(١٠, ٢٠).
    إرجع ٠.
}
```

### 5.3. Calling

**Syntax:** `<name>(<arguments>)`

```baa
صحيح الناتج = جمع(١٠, ٢٠).   // الناتج = ٣٠
صحيح م = مربع(٥).            // م = ٢٥
```

### 5.4. Entry Point (`الرئيسية`)

Every program **must** have a main function:

```baa
صحيح الرئيسية() {
    // Program code here
    إرجع ٠.  // 0 means success
}
```

**Important:** The entry point **must** be named `الرئيسية` (ar-ra'īsīyah). It is exported as `main` in the generated assembly.

**Command Line Arguments [Scheduled v0.3.12.5]:**
The main function can optionally accept arguments:

```baa
صحيح الرئيسية(صحيح عدد، نص[] معاملات) {
    إذا (عدد > ١) {
        اطبع معاملات[١].
    }
    إرجع ٠.
}
```

### 5.5. Recursion (التكرار)

Functions can call themselves (recursion), provided there is a base case to terminate the loop.

```baa
// حساب متتالية فيبوناتشي
صحيح فيبوناتشي(صحيح ن) {
    إذا (ن <= ١) {
        إرجع ن.
    }
    إرجع فيبوناتشي(ن - ١) + فيبوناتشي(ن - ٢).
}
```

### 5.6. Function Pointers (مؤشرات الدوال) [Scheduled v0.3.10.6]

Pass functions as values.

**Syntax:** `دالة(types) -> return_type`

```baa
نوع عملية = دالة(صحيح، صحيح) -> صحيح.

صحيح جمع(صحيح أ، صحيح ب) { إرجع أ + ب. }

صحيح الرئيسية() {
    عملية ع = جمع.
    اطبع ع(١، ٢).
    إرجع ٠.
}
```

### 5.7. Variadic Functions (دوال متغيرة المعاملات) [Scheduled v0.4.0.5]

Functions that accept any number of arguments using `...`.

```baa
عدم سجل(نص رسالة، ...) {
    // Implementation
}
```

---

## 6. Input / Output

### 6.1. Print Statement (`اطبع`)

**Syntax:** `اطبع <expression>.`

Prints a value followed by a newline.

**Current implementation notes:**

- Integer variables (`صحيح`, `ص٨`, `ط٦٤`, etc.) are dynamically printed as 64-bit values (`%lld` or `%llu`) based on their signed/unsigned type.
- Float variables (`عشري`) are printed using `%g\n`.
- String variables (`نص`) are printed using a character iteration loop to correctly emit Baa `حرف` arrays until a null terminator.
- Character literals or variables (`حرف`) correctly print as packed UTF-8 sequences.

```baa
اطبع "مرحباً بالعالم".    // طباعة نص
اطبع ١٠٠.                 // طباعة رقم
اطبع 'أ'.                 // طباعة حرف

// طباعة متغيرات
نص اسم = "علي".
اطبع اسم.
```

### 6.2. Input Statement (`اقرأ`)

**Syntax:** `اقرأ <variable>.`

Reads an input from standard input and stores it in the specified variable.

**Current implementation notes:**

- Input natively supports all signed and unsigned integer sizes up to 64-bit. It resolves the appropriate `scanf` format (e.g., `%lld`, `%llu`) based on the target variable's type, reading into a 64-bit temporary if necessary before truncation.

```baa
صحيح العمر = ٠.
اطبع "كم عمرك؟ ".
اقرأ العمر.

اطبع "عمرك هو: ".
اطبع العمر.
```

---

## 7. Control Flow

### 7.1. Conditional (`إذا` / `وإلا`)

Executes a block based on conditions.

**Syntax:**

```baa
إذا (<condition>) {
    // ...
} وإلا إذا (<condition>) {
    // ...
} وإلا {
    // ...
}
```

**Example:**

```baa
صحيح س = ١٥.

إذا (س > ٢٠) {
    اطبع "كبير جداً".
} وإلا إذا (س > ١٠) {
    اطبع "متوسط".
} وإلا {
    اطبع "صغير".
}
```

### 7.2. While Loop (`طالما`)

Repeats a block while the condition is true.

**Syntax:** `طالما (<condition>) { <body> }`

```baa
صحيح س = ٥.

طالما (س > ٠) {
    اطبع س.
    س = س - ١.
}
// يطبع: ٥ ٤ ٣ ٢ ١
```

### 7.3. For Loop (`لكل`)

C-style loop using **Arabic semicolon `؛`** as separator (NOT regular semicolon).

**Syntax:** `لكل (<init>؛ <condition>؛ <increment>) { <body> }`

```baa
// طباعة الأرقام من ٠ إلى ٩
لكل (صحيح س = ٠؛ س < ١٠؛ س++) {
    اطبع س.
}
```

### 7.4. Loop Control (`توقف` & `استمر`)

- **`توقف` (Break)**: Exits the loop immediately.
- **`استمر` (Continue)**: Skips the rest of the current iteration and proceeds to the next one.

**Example:**

```baa
صحيح الرئيسية() {
    لكل (صحيح س = ٠؛ س < ١٠؛ س++) {
        // تخطي الرقم ٥
        إذا (س == ٥) {
            استمر.
        }
        
        // الخروج عند الوصول للرقم ٨
        إذا (س == ٨) {
            توقف.
        }
        
        اطبع س.
    }
    // الناتج: ٠ ١ ٢ ٣ ٤ ٦ ٧
    إرجع ٠.
}
```

### 7.5. Switch Statement (`اختر`)

Multi-way branching based on integer or character values.

- **`اختر` (Switch)**: Starts the statement.
- **`حالة` (Case)**: Defines a value to match.
- **`افتراضي` (Default)**: Defines the code to run if no case matches.
- **`:` (Colon)**: Separator after the case value.

**Syntax:**

```baa
اختر (<expression>) {
    حالة <value>:
        // code...
        توقف.
    حالة <value>:
        // code...
        توقف.
    افتراضي:
        // code...
        توقف.
}
```

**Note:** Just like in C, execution "falls through" to the next case unless you explicitly use `توقف` (break).

**Example:**

```baa
صحيح س = ٢.

اختر (س) {
    حالة ١:
        اطبع "واحد".
        توقف.
    حالة ٢:
        اطبع "اثنان".
        توقف.
    افتراضي:
        اطبع "رقم آخر".
        توقف.
}
```

---

## 8. Operators

### 8.1. Arithmetic

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `٥ + ٣` → `٨` |
| `-` | Subtraction | `٥ - ٣` → `٢` |
| `*` | Multiplication | `٥ * ٣` → `١٥` |
| `/` | Division | `١٠ / ٢` → `٥` |
| `%` | Modulo | `١٠ % ٣` → `١` |
| `++` | Increment (postfix) | `س++` |
| `--` | Decrement (postfix) | `س--` |
| `-` | Negative (unary) | `-٥` |

### 8.2. Comparison

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `س == ٥` |
| `!=` | Not equal | `س != ٥` |
| `<` | Less than | `س < ١٠` |
| `>` | Greater than | `س > ١٠` |
| `<=` | Less or equal | `س <= ١٠` |
| `>=` | Greater or equal | `س >= ١٠` |

### 8.3. Logical

| Operator | Description | Behavior |
|----------|-------------|----------|
| `&&` | AND | Short-circuit: stops if left is false |
| `\|\|` | OR | Short-circuit: stops if left is true |
| `!` | NOT | Inverts truth value |

**Short-circuit Evaluation:** `&&` stops if left is false; `||` stops if left is true.

```baa
// Short-circuit example
إذا (س > ٠ && س < ١٠) {
    اطبع "س بين ١ و ٩".
}

إذا (!خطأ) {
    اطبع "لا يوجد خطأ".
}
```

### 8.4. Operator Precedence

From highest to lowest:

1. `()` — Parentheses
2. `!`, `-` (unary), `++`, `--` — Unary operators
3. `*`, `/`, `%` — Multiplication, Division, Modulo
4. `+`, `-` — Addition, Subtraction
5. `<`, `>`, `<=`, `>=` — Relational
6. `==`, `!=` — Equality
7. `&&` — Logical AND
8. `||` — Logical OR

**Note:** Use parentheses `()` to override precedence when needed.

---

## 9. Inline Assembly (المجمع المدمج) [Scheduled v0.4.0.6]

Embed raw assembly code directly.

**Syntax:** `مجمع { strings }`

```baa
مجمع {
    "nop"
}
```

---

## 10. Complete Example

```baa
// استخدام الثوابت والماكرو
#تعريف الحد_الأقصى ١٠

// ثابت عام
ثابت صحيح المعامل = ٢.

// Main function
صحيح الرئيسية() {
    // ثابت محلي
    ثابت صحيح البداية = ١.
    
    // طباعة الأرقام المضاعفة
    لكل (صحيح س = البداية؛ س <= الحد_الأقصى؛ س++) {
        اطبع س * المعامل.
    }
    إرجع ٠.
}
```

---

*[← User Guide](USER_GUIDE.md) | [Compiler Internals →](INTERNALS.md)*
