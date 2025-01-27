cmake_minimum_required(VERSION 3.24)

project(uix_test VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")
add_compile_definitions(uix_test PRIVATE _UNICODE UNICODE)

include(FetchContent)
FetchContent_Declare(htcw_uix
    GIT_REPOSITORY https://github.com/codewitch-honey-crisis/uix.git
    GIT_TAG 1.3.1
    GIT_SHALLOW ON
    FIND_PACKAGE_ARGS 1.3.1
)
FetchContent_MakeAvailable(htcw_uix)

add_executable(uix_test WIN32
    src/winmain.cpp
    src/main.cpp
)
target_link_options(uix_test PRIVATE "/SUBSYSTEM:WINDOWS")
target_link_options(uix_test PRIVATE "/ENTRY:wWinMainCRTStartup")

target_link_libraries(uix_test htcw_bits htcw_io htcw_ml htcw_data htcw_gfx htcw_uix)
target_include_directories(uix_test PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
  )
