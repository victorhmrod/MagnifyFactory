find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_ffmpeg_command_builder test_ffmpeg_command_builder.cpp)
target_link_libraries(test_ffmpeg_command_builder PRIVATE Qt6::Test magnify_engines)
add_test(NAME test_ffmpeg_command_builder COMMAND test_ffmpeg_command_builder)

add_executable(test_format_registry test_format_registry.cpp)
target_link_libraries(test_format_registry PRIVATE Qt6::Test magnify_core)
add_test(NAME test_format_registry COMMAND test_format_registry)

add_executable(test_job_manager test_job_manager.cpp)
target_link_libraries(test_job_manager PRIVATE Qt6::Test magnify_core magnify_engines)
add_test(NAME test_job_manager COMMAND test_job_manager)

# Manual end-to-end smoke test (real ffmpeg process, not part of ctest).
add_executable(smoke_convert smoke_convert.cpp)
target_link_libraries(smoke_convert PRIVATE Qt6::Core magnify_core magnify_engines)

add_executable(smoke_mkpath smoke_mkpath.cpp)
target_link_libraries(smoke_mkpath PRIVATE Qt6::Core)

add_executable(smoke_pdf smoke_pdf.cpp)
target_link_libraries(smoke_pdf PRIVATE Qt6::Core Qt6::Gui magnify_core magnify_engines)

add_executable(smoke_hardware smoke_hardware.cpp)
target_link_libraries(smoke_hardware PRIVATE Qt6::Core magnify_core magnify_hardware magnify_engines)

add_executable(smoke_presets smoke_presets.cpp)
target_link_libraries(smoke_presets PRIVATE Qt6::Core magnify_core magnify_hardware magnify_presets magnify_engines)

add_executable(smoke_folder_scan smoke_folder_scan.cpp)
target_link_libraries(smoke_folder_scan PRIVATE Qt6::Core)

add_executable(smoke_watch smoke_watch.cpp)
target_link_libraries(smoke_watch PRIVATE Qt6::Core magnify_core magnify_hardware magnify_engines magnify_watch)

add_executable(smoke_archive smoke_archive.cpp)
target_link_libraries(smoke_archive PRIVATE Qt6::Core magnify_core magnify_hardware magnify_engines)

add_executable(smoke_plugins smoke_plugins.cpp)
target_link_libraries(smoke_plugins PRIVATE Qt6::Core magnify_core magnify_hardware magnify_presets magnify_plugins magnify_engines)

add_executable(smoke_document smoke_document.cpp)
target_link_libraries(smoke_document PRIVATE Qt6::Core magnify_core magnify_hardware magnify_engines)

add_executable(smoke_trim smoke_trim.cpp)
target_link_libraries(smoke_trim PRIVATE Qt6::Core magnify_core magnify_hardware magnify_engines)

add_executable(smoke_media_edit smoke_media_edit.cpp)
target_link_libraries(smoke_media_edit PRIVATE Qt6::Core magnify_core magnify_hardware magnify_engines)
