# Stage-0 Bootstrap Manifest

> **Version:** 0.9.1 | [← Phase 4.5 Handoff](PHASE_4_5_HANDOFF.md) | [Mixed Harness →](MIXED_HARNESS.md)

This document records how v0.9.0.1 consumes the frozen Phase 4.5 artifacts before mixed C+Baa migration begins.

---

## 1. Snapshot

Stage-0 is pinned by the annotated Git tag:

```text
bootstrap-stage0-v0.9.0.1 -> 619619ab04d30cb30bde5fc7d726814bf3c4c933
```

The base release tag remains:

```text
0.5.9 -> c0ec754fd410f01d60fd663d8de1073b6c5a7d92
```

The Stage-0 compiler version consumed by the first Phase 5 migration checkpoints is `0.5.9`. The project metadata version for this manifest checkpoint is `0.9.0.1`.

---

## 2. Machine Manifest

The canonical machine-readable manifest is:

```text
docs/STAGE0_TOOLCHAIN_MANIFEST.json
```

Validate it with:

```bash
python scripts/qa_stage0_manifest.py
```

The validator checks that the bootstrap tag, base release tag, and frozen artifact Git object IDs still match the pinned manifest.

---

## 3. Platform Signoff

Windows handoff/release evidence is present from the Phase 4.5 handoff bundle. Linux `quick/full` evidence remains intentionally deferred and continues to block full Phase 4.5 exit and any cross-platform release-candidate claim.

Phase 5 implementation checkpoints may consume the frozen artifacts, but they must keep Linux parity gated until Linux evidence is attached.

---

*[← Phase 4.5 Handoff](PHASE_4_5_HANDOFF.md) | [Mixed Harness →](MIXED_HARNESS.md)*
