cmake_minimum_required(VERSION 3.10)
project(baa VERSION 0.4.4.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# Build options
option(BAA_ENABLE_WARNINGS "Enable compiler warnings for Baa" ON)
option(BAA_WARNINGS_AS_ERRORS "Treat warnings as errors for hardening builds" OFF)

# List of source files
set(SOURCES
    src/frontend/analysis.c    # Semantic Analysis module
    src/support/error.c
    src/support/read_file.c
    src/middleend/ir.c          # Intermediate Representation (Phase 3)
    src/middleend/ir_arena.c    # IR arena allocator (v0.3.2.6.1)
    src/middleend/ir_mutate.c   # IR mutation helpers (v0.3.2.6.1)
    src/middleend/ir_defuse.c   # IR def-use chains (v0.3.2.6.1)
    src/middleend/ir_data_layout.c # IR data layout queries (v0.3.2.6.6)
    src/middleend/ir_clone.c    # IR cloning (v0.3.2.6.1)
    src/middleend/ir_text.c     # IR text serialization (v0.3.2.6.3)
    src/middleend/ir_loop.c     # IR loop detection (v0.3.2.7.1)
    src/middleend/ir_licm.c     # IR loop invariant code motion (v0.3.2.7.1)
    src/middleend/ir_unroll.c   # IR loop unrolling (v0.3.2.7.1)
    src/middleend/ir_inline.c   # IR inlining (v0.3.2.7.2)
    src/middleend/ir_analysis.c # IR analysis (CFG + dominance) (v0.3.1.1)
    src/middleend/ir_builder.c  # IR Builder pattern API (v0.3.0.2)
    src/middleend/ir_verify_ssa.c # IR SSA verification (التحقق من SSA) (v0.3.2.5.3)
    src/middleend/ir_verify_ir.c  # IR well-formedness verification (--verify-ir) (v0.3.2.6.5)
    src/middleend/ir_verify_common.c # IR/SSA shared verification helpers (v0.3.2.9.4)
    src/middleend/ir_canon.c      # IR canonicalization (v0.3.2.6.5)
    src/middleend/ir_cfg_simplify.c # IR CFG simplification (v0.3.2.6.5)
    src/middleend/ir_mem2reg.c  # IR Mem2Reg (إدراج فاي + SSA Renaming) (v0.3.2.5.2)
    src/middleend/ir_outssa.c   # IR Out-of-SSA (إزالة فاي قبل الخلفية) (v0.3.2.5.2)
    src/middleend/ir_constfold.c # IR constant folding (v0.3.1.2)
    src/middleend/ir_instcombine.c # IR InstCombine (v0.3.2.8.6)
    src/middleend/ir_sccp.c        # IR SCCP (v0.3.2.8.6)
    src/middleend/ir_gvn.c         # IR GVN (v0.3.2.8.6)
    src/middleend/ir_dce.c      # IR dead code elimination (حذف_الميت) (v0.3.1.3)
    src/middleend/ir_copyprop.c # IR copy propagation (نشر_النسخ) (v0.3.1.4)
    src/middleend/ir_cse.c      # IR CSE (حذف_المكرر) (v0.3.1.5)
    src/middleend/ir_lower.c    # AST -> IR lowering (v0.3.0.3)
    src/middleend/ir_pass.c     # IR optimizer pass interface (v0.3.1.1)
    src/middleend/ir_optimizer.c # IR optimization pipeline (v0.3.1.6)
    src/backend/isel.c        # Instruction selection (v0.3.2.1)
    src/backend/regalloc.c    # Register allocation (v0.3.2.2)
    src/backend/emit.c        # Code emission (v0.3.2.3)
    src/backend/target.c      # Target abstraction (v0.3.2.8.1)
    src/frontend/lexer.c
    src/driver/main.c
    src/frontend/parser.c
    src/driver/process.c
    src/driver/driver_cli.c
    src/driver/driver_time.c
    src/driver/driver_toolchain.c
    src/driver/driver_pipeline.c
)

# Add resource file for Windows (Icon & Version Info)
if(WIN32)
    list(APPEND SOURCES src/baa.rc)
    list(APPEND SOURCES src/support/updater.c)
else()
    list(APPEND SOURCES src/support/updater_stub.c)
endif()

# Windows executable extension
add_executable(baa ${SOURCES})

target_include_directories(baa PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/src/backend>
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/src/driver>
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/src/frontend>
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/src/middleend>
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/src/support>
)

# ============================================================================
# Install Rules (Linux packaging)
# ============================================================================

include(GNUInstallDirs)

install(TARGETS baa
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY docs
    DESTINATION ${CMAKE_INSTALL_DATADIR}/baa
)

install(DIRECTORY resources
    DESTINATION ${CMAKE_INSTALL_DATADIR}/baa
)

install(FILES README.md CHANGELOG.md ROADMAP.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/baa
)

# ============================================================================
# CPack (TGZ + DEB)
# ============================================================================

set(CPACK_PACKAGE_NAME "baa")
set(CPACK_PACKAGE_VENDOR "Omar Aglan")
set(CPACK_PACKAGE_CONTACT "Omar Aglan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Baa Compiler (باء)")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "baa-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ;DEB")
    # DEB packages should install into /usr (not /usr/local).
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Omar Aglan")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)

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
    target_link_libraries(baa wintrust crypt32)
endif()
