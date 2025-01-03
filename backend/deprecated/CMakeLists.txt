cmake_minimum_required(VERSION 3.15)

# Set the vcpkg toolchain file
# set(CMAKE_TOOLCHAIN_FILE "E:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(Backend)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Add TinyXML source files
set(TINYXML_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml/tinyxml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml/tinyxmlparser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml/tinystr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml/tinyxmlerror.cpp
)

set(SQLITE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite/sqlite3.c
)

# Add executable with all source files
add_executable(backend 
    main.cpp 
    path_finding.cpp 
    utils.cpp
    ${TINYXML_SOURCES}
    ${SQLITE_SOURCES}
)
