add_library(baa_types
    types.c
)

target_include_directories(baa_types
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# baa_types depends on baa_utils for baa_malloc, baa_strdup, etc.
target_link_libraries(baa_types PRIVATE baa_utils INTERFACE BaaCommonSettings)
