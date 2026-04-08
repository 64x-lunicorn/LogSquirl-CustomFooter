# ============================================================================
# logsquirl-costume-footer — Custom Footer plugin for LogSquirl
#
# This is a standalone CMake project.  It does NOT depend on the LogSquirl
# build tree — only on the vendored logsquirl_plugin_api.h header (MIT).
#
# Build:
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(logsquirl_costume_footer
    VERSION   0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find Qt6 ──────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()

# ── Plugin shared library ─────────────────────────────────────────────────
add_library(logsquirl_costume_footer SHARED
    src/plugin.cpp
    src/footerconfig.cpp
    src/footerscanner.cpp
    src/footereditor.cpp
    src/footerdisplaywidget.cpp
)

set_target_properties(logsquirl_costume_footer PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
)

if(NOT WIN32)
    target_compile_options(logsquirl_costume_footer PRIVATE -fvisibility=hidden)
endif()

target_include_directories(logsquirl_costume_footer
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(logsquirl_costume_footer
    PRIVATE
        Qt6::Core
        Qt6::Widgets
)

# ── Install rule ──────────────────────────────────────────────────────────
include(GNUInstallDirs)

set(PLUGIN_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/LogSquirl/plugins/io.github.logsquirl.costume-footer"
    CACHE PATH "Plugin installation directory"
)

install(TARGETS logsquirl_costume_footer LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}")
install(FILES   plugin.json                      DESTINATION "${PLUGIN_INSTALL_DIR}")

# ── Tests (opt-in) ────────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
