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
- [ ] **Pass Separation** — Completely separate Parsing from Code Generation.
    - `parse()` returns a raw AST.
    - `analyze()` walks the AST to check types and resolve symbols.
    - `codegen()` takes a validated AST.
- [ ] **Scope Analysis** — Implement a tree-based symbol table (not just global/local lists) to handle nested blocks correctly.
- [ ] **Type Checking** — Validate assignments (`int = string` should fail here, not in ASM).
- [ ] **Symbol Resolution** — Check for undefined variables before code generation starts.

### v0.2.5: Multi-File Compilation (The Linker) 🔗
- [ ] **Import System** — Add `استورد "file.b"` syntax.
- [ ] **File Resolution** — Locate files relative to the current source or standard path.
- [ ] **Header Generation** — internal mechanism to expose public symbols.
- [ ] **Driver Update** — Update CLI to accept multiple input files (`baa main.b lib.b`).

---

## ⚙️ Phase 3: The Intermediate Representation (v0.3.x)

*Goal: Decouple the language from x86 Assembly to enable optimizations and multiple backends.*

### v0.3.0: Baa IR (Intermediate Representation)
- [ ] **IR Design** — Define a simplified, linear instruction set (Three-Address Code).
    - Example: `ADD t0, t1, t2` (virtual registers).
- [ ] **AST to IR** — Write a lowering pass to convert the AST tree into a Control Flow Graph (CFG) of IR blocks.
- [ ] **IR Printer** — Debug tool to print the IR in a readable format (`--dump-ir`).

### v0.3.1: The Optimizer ⚡
- [ ] **Control Flow Analysis** — Detect unreachable blocks.
- [ ] **Dead Code Elimination** — Remove instructions that don't affect the output.
- [ ] **Constant Propagation** — If `x = 10` and `y = x + 5`, replace with `y = 15`.
- [ ] **Loop Invariant Code Motion** — Move static calculations out of loops.

### v0.3.2: The Backend (Target Independence)
- [ ] **Instruction Selection** — Convert IR to abstract machine instructions.
- [ ] **Register Allocation** — Map virtual registers (t0, t1...) to physical x64 registers (RAX, RBX...) using Linear Scan or Graph Coloring.
- [ ] **Code Emission** — Write the final assembly text.

---

## 📚 Phase 4: The Ecosystem & Standard Library (v0.4.x)

*Goal: Make Baa useful for real-world applications.*

### v0.4.0: Compound Types (Structs)
- [ ] **Struct Definition** — `هيكل نقطة { صحيح س. صحيح ص. }`.
- [ ] **Member Access** — `نقطة.س = ١٠.`
- [ ] **Memory Layout** — Handle padding and alignment.

### v0.4.1: Pointers & Memory
- [ ] **Address-of Operator** — `&` (or Arabic equivalent like `عنوان`).
- [ ] **Dereference Operator** — `*` (or Arabic equivalent like `قيمة`).
- [ ] **Dynamic Allocation** — Integration with `malloc`/`free`.

### v0.4.2: Standard Library (BaaLib)
- [ ] **IO Module** — File reading/writing (`ملف.اقرأ`, `ملف.اكتب`).
- [ ] **String Module** — String manipulation (length, concat, split).
- [ ] **Math Module** — Advanced math functions (sqrt, pow, sin, cos).
- [ ] **System Module** — Executing commands, environment variables.

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

- [x] **Extended If** — Support `وإلا` (Else) and `وإلا إذا` (Else If)
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