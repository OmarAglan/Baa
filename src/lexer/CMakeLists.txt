add_library(baa_lexer
    lexer.c
    number_parser.c
    lexer_char_utils.c
    token_scanners.c
)

target_include_directories(baa_lexer
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(baa_lexer INTERFACE BaaCommonSettings)
# No direct baa_* library dependencies observed for baa_lexer itself.
