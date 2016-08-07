project(Sloppy)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmakeModules/")

find_package(Boost REQUIRED)
include_directories("." ${Boost_INCLUDE_DIRS})

# detect if we are on windows, because for windows
# we have to use special code for timegm()
IF (WIN32)
  add_definitions(-DIS_WINDOWS_BUILD)
ENDIF()

set(LIB_SOURCES
)

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBS})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

set(LIB_VERSION_MAJOR 0)
set(LIB_VERSION_MINOR 1)
set(LIB_VERSION_PATCH 0)
set(LIB_VERSION_STRING ${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH})

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${LIB_VERSION_STRING}
                                          SOVERSION ${LIB_VERSION_MAJOR})

#
# Unit tests
#
set(LIB_SOURCES_TST
)

find_package(GTest)

if (GTEST_FOUND)
    include_directories("." ${GTEST_INCLUDE_DIRS})
    link_directories(".")

    #add_executable(${PROJECT_NAME}_Tests EXCLUDE_FROM_ALL ${LIB_SOURCES} ${LIB_SOURCES_TST})
    add_executable(${PROJECT_NAME}_Tests ${LIB_SOURCES} ${LIB_SOURCES_TST})
    set_property(TARGET ${PROJECT_NAME}_Tests PROPERTY CXX_STANDARD 14)
    set_property(TARGET ${PROJECT_NAME}_Tests PROPERTY CXX_STANDARD_REQUIRED ON)

    target_link_libraries(${PROJECT_NAME}_Tests ${PROJECT_NAME} ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
endif (GTEST_FOUND)
