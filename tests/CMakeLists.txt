# ============================================================================
# Unit tests for logsquirl-custom-footer
#
# Uses Catch2 v2 (BDD style) to match the host project's test conventions.
# ============================================================================

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.13.10
)
FetchContent_MakeAvailable(Catch2)

# ── Test executable ───────────────────────────────────────────────────────

add_executable(logsquirl_custom_footer_tests
    tests_main.cpp
    plugininfo_test.cpp
    footerscanner_test.cpp
    footerscanner_edge_test.cpp
    footerconfig_test.cpp
    footerconfig_maxlines_test.cpp
    footerdisplaywidget_test.cpp
)

set_target_properties(logsquirl_custom_footer_tests PROPERTIES
    AUTOMOC ON
)

target_include_directories(logsquirl_custom_footer_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include"   # logsquirl_plugin_api.h
        "${CMAKE_SOURCE_DIR}/src"       # internal headers
)

# Compile plugin sources into the test binary so we can test internal
# functions without symbol-export gymnastics.
target_sources(logsquirl_custom_footer_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/plugin.cpp"
        "${CMAKE_SOURCE_DIR}/src/footerconfig.cpp"
        "${CMAKE_SOURCE_DIR}/src/footerscanner.cpp"
        "${CMAKE_SOURCE_DIR}/src/footereditor.cpp"
        "${CMAKE_SOURCE_DIR}/src/footerdisplaywidget.cpp"
)

target_link_libraries(logsquirl_custom_footer_tests
    PRIVATE
        Catch2::Catch2
        Qt6::Core
        Qt6::Widgets
)

# ── Register with CTest ──────────────────────────────────────────────────
add_test(NAME logsquirl_custom_footer_tests COMMAND logsquirl_custom_footer_tests)
