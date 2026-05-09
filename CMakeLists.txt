cmake_minimum_required(VERSION 3.20)

project(AutoThemeSwitcher LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "This project is configured for the GCC/MinGW toolchain. Use a GNU C++ compiler.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(AutoThemeSwitcher WIN32
    source.cpp
)

target_include_directories(AutoThemeSwitcher PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(AutoThemeSwitcher PRIVATE
    UNICODE
    _UNICODE
)

add_executable(installer
    installer/installer.cpp
)

target_include_directories(installer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(installer PRIVATE
    UNICODE
    _UNICODE
)

target_compile_options(AutoThemeSwitcher PRIVATE -Wall -Wextra)
target_compile_options(installer PRIVATE -Wall -Wextra)

if(WIN32)
    target_link_options(AutoThemeSwitcher PRIVATE -municode)
    target_link_options(installer PRIVATE -municode)
endif()

install(TARGETS AutoThemeSwitcher installer
    RUNTIME DESTINATION .
)
