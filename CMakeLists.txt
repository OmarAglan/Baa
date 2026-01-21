cmake_minimum_required(VERSION 3.10)
project(baa VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# List of source files
set(SOURCES
    src/analysis.c    # Semantic Analysis module
    src/codegen.c
    src/error.c
    src/ir.c          # Intermediate Representation (Phase 3)
    src/ir_analysis.c # IR analysis (CFG + dominance) (v0.3.1.1)
    src/ir_builder.c  # IR Builder pattern API (v0.3.0.2)
    src/ir_lower.c    # AST -> IR lowering (v0.3.0.3)
    src/ir_pass.c     # IR optimizer pass interface (v0.3.1.1)
    src/lexer.c
    src/main.c
    src/parser.c
    src/updater.c
)

# Add resource file for Windows (Icon & Version Info)
if(WIN32)
    list(APPEND SOURCES src/baa.rc)
endif()

# Windows executable extension
add_executable(baa ${SOURCES})

# Link Windows libraries for Updater (WinINet, UrlMon)
if(WIN32)
    target_link_libraries(baa urlmon wininet)
endif()