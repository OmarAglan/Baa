cmake_minimum_required(VERSION 3.10)
project(baa VERSION 0.2.4 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# List of source files
set(SOURCES
    src/main.c
    src/lexer.c
    src/parser.c
    src/codegen.c
    src/analysis.c
    src/error.c
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