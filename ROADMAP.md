# Baa Roadmap

> Track the development progress of the Baa programming language.
> **Current Status:** Phase 2 (Architecture Overhaul)

---

## 🏗️ Phase 2: Architecture Overhaul (The Professional Arc)

*Goal: Transform Baa from a linear prototype into a modular, robust compiler toolchain.*

### v0.2.0: The Driver (CLI & Build System) 🖥️
- [x] **CLI Argument Parser** — Implement a custom argument parser to handle flags manually.
- [x] **Input/Output Control** (`-o`, `-S`, `-c`).
- [x] **Information Flags** (`--version`, `--help`, `-v`).
- [x] **Build Pipeline** — Orchestrate Lexer -> Parser -> Codegen -> GCC.

### v0.2.1: Polish & Branding 🎨
- [x] **Executable Icon** — Embed `.ico` resource.
- [x] **Metadata** — Version info, Copyright, Description in `.exe`.

### v0.2.2: The Diagnostic Engine 🚨
- [x] **Source Tracking** — Update `Token` and `Node` to store Filename, Line, and Column.
- [x] **Error Module** — Create a dedicated error reporting system.
- [x] **Pretty Printing** — Display errors with context (`^` pointers).
- [x] **Panic Recovery** — Continue parsing after errors.

### v0.2.3: Distribution & Updater 📦
- [x] **Windows Installer** — Create `setup.exe` using Inno Setup.
- [x] **PATH Integration** — Add compiler to system environment variables.
- [x] **Self-Updater** — Implement `baa update` command.

### v0.2.4: The Semantic Pass (Type Checker) 🧠
- [x] **File Extension Migration** — Change `.b` to `.baa`. Reserved `.baahd` for headers.
- [x] **Pass Separation** — Completely separate Parsing from Code Generation.
    - `parse()` returns a raw AST.
    - `analyze()` walks the AST to check types and resolve symbols.
    - `codegen()` takes a validated AST.
- [x] **Symbol Resolution** — Check for undefined variables before code generation starts.
- [x] **Scope Analysis** — Implement scope stack to properly handle nested blocks and variable shadowing.
- [x] **Type Checking** — Validate assignments (int = string now fails during semantic analysis).

### v0.2.5: Multi-File & Include System 🔗
- [x] **File Extension Migration** — Change `.b` to `.baa`. Reserved `.baahd` for headers.
- [x] **Include Directive** — `#تضمين "file.baahd"` (C-style `#include`).
- [x] **Header Files** — `.baahd` extension for declarations (function signatures, extern variables).
- [x] **Function Prototypes** — Declarations without types `صحيح دالة().` (Added).
- [x] **Multi-file CLI** — Accept multiple inputs: `baa main.baa lib.baa -o out.exe`.
- [x] **Linker Integration** — Compile each file to `.o` then link together.

### v0.2.6: Preprocessor Directives 📝
- [x] **Define** — `#تعريف اسم قيمة` for compile-time constants.
- [x] **Conditional** — `#إذا_عرف`, `#إذا_عرف`, `#إذا_لم_يعرف`, `#وإلا`, `#وإلا_إذا`, `#نهاية_إذا` for conditional compilation.
- [x] **Undefine** — `#الغاء_تعريف` to remove definitions.

### v0.2.7: Constants & Immutability 🔒
- [x] **Constant Keyword** — `ثابت` for immutable variables: `ثابت صحيح حد = ١٠٠.`
- [x] **Const Checking** — Semantic error on reassignment of constants.
- [x] **Array Constants** — Support constant arrays.

### v0.2.8: Warnings & Diagnostics ⚠️
- [x] **Warning System** — Separate warnings from errors (non-fatal).
- [x] **Unused Variables** — Warn if variable declared but never used.
- [x] **Dead Code** — Warn about code after `إرجع` or `توقف`.
- [x] **`-W` Flags** — `-Wall`, `-Werror` to control warning behavior.

### v0.2.9: Input & UX Polish 🎨
- [x] **Input Statement** — `اقرأ س.` (scanf) for reading user input.
- [x] **Boolean Type** — `منطقي` type with `صواب`/`خطأ` literals.
- [x] **Colored Output** — ANSI colors for errors (red), warnings (yellow). *(Implemented in v0.2.8)*
- [x] **Compile Timing** — Show compilation time with `-v`.

---

## ⚙️ Phase 3: The Intermediate Representation (v0.3.x)

*Goal: Decouple the language from x86 Assembly to enable optimizations and multiple backends.*

