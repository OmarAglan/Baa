# Baa Roadmap

## v0.0.1 (Current Goal)
- [ ] Setup CMake build system.
- [ ] **Lexer**: Handle `إرجع` (return), Integers, and `.` (dot).
- [ ] **Parser**: Create a `Program` node and a `ReturnStatement` node.
- [ ] **Codegen**: Output x86_64 Assembly (Linux/macOS compatible) to exit with a code.
- [ ] **Driver**: Compile `.b` file to `.s`, then run `as` and `ld`.

## v0.0.2 (Next)
- [ ] Support Basic Math (`+`, `-`).