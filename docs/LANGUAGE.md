# Baa Language Specification

> **Version:** 0.1.3 | [← User Guide](USER_GUIDE.md) | [Compiler Internals →](INTERNALS.md)

Baa (باء) is a compiled systems programming language using Arabic syntax. It compiles directly to native machine code via Assembly/GCC on Windows.

---

## Table of Contents

- [Program Structure](#1-program-structure)
- [Variables & Types](#2-variables--types)
- [Functions](#3-functions)
- [Input / Output](#4-input--output)
- [Control Flow](#5-control-flow)
- [Operators](#6-operators)
- [Complete Example](#7-complete-example)

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
Since v0.2.6, Baa supports preprocessor directives.

### 2.1. Include Directive (`#تضمين`)
Include other files (headers) into the current file. This works like C's `#include`.

**Syntax:** `#تضمين "file.baahd"`

```baa
#تضمين "math.baahd"

صحيح الرئيسية() {
    ...
}
```

---

## 3. Variables & Types


Baa is statically typed. All variables must be declared with their type.

### 2.1. Supported Types

| Baa Type | C Equivalent | Description | Example |
|----------|--------------|-------------|---------|
| `صحيح` | `int` | 64-bit integer | `صحيح س = ٥.` |
| `نص` | `char*` | String pointer (Reference) | `نص اسم = "باء".` |
| `حرف` | `char` | Single character | `'أ'` |

### 2.2. Scalar Variables

**Syntax:** `<type> <identifier> = <expression>.`

```baa
// Integer
صحيح س = ٥٠.
س = ١٠٠.

// String (Pointer to text)
نص رسالة = "مرحباً".
رسالة = "وداعاً".
```

### 2.3. Arrays

Fixed-size arrays allocated on the stack.

**Syntax:** `صحيح <identifier>[<size>].`

```baa
// تعريف مصفوفة من ٥ عناصر
صحيح قائمة[٥].

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

## 3. Functions

Functions enable code reuse and modularity.

### 3.1. Definition

**Syntax:** `<type> <name>(<parameters>) { <body> }`

```baa
صحيح جمع(صحيح أ, صحيح ب) {
    إرجع أ + ب.
}

صحيح مربع(صحيح س) {
    إرجع س * س.
}
```

### 3.2. Function Prototypes (النماذج الأولية)

To use a function defined in another file (or later in the same file), you can declare its prototype without a body.

**Syntax:** `<type> <name>(<parameters>).`

```baa
// Prototype declaration (notice the dot at the end)
صحيح جمع(صحيح أ, صحيح ب).

صحيح الرئيسية() {
    // This calls 'جمع' which is defined elsewhere
    اطبع جمع(١٠, ٢٠).
    إرجع ٠.
}
```

### 3.3. Calling

**Syntax:** `<name>(<arguments>)`

```baa
صحيح الناتج = جمع(١٠, ٢٠).   // الناتج = ٣٠
صحيح م = مربع(٥).            // م = ٢٥
```

### 3.4. Entry Point (`الرئيسية`)

Every program **must** have a main function:

```baa
صحيح الرئيسية() {
    // كود البرنامج هنا
    إرجع ٠.  // ٠ يعني نجاح التنفيذ
}
```

### 3.5. Recursion (التكرار)

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

---

## 4. Input / Output

### Print Statement

**Syntax:** `اطبع <expression>.`

Prints an integer, string, or character followed by a newline.

```baa
اطبع "مرحباً بالعالم".    // طباعة نص
اطبع ١٠٠.                 // طباعة رقم
اطبع 'أ'.                 // طباعة حرف

// طباعة متغيرات
نص اسم = "علي".
اطبع اسم.
```

---

## 5. Control Flow

### 5.1. Conditional (`إذا` / `وإلا`)

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

### 5.2. While Loop (`طالما`)

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

### 5.3. For Loop (`لكل`)

C-style loop using Arabic semicolon `؛` as separator.

**Syntax:** `لكل (<init>؛ <condition>؛ <increment>) { <body> }`

```baa
// طباعة الأرقام من ٠ إلى ٩
لكل (صحيح س = ٠؛ س < ١٠؛ س++) {
    اطبع س.
}
```

### 5.4. Loop Control (`توقف` & `استمر`)

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

### 5.5. Switch Statement (`اختر`)

Selects a block of code to execute based on a value.

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

**Note:** Just like in C, execution "falls through" to the next case unless you use `توقف` (break).

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

## 6. Operators

### 6.1. Arithmetic

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

### 6.2. Comparison

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `س == ٥` |
| `!=` | Not equal | `س != ٥` |
| `<` | Less than | `س < ١٠` |
| `>` | Greater than | `س > ١٠` |
| `<=` | Less or equal | `س <= ١٠` |
| `>=` | Greater or equal | `س >= ١٠` |

### 6.3. Logical

| Operator | Description | Behavior |
|----------|-------------|----------|
| `&&` | AND | Short-circuit: stops if left is false |
| `\|\|` | OR | Short-circuit: stops if left is true |
| `!` | NOT | Inverts truth value |

```baa
// Short-circuit example
إذا (س > ٠ && س < ١٠) {
    اطبع "س بين ١ و ٩".
}

إذا (س == ٠ || س == ١٠) {
    اطبع "س هو ٠ أو ١٠".
}

إذا (!خطأ) {
    اطبع "لا يوجد خطأ".
}
```

### 6.4. Operator Precedence

From highest to lowest:

1. `()` — Parentheses
2. `!`, `-` (unary), `++`, `--` — Unary operators
3. `*`, `/`, `%` — Multiplication, Division, Modulo
4. `+`, `-` — Addition, Subtraction
5. `<`, `>`, `<=`, `>=` — Relational
6. `==`, `!=` — Equality
7. `&&` — Logical AND
8. `||` — Logical OR

---

## 7. Complete Example

```baa
// برنامج يوضح ميزات اللغة
صحيح مضروب(صحيح ن) {
    صحيح نتيجة = ١.
    لكل (صحيح س = ٢؛ س <= ن؛ س++) {
        نتيجة = نتيجة * س.
    }
    إرجع نتيجة.
}

صحيح الرئيسية() {
    نص ترحيب = "حساب المضروب: ".
    اطبع ترحيب.
    
    // حساب المضروب
    اطبع "مضروب ٥ = ".
    اطبع مضروب(٥).
    
    إرجع ٠.
}
```

---

*[← User Guide](USER_GUIDE.md) | [Compiler Internals →](INTERNALS.md)*