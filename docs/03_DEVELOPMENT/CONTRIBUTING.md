# Contributing to Baa Programming Language

Welcome to the Baa programming language project! We're excited that you're interested in contributing to the development of an Arabic-based programming language.

## 🎯 Project Overview

Baa is a programming language designed to support Arabic syntax while maintaining conceptual compatibility with C language features. The project aims to provide a complete compiler toolchain with Arabic keywords and identifiers.

**Current Status:** Priority 4 Complete (Function Definitions and Calls)
**Next Phase:** Priority 5 (Advanced Type System and Semantic Analysis)

## 🚀 Quick Start for Contributors

### Prerequisites

- **CMake 3.20+**
- **C11-compatible compiler** (GCC, Clang, or MSVC)
- **Git**
- **(Optional)** LLVM development libraries

### Setting Up Development Environment

1. **Fork and Clone**
   ```bash
   git clone https://github.com/OmarAglan/baa.git
   cd baa
   ```

2. **Build the Project**
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```

3. **Run Tests**
   ```bash
   ctest
   ```

## 📋 Areas for Contribution

### High Priority (Priority 5 Goals)

1. **Semantic Analysis**
   - Symbol table implementation
   - Type checking system
   - Scope resolution
   - Control flow analysis

2. **Advanced Type System**
   - Array types implementation
   - Struct and union types
   - Enum types
   - Pointer types

3. **Code Generation**
   - LLVM IR generation
   - Optimization passes
   - Target code generation

### Medium Priority

1. **Standard Library**
   - Arabic I/O functions
   - Math library
   - String manipulation functions
   - Memory management utilities

2. **Developer Tools**
   - Language server protocol support
   - Syntax highlighting for editors
   - Debugging tools

3. **Documentation**
   - API documentation
   - Tutorial content
   - Example programs

### Low Priority

1. **Advanced Features**
   - Generic programming support
   - Module system
   - Package management

## 🛠️ Development Guidelines

### Code Style

1. **C Code Standards**
   - Use consistent indentation (4 spaces)
   - Follow existing naming conventions
   - Add comprehensive comments
   - Include error handling

2. **Documentation Standards**
   - Update documentation for any API changes
   - Include Arabic translations where appropriate
   - Add examples for new features
   - Maintain bilingual documentation structure

### Commit Guidelines

1. **Commit Messages**
   - Use clear, descriptive commit messages
   - Reference issue numbers when applicable
   - Use conventional commit format when possible

2. **Branch Naming**
   - `feature/description` for new features
   - `fix/description` for bug fixes
   - `docs/description` for documentation updates

### Testing Requirements

1. **Unit Tests**
   - Add tests for all new functionality
   - Ensure existing tests continue to pass
   - Test both positive and negative cases

2. **Integration Tests**
   - Test component interactions
   - Verify Arabic language features work correctly
   - Test error handling and recovery

## 📝 Contribution Process

### 1. Planning

1. **Check Existing Issues**
   - Look for existing issues or feature requests
   - Comment on issues you'd like to work on

2. **Create New Issues**
   - Describe the problem or feature clearly
   - Include examples and use cases
   - Tag with appropriate labels

### 2. Development

1. **Create Feature Branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make Changes**
   - Follow coding standards
   - Add tests
   - Update documentation

3. **Test Thoroughly**
   ```bash
   cmake --build .
   ctest
   ```

### 3. Submission

1. **Create Pull Request**
   - Provide clear description of changes
   - Reference related issues
   - Include testing information

2. **Code Review**
   - Address reviewer feedback
   - Make requested changes
   - Ensure CI passes

## 🌍 Arabic Language Considerations

### Arabic Text Handling

1. **Unicode Support**
   - Ensure proper UTF-8/UTF-16 handling
   - Test with Arabic text input
   - Verify RTL text display

2. **Arabic Keywords**
   - Follow established Arabic terminology
   - Maintain consistency across codebase
   - Document new Arabic terms

3. **Error Messages**
   - Provide Arabic error messages
   - Ensure cultural appropriateness
   - Test message formatting

## 📚 Resources

### Documentation

- **[Architecture Overview](../02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md)** - Understanding the codebase
- **[Current Status](../00_OVERVIEW/CURRENT_STATUS.md)** - What's implemented
- **[Roadmap](../04_ROADMAP/ROADMAP_OVERVIEW.md)** - Future plans
- **[Building Guide](BUILDING.md)** - Detailed build instructions

### Communication

- **Issues:** [GitHub Issues](https://github.com/OmarAglan/baa/issues)
- **Discussions:** [GitHub Discussions](https://github.com/OmarAglan/baa/discussions)
- **Documentation:** [Navigation Guide](../NAVIGATION.md)

## 🏆 Recognition

Contributors will be recognized in:
- Project README
- Release notes
- Contributor documentation

## 📄 License

By contributing to Baa, you agree that your contributions will be licensed under the MIT License.

## 🤝 Code of Conduct

We are committed to providing a welcoming and inclusive environment for all contributors. Please be respectful and professional in all interactions.

## ❓ Getting Help

If you need help or have questions:

1. Check existing documentation
2. Search existing issues
3. Create a new issue with the "question" label
4. Join our discussions for general questions

Thank you for contributing to the Baa programming language! 🚀

---

**Happy Coding! / برمجة سعيدة!**
