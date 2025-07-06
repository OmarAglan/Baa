# Enable testing
enable_testing()

# Add test framework
add_subdirectory(framework)

# Common test settings
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

# Enable UTF-8 support for GCC/Clang (but not clang-cl which doesn't support these flags)
if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR (CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_C_COMPILER_FRONTEND_VARIANT MATCHES "MSVC"))
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -finput-charset=UTF-8 -fexec-charset=UTF-8")
endif()

# Enable memory leak detection
if(CMAKE_C_COMPILER_ID MATCHES "GNU" AND NOT WIN32)
    # Enable ASan only on non-Windows platforms with GCC
    # MinGW doesn't fully support ASan on Windows
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif()

# Add unit tests
add_subdirectory(unit)

# Add codegen tests (TODO: Enable after AST and parser are fully implemented)
# add_subdirectory(codegen_tests)

# Add integration tests
add_subdirectory(integration)
