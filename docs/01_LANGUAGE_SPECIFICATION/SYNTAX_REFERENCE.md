# Baa Language Syntax Reference

**Status:** ✅ Core Implementation Complete  
**Last Updated:** 2025-12-05  
**Version:** v0.1.27.0

## Overview

This document provides a comprehensive syntax reference for the Baa programming language. It covers all language constructs, operators, keywords, and syntax rules in detail with complete examples and explanations.

## Table of Contents

- [1. Language Fundamentals](#1-language-fundamentals)
- [2. Data Types](#2-data-types)
- [3. Operators](#3-operators)
- [4. Control Flow](#4-control-flow)
- [5. Functions](#5-functions)
- [6. Variables and Constants](#6-variables-and-constants)
- [7. Arrays and Collections](#7-arrays-and-collections)
- [8. Preprocessor Directives](#8-preprocessor-directives)
- [9. Grammar Reference](#9-grammar-reference)
- [10. Examples](#10-examples)

---

## 1. Language Fundamentals

### 1.1 Character Sets and Encoding

Baa supports full Unicode with the following encodings:

- **Source Files**: UTF-8 (recommended) or UTF-16LE
- **Internal Processing**: UTF-16LE
- **String Literals**: UTF-8/UTF-16 mixed support

### 1.2 Identifiers and Naming Rules

**Identifier Rules:**

- Must start with Arabic letter, English letter, or underscore (`_`)
- Can contain Arabic letters, English letters, Arabic-Indic digits (٠-٩), ASCII digits (0-9), underscores
- Case-sensitive
- No length limit (practical limit ~4096 characters)

**Valid Identifiers:**

```baa
العدد           // Arabic identifier
المتغير_الأول   // Arabic with underscore
myVariable     // English identifier
_private       // Leading underscore
عدد2           // Arabic with ASCII digit
رقم٥           // Arabic with Arabic-Indic digit
```

**Invalid Identifiers:**

```baa
2عدد           // Cannot start with digit
if             // Reserved keyword (use إذا instead)
my-variable    // Hyphen not allowed
عدد صحيح       // Spaces not allowed
```

### 1.3 Keywords and Reserved Words

**Arabic Keywords:**

```baa
// Control Flow
إذا وإلا طالما لكل افعل توقف استمر إرجع

// Types
عدد_صحيح عدد_حقيقي حرف منطقي فراغ

// Storage Classes
ثابت ساكن خارجي تلقائي سجل متطاير مضمن مقيد

// Literals
صحيح خطأ فارغ
```

### 1.4 Comments and Documentation

**Single-Line Comments:**

```baa
// This is a single-line comment
// هذا تعليق من سطر واحد
عدد_صحيح العدد = ٥. // Inline comment
```

**Multi-Line Comments:**

```baa
/* This is a
   multi-line comment */
   
/* هذا تعليق
   متعدد الأسطر */
```

**Documentation Comments:**

```baa
/**
 * Function to calculate sum
 * دالة لحساب المجموع
 * @param أ First number - العدد الأول
 * @param ب Second number - العدد الثاني
 * @return Sum - المجموع
 */
عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب.
}
```

---

## 2. Data Types

### 2.1 Primitive Types

| Baa Type | English | Size | Range | Example |
|----------|---------|------|-------|---------|
| `عدد_صحيح` | int | 32-bit | -2³¹ to 2³¹-1 | `١٢٣` |
| `عدد_حقيقي` | float | 32-bit | IEEE 754 | `٣٫١٤` |
| `حرف` | char | 8-bit | 0-255 or UTF-8 | `'أ'` |
| `منطقي` | bool | 1-bit | `صحيح`/`خطأ` | `صحيح` |
| `فراغ` | void | 0-bit | No value | N/A |

### 2.2 Type Declarations

**Basic Variable Declarations:**

```baa
عدد_صحيح العدد.              // Declaration only
عدد_صحيح العدد = ١٠.         // Declaration with initialization
عدد_حقيقي السعر = ٢٥٫٥٠.    // Float with Arabic decimal
حرف الحرف = 'أ'.             // Character literal
منطقي مكتمل = صحيح.          // Boolean literal
```

---

## 3. Operators

### 3.1 Arithmetic Operators

| Operator | Name | Example | Result |
|----------|------|---------|--------|
| `+` | Addition | `٥ + ٣` | `٨` |
| `-` | Subtraction | `٥ - ٣` | `٢` |
| `*` | Multiplication | `٥ * ٣` | `١٥` |
| `/` | Division | `١٠ / ٣` | `٣` |
| `%` | Modulo | `١٠ % ٣` | `١` |

### 3.2 Assignment Operators

| Operator | Name | Example | Equivalent |
|----------|------|---------|------------|
| `=` | Assignment | `أ = ٥` | `أ = ٥` |
| `+=` | Add assign | `أ += ٣` | `أ = أ + ٣` |
| `-=` | Subtract assign | `أ -= ٣` | `أ = أ - ٣` |

### 3.3 Comparison Operators

| Operator | Name | Example | Result Type |
|----------|------|---------|-------------|
| `==` | Equal | `أ == ب` | `منطقي` |
| `!=` | Not equal | `أ != ب` | `منطقي` |
| `<` | Less than | `أ < ب` | `منطقي` |
| `>` | Greater than | `أ > ب` | `منطقي` |

---

## 4. Control Flow

### 4.1 Conditional Statements

**If Statement:**

```baa
إذا (الشرط) {
    // Execute if condition is true
}

إذا (العدد > ٠) {
    اطبع("العدد موجب").
} وإلا إذا (العدد < ٠) {
    اطبع("العدد سالب").
} وإلا {
    اطبع("العدد صفر").
}
```

### 4.2 Loop Statements

**While Loop:**

```baa
طالما (الشرط) {
    // Loop body
}

عدد_صحيح ي = ٠.
طالما (ي < ١٠) {
    اطبع(ي).
    ي++.
}
```

**For Loop:**

```baa
لكل (التهيئة; الشرط; التحديث) {
    // Loop body
}

لكل (عدد_صحيح ي = ٠; ي < ١٠; ي++) {
    اطبع(ي).
}
```

### 4.3 Jump Statements

**Return Statement:**

```baa
إرجع القيمة.  // Return with value
إرجع.        // Return void
```

**Break and Continue:**

```baa
لكل (عدد_صحيح ي = ٠; ي < ١٠٠; ي++) {
    إذا (ي == ٥٠) {
        توقف.  // Exit loop
    }
    إذا (ي % ٢ == ٠) {
        استمر.  // Skip even numbers
    }
    اطبع(ي).
}
```

---

## 5. Functions

### 5.1 Function Definition

**Basic Function:**

```baa
عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب.
}
```

**Void Function:**

```baa
فراغ اطبع_رسالة(حرف* الرسالة) {
    اطبع(الرسالة).
}
```

### 5.2 Function Calls

```baa
عدد_صحيح النتيجة = جمع(٥، ٣).
اطبع_رسالة("مرحبا").
```

---

## 6. Variables and Constants

### 6.1 Variable Declarations

**Local and Global Variables:**

```baa
عدد_صحيح متغير_عام = ١٠.  // Global scope

فراغ دالة_مثال() {
    عدد_صحيح متغير_محلي = ٥.  // Local scope
}
```

### 6.2 Constants

```baa
ثابت عدد_صحيح الحد_الأقصى = ١٠٠.
ثابت عدد_حقيقي باي = ٣٫١٤١٥٩.
```

---

## 7. Arrays and Collections

### 7.1 Array Declaration

```baa
عدد_صحيح الأرقام[٥].              // Array of 5 integers
عدد_صحيح القيم[] = {١، ٢، ٣، ٤، ٥}. // Initialized array
حرف الاسم[٢٠].                   // Character array
```

### 7.2 Array Access

```baa
عدد_صحيح الأرقام[٥] = {١، ٢، ٣، ٤، ٥}.
عدد_صحيح الأول = الأرقام[٠].     // First element
الأرقام[١] = ١٠.                 // Modify element
```

---

## 8. Preprocessor Directives

### 8.1 File Inclusion

```baa
#تضمين "مكتبة.ب"        // Include local file
#تضمين <نظام.ب>         // Include system file
```

### 8.2 Macro Definitions

```baa
#تعريف الحد_الأقصى ١٠٠
#تعريف المربع(س) ((س) * (س))
```

### 8.3 Conditional Compilation

```baa
#إذا_عُرف تصحيح_الأخطاء
    اطبع("وضع تصحيح الأخطاء").
#وإلا
    اطبع("الوضع العادي").
#نهاية_إذا
```

---

## 9. Grammar Reference

### 9.1 Operator Precedence

| Precedence | Operators | Associativity |
|------------|-----------|---------------|
| 1 (highest) | `()` `[]` | Left to right |
| 2 | `!` `-` (unary) `++` `--` | Right to left |
| 3 | `*` `/` `%` | Left to right |
| 4 | `+` `-` | Left to right |
| 5 | `<` `>` `<=` `>=` | Left to right |
| 6 | `==` `!=` | Left to right |
| 7 | `&&` | Left to right |
| 8 | `\|\|` | Left to right |
| 9 (lowest) | `=` `+=` `-=` | Right to left |

---

## 10. Complete Examples

### 10.1 Hello World Program

```baa
// Simple Hello World in Baa
عدد_صحيح رئيسية() {
    اطبع("مرحبا بالعالم!").
    إرجع ٠.
}
```

### 10.2 Calculator Functions

```baa
// Calculator with Arabic functions
عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب.
}

عدد_صحيح طرح(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ - ب.
}

عدد_صحيح ضرب(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ * ب.
}

عدد_حقيقي قسمة(عدد_صحيح أ، عدد_صحيح ب) {
    إذا (ب == ٠) {
        اطبع("خطأ: القسمة على صفر!").
        إرجع ٠٫٠.
    }
    إرجع (عدد_حقيقي)أ / ب.
}

عدد_صحيح رئيسية() {
    عدد_صحيح العدد_الأول = ١٠.
    عدد_صحيح العدد_الثاني = ٥.
    
    اطبع("الجمع: ").
    اطبع(جمع(العدد_الأول، العدد_الثاني)).
    
    اطبع("الطرح: ").
    اطبع(طرح(العدد_الأول، العدد_الثاني)).
    
    اطبع("الضرب: ").
    اطبع(ضرب(العدد_الأول، العدد_الثاني)).
    
    اطبع("القسمة: ").
    اطبع(قسمة(العدد_الأول، العدد_الثاني)).
    
    إرجع ٠.
}
```

### 10.3 Array Processing

```baa
// Array processing example
فراغ اطبع_مصفوفة(عدد_صحيح المصفوفة[]، عدد_صحيح الحجم) {
    لكل (عدد_صحيح ي = ٠; ي < الحجم; ي++) {
        اطبع(المصفوفة[ي]).
        إذا (ي < الحجم - ١) {
            اطبع("، ").
        }
    }
    اطبع("\س").
}

عدد_صحيح جمع_مصفوفة(عدد_صحيح المصفوفة[]، عدد_صحيح الحجم) {
    عدد_صحيح المجموع = ٠.
    لكل (عدد_صحيح ي = ٠; ي < الحجم; ي++) {
        المجموع += المصفوفة[ي].
    }
    إرجع المجموع.
}

عدد_صحيح رئيسية() {
    عدد_صحيح الأرقام[] = {١، ٢، ٣، ٤، ٥، ٦، ٧، ٨، ٩، ١٠}.
    عدد_صحيح الحجم = ١٠.
    
    اطبع("المصفوفة: ").
    اطبع_مصفوفة(الأرقام، الحجم).
    
    عدد_صحيح المجموع = جمع_مصفوفة(الأرقام، الحجم).
    اطبع("المجموع: ").
    اطبع(المجموع).
    
    إرجع ٠.
}
```

### 10.4 Control Flow Example

```baa
// Fibonacci sequence calculator
عدد_صحيح فيبوناتشي(عدد_صحيح ن) {
    إذا (ن <= ١) {
        إرجع ن.
    }
    إرجع فيبوناتشي(ن - ١) + فيبوناتشي(ن - ٢).
}

فراغ اطبع_فيبوناتشي(عدد_صحيح العدد) {
    اطبع("سلسلة فيبوناتشي حتى ").
    اطبع(العدد).
    اطبع(": ").
    
    لكل (عدد_صحيح ي = ٠; ي <= العدد; ي++) {
        اطبع(فيبوناتشي(ي)).
        إذا (ي < العدد) {
            اطبع("، ").
        }
    }
    اطبع("\س").
}

منطقي هو_عدد_أولي(عدد_صحيح العدد) {
    إذا (العدد <= ١) {
        إرجع خطأ.
    }
    إذا (العدد <= ٣) {
        إرجع صحيح.
    }
    إذا (العدد % ٢ == ٠ || العدد % ٣ == ٠) {
        إرجع خطأ.
    }
    
    لكل (عدد_صحيح ي = ٥; ي * ي <= العدد; ي += ٦) {
        إذا (العدد % ي == ٠ || العدد % (ي + ٢) == ٠) {
            إرجع خطأ.
        }
    }
    إرجع صحيح.
}

عدد_صحيح رئيسية() {
    عدد_صحيح العدد = ١٠.
    
    اطبع_فيبوناتشي(العدد).
    
    اطبع("الأعداد الأولية من ١ إلى ").
    اطبع(العدد).
    اطبع(": ").
    
    لكل (عدد_صحيح ي = ٢; ي <= العدد; ي++) {
        إذا (هو_عدد_أولي(ي)) {
            اطبع(ي).
            اطبع(" ").
        }
    }
    اطبع("\س").
    
    إرجع ٠.
}
```

### 10.5 String Processing

```baa
// String utilities
عدد_صحيح طول_النص(حرف* النص) {
    عدد_صحيح الطول = ٠.
    طالما (النص[الطول] != '\ص') {
        الطول++.
    }
    إرجع الطول.
}

فراغ نسخ_النص(حرف* الهدف، حرف* المصدر) {
    عدد_صحيح ي = ٠.
    طالما (المصدر[ي] != '\ص') {
        الهدف[ي] = المصدر[ي].
        ي++.
    }
    الهدف[ي] = '\ص'.
}

منطقي مقارنة_النصوص(حرف* النص_الأول، حرف* النص_الثاني) {
    عدد_صحيح ي = ٠.
    طالما (النص_الأول[ي] != '\ص' && النص_الثاني[ي] != '\ص') {
        إذا (النص_الأول[ي] != النص_الثاني[ي]) {
            إرجع خطأ.
        }
        ي++.
    }
    إرجع النص_الأول[ي] == النص_الثاني[ي].
}

عدد_صحيح رئيسية() {
    حرف التحية[٢٠].
    حرف* الرسالة = "مرحبا بالعالم".
    
    نسخ_النص(التحية، الرسالة).
    
    اطبع("النص المنسوخ: ").
    اطبع(التحية).
    اطبع("\س").
    
    اطبع("طول النص: ").
    اطبع(طول_النص(التحية)).
    اطبع("\س").
    
    إذا (مقارنة_النصوص(التحية، الرسالة)) {
        اطبع("النصوص متطابقة").
    } وإلا {
        اطبع("النصوص مختلفة").
    }
    
    إرجع ٠.
}
```

### 10.6 Advanced Example with Preprocessor

**Note:** This example demonstrates planned features (struct, typedef, pointers) that are not yet fully implemented in the current version. It shows the intended syntax for future releases.

```baa
#تضمين <stdio.h>
#تعريف الحد_الأقصى ١٠٠
#تعريف المربع(س) ((س) * (س))
#تعريف الأكبر(أ، ب) ((أ) > (ب) ? (أ) : (ب))

#إذا_عُرف تصحيح_الأخطاء
    #تعريف اطبع_تصحيح(رسالة) اطبع("[تصحيح] " رسالة "\س")
#وإلا
    #تعريف اطبع_تصحيح(رسالة)
#نهاية_إذا

// Student grade management system (PLANNED FEATURE)
typedef struct {
    حرف الاسم[٥٠].
    عدد_صحيح العمر.
    عدد_حقيقي الدرجات[٥].
    عدد_صحيح عدد_المواد.
} طالب.

عدد_حقيقي حساب_المتوسط(طالب* الطالب) {
    اطبع_تصحيح("حساب المتوسط للطالب").
    
    إذا (الطالب->عدد_المواد == ٠) {
        إرجع ٠٫٠.
    }
    
    عدد_حقيقي المجموع = ٠٫٠.
    لكل (عدد_صحيح ي = ٠; ي < الطالب->عدد_المواد; ي++) {
        المجموع += الطالب->الدرجات[ي].
    }
    
    إرجع المجموع / الطالب->عدد_المواد.
}

حرف تقدير_الطالب(عدد_حقيقي المتوسط) {
    إذا (المتوسط >= ٩٠٫٠) إرجع 'أ'.
    وإلا إذا (المتوسط >= ٨٠٫٠) إرجع 'ب'.
    وإلا إذا (المتوسط >= ٧٠٫٠) إرجع 'ج'.
    وإلا إذا (المتوسط >= ٦٠٫٠) إرجع 'د'.
    وإلا إرجع 'ف'.
}

فراغ اطبع_تقرير_الطالب(طالب* الطالب) {
    عدد_حقيقي المتوسط = حساب_المتوسط(الطالب).
    حرف التقدير = تقدير_الطالب(المتوسط).
    
    اطبع("=== تقرير الطالب ===\س").
    اطبع("الاسم: ").
    اطبع(الطالب->الاسم).
    اطبع("\سالعمر: ").
    اطبع(الطالب->العمر).
    اطبع("\سعدد المواد: ").
    اطبع(الطالب->عدد_المواد).
    اطبع("\سالدرجات: ").
    
    لكل (عدد_صحيح ي = ٠; ي < الطالب->عدد_المواد; ي++) {
        اطبع(الطالب->الدرجات[ي]).
        إذا (ي < الطالب->عدد_المواد - ١) {
            اطبع("، ").
        }
    }
    
    اطبع("\سالمتوسط: ").
    اطبع(المتوسط).
    اطبع("\سالتقدير: ").
    اطبع(التقدير).
    اطبع("\س").
}

عدد_صحيح رئيسية() {
    طالب الطالب = {
        "أحمد محمد",
        ٢٠,
        {٨٥٫٥، ٩٢٫٠، ٧٨٫٥، ٩٠٫٠، ٨٧٫٥},
        ٥
    }.
    
    اطبع_تقرير_الطالب(&الطالب).
    
    إرجع ٠.
}
```

## Additional Resources

For more detailed information, see:

- [Language Overview](LANGUAGE_OVERVIEW.md) - High-level language features
- [Arabic Features](ARABIC_FEATURES.md) - Arabic-specific capabilities
- [Parser Documentation](../02_COMPILER_ARCHITECTURE/PARSER.md) - Grammar implementation
- [Lexer Documentation](../02_COMPILER_ARCHITECTURE/LEXER.md) - Token definitions

---

*This document is part of the [Baa Language Documentation](../index.md)*
