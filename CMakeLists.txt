cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # INTERFACE_LINK_LIBRARIES defines the link interface
endif()
if(POLICY CMP0067)
  cmake_policy(SET CMP0067 NEW) # Honor visibility properties for target_compile_definitions
endif()
if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # MACOSX_RPATH is enabled by default
endif()
# Add other policies as you discover needs, e.g., CMP0077 for overriding exported targets.

project(baa
    VERSION 0.1.17.0
    DESCRIPTION "لغة باء - The Baa Programming Language"
    LANGUAGES C
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BaaCompilerSettings)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory (e.g., mkdir build && cd build) and run CMake from there.")
endif()

# إعداد معيار لغة C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /IGNORE:4006 /IGNORE:4099") # Ignore certain linker warnings if needed

add_subdirectory(src)

# --- البرنامج الرئيسي ---
add_executable(baa
    src/main.c
)

# Link component libraries to the main executable
target_link_libraries(baa PRIVATE baa_compiler_lib BaaCommonSettings) # main.c links only the compiler library
# Include directories and compile definitions for the 'baa' executable
target_include_directories(baa PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src       # Allow includes like "lexer/lexer_char_utils.h" or relative from within src
)

# --- Conditional LLVM Handling ---
# Check if LLVM is available (You might need to adjust how LLVM_AVAILABLE is set/found)
# For now, assume it's OFF unless explicitly enabled or found via find_package(LLVM)
option(USE_LLVM "Enable LLVM backend" OFF) # Example option

if(USE_LLVM)
    # Attempt to find LLVM (adjust paths/version as needed)
    find_package(LLVM CONFIG) # Make it non-REQUIRED, handle LLVM_FOUND explicitly

    if(LLVM_FOUND)
        message(STATUS "LLVM Found: ${LLVM_PACKAGE_VERSION}")
        message(STATUS "LLVM Include Dirs: ${LLVM_INCLUDE_DIRS}")
        message(STATUS "LLVM Library Dirs: ${LLVM_LIBRARY_DIRS}")

    # LLVM is found, baa_codegen will use it.
    # The LLVM_AVAILABLE definition will be set for baa_codegen in its CMakeLists.txt
    else()
        message(WARNING "USE_LLVM is ON but LLVM was not found. Using stub backend.")
        # LLVM_FOUND will be false, baa_codegen will use stub.
    endif()
else()
    message(STATUS "LLVM backend disabled. Using stub backend.")
    # LLVM_FOUND will be false (as find_package wasn't called), baa_codegen will use stub.
endif()

# --- Standalone Preprocessor Tester ---
add_executable(baa_preprocessor_tester
    tools/baa_preprocessor_tester.c
)

# Link dependencies for the preprocessor tester
target_link_libraries(baa_preprocessor_tester PRIVATE
    baa_preprocessor
    baa_utils
    BaaCommonSettings
)

# Include directories for the preprocessor tester
target_include_directories(baa_preprocessor_tester
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include       # For baa/preprocessor/preprocessor.h
        ${CMAKE_SOURCE_DIR}/src/preprocessor # For preprocessor_internal.h
)


# --- Standalone Lexer Tester ---
add_executable(baa_lexer_tester
    tools/baa_lexer_tester.c
)

# Link dependencies for the lexer tester
target_link_libraries(baa_lexer_tester PRIVATE
    baa_lexer
    baa_utils
    BaaCommonSettings
)

# Include directories for the lexer tester
target_include_directories(baa_lexer_tester
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include  # For baa/lexer/lexer.h and baa/utils/*.h
        ${CMAKE_SOURCE_DIR}/src/lexer # For lexer_internal.h if needed by included lexer files
)

# --- Standalone AST Tester ---
add_executable(baa_ast_tester
    tools/baa_ast_tester.c
)

# Link dependencies for the AST tester
target_link_libraries(baa_ast_tester PRIVATE
    baa_ast          # The new AST library we defined
    baa_utils        # AST uses utils for memory, and tester might too
    BaaCommonSettings # For common compiler/project settings
)

# Include directories for the AST tester
target_include_directories(baa_ast_tester
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include  # To find "baa/ast/ast.h", "baa/utils/utils.h"
        # No need for ${CMAKE_SOURCE_DIR}/src/ast here as public headers are in include/
)
# تفعيل الاختبارات (معطل مؤقتاً)
# enable_testing()
# add_subdirectory(tests) # Keep this if tests are separate
