# ─────────────────────────────────────────────────────────────────────────────
#  mauvely-signin — Mauvely OS's sign-in window. Built only with
#  -DMAUVELY_SIGNIN=ON, so the installer and its live image never need
#  Qt WebEngine. os-image builds it from here at the commit in installer.ref.
#
#  It shares the installer's brand.h, tokens.h, theme and fonts, and carries
#  app-base's frameless window (src/ui/framelesswindow.*), which the installer
#  itself does not use.
# ─────────────────────────────────────────────────────────────────────────────

find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus Gui Svg Widgets WebEngineCore WebEngineWidgets)

# The parts with no widgets and no web engine: what the tests reach.
add_library(signin_core STATIC
    src/navpolicy.cpp   src/navpolicy.h
    src/protocol.cpp    src/protocol.h)
target_include_directories(signin_core PUBLIC src)
target_link_libraries(signin_core PUBLIC Qt6::Core)

add_executable(mauvely-signin
    src/main.cpp
    src/signinpage.cpp      src/signinpage.h
    src/signinwindow.cpp    src/signinwindow.h
    ${PROJECT_SOURCE_DIR}/src/core/brand.h
    ${PROJECT_SOURCE_DIR}/src/core/theme.cpp    ${PROJECT_SOURCE_DIR}/src/core/theme.h
    ${PROJECT_SOURCE_DIR}/src/core/tokens.h
    ${PROJECT_SOURCE_DIR}/src/ui/framelesswindow.cpp ${PROJECT_SOURCE_DIR}/src/ui/framelesswindow.h
    ${PROJECT_SOURCE_DIR}/src/ui/style.cpp      ${PROJECT_SOURCE_DIR}/src/ui/style.h
    ${PROJECT_SOURCE_DIR}/resources/resources.qrc
    resources/signin.qrc)
target_include_directories(mauvely-signin PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(mauvely-signin PRIVATE
    signin_core Qt6::DBus Qt6::Svg Qt6::Widgets Qt6::WebEngineCore Qt6::WebEngineWidgets)
target_compile_definitions(mauvely-signin PRIVATE APP_VERSION="${PROJECT_VERSION}")

install(TARGETS mauvely-signin RUNTIME DESTINATION bin COMPONENT signin)
install(FILES dist/com.mauvely.signin.desktop
        DESTINATION share/applications COMPONENT signin)
install(FILES resources/mark.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME com.mauvely.signin.svg COMPONENT signin)

if(INSTALLER_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Network Test)
    enable_testing()
    add_executable(tst_signin tests/tst_signin.cpp)
    target_link_libraries(tst_signin PRIVATE signin_core Qt6::Network Qt6::Test)
    # The process tests run the real binary.
    add_dependencies(tst_signin mauvely-signin)
    target_compile_definitions(tst_signin PRIVATE
        SIGNIN_BINARY="$<TARGET_FILE:mauvely-signin>")
    add_test(NAME signin COMMAND tst_signin)
    set_tests_properties(signin PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
