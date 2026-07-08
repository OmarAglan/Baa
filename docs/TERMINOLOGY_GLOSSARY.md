# Baa Terminology Glossary

> **Version:** 0.6.0 | [← Style Guide](STYLE_GUIDE.md) | [Arabic Book →](BAA_BOOK_AR.md)

This document is the canonical terminology source for Baa documentation, comments, and user-facing diagnostics. On first mention in prose, prefer the form `Arabic term (English term)` when the English term helps recognition; after that, use the Arabic term consistently.

When a new compiler or language concept becomes public, add one preferred Arabic term here before spreading it across the docs.

## Core compiler pipeline

| English term | Preferred Arabic term | Usage note |
|--------------|-----------------------|------------|
| Baa compiler | مُصرِّف باء | Use for the C reference compiler. |
| Compiler | المُصرِّف | Avoid “المترجم” for the compiler itself. |
| Source file | ملف مصدري | For `.baa` and `.baahd` inputs. |
| Header file | ملف ترويسة | For `.baahd` declarations. |
| Compilation pipeline | خطّ التصريف | Use when describing staged compiler flow. |
| Driver | المُشغِّل | The CLI orchestration layer. |
| Frontend | الواجهة الأمامية | Lexer, preprocessor, parser, and AST construction. |
| Middle-end | الوسط | Semantic analysis, IR, verification, and optimization. |
| Backend | الخلفية | Target lowering, register allocation, and emission. |
| Lexer | المحلِّل اللفظي | Tokenizes source text. |
| Token | الرمز اللفظي | Keep distinct from symbol-table “symbol”. |
| Preprocessor | المعالج القبلي | Handles include/define/conditional directives. |
| Parser | المحلِّل النحوي | Builds the AST. |
| Abstract Syntax Tree | شجرة الصياغة المجرّدة | `AST` is acceptable after first mention. |
| Semantic analysis | التحليل الدلالي | Type/scope/name checks. |
| Type checker | مدقِّق الأنواع | Component inside semantic analysis. |
| Intermediate Representation | التمثيل الوسيط | `IR` is acceptable after first mention. |
| Optimizer | المُحسِّن | Pipeline or component. |
| Optimization pass | ممرّ تحسين | One transformation/analysis pass. |
| Backend target | هدف الخلفية | Example: `x86_64-windows`. |
| Code generation | توليد الشيفرة | Emitting assembly or object-level output. |

## IR and optimizer terms

| English term | Preferred Arabic term | Usage note |
|--------------|-----------------------|------------|
| Control-flow graph | مخطط تدفّق التحكم | `CFG` is acceptable after first mention. |
| Basic block | كتلة أساسية | A straight-line block in IR. |
| Terminator | خاتمة الكتلة | Branch/return instruction ending a block. |
| SSA | صيغة الإسناد المفرد | `SSA` is acceptable after first mention. |
| Phi node | عقدة فاي | Use `فاي` for the IR instruction name. |
| Dominator | مسيطِر | For dominance analysis. |
| Dominance frontier | حدّ السيطرة | For phi placement. |
| Liveness | الحيوية | Register/data-flow liveness. |
| Data layout | تخطيط البيانات | Size/alignment/layout rules. |
| Verification | التحقّق | IR/SSA/gate checks. |
| Canonicalization | التوحيد القياسي | Normalizing equivalent IR forms. |
| Constant folding | طيّ الثوابت | Compile-time reduction. |
| Dead-code elimination | حذف الشيفرة الميتة | DCE acceptable in implementation notes. |
| Common subexpression elimination | حذف التعبيرات المشتركة | CSE acceptable after first mention. |
| Loop invariant code motion | نقل ثوابت الحلقات | LICM acceptable after first mention. |
| Inlining | التضمين الداخلي | Function-body expansion. |

## Backend and ABI terms

