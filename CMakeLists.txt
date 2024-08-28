cmake_minimum_required(VERSION 3.20.0)

project(VTerm)
set(CMAKE_CXX_STANDARD 23)


add_library(libvterm
    src/encoding.c src/keyboard.c
    src/mouse.c src/parser.c src/pen.c src/screen.c
    src/state.c src/unicode.c src/vterm.c
)

target_include_directories(libvterm PUBLIC include/ )


add_executable(unterm bin/unterm.cpp
 ui/TermEmu.cpp ui/TermEmu.h)
target_link_libraries(unterm libvterm)
target_include_directories(unterm PUBLIC ui/ . )


#add_executable(vt-ctrl bin/vterm-ctrl.c)
#target_link_libraries(vt-ctrl libvterm)


#add_executable(vt-dump bin/vterm-dump.c)
#target_link_libraries(vt-dump libvterm)


find_package(Qt5 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets
    OpenGL Qml Quick QuickWidgets Svg WebEngineWidgets
    WebEngineCore PrintSupport Network Positioning WebChannel Test
    REQUIRED)




#target_include_directories(${AppName} PUBLIC ${Qt_Include_Dirs} ${CMAKE_CURRENT_SOURCE_DIR}
#    ${PYTHON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/external/pybind11/include/ ${FFMS2_INC})

target_link_libraries(unterm
    Qt5::Core Qt5::Widgets # Qt5::Multimedia Qt5::OpenGL Qt5::WebEngineWidgets
    )


