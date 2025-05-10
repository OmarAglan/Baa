cmake_minimum_required(VERSION 3.20)
project(baa
    VERSION 0.1.12.0
    DESCRIPTION "لغة باء - The Baa Programming Language"
    LANGUAGES C
)

# إعداد معيار لغة C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Option to skip parser compilation for development (No longer directly applicable this way)
# option(SKIP_PARSER "Skip compiling the parser components" OFF)

add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /IGNORE:4006 /IGNORE:4099") # Ignore certain linker warnings if needed

# إضافة مجلد التضمين الرئيسي
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- البرنامج الرئيسي ---
add_executable(baa
    # Core executable files
    src/main.c
    src/compiler.c

    # Utility files
    src/utils/utils.c

    # Type system files
    src/types/types.c

    # Operator files
    src/operators/operators.c

    # Lexer files
    src/lexer/lexer.c
    src/lexer/number_parser.c
    src/lexer/lexer_char_utils.c
    src/lexer/token_scanners.c   # Added token scanners source

    # Preprocessor files
    src/preprocessor/preprocessor.c
    src/preprocessor/preprocessor_utils.c
    src/preprocessor/preprocessor_macros.c
    src/preprocessor/preprocessor_expansion.c
    src/preprocessor/preprocessor_conditionals.c
    src/preprocessor/preprocessor_expr_eval.c
    src/preprocessor/preprocessor_core.c
    src/preprocessor/preprocessor_directives.c
    src/preprocessor/preprocessor_line_processing.c

    # Analysis files
    src/analysis/flow_analysis.c
    src/analysis/flow_errors.c

    # Codegen core file (always needed)
    src/codegen/codegen.c
)

# Include directories for the executable
target_include_directories(baa
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src       # Allow includes like "lexer/lexer_char_utils.h" or relative from within src
        # ${CMAKE_SOURCE_DIR}/src/preprocessor # Already covered by ../src
)

# --- Conditional LLVM Handling ---
# Check if LLVM is available (You might need to adjust how LLVM_AVAILABLE is set/found)
# For now, assume it's OFF unless explicitly enabled or found via find_package(LLVM)
option(USE_LLVM "Enable LLVM backend" OFF) # Example option

if(USE_LLVM)
    # Attempt to find LLVM (adjust paths/version as needed)
    find_package(LLVM REQUIRED CONFIG) # Or adjust version/components

    if(LLVM_FOUND)
        message(STATUS "LLVM Found: ${LLVM_PACKAGE_VERSION}")
        message(STATUS "LLVM Include Dirs: ${LLVM_INCLUDE_DIRS}")
        message(STATUS "LLVM Library Dirs: ${LLVM_LIBRARY_DIRS}")

        # Add LLVM source file
        target_sources(baa PRIVATE src/codegen/llvm_codegen.c)

        # Add LLVM include directories
        target_include_directories(baa PRIVATE ${LLVM_INCLUDE_DIRS})

        # Add LLVM compile definitions
        target_compile_definitions(baa PRIVATE LLVM_AVAILABLE=1)

        # Link LLVM libraries
        # list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}") # Usually handled by find_package
        # include(AddLLVM) # Usually handled by find_package
        llvm_map_components_to_libnames(llvm_libs
            core support irreader analysis target mcjit native executionengine
            # Add other necessary components
        )
        message(STATUS "Linking LLVM Libraries: ${llvm_libs}")
        target_link_libraries(baa PRIVATE ${llvm_libs})

    else()
        message(WARNING "USE_LLVM is ON but LLVM was not found. Using stub backend.")
        target_sources(baa PRIVATE src/codegen/llvm_stub.c)
        target_compile_definitions(baa PRIVATE LLVM_AVAILABLE=0)
    endif()
else()
    message(STATUS "LLVM backend disabled. Using stub backend.")
    # Add stub source file
    target_sources(baa PRIVATE src/codegen/llvm_stub.c)
    # Add compile definition
    target_compile_definitions(baa PRIVATE LLVM_AVAILABLE=0)
endif()

# --- Standalone Preprocessor Tester ---
add_executable(baa_preprocessor_tester
    tools/baa_preprocessor_tester.c

    # Preprocessor files (needed by the tester)
    src/preprocessor/preprocessor.c
    src/preprocessor/preprocessor_utils.c
    src/preprocessor/preprocessor_macros.c
    src/preprocessor/preprocessor_expansion.c
    src/preprocessor/preprocessor_conditionals.c
    src/preprocessor/preprocessor_expr_eval.c
    src/preprocessor/preprocessor_core.c
    src/preprocessor/preprocessor_directives.c
    src/preprocessor/preprocessor_line_processing.c

    # Utility files (needed by the preprocessor)
    src/utils/utils.c
)

# Include directories for the tester
target_include_directories(baa_preprocessor_tester
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include       # For baa/preprocessor/preprocessor.h
        ${CMAKE_SOURCE_DIR}/src/preprocessor # For preprocessor_internal.h
)

# Set output directory for the tester executable (optional, but good practice)
set_target_properties(baa_preprocessor_tester PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


# تفعيل الاختبارات (معطل مؤقتاً)
# enable_testing()
# add_subdirectory(tests) # Keep this if tests are separate


