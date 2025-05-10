# Enable testing
enable_testing()

# Add test framework
add_library(test_framework
    framework/test_framework.c
)
target_include_directories(test_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/framework
)

# Common test settings
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

# Enable memory leak detection and UTF-8 support for MinGW
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -finput-charset=UTF-8 -fexec-charset=UTF-8")
if(CMAKE_C_COMPILER_ID MATCHES "GNU" AND NOT WIN32)
    # Enable ASan only on non-Windows platforms with GCC
    # MinGW doesn't fully support ASan on Windows
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif()

# Add unit tests
add_subdirectory(unit)

# Add codegen tests
add_subdirectory(codegen_tests)

# Add integration tests
add_subdirectory(integration)
