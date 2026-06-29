# Baa Diagnostics JSON Schema

> **Version:** draft-0.1 | **Schema:** `diagnostics-json-v1`

This document defines the machine-readable diagnostic format for Baa tools.

---

## 1. CLI

```bash
baa --check --diagnostics=json file.baa
baa --diagnostics=json file.baa -o app
```

When `--diagnostics=json` is used, diagnostics are written as JSON to stdout or to the path given by a future `--diagnostics-output <file>` option.

---

## 2. Top-Level Shape

```json
{
  "schema_version": "diagnostics-json-v1",
  "compiler": {
    "name": "baa",
    "version": "0.7.2"
  },
  "invocation": {
    "mode": "check",
    "target": "x86_64-linux",
    "working_directory": "/repo"
  },
  "summary": {
    "errors": 1,
    "warnings": 0,
    "notes": 1
  },
  "diagnostics": []
}
```

---

## 3. Diagnostic Object

```json
{
  "code": "B0001",
  "severity": "error",
  "category": "syntax",
  "message": "توقعت نهاية العبارة `.` هنا.",
  "file": "examples/bad.baa",
  "span": {
    "start": { "line": 3, "column": 18, "byte": 42 },
    "end": { "line": 3, "column": 19, "byte": 43 }
  },
  "primary_label": "أضف `.` لإنهاء العبارة.",
  "hints": [
    "تنتهي عبارات باء بنقطة `.` وليس بفاصلة منقوطة إنجليزية."
  ],
  "related": [
    {
      "message": "بدأت هذه العبارة هنا.",
      "file": "examples/bad.baa",
      "span": {
        "start": { "line": 3, "column": 5, "byte": 29 },
        "end": { "line": 3, "column": 10, "byte": 34 }
      }
    }
  ]
}
```

---

## 4. Fields

| Field | Required | Meaning |
|---|---:|---|
| `code` | yes | stable diagnostic ID |
| `severity` | yes | `error`, `warning`, `note`, `help` |
| `category` | yes | `syntax`, `semantic`, `include`, `backend`, `runtime`, `toolchain`, `internal` |
| `message` | yes | Arabic user-facing message |
| `file` | yes | source path as reported by compiler |
| `span` | yes | primary source span |
| `primary_label` | no | short Arabic label for UI underline |
| `hints` | no | Arabic fix/help hints |
| `related` | no | secondary spans |

---

## 5. Stable Diagnostic Code Ranges

| Range | Category |
|---|---|
| `B0001`-`B0999` | lexer/syntax/parser |
| `B1000`-`B1999` | semantic/type/scope |
| `B2000`-`B2999` | include/preprocessor |
| `B3000`-`B3999` | IR/verifier/optimizer |
| `B4000`-`B4999` | backend/target/ABI |
| `B5000`-`B5999` | stdlib/runtime checks |
| `B9000`-`B9999` | internal compiler errors |

---

## 6. Line, Column, and Byte Rules

- Lines are 1-based.
- Columns are 1-based and count Unicode scalar display positions only approximately.
- Byte offsets are 0-based UTF-8 byte offsets from the start of the file.
- Tools should use byte offsets for exact slicing and columns for display.
- Multi-line spans are allowed.

---

## 7. Qalam-IDE Mapping

| JSON Field | Qalam Use |
|---|---|
| `severity` | marker color/icon |
| `code` | hover/code action/`--explain` |
| `span` | underline range |
| `message` | diagnostics panel |
| `hints` | quick-fix/help text |
| `related` | secondary references |

---

## 8. Takween Mapping

Takween should:

- group diagnostics by source file,
- show the first error summary after failed builds,
- preserve Baa's Arabic message text,
- use exit codes for build status,
- optionally write the JSON diagnostics as build artifacts.
