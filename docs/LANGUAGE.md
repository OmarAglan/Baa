# Baa Language Specification (v0.0.4)

Baa (باء) is a compiled systems programming language using Arabic syntax. It compiles directly to native machine code (via Assembly/GCC) on Windows.

## 1. File Format
*   **Extension:** `.b` (recommended).
*   **Encoding:** Must be **UTF-8**.
*   **Structure:** A list of statements executed sequentially.

## 2. Statements
Every statement in Baa must end with a period (`.`).

### 2.1. Return / Exit
Exits the program and returns a status code to the operating system.

**Syntax:**
```baa
إرجع <expression>.
```

**Example:**
```baa
إرجع ٠.
```

### 2.2. Print
Prints a signed integer followed by a new line to the console.

**Syntax:**
```baa
اطبع <expression>.
```

**Example:**
```baa
اطبع ١٠٠.
```

### 2.3. Variables & Types
Baa is statically typed. It mimics C data types using Arabic terms.

| Baa Type | C Equivalent | Description |
| :--- | :--- | :--- |
| **`صحيح`** | `int` | Integer number (64-bit currently) |

**Declaration Syntax:**
```baa
صحيح <identifier> = <expression>.
```

**Example:**
```baa
// هذا تعليق (Comment)
صحيح س = ٥٠.
```

## 3. Expressions

### 3.1. Literals
*   **Arabic-Indic Digits:** `٠` `١` `٢` `٣` `٤` `٥` `٦` `٧` `٨` `٩`
*   **Western Digits:** `0` `1` `2` `3` `4` `5` `6` `7` `8` `9`

### 3.2. Operators
Supported math operators:
*   `+` : Addition
*   `-` : Subtraction

### 3.3. Identifiers
Variable names can contain Arabic letters, English letters, and underscores. They must not start with a digit.

## 4. Complete Example

```baa
رقم الأول = ١٠.
رقم الثاني = ٢٠.
رقم المجموع = الأول + الثاني.

اطبع المجموع.
إرجع ٠.
```