### v0.3.0: Baa IR (Intermediate Representation)
- [ ] **IR Design** – Define a simplified, linear instruction set (Three-Address Code).
- Example: `ADD t0, t1, t2` (virtual registers).
- [ ] **AST to IR** – Write a lowering pass to convert the AST tree into a Control Flow Graph (CFG) of IR blocks.
- [ ] **IR Printer** – Debug tool to print the IR in a readable format (`--dump-ir`).

### v0.3.1: The Optimizer ⚡
- [ ] **Control Flow Analysis** – Detect unreachable blocks.
- [ ] **Dead Code Elimination** – Remove instructions that don't affect the output.
- [ ] **Constant Propagation** – If `x = 10` and `y = x + 5`, replace with `y = 15`.
- [ ] **Loop Invariant Code Motion** – Move static calculations out of loops.

### v0.3.2: The Backend (Target Independence)
- [ ] **Instruction Selection** – Convert IR to abstract machine instructions.
- [ ] **Register Allocation** – Map virtual registers (t0, t1...) to physical x64 registers (RAX, RBX...) using Linear Scan or Graph Coloring.
- [ ] **Code Emission** – Write the final assembly text.

---

## 📚 Phase 3.5: Language Completeness (v0.3.3 - v0.3.9)

*Goal: Add essential features to make Baa practical for real-world programs before Phase 4.*

### v0.3.3: Array Initialization 📊
**Goal:** Enable direct initialization of arrays with values.
#### Features
- [ ] **Array Literal Syntax** – Initialize arrays with comma-separated values using `{` `}`.
  
**Syntax:**
```baa
صحيح قائمة[٥] = {١، ٢، ٣، ٤، ٥}.

// With Arabic comma (،) or regular comma (,)
صحيح أرقام[٣] = {١٠، ٢٠، ٣٠}.
```

#### Implementation Tasks
- [ ] **Parser**: Handle `{` `}` initializer list after array declaration.
- [ ] **Parser**: Support both Arabic comma `،` (U+060C) and regular comma `,` as separators.
- [ ] **Semantic Analysis**: Verify initializer count matches array size.
- [ ] **Codegen**: Generate sequential assignments in `.data` section (for globals) or stack initialization (for locals).

#### Deferred to v0.3.8
- Multi-dimensional arrays: `صحيح مصفوفة[٣][٤].`
- Array length operator: `صحيح طول = حجم(قائمة).`

---

### v0.3.4: Enumerations & Structures 🏗️
**Goal:** Add compound types for better code organization and type safety.

#### Features
- [ ] **Enum Declaration** – Named integer constants with type safety.
- [ ] **Struct Declaration** – Group related data into composite types.
- [ ] **Member Access** – Use `:` (colon) operator for accessing members.

**Complete Example:**
```baa
// ١. تعريف التعداد (Enumeration)
// يحدد مجموعة من الألوان الممكنة
تعداد لون {
    أحمر،
    أزرق،
    أسود،
    أبيض
}
// ٢. تعريف الهيكل (Structure)
// يجمع بيانات السيارة
هيكل سيارة {
    نص موديل.
    صحيح سنة_الصنع.
    تعداد لون لون_السيارة.
}

صحيح الرئيسية() {
    // تعريف متغير من نوع الهيكل
    هيكل سيارة س.
    
    // ٣. استخدام النقطتين (:) للوصول لأعضاء الهيكل
    س:موديل = "تويوتا كورولا".
    س:سنة_الصنع = ٢٠٢٤.
    
    // ٤. استخدام النقطتين (:) للوصول لقيم التعداد
    س:لون_السيارة = لون:أحمر.
    
    // طباعة البيانات
    اطبع "بيانات السيارة الجديدة:".
    اطبع س:موديل.
    اطبع س:سنة_الصنع.
    
    // ٥. استخدام التعداد في الشروط
    إذا (س:لون_السيارة == لون:أحمر) {
        اطبع "تحذير: السيارات الحمراء سريعة!".
    } وإلا {
        اطبع "لون السيارة هادئ.".
    }
    
    إرجع ٠.
}
```

#### Implementation Tasks
**Enumerations:**
- [ ] **Token**: Add `TOKEN_ENUM` for `تعداد` keyword.
- [ ] **Parser**: Parse enum declaration: `تعداد <name> { <members> }`.
- [ ] **Parser**: Support Arabic comma `،` between enum members.
- [ ] **Semantic**: Auto-assign integer values (0, 1, 2...).
- [ ] **Semantic**: Enum values accessible via `<enum_name>:<value_name>`.
- [ ] **Type System**: Add `TYPE_ENUM` to `DataType`.

