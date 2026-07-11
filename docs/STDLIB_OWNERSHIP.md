# Standard Library Ownership Contract

> Baseline: v0.6.2 standard-library core. Include `stdlib/baalib.baahd` for the public declarations.

This page is the single ownership index for public stdlib helpers that allocate, release, or return borrowed storage.

## General rules

- `عدم` from an allocating function means allocation/opening failed and no new ownership was transferred.
- Owned heap results must be released exactly once with the matching release helper.
- Borrowed pointers remain owned by the original handle; do not free them separately.
- Functions that copy into a caller-provided destination do not transfer ownership of that destination.
- `طول_نص`/`قارن_نص`/`نسخ_نص` require a valid `نص`; check allocation-returning helpers for `عدم` before passing their result to text helpers.

## Owned results and handles

| API | Owned result | Release with | Notes |
|-----|--------------|--------------|-------|
| `نسخ_نص(نص)` | New `نص` | `حرر_نص` or `تحرير_ذاكرة` | Independent duplicate of the input string, or `عدم` on allocation failure. |
| `دمج_نص(نص، نص)` | New `نص` | `حرر_نص` or `تحرير_ذاكرة` | Independent concatenated string, or `عدم` on allocation failure. |
| `نسق(نص، ...)` | New `نص` | `حرر_نص` or `تحرير_ذاكرة` | Formatted string builtin. |
| `اقرأ_سطر()` | New `نص` or `عدم` | `حرر_نص` or `تحرير_ذاكرة` | Reads from stdin. |
| `اقرأ_سطر(عدم* ملف)` | New `نص` or `عدم` | `حرر_نص` or `تحرير_ذاكرة` | Returns `عدم` on EOF before any byte. |
| `متغير_بيئة(نص)` | New `نص` or `عدم` | `حرر_نص` or `تحرير_ذاكرة` | Copies the host environment value. |
| `وقت_كنص(صحيح)` | New `نص` | `حرر_نص` or `تحرير_ذاكرة` | Snapshot of host time text. |
| `نص_كود_خطأ(صحيح)` | New `نص` | `حرر_نص` or `تحرير_ذاكرة` | Snapshot of host error text. |
| `ضم_مسار/مجلد_مسار/اسم_ملف_مسار/امتداد_مسار/طبع_مسار` | New `نص` or `عدم` | `حرر_نص` or `تحرير_ذاكرة` | Lexical path strings only. |
| `نص_الباني(باني_نص)` | New `نص` or `عدم` | `حرر_نص` or `تحرير_ذاكرة` | Independent snapshot; freeing the builder does not free prior snapshots. |
| `حجز_ذاكرة(صحيح)` | New `عدم*` or `عدم` | `تحرير_ذاكرة` | Raw heap bytes. |
| `إعادة_حجز(عدم*، صحيح)` | Reallocated `عدم*` or `عدم` | `تحرير_ذاكرة` | Mirrors `realloc`; on failure the original pointer remains the caller's responsibility. |
| `أنشئ_متجه(صحيح)` | New `متجه` or `عدم` | `حرر_متجه` | Frees vector storage, not pointers stored as element values. |
| `أنشئ_مخزن_بايتات()` | New `مخزن_بايتات` or `عدم` | `حرر_مخزن_بايتات` | Frees byte-buffer storage. |
| `أنشئ_باني_نص()` | New `باني_نص` or `عدم` | `حرر_باني_نص` | Frees builder storage, not strings returned earlier by `نص_الباني`. |
| `فتح_ملف(نص، نص)` | File handle `عدم*` or `عدم` | `اغلق_ملف` | Opaque host `FILE*` handle. |
| `ابدأ_عملية(...)` | Process handle `مقبض_عملية` or `عدم` | `حرر_عملية` | The handle owns the host process reference; freeing a running handle cancels and collects it. |

## Borrowed or caller-owned results

| API | Returned value | Ownership rule |
|-----|----------------|----------------|
| `بيانات_متجه(متجه)` | Internal storage pointer | Borrowed; may change after `ادفع_متجه`; do not free separately. |
| `بيانات_مخزن_بايتات(مخزن_بايتات)` | Internal storage pointer | Borrowed; may change after `أضف_بايت`; do not free separately. |
| `نسخ_ذاكرة(وجهة، مصدر، عدد)` | `وجهة` | Caller still owns `وجهة`; no new allocation. |
| `تعيين_ذاكرة(مؤشر، قيمة، عدد)` | `مؤشر` | Caller still owns `مؤشر`; no new allocation. |
| `ادفع_متجه(متجه، عدم*)` | `منطقي` | Copies element bytes; does not take ownership of the source pointer. |
| `اسحب_متجه(متجه، عدم*)` | `منطقي` | Copies bytes into caller-provided destination when non-`عدم`; destination remains caller-owned. |
| `أضف_نص_للباني(باني_نص، نص)` | `منطقي` | Copies text bytes; does not retain or free the input `نص`. |

## Non-owning status helpers

`نتيجة_ناجحة`, `نتيجة_فاشلة`, and `كود_نتيجة` only convert between boolean and integer status conventions. They allocate nothing and transfer no ownership.
