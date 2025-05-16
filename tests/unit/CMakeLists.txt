# Core tests
add_subdirectory(core)

# Lexer tests
add_subdirectory(lexer)

# Preprocessor tests
add_subdirectory(preprocessor)

# Utils tests
add_subdirectory(utils)

# إعداد مشترك لجميع الاختبارات
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

# تمكين اكتشاف التسريبات في الذاكرة
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MTd")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif()
