cmake_minimum_required(VERSION 3.21)
project(MagnifyFactory VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

set(MAGNIFY_CORE_SOURCES
    src/core/ConversionJob.h
    src/core/ConversionJob.cpp
    src/core/JobManager.h
    src/core/JobManager.cpp
    src/core/FormatRegistry.h
    src/core/FormatRegistry.cpp
)

set(MAGNIFY_ENGINE_SOURCES
    src/engines/IMediaEngine.h
    src/engines/ffmpeg/FFmpegCommandBuilder.h
    src/engines/ffmpeg/FFmpegCommandBuilder.cpp
    src/engines/ffmpeg/FFprobe.h
    src/engines/ffmpeg/FFprobe.cpp
    src/engines/ffmpeg/FFmpegMediaEngine.h
    src/engines/ffmpeg/FFmpegMediaEngine.cpp
)

set(MAGNIFY_UI_SOURCES
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/ConvertDialog.h
    src/ui/ConvertDialog.cpp
)

add_library(magnify_core STATIC ${MAGNIFY_CORE_SOURCES})
target_include_directories(magnify_core PUBLIC src)
target_link_libraries(magnify_core PUBLIC Qt6::Widgets)

add_library(magnify_engines STATIC ${MAGNIFY_ENGINE_SOURCES})
target_include_directories(magnify_engines PUBLIC src)
target_link_libraries(magnify_engines PUBLIC Qt6::Widgets magnify_core)

set(MAGNIFY_RESOURCES resources/icon.qrc)
if(WIN32)
    list(APPEND MAGNIFY_RESOURCES resources/app.rc)
endif()

add_executable(MagnifyFactory WIN32
    src/main.cpp
    ${MAGNIFY_UI_SOURCES}
    ${MAGNIFY_RESOURCES}
)
target_include_directories(MagnifyFactory PRIVATE src)
target_link_libraries(MagnifyFactory PRIVATE Qt6::Widgets magnify_core magnify_engines)

# Qt cannot start without its "platforms" plugin (qwindows[d].dll). windeployqt
# isn't available from this vcpkg install, so copy the plugin directly next
# to the executable as part of the build.
if(TARGET Qt6::QWindowsIntegrationPlugin)
    add_custom_command(TARGET MagnifyFactory POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:MagnifyFactory>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:MagnifyFactory>/platforms/"
    )
endif()

enable_testing()
add_subdirectory(tests)
