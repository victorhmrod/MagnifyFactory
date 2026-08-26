cmake_minimum_required(VERSION 3.21)
project(MagnifyFactory VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
qt_standard_project_setup()

set(MAGNIFY_CORE_SOURCES
    src/core/ConversionJob.h
    src/core/ConversionJob.cpp
    src/core/JobManager.h
    src/core/JobManager.cpp
    src/core/FormatRegistry.h
    src/core/FormatRegistry.cpp
)

set(MAGNIFY_HARDWARE_SOURCES
    src/hardware/HardwareAccelerationManager.h
    src/hardware/HardwareAccelerationManager.cpp
)

set(MAGNIFY_PRESETS_SOURCES
    src/presets/Preset.h
    src/presets/PresetRegistry.h
    src/presets/PresetRegistry.cpp
)

set(MAGNIFY_ENGINE_SOURCES
    src/engines/IMediaEngine.h
    src/engines/ffmpeg/FFmpegCommandBuilder.h
    src/engines/ffmpeg/FFmpegCommandBuilder.cpp
    src/engines/ffmpeg/FFprobe.h
    src/engines/ffmpeg/FFprobe.cpp
    src/engines/ffmpeg/FFmpegMediaEngine.h
    src/engines/ffmpeg/FFmpegMediaEngine.cpp
    src/engines/pdf/PdfImageWriter.h
    src/engines/pdf/PdfImageWriter.cpp
    src/engines/pdf/PdfEngine.h
    src/engines/pdf/PdfEngine.cpp
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

add_library(magnify_hardware STATIC ${MAGNIFY_HARDWARE_SOURCES})
target_include_directories(magnify_hardware PUBLIC src)
target_link_libraries(magnify_hardware PUBLIC Qt6::Widgets)

add_library(magnify_presets STATIC ${MAGNIFY_PRESETS_SOURCES})
target_include_directories(magnify_presets PUBLIC src)
target_link_libraries(magnify_presets PUBLIC Qt6::Widgets magnify_core)

add_library(magnify_engines STATIC ${MAGNIFY_ENGINE_SOURCES})
target_include_directories(magnify_engines PUBLIC src)
target_link_libraries(magnify_engines PUBLIC Qt6::Widgets magnify_core magnify_hardware)

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
target_link_libraries(MagnifyFactory PRIVATE Qt6::Widgets Qt6::Concurrent magnify_core magnify_hardware magnify_presets magnify_engines)

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

# JPEG is not natively supported by QtGui (unlike PNG) — it needs the
# imageformats plugin, used by the PDF Tools engine when embedding non-JPEG
# images into a PDF. The plugin itself depends on libjpeg-turbo's runtime DLL,
# which — unlike MagnifyFactory's own direct dependencies — isn't picked up
# automatically since plugins are loaded dynamically, not linked at build
# time, so it's located and copied explicitly here.
if(TARGET Qt6::QJpegPlugin)
    find_file(MAGNIFY_JPEG_RUNTIME_DLL NAMES jpeg62.dll PATH_SUFFIXES bin)
    add_custom_command(TARGET MagnifyFactory POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:MagnifyFactory>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QJpegPlugin>"
            "$<TARGET_FILE_DIR:MagnifyFactory>/imageformats/"
    )
    if(MAGNIFY_JPEG_RUNTIME_DLL)
        add_custom_command(TARGET MagnifyFactory POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MAGNIFY_JPEG_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:MagnifyFactory>/"
        )
    endif()
endif()

enable_testing()
add_subdirectory(tests)
