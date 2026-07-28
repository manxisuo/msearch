cmake_minimum_required(VERSION 3.10)
project(MSearch VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets)

set(MSEARCH_HAVE_X11 OFF)
if(UNIX AND NOT APPLE)
  find_package(Qt5 COMPONENTS X11Extras QUIET)
  find_package(X11 QUIET)
  find_library(XCB_LIBRARY xcb)
  if(Qt5X11Extras_FOUND AND X11_FOUND AND XCB_LIBRARY)
    set(MSEARCH_HAVE_X11 ON)
  endif()
endif()

set(MSEARCH_SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/SettingsDialog.cpp
    src/app/GlobalHotkey.cpp
    src/app/Autostart.cpp
    src/app/HelpDialog.cpp
    src/index/FileEntry.cpp
    src/index/IndexDatabase.cpp
    src/index/Indexer.cpp
    src/index/FsWatcher.cpp
    src/search/SearchEngine.cpp
    src/search/QueryParser.cpp
    src/search/Pinyin.cpp
    src/model/ResultModel.cpp
)

set(MSEARCH_HEADERS
    src/app/MainWindow.h
    src/app/SettingsDialog.h
    src/app/IndexOptions.h
    src/app/GlobalHotkey.h
    src/app/Autostart.h
    src/app/HelpDialog.h
    src/index/FileEntry.h
    src/index/IndexDatabase.h
    src/index/Indexer.h
    src/index/FsWatcher.h
    src/search/SearchEngine.h
    src/search/QueryParser.h
    src/search/Pinyin.h
    src/model/ResultModel.h
)

add_executable(msearch ${MSEARCH_SOURCES} ${MSEARCH_HEADERS})

target_include_directories(msearch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(msearch
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

if(MSEARCH_HAVE_X11)
  target_compile_definitions(msearch PRIVATE MSEARCH_HAVE_X11)
  target_link_libraries(msearch Qt5::X11Extras ${X11_LIBRARIES} ${XCB_LIBRARY})
  message(STATUS "MSearch: X11 global hotkey enabled")
else()
  message(STATUS "MSearch: X11 global hotkey disabled (Windows RegisterHotKey still available on Win32)")
endif()

if(WIN32)
  target_link_libraries(msearch user32 advapi32)
endif()

install(TARGETS msearch RUNTIME DESTINATION bin)
