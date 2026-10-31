cmake_minimum_required(VERSION 3.16)

project(FirstProject VERSION 1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(compression_core
    Archivator.cpp
    Builder.cpp
    Deflate.cpp
    LZ77.cpp
)
target_include_directories(compression_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(archivator main.cpp)
target_link_libraries(archivator PRIVATE compression_core)

if(MINGW)
    target_link_options(archivator PRIVATE -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    target_compile_options(compression_core PRIVATE /W4 /permissive-)
    target_compile_options(archivator PRIVATE /W4 /permissive-)
else()
    target_compile_options(compression_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(archivator PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(compression_tests tests.cpp)
    target_link_libraries(compression_tests PRIVATE compression_core)

    if(MINGW)
        target_link_options(compression_tests PRIVATE -static-libgcc -static-libstdc++)
    endif()

    if(MSVC)
        target_compile_options(compression_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(compression_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME compression_tests COMMAND compression_tests)
endif()
