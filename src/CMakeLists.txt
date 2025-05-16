# Add subdirectories for each component
add_subdirectory(lexer)
add_subdirectory(codegen)
add_subdirectory(utils)
add_subdirectory(types)
add_subdirectory(operators)
add_subdirectory(analysis) # Corrected path

# The main executable 'baa' is defined in the root CMakeLists.txt
# Remove the redundant definition below:
# add_executable(Baa main.c)
# target_link_libraries(Baa PRIVATE ...)
