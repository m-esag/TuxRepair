cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(tuxrepair-ui
    main.cpp
    main_window.cpp
    main_window.h
    traveler_renderer.cpp
    traveler_renderer.h
    qol_dialogs.cpp
    qol_dialogs.h
    register_dialog.cpp
    register_dialog.h
    template_editor_dialog.cpp
    template_editor_dialog.h
    resources.qrc
)

target_link_libraries(tuxrepair-ui
    PRIVATE
        tuxrepair-core
        Qt6::Widgets
        Qt6::PrintSupport
)
