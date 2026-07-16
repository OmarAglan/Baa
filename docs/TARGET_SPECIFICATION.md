# Baa Target Specification

> **Version:** draft-0.1 | **Schema:** `target-spec-v1`

This document defines how Baa describes compilation targets.

---

## 1. Purpose

Baa currently supports hosted x86-64 Windows/Linux targets. Future OS-development work needs explicit freestanding targets such as `i386-elf` and `i386-pyramidos`.

Target specifications make target behavior explicit instead of scattering it across backend code.

---

## 2. Target Naming

Recommended target name format:

```text
<arch>-<system>[-<environment>]
```

Examples:

```text
x86_64-windows
x86_64-linux
i386-elf
i386-pyramidos
```

---

## 3. JSON Shape

```json
{
  "schema_version": "target-spec-v1",
  "name": "x86_64-linux",
  "arch": "x86_64",
  "bits": 64,
  "environment": "hosted",
  "object_format": "elf",
  "executable_suffix": "",
  "assembly_syntax": "gas-att",
  "pointer_width": 64,
  "endianness": "little",
  "calling_convention": "sysv-amd64",
  "stack_alignment": 16,
  "supports_libc": true,
  "supports_stdlib": true,
  "default_linker": "host-gcc",
  "features": {
    "pic": true,
    "pie": true,
    "stack_protector": true,
    "nazm_source": true,
    "structured_arch_ops": true,
    "inline_asm": false,
    "freestanding": false
  }
}
```

---

## 4. Required Fields

| Field | Meaning |
|---|---|
| `schema_version` | target spec schema version |
| `name` | compiler target name |
| `arch` | CPU architecture |
| `bits` | pointer/integer ABI family width when relevant |
| `environment` | `hosted` or `freestanding` |
| `object_format` | `coff`, `elf`, or future format |
| `executable_suffix` | suffix for linked executables (`.exe` on Windows, empty on Linux) |
| `assembly_syntax` | emitted assembly flavor |
| `pointer_width` | pointer width in bits |
| `endianness` | `little` or `big` |
| `calling_convention` | ABI calling convention name |
| `stack_alignment` | required call-boundary stack alignment |
| `supports_libc` | whether libc calls are allowed |
| `supports_stdlib` | whether Baa stdlib is available |
| `default_linker` | linking strategy |
| `features` | target feature flags |

---

## 5. Hosted vs Freestanding

Hosted targets may use:

- libc,
- CRT startup,
- file I/O,
- process environment,
- normal `الرئيسية` entry.

Freestanding targets must not assume:

- libc,
- host OS files,
- environment variables,
- CRT startup,
- standard process entry,
- standard output.

---

## 6. PyramidOS Requirements

A future PyramidOS target needs:

- 32-bit code generation,
- `i386` or `i686` architecture,
- ELF object output,
- freestanding mode,
- no stdlib by default,
- no libc lowering,
- custom linker script compatibility,
- section placement controls,
- packed/aligned structs,
- volatile memory access,
- inline assembly for privileged instructions,
- object-only output for mixed C/Baa kernel linking.

Recommended first target names:

```text
i386-elf
i386-pyramidos
```

---

## 7. Stable CLI Discovery

```bash
baa --target-info=json
baa --target=x86_64-linux --target-info=json
baa --target=x86_64-linux file.baa
```

`--target-info=json` emits `target-info-v1`. The document includes the compiler version,
host and selected target names, and every currently supported compiler target. Each target
record exposes its triple, object format, executable suffix, whether it is the host, and
capabilities for assembly, object emission, linking, libc, stdlib, PIC/PIE, stack protection,
direct Arabic Nazm roots, and inline assembly. `nazm_source` means the driver
can send a `.نظم` root to the configured/PATH Nazm CLI. General object emission
and linking remain true only for the host target; direct or generated Nazm
object-only invocations may select the other implemented object format.

---

## 8. Target Conformance Tests

Each target must have tests for:

- data layout,
- function calls,
- return values,
- stack alignment,
- global data emission,
- string emission when supported,
- inline assembly when supported,
- freestanding rejection of hosted-only APIs.
