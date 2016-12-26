project(Sloppy)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmakeModules/")

find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

find_package(Boost REQUIRED COMPONENTS log log_setup thread date_time filesystem system)
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})
add_definitions(-DBOOST_LOG_DYN_LINK=1)  # linking fails if this is not set

# detect if we are on windows, because for windows
# we have to use special code for timegm()
IF (WIN32)
  add_definitions(-DIS_WINDOWS_BUILD)
ENDIF()

set(LIB_SOURCES
    Sloppy/libSloppy.cpp
    Sloppy/DateTime/DateAndTime.cpp
    Sloppy/Logger/Logger.cpp
    Sloppy/MailAndMIME/MailAndMIME.cpp
    Sloppy/MailAndMIME/Message.cpp
    Sloppy/MailAndMIME/Header.cpp
    Sloppy/MailAndMIME/MIME_Message.cpp
    Sloppy/ConfigFileParser/ConfigFileParser.cpp
    Sloppy/TemplateProcessor/Template.cpp
    Sloppy/TemplateProcessor/TemplateCollection.cpp
    Sloppy/Crypto/Crypto.cpp
    Sloppy/HTML/StyledElement.cpp
    Sloppy/HTML/BasicElements.cpp
    Sloppy/HTML/Table.cpp
    Sloppy/HTML/Bootstrap.cpp
)

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBS})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

set(LIB_VERSION_MAJOR 0)
set(LIB_VERSION_MINOR 1)
set(LIB_VERSION_PATCH 3)
set(LIB_VERSION_STRING ${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH})

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${LIB_VERSION_STRING}
                                          SOVERSION ${LIB_VERSION_MAJOR})

#
# Installation data for the lib
#
# CMake's default destination path is /usr/local which is just fine
#
install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib
       )
install(DIRECTORY Sloppy DESTINATION include
    FILES_MATCHING PATTERN "*.h"
    )

#
# Unit tests
#
set(LIB_SOURCES_TST
    tests/unitTestMain.cpp
    tests/BasicTestClass.cpp
    tests/tstDateTime.cpp
    tests/tstTimestamps.cpp
    tests/tstTimePeriods.cpp
    tests/tstDatePeriods.cpp
    tests/tstRFC822.cpp
    tests/tstRFC2045.cpp
    tests/tstConfigParser.cpp
    tests/tstTemplateProcessor.cpp
    tests/tstEmailPattern.cpp
    tests/tstHTML.cpp
)

find_package(GTest)

if (GTEST_FOUND AND BUILD_TESTS)
    include_directories(${GTEST_INCLUDE_DIRS})

    add_executable(${PROJECT_NAME}_Tests ${LIB_SOURCES} ${LIB_SOURCES_TST})
    set_property(TARGET ${PROJECT_NAME}_Tests PROPERTY CXX_STANDARD 14)
    set_property(TARGET ${PROJECT_NAME}_Tests PROPERTY CXX_STANDARD_REQUIRED ON)

    message("Using these libs: ${LIBS}")

    target_link_libraries(${PROJECT_NAME}_Tests ${LIBS} ${GTEST_BOTH_LIBRARIES})
endif (GTEST_FOUND AND BUILD_TESTS)
