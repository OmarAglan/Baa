# Debugging Baa Compiler

This guide provides debugging techniques and tools for working with the Baa programming language compiler.

## 🎯 Overview

The Baa compiler consists of several stages, each of which can be debugged independently:
1. **Preprocessor** - Handles Arabic directives and macro expansion
2. **Lexer** - Tokenizes Arabic and English text
3. **Parser** - Builds Abstract Syntax Tree (AST)
4. **Semantic Analysis** - Type checking and symbol resolution *(Planned)*
5. **Code Generation** - LLVM IR generation *(Planned)*

## 🔧 Debug Tools and Utilities

### Built-in Test Tools

The Baa project includes several testing utilities for debugging specific components:

```bash
# Test preprocessor
./build/tools/baa_preprocessor_tester input.baa

# Test lexer
./build/tools/baa_lexer_tester input.baa

# Test parser (when available)
./build/tools/baa_parser_tester input.baa
```

### Debug Build Configuration

For debugging, build with debug symbols and additional logging:

```bash
mkdir debug_build && cd debug_build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

## 🐛 Common Debugging Scenarios

### 1. Preprocessor Issues

**Symptoms:**
- Macro expansion not working correctly
- Include files not found
- Conditional compilation problems

**Debugging Steps:**

1. **Enable Preprocessor Verbose Output:**
   ```bash
   ./build/tools/baa_preprocessor_tester -v input.baa
   ```

2. **Check Macro Definitions:**
   - Verify Arabic macro names are correctly encoded
   - Check for macro redefinition conflicts
   - Ensure proper parameter substitution

3. **Include Path Issues:**
   ```bash
   # Check current working directory
   pwd
   # Verify file exists
   ls -la path/to/include/file.baa
   ```

**Example Debug Session:**
```baa
// test_macro.baa
#تعريف PI ٣.١٤١٥٩
#تعريف SQUARE(x) ((x) * (x))

عدد_حقيقي area = PI * SQUARE(٥).
```

### 2. Lexer Issues

**Symptoms:**
- Arabic keywords not recognized
- Numeric literals with Arabic digits failing
- String literal parsing errors

**Debugging Steps:**

1. **Check Token Output:**
   ```bash
   ./build/tools/baa_lexer_tester input.baa
   # Output written to lexer_test_output.txt
   cat lexer_test_output.txt
   ```

2. **Verify Unicode Encoding:**
   ```bash
   # Check file encoding
   file input.baa
   # Should show UTF-8 or UTF-16
   ```

3. **Arabic Numeral Issues:**
   ```baa
   // Test Arabic numerals
   عدد_صحيح x = ١٢٣؛  // Arabic-Indic digits
   عدد_صحيح y = 123؛   // Western digits
   ```

### 3. Parser Issues

**Symptoms:**
- Syntax errors with valid Arabic code
- AST not building correctly
- Function definitions not parsing

**Debugging Steps:**

1. **Check Parser Output:**
   ```bash
   ./build/tools/baa_parser_tester input.baa
   ```

2. **Verify Syntax:**
   ```baa
   // Correct function syntax
   دالة عدد_صحيح الرئيسية() {
       إرجاع ٠؛
   }
   ```

3. **AST Inspection:**
   - Use parser debug output to examine AST structure
   - Verify node types and relationships

## 🔍 Memory Debugging

### Using Valgrind (Linux/macOS)

```bash
# Check for memory leaks
valgrind --leak-check=full ./build/tools/baa_lexer_tester input.baa

# Check for memory errors
valgrind --tool=memcheck ./build/tools/baa_parser_tester input.baa
```

### Using AddressSanitizer

```bash
# Build with AddressSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address" ..
cmake --build .

# Run with sanitizer
./build/tools/baa_lexer_tester input.baa
```

## 📝 Logging and Tracing

### Enable Debug Logging

The Baa compiler includes debug logging capabilities:

```c
// In source code, use debug macros
DEBUG_LOG("Processing Arabic keyword: %s", keyword);
ERROR_LOG("Failed to parse expression at line %d", line_num);
```

### Custom Debug Output

Add temporary debug output for specific issues:

```c
// Example: Debug macro expansion
printf("DEBUG: Expanding macro '%s' with args: ", macro_name);
for (int i = 0; i < arg_count; i++) {
    printf("'%s' ", args[i]);
}
printf("\n");
```

## 🧪 Test-Driven Debugging

### Create Minimal Test Cases

1. **Isolate the Problem:**
   ```baa
   // minimal_test.baa - Simplest case that reproduces the issue
   عدد_صحيح x = ١؛
   ```

2. **Gradually Add Complexity:**
   ```baa
   // Add one feature at a time
   عدد_صحيح x = ١؛
   عدد_صحيح y = x + ٢؛
   ```

### Regression Testing

```bash
# Run all tests to ensure fixes don't break existing functionality
cd build
ctest -V
```

## 🌍 Arabic-Specific Debugging

### Unicode Issues

1. **Check Text Encoding:**
   ```bash
   # Verify UTF-8 encoding
   hexdump -C input.baa | head
   ```

2. **Arabic Text Direction:**
   - Ensure proper handling of RTL text
   - Check for mixed LTR/RTL content

3. **Arabic Numeral Conversion:**
   ```c
   // Debug Arabic numeral parsing
   printf("Arabic digit: %s -> Western: %d\n", arabic_digit, converted_value);
   ```

### Keyword Recognition

```c
// Debug Arabic keyword matching
if (is_arabic_keyword(token)) {
    printf("Recognized Arabic keyword: %s (type: %d)\n", token, keyword_type);
}
```

## 🚨 Error Recovery Debugging

### Parser Error Recovery

1. **Check Synchronization Points:**
   - Verify parser recovers at statement boundaries
   - Ensure error messages are helpful

2. **Multiple Error Reporting:**
   ```bash
   # Test with file containing multiple errors
   ./build/tools/baa_parser_tester error_test.baa
   ```

## 📊 Performance Debugging

### Profiling

```bash
# Profile with gprof
gcc -pg -o baa_debug src/*.c
./baa_debug input.baa
gprof baa_debug gmon.out > profile.txt
```

### Memory Usage

```bash
# Monitor memory usage
/usr/bin/time -v ./build/tools/baa_parser_tester large_file.baa
```

## 🔧 IDE Integration

### VS Code Debugging

Create `.vscode/launch.json`:
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug Baa Lexer",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/tools/baa_lexer_tester",
            "args": ["test_input.baa"],
            "cwd": "${workspaceFolder}",
            "environment": [],
            "console": "integratedTerminal"
        }
    ]
}
```

## 📚 Additional Resources

- **[Architecture Overview](../02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md)** - Understanding component interactions
- **[Testing Guide](TESTING.md)** - Comprehensive testing strategies
- **[Building Guide](BUILDING.md)** - Build system details

## 🤝 Getting Help

If you encounter issues not covered here:

1. Check existing GitHub issues
2. Create a minimal reproduction case
3. Include debug output and system information
4. Tag the issue with "debugging" label

---

**Happy Debugging! / تصحيح سعيد!**
