cmake_minimum_required(VERSION 3.24)

project(ThedoriusUi VERSION 0.3.0 LANGUAGES CXX)

option(THEDORIUS_REQUIRE_STATIC_QT "Reject shared Qt builds used for release packaging" OFF)
set(THEDORIUS_CLIENT_DLL "${CMAKE_CURRENT_LIST_DIR}/../build/Thedorius-Lite.dll" CACHE FILEPATH
    "Signed client DLL embedded in the launcher")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick Network)
qt_standard_project_setup(REQUIRES 6.11)

if(THEDORIUS_REQUIRE_STATIC_QT)
    get_target_property(_qt_core_type Qt6::Core TYPE)
    if(NOT _qt_core_type STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "Release packaging requires the commercial static Qt 6.11 toolchain.")
    endif()
endif()

qt_add_executable(Thedorius WIN32
    src/main.cpp
    src/AppController.cpp
    src/AppController.hpp
    src/bridge/BridgeClient.cpp
    src/bridge/BridgeClient.hpp
    src/bridge/Protocol.cpp
    src/bridge/Protocol.hpp
    src/launcher/EmbeddedClient.cpp
    src/launcher/EmbeddedClient.hpp
    src/launcher/GameLauncher.cpp
    src/launcher/GameLauncher.hpp
    src/launcher/Injector.cpp
    src/launcher/Injector.hpp
    src/launcher/ProcessDiscovery.cpp
    src/launcher/ProcessDiscovery.hpp
    src/launcher/SessionBootstrap.cpp
    src/launcher/SessionBootstrap.hpp
)

qt_add_qml_module(Thedorius
    URI Thedorius
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/AppLogo.qml
        qml/components/AppSlider.qml
        qml/components/AppToggle.qml
        qml/components/BackgroundLayer.qml
        qml/components/ColorEditor.qml
        qml/components/ConfigCard.qml
        qml/components/GlassSurface.qml
        qml/components/HudPreviewPanel.qml
        qml/components/ModuleCard.qml
        qml/components/ModuleGrid.qml
        qml/components/ModuleSettingsPanel.qml
        qml/components/RangeSlider.qml
        qml/components/SearchPopover.qml
        qml/components/Sidebar.qml
        qml/components/SidebarItem.qml
        qml/components/TopBar.qml
        qml/components/Toast.qml
        qml/pages/ConfigsPage.qml
        qml/pages/FriendsPage.qml
        qml/pages/HudPage.qml
        qml/pages/LaunchPage.qml
        qml/pages/ModulesPage.qml
        qml/overlays/ArrayListOverlay.qml
        qml/overlays/WatermarkOverlay.qml
    RESOURCES
        qml/assets/logo/20-thedorius-logo.png
        qml/assets/backgrounds/21-main-background.png
        qml/assets/fonts/GoogleSans-SemiBold.ttf
        qml/assets/fonts/GoogleSans-Bold.ttf
        qml/assets/fonts/FontAwesome-Free-Solid-900.otf
        qml/assets/fonts/LICENSE-Font-Awesome.txt
        qml/assets/fonts/OFL-Google-Sans.txt
)

if(EXISTS "${THEDORIUS_CLIENT_DLL}")
    qt_add_resources(Thedorius embedded_client
        PREFIX "/embedded"
        BASE "${CMAKE_CURRENT_LIST_DIR}/../build"
        FILES "${THEDORIUS_CLIENT_DLL}"
    )
    target_compile_definitions(Thedorius PRIVATE THEDORIUS_HAS_EMBEDDED_CLIENT=1)
else()
    message(WARNING "Client DLL was not found; launcher will run in mock/UI-only mode.")
endif()

target_link_libraries(Thedorius PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
)

target_compile_definitions(Thedorius PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

set_target_properties(Thedorius PROPERTIES
    OUTPUT_NAME "Thedorius"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../dist"
)

include(CTest)
if(BUILD_TESTING)
    find_program(QMLLINT_EXECUTABLE qmllint HINTS "${QT_HOST_PATH}/bin")
    if(QMLLINT_EXECUTABLE)
        add_test(NAME thedorius-qmllint
            COMMAND "${QMLLINT_EXECUTABLE}" -I "$<TARGET_FILE_DIR:Thedorius>" "${CMAKE_CURRENT_LIST_DIR}/qml/Main.qml")
    endif()
endif()