**Structures:**
- [ ] **Token**: Add `TOKEN_STRUCT` for `هيكل` keyword.
- [ ] **Token**: Add `TOKEN_COLON` for `:` (already exists, verify usage).
- [ ] **Parser**: Parse struct declaration: `هيكل <name> { <fields> }`.
- [ ] **Parser**: Parse struct instantiation: `هيكل <name> <var>.`
- [ ] **Parser**: Parse member access: `<var>:<member>`.
- [ ] **Semantic**: Track struct definitions in symbol table.
- [ ] **Semantic**: Validate member access against struct definition.
- [ ] **Memory Layout**: Calculate field offsets with padding/alignment.
- [ ] **Codegen**: Emit struct definitions and member access code.

### v0.3.5: Character Type 📝
**Goal:** Add proper character type to align with C conventions.

#### Features
- [ ] **Character Type (`حرف`)** – Proper 1-byte character type (like C's `char`).
- [ ] **String-Char Relationship** – Strings (`نص`) become arrays of characters (`حرف[]`).

**Syntax:**
```baa
// Character variable
حرف ح = 'أ'.
// String as char array (internal representation)
نص اسم = "أحمد".  // Equivalent to: حرف اسم[] = {'أ', 'ح', 'م', 'د', '\0'}.
```

#### Implementation Tasks
- [ ] **Token**: Already have `TOKEN_CHAR` for literals.
- [ ] **Token**: Add `TOKEN_KEYWORD_CHAR` for `حرف` type keyword.
- [ ] **Type System**: Add `TYPE_CHAR` to `DataType` enum.
- [ ] **Semantic**: Distinguish between `char` and `int` (currently chars are ints).
- [ ] **Codegen**: Generate 1-byte storage for `حرف` (currently 8-byte).
- [ ] **String Representation**: Update internal string handling to use `char*`.

#### Deferred to v0.3.9
- String operations: `طول_نص()`, `دمج_نص()`, `قارن_نص()`
- String indexing: `اسم[٠]` returns `حرف`

---

### v0.3.6: System Improvements 🔧
**Goal:** Refine and enhance existing compiler systems.

#### Focus Areas
- [ ] **Error Messages** – Improve clarity and helpfulness of diagnostic messages.
- [ ] **Code Quality** – Refactor complex functions, improve code organization.
- [ ] **Memory Management** – Fix memory leaks, improve buffer handling.
- [ ] **Performance** – Profile and optimize slow compilation paths.
- [ ] **Documentation** – Update all docs to reflect v0.3.3-0.3.5 changes.
- [ ] **Edge Cases** – Fix known bugs and handle corner cases.

#### Specific Improvements
- [ ] Improve panic mode recovery in parser.
- [ ] Better handling of UTF-8 edge cases in lexer.
- [ ] Optimize symbol table lookups (consider hash table).
- [ ] Add more comprehensive error recovery.
- [ ] Improve codegen output readability (comments in assembly).

---

### v0.3.7: Testing & Quality Assurance ✅
**Goal:** Establish robust testing infrastructure and fix accumulated issues.

#### Test System
- [ ] **Test Framework** – Create automated test runner.
  - Script to compile and run `.baa` test files.
  - Compare actual output vs expected output.
  - Report pass/fail with clear diagnostics.

- [ ] **Test Categories**:
  - [ ] **Lexer Tests** – Token generation, UTF-8 handling, preprocessor.
  - [ ] **Parser Tests** – Syntax validation, error recovery.
  - [ ] **Semantic Tests** – Type checking, scope validation.
  - [ ] **Codegen Tests** – Correct assembly output, execution results.
  - [ ] **Integration Tests** – Full programs with expected output.

- [ ] **Test Coverage**:
  - [ ] All language features (v0.0.1 - v0.3.6).
  - [ ] Edge cases and corner cases.
  - [ ] Error conditions (syntax errors, type mismatches, etc.).
  - [ ] Multi-file compilation scenarios.
  - [ ] Preprocessor directive combinations.

#### Bug Fixes & Refinements
- [ ] **Known Issues** – Fix all open bugs from previous versions.
- [ ] **Regression Testing** – Ensure new features don't break old code.
- [ ] **Stress Testing** – Test with large files, deep nesting, many symbols.
- [ ] **Arabic Text Edge Cases** – Test various Arabic Unicode scenarios.

#### Documentation
- [ ] **Testing Guide** – Document how to run tests and add new ones.
- [ ] **Known Limitations** – Document current language limitations.
- [ ] **Migration Guide** – Help users update code for v0.3.x changes.

---

### v0.3.8: Advanced Arrays & String Operations 📐
**Goal:** Complete array and string functionality.

#### Features
- [ ] **Multi-dimensional Arrays**:
  ```baa
  صحيح مصفوفة[٣][٤].
  مصفوفة[٠][٠] = ١٠.
  مصفوفة[١][٢] = ٢٠.
  ```
- [ ] **Array Length Operator**:
  ```baa
  صحيح قائمة[١٠].
  صحيح الطول = حجم(قائمة).  // Returns 10
  ```

- [ ] **Array Bounds Checking** (Optional debug mode):
  - Runtime checks with `-g` flag.
  - Panic on out-of-bounds access.

#### Implementation
- [ ] **Parser**: Parse multi-dimensional array declarations and access.
- [ ] **Semantic**: Track array dimensions in symbol table.
- [ ] **Codegen**: Calculate offsets for multi-dimensional arrays (row-major order).
- [ ] **Built-in**: Implement `حجم()` as compiler intrinsic or standard function.

---

### v0.3.9: String Operations Library 🔤
**Goal:** Make strings practical for real programs.

#### Features
- [ ] **String Length**:
  ```baa
  نص اسم = "أحمد".
  صحيح الطول = طول_نص(اسم).  // Returns 4
  ```

- [ ] **String Concatenation**:
  ```baa
  نص كامل = دمج_نص(اسم, " علي").  // "أحمد علي"
  ```

- [ ] **String Comparison**:
  ```baa
  صحيح نتيجة = قارن_نص(اسم, "محمد").  // 0 if equal, -1/<0/1 otherwise
  ```

- [ ] **String Indexing** (read-only):
  ```baa
  حرف أول = اسم[٠].  // Get character at index
  ```
- [ ] **String Copy**:
  ```baa
  نص نسخة = نسخ_نص(اسم).
  ```

#### Implementation
- [ ] **Standard Library**: Create `baalib.baa` with string functions.
- [ ] **C Integration**: Wrap C string functions (`strlen`, `strcmp`, `strcpy`, etc.).
- [ ] **UTF-8 Aware**: Ensure functions handle multi-byte Arabic characters correctly.
- [ ] **Memory Safety**: Document string memory management rules.

---

## 📚 Phase 4: Advanced Features & Standard Library (v0.4.x)

*Goal: Make Baa useful for real-world applications.*

### v0.4.0: Pointers & Memory Management 🎯
**Goal:** Add manual memory management capabilities.
- [ ] **Pointer Type**:
  ```baa
  صحيح* مؤشر.  // Pointer to integer
  ```
- [ ] **Address-of Operator** — `&` (or Arabic equivalent like `عنوان`).
- [ ] **Dereference Operator** — `*` (or Arabic equivalent like `قيمة`).
- [ ] **Dynamic Allocation**:
  ```baa
  صحيح* ذاكرة = حجز_ذاكرة(١٠ * حجم(صحيح)).  // malloc equivalent
  تحرير_ذاكرة(ذاكرة).  // free equivalent
  ```

- [ ] **Null Pointer** – `عدم` keyword for NULL.

### v0.4.1: Formatted Output & Input 🖨️
**Goal:** Professional I/O capabilities.

- [ ] **Formatted Output**:
  ```baa
  // Printf-style
  اطبع_منسق("الاسم: %s، العمر: %d", اسم, عمر).
  
  // Or interpolation
  اطبع("الاسم: {اسم}، العمر: {عمر}").
  ```
- [ ] **User Input**:
  ```baa
  نص إدخال = اقرأ_سطر().
  صحيح رقم = اقرأ_رقم().
  ```

### v0.4.2: File I/O 📁
**Goal:** Read and write files.

- [ ] **File Operations**:
  ```baa
  صحيح ملف = فتح_ملف("data.txt", "قراءة").
  نص سطر = اقرأ_سطر_من_ملف(ملف).
  اكتب_إلى_ملف(ملف, "نص جديد").
  اغلق_ملف(ملف).
  ```

### v0.4.3: Standard Library (BaaLib) 📚
- [ ] **IO Module** — File reading/writing (`ملف.اقرأ`, `ملف.اكتب`).
- [ ] **Math Module** – Advanced math functions:
  ```baa
  صحيح جذر = جذر_تربيعي(١٦).
  صحيح قوة = أس(٢, ١٠).
  ```
- [ ] **System Module** — Executing commands, environment variables.
- [ ] **Time Module** – Date/time operations.

### v0.4.4: Floating Point Support 🔢
**Goal:** Add decimal number support.

- [ ] **Float Type (`عشري`)**:
  ```baa
  عشري باي = ٣.١٤١٥٩.
  عشري نصف = ٠.٥.
  ```

- [ ] **Float Operations** – Arithmetic, comparison, math functions.
- [ ] **Type Conversion** – `صحيح إلى عشري()`, `عشري إلى صحيح()`.

### v0.4.5: Error Handling 🛡️
**Goal:** Graceful error management.

- [ ] **Assertions**:
  ```baa
  تأكد(س > ٠, "س يجب أن يكون موجباً").
  ```

- [ ] **Error Returns** – Convention for returning error codes.
- [ ] **Panic/Abort** – `توقف_فوري("رسالة خطأ")`.
 
---

## 🚀 Phase 5: Self-Hosting (v1.0.0)

*Goal: The ultimate proof of capability — Baa compiling itself.*

- [ ] **Rewrite Compiler** — Port `src/*.c` to `src/*.b`.
- [ ] **Bootstrap** — Use the C compiler (v0.4) to compile the Baa compiler (v1.0).
- [ ] **Optimization** — Ensure the Baa-written compiler is as fast as the C one.

---

## 📦 Phase 1: Language Foundation (v0.1.x) - Completed

<details>
<summary><strong>v0.1.3</strong> — Control Flow & Optimizations</summary>

- [x] **Extended If** — Support `وإلا` (Else) and `وإلا إذا` (Else If) blocks.
- [x] **Switch Statement** — `اختر` (Switch), `حالة` (Case), `افتراضي` (Default)
- [x] **Constant Folding** — Compile-time math (`١ + ٢` → `٣`)

</details>

<details>
<summary><strong>v0.1.2</strong> — Recursion & Strings</summary>

- [x] **Recursion** — Stack alignment fix
- [x] **String Variables** — `نص` type
- [x] **Loop Control** — `توقف` (Break) & `استمر` (Continue)

</details>

<details>
<summary><strong>v0.1.1</strong> — Structured Data</summary>

- [x] **Arrays** — Fixed-size stack arrays (`صحيح قائمة[١٠]`)
- [x] **For Loop** — `لكل (..؛..؛..)` syntax
- [x] **Logic Operators** — `&&`, `||`, `!` with short-circuiting
- [x] **Postfix Operators** — `++`, `--`

</details>

<details>
<summary><strong>v0.1.0</strong> — Text & Unary</summary>

- [x] **Strings** — String literal support (`"..."`)
- [x] **Characters** — Character literals (`'...'`)
- [x] **Printing** — Updated `اطبع` to handle multiple types
- [x] **Negative Numbers** — Unary minus support

</details>

<details>
<summary><strong>v0.0.9</strong> — Advanced Math</summary>

- [x] **Math** — Multiplication, Division, Modulo
- [x] **Comparisons** — Greater/Less than logic (`<`, `>`, `<=`, `>=`)
- [x] **Parser** — Operator Precedence Climbing (PEMDAS)

</details>

<details>
<summary><strong>v0.0.8</strong> — Functions</summary>

- [x] **Functions** — Function definitions and calls
- [x] **Entry Point** — Mandatory `الرئيسية` exported as `main`
- [x] **Scoping** — Global vs Local variables
- [x] **Windows x64 ABI** — Register passing, stack alignment, shadow space

</details>

<details>
<summary><strong>v0.0.7</strong> — Loops</summary>

- [x] **While Loop** — `طالما` implementation
- [x] **Assignments** — Update existing variables

</details>

<details>
<summary><strong>v0.0.6</strong> — Control Flow</summary>

- [x] **If Statement** — `إذا` with blocks
- [x] **Comparisons** — `==`, `!=`
- [x] **Documentation** — Comprehensive Internals & API docs

</details>

<details>
<summary><strong>v0.0.5</strong> — Type System</summary>

- [x] Renamed `رقم` to `صحيح` (int)
- [x] Single line comments (`//`)

</details>

<details>
<summary><strong>v0.0.4</strong> — Variables</summary>

- [x] Variable declarations and stack offsets
- [x] Basic symbol table

</details>

<details>
<summary><strong>v0.0.3</strong> — I/O</summary>

- [x] `اطبع` (Print) via Windows `printf`
- [x] Multiple statements support

</details>

<details>
<summary><strong>v0.0.2</strong> — Math</summary>

- [x] Arabic numerals (٠-٩)
- [x] Addition and subtraction

</details>

<details>
<summary><strong>v0.0.1</strong> — Foundation</summary>

- [x] Basic pipeline: Lexer → Parser → Codegen → GCC

</details>

---

*For detailed changes, see the [Changelog](CHANGELOG.md)*
