# Baa Tooling Contracts

> **Version:** draft-0.1 | **Applies to:** Baa v0.7.x+

This document defines the compiler surfaces that external tools may rely on.

---

## 1. Principles

- Human-readable Arabic output is for users.
- Machine-readable JSON output is for Takween, Qalam-IDE, CI, and scripts.
- Tools must not parse unstable human text when JSON exists.
- Every machine-readable output has a `schema_version` field.
- Stable fields are never removed without a schema version bump.

---

## 2. Compiler Modes

| Mode | Purpose | Stable Consumer |
|---|---|---|
| compile/link default | produce executable | users, Takween |
| `-S` | produce assembly | compiler tests, backend tests |
| `-c` | produce object | Takween, future OS experiments |
| `--check` | parse + semantic check only | Qalam, Takween |
| `--emit-build-manifest <file>` | dependency/cache manifest | Takween |
| `--diagnostics=json` | machine-readable diagnostics | Qalam, Takween |
| `--dump-tokens=json` | stable token stream | Qalam/debug tools |
| `--dump-symbols=json` | symbol outline | Qalam |
| `--target=<target>` | target selection | Takween, OS experiments |

---

## 3. Exit Codes

| Code | Meaning |
|---:|---|
| 0 | success |
| 1 | user/source error: syntax, semantic, include, diagnostic failure |
| 2 | invalid compiler invocation |
| 3 | unsupported target or unsupported feature for selected mode |
| 4 | toolchain/backend failure |
| 5 | internal compiler error |

Exit-code meanings are part of `compiler-cli-v1`.

---

## 4. Build Manifest Contract

`--emit-build-manifest <file>` should include:

```json
{
  "schema_version": "build-manifest-v1",
  "compiler": {
    "name": "baa",
    "version": "0.7.0",
    "target": "x86_64-linux"
  },
  "mode": "compile-link",
  "inputs": [
    {
      "path": "src/main.baa",
      "canonical_path": "/abs/path/src/main.baa",
      "sha256": "...",
      "dependencies": [
        {
          "path": "include/lib.baahd",
          "canonical_path": "/abs/path/include/lib.baahd",
          "sha256": "..."
        }
      ]
    }
  ],
  "outputs": [
    { "kind": "executable", "path": "build/app" }
  ],
  "cache": {
    "enabled": true,
    "hits": 1,
    "misses": 0
  }
}
```

---

## 5. JSON Output Rules

All JSON outputs must:

- be UTF-8,
- use LF line endings,
- include `schema_version`,
- include compiler `version`,
- use byte offsets and line/column spans where applicable,
- preserve Arabic identifiers as UTF-8 strings,
- avoid localized field names; field names stay English for tool stability,
- put Arabic user-facing messages in values, not keys.

---

## 6. Contract Versioning

| Change | Required action |
|---|---|
| Add optional field | no version bump required |
| Add required field | minor contract version bump |
| Remove field | major contract version bump |
| Change field meaning | major contract version bump |
| Change diagnostic code meanings | major contract version bump |

---

## 7. Tool Responsibilities

Takween should consume:

- exit codes,
- build manifest,
- diagnostic JSON,
- target support list.

Qalam should consume:

- `--check`,
- diagnostic JSON,
- token JSON,
- symbol JSON,
- completion metadata.

PyramidOS experiments should consume:

- target specs,
- freestanding mode,
- object-only output,
- ABI/layout test outputs.