| English term | Preferred Arabic term | Usage note |
|--------------|-----------------------|------------|
| Instruction selection | اختيار التعليمات | IR-to-machine lowering. |
| Register allocation | تخصيص السجلات | Assigning virtual to physical registers. |
| Virtual register | سجل افتراضي | IR/machine virtual value. |
| Physical register | سجل فعلي | Hardware register. |
| Spill | سكب إلى المكدّس | Use for register spill events. |
| Stack | المكدّس | Call stack/storage. |
| Heap | الكومة | Dynamic allocation area. |
| Assembly | التجميع | Textual machine-code representation. |
| Assembler | المُجمِّع | Program that produces objects from assembly. |
| Linker | الرابط | Program that combines objects/libraries. |
| Object file | ملف كائن | `.o`/`.obj`. |
| Executable | ملف تنفيذي | Final runnable artifact. |
| ABI | واجهة التطبيق الثنائية | `ABI` acceptable after first mention. |
| Calling convention | اصطلاح الاستدعاء | Windows x64/SystemV rules. |
| Relocation | إعادة التمركز | Object/linker fixups. |
| Symbol table | جدول الرموز | Object/compiler symbol collection. |

## Language terms

| English term | Preferred Arabic term | Usage note |
|--------------|-----------------------|------------|
| Type | نوع | Prefer plain form in prose. |
| Integer | عدد صحيح | For the concept; keep keyword `صحيح` in code. |
| Unsigned integer | عدد صحيح غير موقّع | For `ط*` integer families. |
| Floating point | فاصلة عائمة | For `عشري`. |
| Boolean | منطقي | For concept and keyword. |
| Character | محرف | For the concept; keep keyword `حرف` in code. |
| Unicode scalar value | قيمة Unicode scalar | Keep the English phrase for precision. |
| Grapheme cluster | عنقود كتابي | Use when explaining what Baa does not segment. |
| String | نص | For concept and keyword. |
| Array | مصفوفة | Fixed-size arrays. |
| Pointer | مؤشر | Pointer type/value. |
| Null pointer | مؤشر فارغ | Use `عدم` for the literal in code. |
| Struct | هيكل | For concept and keyword. |
| Union | اتحاد | For concept and keyword. |
| Enum | تعداد | For concept and keyword. |
| Function | دالة | For declarations/definitions/calls. |
| Variable | متغيّر | For mutable storage. |
| Constant | ثابت | For immutable storage. |
| Scope | نطاق | Lexical/name-resolution scope. |
| Statement | عبارة | Complete executable construct. |
| Expression | تعبير | Produces a value. |
| Literal | قيمة حرفية | Source literal token. |
| Keyword | كلمة مفتاحية | Reserved word. |
| Identifier | معرّف | User-defined name. |
| Operator | مُعامِل | Symbol/action such as `+`. |
| Operand | مُعامَل | Value consumed by an operator. |
| Control flow | تدفّق التحكم | Branching and loops. |
| Loop | حلقة | `طالما`/`لكل`. |
| Condition | شرط | Boolean decision expression. |

## Diagnostics, testing, and release terms

| English term | Preferred Arabic term | Usage note |
|--------------|-----------------------|------------|
| Diagnostic | تشخيص | Generic compiler message. |
| Error | خطأ | Fatal diagnostic. |
| Warning | تحذير | Non-fatal diagnostic unless `-Werror`. |
| Hint | تلميح | Guidance line in diagnostics. |
| Fix-it | إصلاح مقترح | Suggested source edit. |
| Runtime check | فحص وقت التشغيل | Optional guard emitted into program. |
| Panic | فشل سريع | For fail-fast runtime paths. |
| Standard library | المكتبة القياسية | Public `stdlib` surface. |
| Ownership | الملكية | Storage/resource responsibility. |
| Owned result | نتيجة مملوكة | Caller must release. |
| Borrowed pointer | مؤشر مُعار | Caller must not free. |
| Build manifest | بيان البناء | `--emit-build-manifest` output. |
| Incremental cache | ذاكرة البناء المؤقتة | Object-cache/invalidation feature. |
| Test suite | حزمة الاختبارات | Whole test collection. |
| Regression test | اختبار ارتداد | Prevents a bug from returning. |
| Stress test | اختبار ضغط | Larger/scale-oriented case. |
| Release gate | بوابة الإصدار | Required pre-release validation. |
