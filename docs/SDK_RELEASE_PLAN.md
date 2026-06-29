# Baa SDK Release Plan

> **Version:** draft-0.1 | **Applies to:** post-v0.7 planning

This document defines the future Baa SDK concept. It is not a new repository requirement yet.

---

## 1. Purpose

Users should eventually install one coherent Baa SDK instead of discovering several disconnected repositories.

The SDK is a release bundle, not a monorepo.

---

## 2. SDK Components

| Component | Owner | Required for SDK |
|---|---|---:|
| `baa` compiler | Baa | yes |
| `baalib` stdlib | Baa | yes |
| target specs | Baa | yes |
| documentation | Baa | yes |
| examples | Baa | yes |
| `takween` | Takween | recommended |
| Qalam integration metadata | Baa/Qalam | recommended |
| Qalam-IDE binary | Qalam | optional separate download |
| PyramidOS templates | PyramidOS | no, future optional |

---

## 3. Versioning

SDK version should follow the Baa compiler version at first:

```text
Baa SDK 0.9.0
  baa 0.9.0
  baalib 0.9.0
  target-spec-v1
  diagnostics-json-v1
  takween compatible version: see COMPATIBILITY_MATRIX.md
  qalam compatible version: see COMPATIBILITY_MATRIX.md
```

---

## 4. Suggested Bundle Layout

```text
baa-sdk-0.9.0/
  bin/
    baa
    takween              # if bundled
  stdlib/
    baalib.baahd
  targets/
    x86_64-linux.json
    x86_64-windows.json
  docs/
    USER_GUIDE.md
    LANGUAGE.md
    BAA_BOOK_AR.md
    COMPATIBILITY_MATRIX.md
  examples/
  schemas/
    diagnostics-json-v1.schema.json
    build-manifest-v1.schema.json
```

---

## 5. Profiles

| Profile | Includes |
|---|---|
| minimal | compiler + stdlib + target specs |
| default | minimal + docs + examples + Takween |
| full | default + Qalam metadata and optional IDE link |
| ci | compiler + stdlib + target specs, no local docs |

---

## 6. Release Checklist

- [ ] Baa release built and tested.
- [ ] Takween compatibility checked.
- [ ] Qalam compatibility checked.
- [ ] Docs version headers synced.
- [ ] Examples compile.
- [ ] Target specs included.
- [ ] JSON schemas included.
- [ ] Checksums generated.
- [ ] Known limitations included.

---

## 7. Not Now

Do not build a public package registry until:

- Baa v0.9 stable beta is complete,
- conformance-v1 exists,
- compatibility matrix is maintained,
- local package conventions are stable.
