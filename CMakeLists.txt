cmake_minimum_required(VERSION 3.16)

project(LiJointMaster3 VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32 AND MINGW)
    get_filename_component(LIJOINT_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if(EXISTS "${LIJOINT_MINGW_BIN_DIR}")
        set(LIJOINT_BUILD_PATH "${LIJOINT_MINGW_BIN_DIR};$ENV{PATH}")
        string(REPLACE ";" "\\;" LIJOINT_BUILD_PATH_ESCAPED "${LIJOINT_BUILD_PATH}")
        set(LIJOINT_PATH_LAUNCHER
            "${CMAKE_COMMAND} -E env \"PATH=${LIJOINT_BUILD_PATH_ESCAPED}\"")
        # Qt Creator's MinGW kit may invoke custom/compile steps without the MinGW
        # bin directory in PATH, which prevents g++ from launching cc1plus.exe.
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE
            "${LIJOINT_PATH_LAUNCHER}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_CUSTOM
            "${LIJOINT_PATH_LAUNCHER}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK
            "${LIJOINT_PATH_LAUNCHER}")
        message(STATUS "Windows MinGW PATH injection enabled for build steps: ${LIJOINT_MINGW_BIN_DIR}")
    endif()
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS SerialPort)

set(LIJOINT_QT_LIBS
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
)

if(TARGET Qt${QT_VERSION_MAJOR}::SerialPort)
    list(APPEND LIJOINT_QT_LIBS Qt${QT_VERSION_MAJOR}::SerialPort)
    set(LIJOINT_HAS_SERIALPORT ON)
else()
    set(LIJOINT_HAS_SERIALPORT OFF)
    message(WARNING "Qt SerialPort was not found. Building LiJointMaster3 with a disabled serial backend.")
endif()

set(PROJECT_SOURCES
        src/app/main.cpp
        src/app/appicon.rc
        src/app/resources.qrc
        src/ui/widget.cpp
        src/ui/widget.h
        src/ui/widget.ui
        src/plot/plotmanager.cpp
        src/plot/plotmanager.h
        src/serial/serialmanager.cpp
        src/serial/serialmanager.h
        src/serial/serialcommand.h
        src/third_party/qcustomplot/qcustomplot.cpp
        src/third_party/qcustomplot/qcustomplot.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${PROJECT_SOURCES})

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(LiJointMaster3
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET LiJointMaster3 APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(LiJointMaster3 SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(LiJointMaster3
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(LiJointMaster3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(LiJointMaster3 PRIVATE ${LIJOINT_QT_LIBS})

target_compile_definitions(LiJointMaster3 PRIVATE
    LIJOINT_HAS_SERIALPORT=$<BOOL:${LIJOINT_HAS_SERIALPORT}>
)

if(MINGW)
    target_compile_options(LiJointMaster3 PRIVATE -Wa,-mbig-obj)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.LiJointMaster3)
endif()
set_target_properties(LiJointMaster3 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS LiJointMaster3
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(LiJointMaster3)
endif()
