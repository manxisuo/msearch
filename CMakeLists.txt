cmake_minimum_required(VERSION 3.10)
project(MSearch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets)

set(MSEARCH_SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/SettingsDialog.cpp
    src/index/FileEntry.cpp
    src/index/IndexDatabase.cpp
    src/index/Indexer.cpp
    src/search/SearchEngine.cpp
    src/model/ResultModel.cpp
)

set(MSEARCH_HEADERS
    src/app/MainWindow.h
    src/app/SettingsDialog.h
    src/app/IndexOptions.h
    src/index/FileEntry.h
    src/index/IndexDatabase.h
    src/index/Indexer.h
    src/search/SearchEngine.h
    src/model/ResultModel.h
)

add_executable(msearch ${MSEARCH_SOURCES} ${MSEARCH_HEADERS})

target_include_directories(msearch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(msearch
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

install(TARGETS msearch RUNTIME DESTINATION bin)
