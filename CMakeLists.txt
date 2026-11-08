cmake_minimum_required(VERSION 3.20)

project(simplex_method VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIMPLEX_BUILD_TESTS "Build project tests" ON)

add_library(simplex_core src/simplex.cpp)
add_library(simplex::core ALIAS simplex_core)
target_include_directories(simplex_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(simplex_core PUBLIC cxx_std_20)

add_executable(simplex_cli app/main.cpp)
target_link_libraries(simplex_cli PRIVATE simplex::core)

if(MSVC)
    target_compile_options(simplex_core PRIVATE /W4 /permissive-)
    target_compile_options(simplex_cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(simplex_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(simplex_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SIMPLEX_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

find_program(LATEXMK_EXECUTABLE latexmk)
find_program(LUALATEX_EXECUTABLE lualatex)
find_program(BIBER_EXECUTABLE biber)
if(LATEXMK_EXECUTABLE AND LUALATEX_EXECUTABLE AND BIBER_EXECUTABLE)
    add_custom_target(report
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CMAKE_CURRENT_BINARY_DIR}/texmf-cache
        COMMAND ${CMAKE_COMMAND} -E env
                TEXMFVAR=${CMAKE_CURRENT_BINARY_DIR}/texmf-cache
                TEXMFCACHE=${CMAKE_CURRENT_BINARY_DIR}/texmf-cache
                ${LATEXMK_EXECUTABLE} -lualatex -interaction=nonstopmode
                -halt-on-error main.tex
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/report
        COMMENT "Building the LaTeX report with LuaLaTeX"
        VERBATIM
    )
else()
    add_custom_target(report
        COMMAND ${CMAKE_COMMAND} -E echo
                "Report requires LuaLaTeX, latexmk and Biber."
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()
