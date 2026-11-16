# A separable library: plain C++, no Qt, its own tests. The API is include/Shapes/, the rest private.

set(CMAKE_AUTOMOC OFF)

add_library(shapes STATIC)
add_library(Shapes::Shapes ALIAS shapes)

target_sources(shapes
    PRIVATE
        src/Recognizer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/Shapes/Recognizer.h
)
target_compile_features(shapes PUBLIC cxx_std_23)
app_target_defaults(shapes)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
