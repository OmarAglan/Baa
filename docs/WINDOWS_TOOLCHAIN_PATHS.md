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
3. Unicode, spaced, or long paths are represented to GCC/LD by a
   `GetShortPathNameW` alias to that same filesystem entity.
4. GCC/LD reads or writes the real artifact through the alias; no bytes are
   copied and no staging directory exists.
5. If Windows cannot provide an ASCII short alias, Baa returns external
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

An admitted portable toolchain can declare direct Unicode support through
`BAA-TOOLCHAIN-MANIFEST.txt` at its root. Baa requires both
`format=baa-portable-toolchain-v1` and `unicode_paths=direct` before passing
ordinary Unicode and spaced paths directly. Baa also verifies that the active
Windows code page can represent Arabic without replacement. This matters for
MinGW programs that receive a wide `CreateProcessW` command line and then
convert it back to narrow `argv`: a Western code page would otherwise turn the
path into question marks before GCC/LD opens it. When that host check fails,
Baa retains the measured no-copy filesystem-alias adapter used by unmarked GCC
installations.

The Qalam Windows package uses the relocatable Qt MinGW-w64 kit that passed
direct Arabic runtime-archive and executable paths on Arabic-capable and UTF-8
Windows hosts. Western-code-page hosts use the same real artifacts through
ASCII filesystem aliases. Paths at or beyond the driver's conservative
long-path threshold also use that previously admitted filesystem-alias
contract.

Nazm owns its Unicode-aware object output path independently and is the
production assembler default after the separate admission gate completed.
Both Baa and Nazm normalize Windows filesystem operations to absolute extended
paths when required. The hosted gate builds and runs through default Nazm and
explicit GAS under a spaced Arabic path beyond 260 UTF-16 units, checks positive
assemble/link phase timings, and verifies that no staging tree or temporary
artifact remains.
The short-name adapter remains relevant only to the explicit GAS rollback and
the external linker/runtime inputs that still require it.
