# Baa Language Specification (v0.0.9)

Baa (باء) is a compiled systems programming language using Arabic syntax. It compiles directly to native machine code (via Assembly/GCC) on Windows.

## 1. Program Structure
A Baa program is a collection of **Global Variables** and **Functions**.
*   **File Format:** Source files must be **UTF-8** encoded. Extension `.b`.
*   **Entry Point:** Execution starts at the function named **`الرئيسية`** (Main).
*   **Statements:** End with a period (`.`).
*   **Comments:** Start with `//`.

## 2. Variables & Types
Baa is statically typed.

| Baa Type | C Equivalent | Description |
| :--- | :--- | :--- |
| **`صحيح`** | `int` | Integer number (64-bit currently) |

### 2.1. Global Variables
Defined outside of any function. Visible everywhere.
```baa
صحيح النسخة = ١.
```

### 2.2. Local Variables
Defined inside a function. Visible only within that function.
```baa
صحيح الرئيسية() {
    صحيح س = ٥٠. // Local variable
    ...
}
```

### 2.3. Assignment
Updates the value of an existing variable.
```baa
س = س + ١.
```

## 3. Functions
Functions allow code reuse and modularity.

### 3.1. Definition
**Syntax:** `Type Name(Params) { Body }`

```baa
صحيح جمع(صحيح أ, صحيح ب) {
    إرجع أ + ب.
}
```

### 3.2. Calling
**Syntax:** `Name(Args)`

```baa
صحيح الناتج = جمع(١٠, ٢٠).
```

### 3.3. The Entry Point (`الرئيسية`)
Every program must have this function. It takes no arguments and returns an integer (exit code).

```baa
صحيح الرئيسية() {
    // Your code here
    إرجع ٠.
}
```

## 4. Input / Output
**Syntax:** `اطبع <expression>.`

Prints a signed integer followed by a newline.

```baa
اطبع ١٠٠.
```

## 5. Control Flow

### 5.1. Conditional (If)
Executes a block if the condition is true.

```baa
إذا (س > ١٠) {
    اطبع ١.
}
```

### 5.2. Loops (While)
Repeats a block as long as the condition is true.

```baa
طالما (س > ٠) {
    س = س - ١.
}
```

## 6. Operators

Baa follows standard mathematical order of operations (PEMDAS).

### 6.1. Arithmetic
| Operator | Description | Example | Result |
| :--- | :--- | :--- | :--- |
| `+` | Addition | `١٠ + ٥` | `١٥` |
| `-` | Subtraction | `١٠ - ٥` | `٥` |
| `*` | Multiplication | `١٠ * ٥` | `٥٠` |
| `/` | Division (Integer) | `١٠ / ٣` | `٣` |
| `%` | Modulo (Remainder) | `١٠ % ٣` | `١` |

### 6.2. Comparison
Returns `1` for True, `0` for False.

| Operator | Description | Example |
| :--- | :--- | :--- |
| `==` | Equal | `٥ == ٥` |
| `!=` | Not Equal | `٥ != ٤` |
| `<` | Less Than | `٣ < ٥` |
| `>` | Greater Than | `٥ > ٣` |
| `<=` | Less or Equal | `٥ <= ٥` |
| `>=` | Greater or Equal | `٥ >= ٤` |

## 7. Complete Example

```baa
// متغير عام (Global)
صحيح عامل_الضرب = ٢.

// تعريف دالة (Function Definition)
صحيح ضاعف_الرقم(صحيح س) {
    إرجع س * عامل_الضرب.
}

// نقطة البداية (Entry Point)
صحيح الرئيسية() {
    صحيح البداية = ١٠.
    
    // ٢٠
    صحيح الناتج = ضاعف_الرقم(البداية).
    
    // باقي القسمة: ٢٠ % ٣ = ٢
    صحيح الباقي = الناتج % ٣.
    
    اطبع الباقي.
    
    إرجع ٠.
}
```