cmake_minimum_required(VERSION 3.10)
project(baa VERSION 0.6.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# Build options
option(BAA_ENABLE_WARNINGS "Enable compiler warnings for Baa" ON)
option(BAA_WARNINGS_AS_ERRORS "Treat warnings as errors for hardening builds" OFF)
option(BAA_ENABLE_EMBEDDED_NAZM
       "Link the in-process nazm-api-v1 assembler into baa" OFF)
option(BAA_EMBEDDED_NAZM_DEFAULT
       "Assemble in-process by default; an explicit Nazm executable still wins" OFF)
set(BAA_NAZM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Nazm" CACHE PATH
    "Nazm source tree used when BAA_ENABLE_EMBEDDED_NAZM is enabled")
set(BAA_NAZM_REVISION "" CACHE STRING
    "Nazm source revision shown by baa --version (read from git when empty)")
set(BAA_REPRODUCIBLE_BUILD_DATE "" CACHE STRING "Override baa --version build date for reproducible builds")

if(BAA_EMBEDDED_NAZM_DEFAULT AND NOT BAA_ENABLE_EMBEDDED_NAZM)
    message(FATAL_ERROR
        "BAA_EMBEDDED_NAZM_DEFAULT requires BAA_ENABLE_EMBEDDED_NAZM=ON")
endif()

if(BAA_ENABLE_EMBEDDED_NAZM)
    if(NOT EXISTS "${BAA_NAZM_SOURCE_DIR}/include/nazm.h")
        message(FATAL_ERROR
            "BAA_ENABLE_EMBEDDED_NAZM requires a Nazm source tree at "
            "${BAA_NAZM_SOURCE_DIR}")
    endif()
    set(NAZM_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(NAZM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BAA_NAZM_SOURCE_DIR}"
                     "${CMAKE_BINARY_DIR}/nazm-embedded"
                     EXCLUDE_FROM_ALL)

    # The linked assembler is identified by the exact source revision it was
    # built from. Only a checkout whose root is the Nazm tree itself counts, so
    # a tree unpacked inside another repository never reports that repository.
    set(baa_nazm_revision "${BAA_NAZM_REVISION}")
    if(NOT baa_nazm_revision)
        find_package(Git QUIET)
        if(GIT_FOUND)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${BAA_NAZM_SOURCE_DIR}"
                        rev-parse --show-toplevel
                OUTPUT_VARIABLE baa_nazm_toplevel
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE baa_nazm_git_result)
            get_filename_component(baa_nazm_source_real
                "${BAA_NAZM_SOURCE_DIR}" REALPATH)
            if(baa_nazm_git_result EQUAL 0 AND baa_nazm_toplevel)
                get_filename_component(baa_nazm_toplevel_real
                    "${baa_nazm_toplevel}" REALPATH)
            endif()
            if(baa_nazm_git_result EQUAL 0 AND
               "${baa_nazm_toplevel_real}" STREQUAL "${baa_nazm_source_real}")
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" -C "${BAA_NAZM_SOURCE_DIR}"
                            rev-parse HEAD
                    OUTPUT_VARIABLE baa_nazm_revision
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" -C "${BAA_NAZM_SOURCE_DIR}"
                            status --porcelain --untracked-files=no
                    OUTPUT_VARIABLE baa_nazm_dirty
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
                if(baa_nazm_revision AND baa_nazm_dirty)
                    string(APPEND baa_nazm_revision "-dirty")
                endif()
            endif()
        endif()
    endif()
    if(NOT baa_nazm_revision)
        set(baa_nazm_revision "unknown")
    endif()
    if(NOT baa_nazm_revision MATCHES "^[A-Za-z0-9._-]+$")
        message(FATAL_ERROR
            "BAA_NAZM_REVISION may only contain letters, digits, '.', '_' and '-'")
    endif()
    message(STATUS "Embedded Nazm revision: ${baa_nazm_revision}")
endif()

