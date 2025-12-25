# Baa Studio Roadmap

Since we are building everything from scratch, we will build our own Code Editor for Baa.

## Phase 1: The Highlighter (CLI)
**Goal:** A command-line tool that prints Baa code with colors.
**Tech:** C, ANSI Escape Codes.

- [ ] Read `.b` file.
- [ ] Reuse `src/lexer.c` logic.
- [ ] Print Keywords in **Blue**.
- [ ] Print Numbers in **Green**.
- [ ] Print Comments in **Gray**.

## Phase 2: The Window (GUI)
**Goal:** A native Windows window that displays text.
**Tech:** C, `windows.h` (Win32 API).

- [ ] Create a basic Win32 window entry point (`WinMain`).
- [ ] Handle `WM_PAINT` to draw text.
- [ ] Handle `WM_DESTROY` to close.

## Phase 3: The Editor (Input)
**Goal:** Typing and navigating.
**Tech:** Win32 API.

- [ ] Handle `WM_CHAR` to accept input.
- [ ] Implement a Gap Buffer or Rope data structure for text storage.
- [ ] Implement a Caret (Cursor) that moves.

## Phase 4: Integration
**Goal:** Run Baa inside Baa Studio.
- [ ] Add a "Build" button that calls `baa.exe`.
- [ ] Capture output and show it in a console pane.
```

---