cmake_minimum_required(VERSION 3.10)
project(baa C)
set(CMAKE_C_STANDARD 11)
# Windows executable extension
add_executable(baa src/main.c src/lexer.c src/parser.c src/codegen.c)