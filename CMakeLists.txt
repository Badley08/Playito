# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)

set(PLAYITO_VERSION "0.1.0" CACHE STRING "Playito release version")
project(Playito VERSION ${PLAYITO_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)

add_executable(playito
    src/main.cpp
    src/app/application.cpp
    src/app/file_browser.cpp
    src/media/media_library.cpp
    src/media/media_player.cpp
    src/ui/ui_renderer.cpp
    src/update/update_manager.cpp
)
target_include_directories(playito PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(playito PRIVATE
    PkgConfig::MPV
    PkgConfig::GLFW
    PkgConfig::CURL
    OpenGL::GL
    OpenSSL::Crypto
    Threads::Threads
    nlohmann_json::nlohmann_json
)
target_compile_options(playito PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(playito PRIVATE PLAYITO_VERSION="${PROJECT_VERSION}")

install(TARGETS playito RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_executable(media_library_tests
        tests/media_library_test.cpp
        src/media/media_library.cpp
    )
    target_include_directories(media_library_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_options(media_library_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME media_library_tests COMMAND media_library_tests)
endif()
