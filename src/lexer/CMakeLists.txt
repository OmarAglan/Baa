add_library(baa_lexer
    lexer.c
    number_parser.c
)

target_include_directories(baa_lexer
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(baa_lexer
    PRIVATE
    m  # Link with math library
)
