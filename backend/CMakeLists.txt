cmake_minimum_required(VERSION 3.10)
project(Backend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# set(Boost_NO_SYSTEM_PATHS TRUE)

# Set Boost root and library directories
set(BOOST_ROOT "E:/Boost")
set(BOOST_LIBRARYDIR "E:/Boost/lib")

# Set Boost to use static libraries
# set(Boost_USE_STATIC_LIBS ON)

# Find Boost package with serialization component
find_package(Boost REQUIRED COMPONENTS serialization)

# Include directories
include_directories(${CMAKE_SOURCE_DIR} ${Boost_INCLUDE_DIRS})

# Add pugixml source files
set(PUGIXML_SOURCES
    pugixml/pugixml.cpp
)

# Add executable with all source files
add_executable(backend main.cpp path_finding.cpp k-dtree.cpp ${PUGIXML_SOURCES})

# Link Boost libraries
target_link_libraries(backend PRIVATE ${Boost_LIBRARIES})

# Explicitly specify the Boost library paths
link_directories(${BOOST_LIBRARYDIR})

# Debug messages 
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
message(STATUS "BOOST_LIBRARYDIR: ${BOOST_LIBRARYDIR}")
