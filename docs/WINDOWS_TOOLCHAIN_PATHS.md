# Windows Toolchain Path Contract

## Status

The normal Windows artifact pipeline no longer copies assembly, objects, the
runtime archive, or the executable through a shared ASCII staging directory.
`-S` writes its requested output directly. Assemble and link phases operate on
the real Baa/Takween-selected files.

## Retired staging cost

For a normal build containing `N` Baa translation units, the former bridge
performed exactly `3N + 2` staging copies:

- `N` assembly copy-ins before GCC;
- `N` object copy-outs after assembly;
- `N` object copy-ins before linking;
- one runtime-archive copy-in;
- one executable copy-out.

The copied-byte total was:

```text
sum(assembly sizes) + 2 * sum(object sizes) + runtime size + executable size
```

Assembly copy-in happened before the recorded assemble timer, object copy-out
happened after it, link-input/runtime copies happened before the recorded link
timer, and executable copy-out happened after it. The old phase timings
therefore excluded all staging I/O. `-S` also emitted to a temporary `.s` file
and copied it once despite invoking no external tool.

The shared `baa_stage` directory used process-local counters. Two compiler
processes could both choose names such as `unit_1.s`, causing missing or
mismatched artifacts. The replacement uses process-id plus invocation-local
artifact identities and has a six-process same-directory regression.

## Selected toolchain capability matrix

The Windows gate covers:

- Arabic assembly input;
- Arabic object output;
- Arabic object input while linking;
- Arabic executable output;
- Arabic paths containing spaces;
- real paths beyond the legacy 260 UTF-16-unit limit;
- multiple objects;
- a UTF-8 response file containing `الرئيسية_بدء`;
- successful link and runtime behavior.

The locally selected MSYS2 GCC 15.2 cannot open even a short Arabic filesystem
path supplied through `CreateProcessW`. Moving the same UTF-8 paths into a GCC
response file does not fix that limitation. It can, however, consume the
filesystem short-name aliases returned for the same real files.

Accordingly, the driver uses this no-copy contract on Windows:

1. Baa creates or opens the requested real UTF-8 artifact with wide Windows
   filesystem APIs.
2. Plain safe ASCII paths are passed unchanged.
3. Unicode, spaced, or long file paths use an ASCII 8.3 alias when Windows
   provides one; otherwise Baa creates a temporary hard link to the same file
   entity on the same volume.
4. When the admitted portable GCC root itself is below a Unicode path, Baa
   exposes that directory through a temporary ASCII junction and supplies
   explicit `-B` and `-L` search prefixes from the pinned manifest.
5. GCC/LD reads or writes the real artifact through these aliases; no artifact
   bytes are copied and no staging directory exists. Temporary aliases are
   removed when the invocation or compiler process ends.
6. If Windows cannot provide the required no-copy aliases, Baa returns external
   toolchain status `4` with an explicit diagnostic. It never silently restores
   staging.

The linker response file is a small control input, not an artifact copy. It is
created beside the requested output with a process-unique name, carries the
Arabic entry symbol as UTF-8, and is deleted after the link.

Because Baa supplies its own Arabic startup instead of MinGW's normal CRT
startup files, an admitted portable toolchain may also require the driver to
retain `_pei386_runtime_relocator` explicitly. Baa does this only for the
versioned portable-toolchain contract when it contains
`pei386_runtime_relocator=retain`, making that distribution pull required
pseudo-reloc support from `libmingw32` without changing unmarked UCRT toolchain
behavior.

Build manifests record a stable source-derived logical `.o` identity for each
link-mode unit. The process-unique physical object/assembly paths and the
invocation's final executable spelling are private driver details and never
enter deterministic per-unit manifests or cache identities. Source and
dependency canonicalization also uses a UTF-8 → wide full-path → UTF-8
round trip, so Arabic working directories cannot leak ANSI bytes into JSON.

## Direct-Unicode portable toolchains

An admitted portable toolchain records the result of its Unicode-path probe in
`BAA-TOOLCHAIN-MANIFEST.txt`. The value is `unicode_paths=direct` only when the
pinned GCC actually passes; otherwise it is `unicode_paths=short-path`. Baa
requires the portable manifest and the direct value before passing ordinary
Unicode and spaced paths directly, and also verifies that the active Windows
code page is UTF-8. Merely representing Arabic in an ANSI page such as 1256 is
insufficient: some MinGW/GNU tools reinterpret the resulting narrow bytes as
UTF-8 and corrupt the path. The measured `short-path` mode uses the same real
filesystem entities through no-copy ASCII aliases: 8.3 names or hard links for
files, and a directory junction plus manifest-derived search prefixes for an
admitted portable GCC root.

The standalone Windows package uses a pinned relocatable WinLibs kit. UTF-8
Windows hosts may pass direct Arabic runtime-archive and executable paths;
other code pages use the same real artifacts through ASCII filesystem aliases.
Paths at or beyond the driver's conservative long-path threshold also use that
previously admitted filesystem-alias contract.

Nazm owns its Unicode-aware object output path independently and is the
production assembler default after the separate admission gate completed.
Both Baa and Nazm normalize Windows filesystem operations to absolute extended
paths when required. The hosted gate builds and runs through default Nazm and
explicit GAS under a spaced Arabic path beyond 260 UTF-16 units, checks positive
assemble/link phase timings, and verifies that no staging tree or temporary
artifact remains.
The short-name adapter remains relevant only to the explicit GAS rollback and
the external linker/runtime inputs that still require it.
