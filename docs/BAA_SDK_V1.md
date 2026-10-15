# Baa SDK v1 (`baa-sdk-v1`)

> **Status:** admitted | **Applies to:** Baa 0.6.0 and later

A Baa SDK is one immutable, versioned directory that holds everything needed to
compile and link Baa programs for its host: the compiler, the runtime, the
standard library, the target specifications and, on Windows, the linker. Any
number of SDKs can sit side by side; nothing in one refers to another, to the
`PATH`, or to an absolute path.

`scripts/package_sdk.py` builds the archive. `baa --sdk-info=json` reports the
paths a compiler actually uses. Takween installs, verifies and selects SDKs
from these two contracts and never runs code from the archive to do it.

---

## 1. Layout

```text
baa-sdk-<version>-<host>/
  BAA-SDK.json                  identity, components and file inventory
  bin/
    baa(.exe)
  lib/baa/
    libbaa_runtime.a
  share/baa/
    stdlib/                     baalib.baahd and its sources
    targets/                    target-spec-v1 files
    docs/  resources/  examples/
    README.md  CHANGELOG.md
  gcc/                          Windows only: the bundled linker kit
    BAA-TOOLCHAIN-MANIFEST.txt
    bin/gcc.exe ...
```

`<version>` is the compiler's SemVer version and `<host>` its host target
(`x86_64-windows`, `x86_64-linux`), for example
`baa-sdk-0.6.0-x86_64-windows`.

The compiler finds every part relative to its own `bin/` directory, with the
same rules it uses for a source build or an installed copy:

| Part | Found at |
|---|---|
| runtime | `bin/../lib/baa/libbaa_runtime.a` |
| install home | `bin/../share/baa` (contains `stdlib/baalib.baahd`) |
| bundled linker | `bin/../gcc/bin/gcc.exe`, accepted only with a valid `BAA-TOOLCHAIN-MANIFEST.txt` |

An extracted SDK therefore works from any directory, including one whose path
contains Arabic or spaces, with no environment variables.

---

## 2. `BAA-SDK.json`

UTF-8 JSON, written with sorted inventory and stable key order:

```json
{
  "schema_version": "baa-sdk-v1",
  "name": "baa-sdk",
  "version": "0.6.0",
  "host": "x86_64-windows",
  "targets": ["x86_64-windows"],
  "components": {
    "compiler":  { "path": "bin/baa.exe", "version": "0.6.0", "revision": "<git sha>" },
    "assembler": { "kind": "nazm-embedded", "version": "1.0.0", "revision": "<nazm sha>" },
    "linker":    { "kind": "gcc", "bundled": true, "path": "gcc/bin/gcc.exe",
                   "version": "16.1.0", "manifest": "gcc/BAA-TOOLCHAIN-MANIFEST.txt" },
    "runtime":   { "path": "lib/baa/libbaa_runtime.a" },
    "stdlib":    { "path": "share/baa/stdlib" },
    "targets":   { "path": "share/baa/targets" }
  },
  "files": [
    { "path": "bin/baa.exe", "size": 1234, "sha256": "<hex>", "executable": true }
  ]
}
```

- `files` lists every file in the SDK except `BAA-SDK.json` itself, sorted by
  the UTF-8 bytes of `path`. Paths are relative, use `/`, and never contain
  `.` or `..` segments.
- `executable` is true for files under `bin/`, `gcc/bin/` and `gcc/libexec/`
  and for `.exe` and `.dll` files.
- `assembler.kind` is `nazm-embedded` when Nazm is linked into the compiler and
  `external` otherwise (then `version` and `revision` are null).
- **The linker is a replaceable component.** `linker.kind` names it; today it
  is always `gcc`. When Baa's own linker replaces GCC, it gets a new `kind`,
  and the SDK stops carrying `gcc/`. Consumers must read `kind` and `path`
  instead of assuming GCC. On Linux `bundled` is false and `path` is `gcc`,
  the system linker.

Limits: at most 8192 files, 256 MiB per file and 1 GiB in total.

---

## 3. Archive

`baa-sdk-<version>-<host>.tar.gz`, with a sidecar
`baa-sdk-<version>-<host>.tar.gz.sha256` in `sha256sum` format.

The archive is reproducible: packaging the same inputs twice gives the same
bytes.

- POSIX ustar, entries sorted by the UTF-8 bytes of their path, all under the
  single top-level directory `baa-sdk-<version>-<host>/`;
- regular files only: no directory, link or device entries;
- mtime 0, uid and gid 0, empty user and group names, mode `0755` for
  executable files and `0644` for the others;
- gzip with mtime 0, no file name, compression level 9.

An installer must reject any entry that breaks these rules, and any file whose
size or SHA-256 differs from the inventory or that the inventory does not list.

---

## 4. `baa --sdk-info=json` (`baa-sdk-info-v1`)

Reports what this compiler will use, as resolved at run time. It must be the
only argument.

```json
{
  "schema_version": "baa-sdk-info-v1",
  "compiler_version": "0.6.0",
  "host_target": "x86_64-windows",
  "executable_dir": "C:/sdks/baa-sdk-0.6.0-x86_64-windows/bin",
  "sdk_root": "C:/sdks/baa-sdk-0.6.0-x86_64-windows",
  "install_home": "C:/sdks/baa-sdk-0.6.0-x86_64-windows/share/baa",
  "stdlib_dir": "C:/sdks/baa-sdk-0.6.0-x86_64-windows/share/baa/stdlib",
  "targets_dir": "C:/sdks/baa-sdk-0.6.0-x86_64-windows/share/baa/targets",
  "runtime_library": "C:/sdks/baa-sdk-0.6.0-x86_64-windows/lib/baa/libbaa_runtime.a",
  "assembler": { "kind": "nazm-embedded", "default": true, "version": "1.0.0", "revision": "<sha>" },
  "linker": { "kind": "gcc", "bundled": true,
              "path": "C:/sdks/baa-sdk-0.6.0-x86_64-windows/gcc/bin/gcc.exe", "version": "16.1.0" }
}
```

- Paths are absolute, use `/`, and have `.` and `..` folded. A part that was
  not found is `null`; nothing is guessed.
- `sdk_root` is the parent of `bin/` when it holds `BAA-SDK.json`, otherwise
  null (a source build or a classic install).
- `linker.path` is the bundled linker when one was accepted, otherwise the
  command the compiler will run (`gcc`); `version` is known only for a bundled
  linker with a valid manifest.

The in-process frontend library does not run this command.

---

## 5. Packaging

```bash
python scripts/package_sdk.py --baa build/baa.exe --linker-dir gcc --output-dir dist
python scripts/package_sdk.py --baa build-linux/baa --output-dir dist
```

`--runtime` overrides the runtime (default: beside the compiler).
`--layout-only` writes the directory instead of the archive.
`--no-bundled-linker` packages a Windows SDK without `gcc/` and is for tests
only. The packager rejects symbolic links.

`tests/test_sdk.py` (QA step `sdk-tests`) checks the info document, packages
twice and compares the bytes, checks every archive entry against the
inventory, extracts the SDK under an Arabic directory name, and builds and
runs a program with it.
