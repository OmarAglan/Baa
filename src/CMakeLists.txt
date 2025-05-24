# Add subdirectories for each component
add_subdirectory(utils)
add_subdirectory(types)
add_subdirectory(operators)
add_subdirectory(analysis)
add_subdirectory(ast)

# The main executable 'baa' is defined in the root CMakeLists.txt
# Remove the redundant definition below:
# add_executable(Baa main.c)
# target_link_libraries(Baa PRIVATE ...)
