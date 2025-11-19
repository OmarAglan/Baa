# Baa Language Lexer Roadmap

**Status: ✅ PRODUCTION READY (v0.1.33.0+)**
**Current Focus: Maintenance & Performance Optimization**

This roadmap outlines the status of the Baa language lexer. This component is considered **complete and stable**, operating on preprocessed UTF-16LE input.

## 🎯 Current Status Summary

### ✅ Maturity Level: Production

* **Core Tokenization:** 100% Complete & Tested.
* **Arabic Support:** 100% Complete (Keywords, Indic Digits, Suffixes, Escape Sequences).
* **Error System:** 100% Complete (Enhanced multi-error reporting integrated).
* **Documentation:** 100% Complete.

---

## 1. Core Lexer Functionality (✅ COMPLETED)

* [x] **Token Management:** `BaaToken` struct with `lexeme`, `type`, `line`, `column`.
* [x] **Input Handling:** Processing UTF-16LE streams from the preprocessor.
* [x] **Whitespace:** Tokenizing whitespace/newlines (crucial for source fidelity tools).
* [x] **Enhanced Error System:**
  * Integration with `make_specific_error_token`.
  * Specific error codes (1001-1009) for precise diagnostics.
  * Arabic error messages and "Smart Suggestions".

## 2. Token Type Implementation (✅ COMPLETED)

* **Comments:**
  * [x] Single-line (`//`) and Multi-line (`/* ... */`).
  * [x] Documentation comments (`/** ... */`).
* **Literals:**
  * [x] **Integers:** Decimal, Hex (`0x`), Binary (`0b`).
  * [x] **Arabic Integers:** Arabic-Indic digits (`٠-٩`).
  * [x] **Suffixes:** Arabic suffixes (`غ` unsigned, `ط` long, `طط` long long).
  * [x] **Floats:** Decimal (`.`), Arabic Decimal (`٫`), Scientific (`أ` exponent).
  * [x] **Strings:** Standard (`"..."`), Multiline (`"""..."""`), Raw (`خ"..."`).
  * [x] **Chars:** Standard (`'a'`) and Unicode (`\يXXXX`).
* **Language Elements:**
  * [x] **Keywords:** Full Arabic keyword set (`إذا`, `لكل`, `ثابت`...).
  * [x] **Operators:** Full C-style operator set compatible with Baa.

## 3. Number Parsing Integration (✅ COMPLETED)

The lexer works seamlessly with `src/lexer/number_parser.c`.

* [x] **Hand-off:** Lexer identifies the raw lexeme string (including mixed Arabic/Latin digits).
* [x] **Parsing:** `number_parser.c` normalizes Arabic-Indic digits to integer values.
* [x] **Suffix Handling:** Correctly interprets `غ`, `ط`, `ح` flags.

## 4. String & Character Processing (✅ COMPLETED)

* [x] **Escape Sequences:**
  * Standard: `\n`, `\t`, etc.
  * **Arabic Specific:** `\س` (newline), `\م` (tab), `\ر` (CR), `\ص` (null).
* [x] **Encodings:** Internal conversion of `\يXXXX` to wide characters.

## 5. Error Handling Overhaul (✅ COMPLETED)

The lexer now uses the unified, robust error system.

* [x] **Specific Error Tokens:**
  * `BAA_TOKEN_ERROR_UNTERMINATED_STRING`
  * `BAA_TOKEN_ERROR_INVALID_NUMBER`
  * `BAA_TOKEN_ERROR_INVALID_CHARACTER` (and others).
* [x] **Context Awareness:** Source context extraction (30 chars before/after) for error messages.
* [x] **Recovery:** Intelligent synchronization (e.g., finding the next quote or whitespace) to prevent cascading errors.

## 6. Performance & Optimization (Ongoing)

* [x] **Keyword Lookup:** Optimized length-first comparison.
* [x] **Character Classification:** Fast Unicode range checks for Arabic letters/digits.
* [ ] **String Interning:** (Future) Deduplicate identifier strings to save memory.
* [ ] **SIMD Scanning:** (Future) For massive file throughput.

## 7. Testing & Validation (✅ COMPLETED)

* [x] **Unit Tests:** `tests/unit/lexer/` covers all token types.
* [x] **Integration:** Verified against Preprocessor output and Parser input.
* [x] **Tooling:** `tools/baa_lexer_tester.c` provides CLI debugging for token streams.

## Future Roadmap (Post-v1.0)

1. **Deeper Unicode Compliance:** Implement UAX #31 (Unicode Identifier and Pattern Syntax) fully.
2. **Syntax Highlighting Support:** Export token streams formats compatible with VS Code / Editors.
3. **Incremental Lexing:** For IDE support (re-lexing only changed lines).
