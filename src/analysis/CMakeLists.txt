# CMakeLists.txt for the analysis module

add_library(baa_flow_analysis
    flow_analysis.c
    flow_errors.c
)

target_include_directories(baa_flow_analysis
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include # Access to baa/analysis/ headers
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include # Access to baa/ast/, baa/types/, etc.
)

target_link_libraries(baa_flow_analysis
    PRIVATE
        baa_utils
        baa_types
        baa_ast
)

# Optional: Install header files if this is part of a public API
# install(FILES
#     ${CMAKE_CURRENT_SOURCE_DIR}/../include/baa/analysis/flow_analysis.h
#     ${CMAKE_CURRENT_SOURCE_DIR}/../include/baa/analysis/flow_errors.h
#     DESTINATION include/baa/analysis
# )
