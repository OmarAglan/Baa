# Baa Module and Visibility Contract

> **Version:** draft-0.1 | **Applies to:** Baa v0.7.1+

This document defines the source/header convention and current cross-file visibility rules for
Baa projects.

---

## 1. Purpose

Baa supports multi-file programs today through direct compiler inputs and `#تضمين` includes.
Takween will later own the project-level build workflow, but the compiler repository still owns
the language-level module, header, and visibility contract.

---

## 2. File Roles

| Extension | Role | Passed as compiler input? | Recommended contents |
|---|---|---:|---|
| `.baa` | implementation/source unit | yes | function bodies, global definitions, local helpers, entry point |
| `.baahd` | header/declaration unit | only with `--check-header` | prototypes, `خارجي` declarations, shared type declarations, macros |

Rules:

- Compile implementation units (`.baa`) as roots.
- Include headers (`.baahd`) from implementation units with `#تضمين`.
- Do not use `.baahd` files as independent implementation inputs in normal builds.
- Use `--check-header` to parse and semantically validate a header without emitting IR,
  assembly, object files, or an executable.
- New public headers should be declaration-only by convention.
- The current parser still processes included headers as normal Baa source; `--check-header`
  does not yet enforce a header-only grammar.

---

## 3. Header Pattern

Preferred header:

```baa
// math.baahd
خارجي صحيح اجمع(صحيح أ، صحيح ب).
خارجي صحيح عداد_عام.
```

Preferred implementation:

```baa
// math.baa
#تضمين "math.baahd"

صحيح عداد_عام = ٠.

صحيح اجمع(صحيح أ، صحيح ب) {
    إرجع أ + ب.
}
```

Preferred consumer:

```baa
// main.baa
#تضمين "math.baahd"

صحيح الرئيسية() {
    إرجع اجمع(١، ٢) - ٣.
}
```

Build the implementation files, not the header:

```bash
baa main.baa math.baa -o app
```

---

## 4. Visibility Rules

Current visibility is file/linkage based:

| Source form | Current meaning |
|---|---|
| top-level function definition | exported/default-visible symbol |
| top-level global or array definition | exported/default-visible storage |
| `خارجي` function prototype | declaration only; definition must be provided elsewhere |
| `خارجي` global or array | declaration only; no local storage emitted |
| `ساكن` global or array | file-local/internal storage |
| local `ساكن` variable or array | static-duration local storage |
| `ساكن` function definition | not supported; rejected by the parser |
| `ثابت` | immutability/storage policy, not a visibility modifier |

There are no `public`/`internal` keywords yet. Use default top-level definitions for public
symbols and `ساكن` globals/arrays for file-local storage.

---

## 5. `خارجي` Contract

`خارجي` is allowed only at top level.

Rules:

- A `خارجي` function declaration ends with `.` and has no body.
- A `خارجي` global/array declaration has no initializer.
- Repeated `خارجي` declarations must match in type, constness, and array shape.
- Exactly one compatible non-`خارجي` definition should exist in the linked implementation set.
- `خارجي` cannot be combined with `ساكن`.

---

## 6. One-Definition Policy

Within the compiler front end, Baa validates many declaration conflicts, including incompatible
`خارجي` declarations. Across separately compiled implementation inputs, the current toolchain may
still be the component that reports duplicate exported definitions.

The language policy is stricter than the current diagnostic quality:

- one exported function definition per symbol,
- one exported global/array definition per symbol,
- any number of compatible `خارجي` declarations,
- file-local `ساكن` globals/arrays do not conflict across files.

Better front-end duplicate-symbol diagnostics remain a v0.7.1 follow-up.

---

## 7. Migration Path to Takween

Raw multi-file compiler invocations remain valid:

```bash
baa main.baa math.baa -o app
```

Takween-managed builds should preserve the same compiler contract:

1. Treat `.baa` files as implementation roots.
2. Treat `.baahd` files as include/declaration surfaces, optionally checked with `--check-header`.
3. Pass include directories with `-I` in project-config order.
4. Pass all implementation inputs explicitly to Baa.
5. Consume `--emit-build-manifest` for dependency hashes and invalidation.
6. Run and clean outside Baa, because `baa build`, `baa run`, and `baa clean` are not compiler
   subcommands.

---

## 8. Current Non-Goals

- No enforced header-only grammar for `.baahd` yet.
- No `public`/`internal` keyword pair yet.
- No Baa-owned project build system; Takween owns that workflow.
