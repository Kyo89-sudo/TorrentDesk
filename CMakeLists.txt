cmake_minimum_required(VERSION 3.20)
project(TorrentDesk VERSION 0.3.0 LANGUAGES CXX RC)
if(NOT WIN32)
  message(FATAL_ERROR "TorrentDesk uses the Windows Win32 API.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(UNICODE _UNICODE NOMINMAX)
if(MSVC)
  add_compile_options(/W4 /utf-8 /EHsc)
endif()
add_executable(TorrentDesk WIN32 src/main.cpp app.rc)
target_link_libraries(TorrentDesk PRIVATE user32 gdi32 comdlg32 shell32 comctl32 uxtheme ole32 bcrypt winhttp)
if(MSVC)
  target_link_options(TorrentDesk PRIVATE /MANIFEST:NO)
endif()
target_include_directories(TorrentDesk PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_custom_command(TARGET TorrentDesk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/profiles" "$<TARGET_FILE_DIR:TorrentDesk>/profiles")
add_custom_command(TARGET TorrentDesk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/languages" "$<TARGET_FILE_DIR:TorrentDesk>/languages"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/docs" "$<TARGET_FILE_DIR:TorrentDesk>/docs"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TorrentDesk>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/assets/flow-logo.png" "$<TARGET_FILE_DIR:TorrentDesk>/assets/flow-logo.png")
add_executable(TorrentDeskTests src/tests.cpp)
target_link_libraries(TorrentDeskTests PRIVATE bcrypt winhttp)
enable_testing()
add_test(NAME core COMMAND TorrentDeskTests "${CMAKE_CURRENT_SOURCE_DIR}")
