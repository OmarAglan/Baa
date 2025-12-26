# Baa Roadmap

> Track the development progress of the Baa programming language

---

## 🎯 Current Focus: v0.1.2

### Recursion & Advanced Types
- [x] **Recursion** — Stress test stack frame logic with recursive functions (Fibonacci, Factorial)
- [ ] **String Variables** — `نص` type (currently strings are only literals)
- [ ] **Break/Continue** — Loop control statements (`توقف`, `استمر`)

### v0.1.3 (Planned: Optimization)
- [ ] **Constant Folding** — Evaluate `1 + 2` at compile time
- [ ] **Dead Code Elimination** — Remove unreachable code after return

---

## 🖥️ Baa Studio (Editor)

*A custom code editor built from scratch in C*

See [Editor Roadmap](docs/EDITOR_ROADMAP.md) for detailed phases.

| Phase | Goal | Status |
|-------|------|--------|
| Phase 1 | CLI Syntax Highlighter (ANSI codes) | 📋 Planned |
| Phase 2 | GUI Window (Win32 API) | 📋 Planned |
| Phase 3 | Text Editing & Input | 📋 Planned |
| Phase 4 | Compiler Integration | 📋 Planned |

---

## ✅ Completed Milestones

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