# List of source files
set(SOURCES
    src/frontend/analysis.c    # Semantic Analysis module
    src/frontend/language_profile.c # Central language/completion metadata
    src/frontend/formatter.c  # Canonical source formatting for tools
    src/frontend/source_tokens.c  # Raw source token stream for tooling
    src/support/error.c
    src/support/file_io.c
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
    src/backend/emit_nazm.c   # Canonical Arabic Nazm source emission
    src/backend/target.c      # Target abstraction (v0.3.2.8.1)
    src/frontend/lexer.c
    src/driver/main.c
    src/frontend/parser.c
    src/driver/process.c
    src/driver/driver_cli.c
    src/driver/driver_artifacts.c
    src/driver/driver_time.c
    src/driver/driver_toolchain.c
    src/driver/driver_nazm.c
    src/driver/driver_startup.c
    src/driver/driver_pipeline.c
    src/driver/driver_symbols.c
    src/driver/driver_tokens.c
    src/driver/driver_structure.c
    src/driver/driver_completion.c
    src/driver/driver_format.c
    src/driver/driver_semantic.c
    src/driver/driver_build.c
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

if(BAA_ENABLE_EMBEDDED_NAZM)
    target_link_libraries(baa PRIVATE libnazm)
    target_compile_definitions(baa PRIVATE
        BAA_EMBEDDED_NAZM=1
        BAA_EMBEDDED_NAZM_REVISION="${baa_nazm_revision}")
    if(BAA_EMBEDDED_NAZM_DEFAULT)
        target_compile_definitions(baa PRIVATE BAA_EMBEDDED_NAZM_DEFAULT=1)
    endif()
endif()

# مكتبة وقت التشغيل التي ترتبط بها برامج باء عند استخدام واجهات النظام.
add_library(baa_runtime STATIC
    src/runtime/arabic_abi_runtime.c
    src/runtime/entry_runtime.c
    src/runtime/hash_runtime.c
    src/runtime/process_runtime.c
    src/support/file_io.c
)
set_target_properties(baa_runtime PROPERTIES
    OUTPUT_NAME baa_runtime
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# MinGW needs the Unicode CRT entrypoint so wmain receives the original UTF-16
# command line. main.c converts it once to the compiler's internal UTF-8 contract.
if(MINGW)
    set_property(TARGET baa APPEND_STRING PROPERTY LINK_FLAGS " -municode")
endif()

if(BAA_REPRODUCIBLE_BUILD_DATE)
    target_compile_definitions(baa PRIVATE BAA_BUILD_DATE_OVERRIDE="${BAA_REPRODUCIBLE_BUILD_DATE}")
endif()

# نبقي مسارات التضمين العامة مغلقة عمداً.
# يجب أن تعتمد الملفات على ترويساتها المحلية أو على مسارات نسبية صريحة بين المكونات.

# ============================================================================
# Install Rules (Linux packaging)
# ============================================================================

include(GNUInstallDirs)

install(TARGETS baa
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS baa_runtime
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/baa
)

install(DIRECTORY docs
    DESTINATION ${CMAKE_INSTALL_DATADIR}/baa
)

install(DIRECTORY resources
    DESTINATION ${CMAKE_INSTALL_DATADIR}/baa
)

install(DIRECTORY stdlib
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
    # Baa links through the host GCC, so the package declares it instead of
    # assuming it; shared-library dependencies are appended to this list.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "gcc, libc6-dev")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)

# Warnings / hardening flags (two-tier)
if(BAA_ENABLE_WARNINGS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(baa PRIVATE -Wall -Wextra -Wformat=2 -Wshadow)
        target_compile_options(baa_runtime PRIVATE -Wall -Wextra -Wformat=2 -Wshadow)
        if(BAA_WARNINGS_AS_ERRORS)
            target_compile_options(baa PRIVATE -Werror)
            target_compile_options(baa_runtime PRIVATE -Werror)
        endif()
    endif()
endif()

# Link Windows libraries for Updater (WinINet, UrlMon)
if(WIN32)
    target_link_libraries(baa PRIVATE urlmon wininet)
    target_link_libraries(baa PRIVATE wintrust crypt32)
endif()
