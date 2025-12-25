# Baa Language Specification (v0.0.7)

Baa (باء) is a compiled systems programming language using Arabic syntax. It compiles directly to native machine code (via Assembly/GCC) on Windows.

## 1. Structure
*   **File Format:** Source files must be **UTF-8** encoded. Extension `.b` is recommended.
*   **Statements:** Every statement must end with a period (`.`).
*   **Comments:** Single-line comments start with `//`.

## 2. Variables & Types
Baa is statically typed. It mimics C data types using Arabic terms.

| Baa Type | C Equivalent | Description |
| :--- | :--- | :--- |
| **`صحيح`** | `int` | Integer number (64-bit currently) |

### 2.1. Declaration
Creates a new variable.
**Syntax:** `صحيح <identifier> = <expression>.`

```baa
صحيح س = ٥٠.
```

### 2.2. Assignment
Updates the value of an existing variable.
**Syntax:** `<identifier> = <expression>.`

```baa
س = س + ١.
```

## 3. Input / Output
**Syntax:** `اطبع <expression>.`

Prints a signed integer followed by a newline.

```baa
اطبع ١٠٠.
```

## 4. Control Flow

### 4.1. Blocks
Statements can be grouped together using curly braces `{ ... }` to create a scope.

### 4.2. Conditional (If)
Executes a block of code only if the condition is true (non-zero).

**Syntax:**
```baa
إذا (<condition>) {
    <statements>
}
```

### 4.3. Loops (While)
Repeats a block of code as long as the condition is true.

**Syntax:**
```baa
طالما (<condition>) {
    <statements>
}
```

## 5. Expressions & Operators

### 5.1. Literals
*   **Arabic-Indic Digits:** `٠` `١` `٢` `٣` `٤` `٥` `٦` `٧` `٨` `٩`
*   **Western Digits:** `0`-`9`

### 5.2. Operators

| Operator | Description | Result |
| :--- | :--- | :--- |
| `+` | Addition | Integer |
| `-` | Subtraction | Integer |
| `==` | Equals | 1 (True) or 0 (False) |
| `!=` | Not Equals | 1 (True) or 0 (False) |

### 5.3. Identifiers
Variable names can contain Arabic letters, English letters, and underscores. They must not start with a digit.

## 6. Program Exit
Exits the program and returns a status code to the operating system.

**Syntax:** `إرجع <code >.`

```baa
إرجع ٠.
```

## 7. Complete Example

```baa
// برنامج العد التنازلي
صحيح العداد = ٥.

طالما (العداد != ٠) {
    اطبع العداد.
    العداد = العداد - ١.
}

إرجع ٠.
```