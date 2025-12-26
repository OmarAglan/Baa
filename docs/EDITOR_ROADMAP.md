# Baa Studio Roadmap

> A custom code editor built from scratch in C for the Baa programming language

*[← Back to Main Roadmap](../ROADMAP.md)*

---

## Overview

Since Baa is built from scratch, we're also building our own dedicated code editor. Baa Studio will feature native Arabic text support, syntax highlighting, and integrated compilation.

---

## Development Phases

### Phase 1: CLI Syntax Highlighter 📋

**Goal:** A command-line tool that prints Baa code with colors.

**Technology:** C, ANSI Escape Codes

| Task | Status |
|------|--------|
| Read `.b` file from command line | ⬜ |
| Reuse `src/lexer.c` token logic | ⬜ |
| Print keywords in **blue** (`صحيح`, `إذا`, `لكل`...) | ⬜ |
| Print strings in **yellow** (`"..."`) | ⬜ |
| Print numbers in **green** | ⬜ |
| Print comments in **gray** (`//...`) | ⬜ |

**Deliverable:** `baa-highlight.exe` that outputs colored source to terminal.

---

### Phase 2: GUI Window 📋

**Goal:** A native Windows window that displays text.

**Technology:** C, `windows.h` (Win32 API)

| Task | Status |
|------|--------|
| Create Win32 window (`WinMain`) | ⬜ |
| Handle `WM_PAINT` for text rendering | ⬜ |
| Implement `DrawText` or `TextOut` for display | ⬜ |
| Handle `WM_DESTROY` for cleanup | ⬜ |
| Support Arabic font rendering (RTL) | ⬜ |

**Deliverable:** Window that displays static Baa source code.

---

### Phase 3: Text Editing 📋

**Goal:** Full text input and navigation capabilities.

**Technology:** Win32 API

| Task | Status |
|------|--------|
| Handle `WM_CHAR` for UTF-16 input | ⬜ |
| Implement Gap Buffer for efficient editing | ⬜ |
| Caret (cursor) positioning with RTL support | ⬜ |
| Keyboard navigation (arrows, home, end) | ⬜ |
| Selection and copy/paste | ⬜ |
| Undo/Redo stack | ⬜ |

**Deliverable:** Editable text area with Arabic typing support.

---

### Phase 4: Compiler Integration 📋

**Goal:** Compile and run Baa programs from within the editor.

| Task | Status |
|------|--------|
| Add "Build" toolbar button | ⬜ |
| Invoke `baa.exe` as subprocess | ⬜ |
| Capture `stdout`/`stderr` output | ⬜ |
| Display output in console pane | ⬜ |
| Parse error messages (line/column) | ⬜ |
| Highlight error lines in editor | ⬜ |
| Add "Run" button for `out.exe` | ⬜ |

**Deliverable:** Integrated development environment for Baa.

---

## Future Enhancements

- **Auto-completion** for keywords and identifiers
- **Code folding** for functions and blocks
- **Multiple tabs** for editing several files
- **Project management** for multi-file programs
- **Themes** (light/dark mode)

---

*This is a long-term project. Contributions welcome!*