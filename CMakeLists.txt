# ─────────────────────────────────────────────────────────────────────────────
#  mauvely-installer — the Mauvely OS installer.
#
#  A minimal fork of Programs/app-base by copy: brand.h (pinned), tokens.h and
#  a theme without the organisation override. No window chrome, no MCP, no
#  updater, no packaging: it runs only inside the installer image, which the
#  Containerfile at the root of this repository builds.
#
#  Sources are listed by hand, as in app-base.
# ─────────────────────────────────────────────────────────────────────────────
cmake_minimum_required(VERSION 3.21)

project(MauvelyInstaller
    VERSION 0.1.0
    DESCRIPTION "Writes Mauvely OS to a disk from the install medium"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# ── The logic and the pages, shared by the binary and the tests ──────────────
add_library(installer_core STATIC
    src/core/bootc.cpp          src/core/bootc.h
    src/core/brand.h
    src/core/cmdline.cpp        src/core/cmdline.h
    src/core/config.h
    src/core/disks.cpp          src/core/disks.h
    src/core/enrolment.cpp      src/core/enrolment.h
    src/core/host.cpp           src/core/host.h
    src/core/installer.cpp      src/core/installer.h
    src/core/luks.cpp           src/core/luks.h
    src/core/runner.cpp         src/core/runner.h
    src/core/target.cpp         src/core/target.h
    src/core/theme.cpp          src/core/theme.h
    src/core/tokens.h
    src/ui/pages.cpp            src/ui/pages.h
    src/ui/style.cpp            src/ui/style.h
    src/ui/window.cpp           src/ui/window.h
    resources/resources.qrc)
target_include_directories(installer_core PUBLIC src)
target_link_libraries(installer_core PUBLIC Qt6::Widgets)
target_compile_definitions(installer_core PUBLIC APP_VERSION="${PROJECT_VERSION}")

add_executable(mauvely-installer src/main.cpp)
target_link_libraries(mauvely-installer PRIVATE installer_core)

install(TARGETS mauvely-installer RUNTIME DESTINATION bin)

# ── Tests ────────────────────────────────────────────────────────────────────
option(INSTALLER_TESTS "Build the tests" ON)
if(INSTALLER_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    foreach(t logic installer flow)
        add_executable(tst_${t} tests/tst_${t}.cpp tests/fakerunner.h)
        target_link_libraries(tst_${t} PRIVATE installer_core Qt6::Test)
        target_compile_definitions(tst_${t} PRIVATE FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        add_test(NAME ${t} COMMAND tst_${t})
        set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
endif()
