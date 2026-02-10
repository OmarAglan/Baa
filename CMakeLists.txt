cmake_minimum_required(VERSION 3.10)
project(baa VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# Build options
option(BAA_ENABLE_WARNINGS "Enable compiler warnings for Baa" ON)
option(BAA_WARNINGS_AS_ERRORS "Treat warnings as errors for hardening builds" OFF)

# List of source files
set(SOURCES
    src/analysis.c    # Semantic Analysis module
    src/error.c
    src/ir.c          # Intermediate Representation (Phase 3)
    src/ir_analysis.c # IR analysis (CFG + dominance) (v0.3.1.1)
    src/ir_builder.c  # IR Builder pattern API (v0.3.0.2)
    src/ir_verify_ssa.c # IR SSA verification (التحقق من SSA) (v0.3.2.5.3)
    src/ir_mem2reg.c  # IR Mem2Reg (إدراج فاي + SSA Renaming) (v0.3.2.5.2)
    src/ir_outssa.c   # IR Out-of-SSA (إزالة فاي قبل الخلفية) (v0.3.2.5.2)
    src/ir_constfold.c # IR constant folding (v0.3.1.2)
    src/ir_dce.c      # IR dead code elimination (حذف_الميت) (v0.3.1.3)
    src/ir_copyprop.c # IR copy propagation (نشر_النسخ) (v0.3.1.4)
    src/ir_cse.c      # IR CSE (حذف_المكرر) (v0.3.1.5)
    src/ir_lower.c    # AST -> IR lowering (v0.3.0.3)
    src/ir_pass.c     # IR optimizer pass interface (v0.3.1.1)
    src/ir_optimizer.c # IR optimization pipeline (v0.3.1.6)
    src/isel.c        # Instruction selection (v0.3.2.1)
    src/regalloc.c    # Register allocation (v0.3.2.2)
    src/emit.c        # Code emission (v0.3.2.3)
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

# Warnings / hardening flags (two-tier)
if(BAA_ENABLE_WARNINGS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(baa PRIVATE -Wall -Wextra -Wformat=2 -Wshadow)
        if(BAA_WARNINGS_AS_ERRORS)
            target_compile_options(baa PRIVATE -Werror)
        endif()
    endif()
endif()

# Link Windows libraries for Updater (WinINet, UrlMon)
if(WIN32)
    target_link_libraries(baa urlmon wininet)
endif()
