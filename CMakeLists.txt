project(Sloppy)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

set(BUILD_TESTS 1)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmakeModules/")

find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

# add the dynamic object loader lib
set(LIBS ${LIBS} dl)

set(LIB_SOURCES
    Sloppy/DateTime/DateAndTime.cpp
    Sloppy/Logger/Logger.cpp
    Sloppy/MailAndMIME/MailAndMIME.cpp
    Sloppy/MailAndMIME/Message.cpp
    Sloppy/MailAndMIME/Header.cpp
    Sloppy/MailAndMIME/MIME_Message.cpp
    Sloppy/ConfigFileParser/ConfigFileParser.cpp
    Sloppy/ConfigFileParser/ConstraintChecker.cpp
    Sloppy/Crypto/Crypto.cpp
    Sloppy/Crypto/Sodium.cpp
    Sloppy/HTML/StyledElement.cpp
    Sloppy/HTML/BasicElements.cpp
    Sloppy/HTML/Table.cpp
    Sloppy/HTML/Bootstrap.cpp
    Sloppy/TemplateProcessor/TemplateSys.cpp
    Sloppy/Net/Net.cpp
    Sloppy/Timer.cpp
    Sloppy/String.cpp
    Sloppy/Memory.cpp
    Sloppy/Utils.cpp
    Sloppy/DateTime/tz.cpp
    Sloppy/Crypto/MiniCert.cpp
    Sloppy/BasicException.h
    Sloppy/CyclicWorkerThread.h
    Sloppy/CyclicWorkerThread.cpp
    Sloppy/ThreadSafeQueue.h
    Sloppy/ThreadSafeQueue.cpp
    Sloppy/AsyncWorker.h
    Sloppy/AsyncWorker.cpp
    Sloppy/ThreadStats.h
    Sloppy/ThreadStats.cpp
    Sloppy/NamedType.h
    Sloppy/GenericRange.h
    Sloppy/CSV.h
    Sloppy/CSV.cpp
    Sloppy/ResultOrError.h
)

#
# add modules that do not compile
# under windows
#
IF (NOT WIN32)
  set(LIB_SOURCES ${LIB_SOURCES}
      #Sloppy/Net/TcpClientServer.cpp
      #Sloppy/Net/CryptoClientServer.cpp
      Sloppy/ManagedFileDescriptor.cpp
      Sloppy/Net/ManagedSocket.cpp
      Sloppy/SubProcess.h
      Sloppy/SubProcess.cpp
      )
ENDIF()

#
# add the Winsock lib when
# building under windows
#
# we need this for htonl() and the like
#
IF (WIN32)
  set(LIBS ${LIBS} ws2_32.dll)
ENDIF()

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBS})
target_compile_options(${PROJECT_NAME} PRIVATE "-Wall")
target_compile_options(${PROJECT_NAME} PRIVATE "-Wextra")
#target_compile_options(${PROJECT_NAME} PRIVATE "-Weffc++")
target_compile_options(${PROJECT_NAME}
    PRIVATE "-pedantic"
    PRIVATE "-Wcast-align"
    #PRIVATE "-Wcast-qual"
    PRIVATE "-Wctor-dtor-privacy"
    PRIVATE "-Wdisabled-optimization"
    PRIVATE "-Wformat=2"
    PRIVATE "-Winit-self"
    PRIVATE "-Wlogical-op"
    PRIVATE "-Wmissing-declarations"
    PRIVATE "-Wmissing-include-dirs"
    PRIVATE "-Wnoexcept"
    #PRIVATE "-Wold-style-cast"
    PRIVATE "-Woverloaded-virtual"
    PRIVATE "-Wredundant-decls"
    PRIVATE "-Wshadow"
    #PRIVATE "-Wsign-conversion"
    #PRIVATE "-Wsign-promo"
    PRIVATE "-Wstrict-null-sentinel"
    PRIVATE "-Wstrict-overflow=5"
    #PRIVATE "-Wswitch-default"
    PRIVATE "-Wundef"
    PRIVATE "-Wno-unused"
    PRIVATE "-Wno-format-nonliteral"
    )
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

set(LIB_VERSION_MAJOR 0)
set(LIB_VERSION_MINOR 7)
set(LIB_VERSION_PATCH 1)
set(LIB_VERSION_STRING ${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH})

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${LIB_VERSION_STRING}
                                          SOVERSION ${LIB_VERSION_MAJOR})

#
# Installation data for the lib
#
# CMake's default destination path is /usr/local which is just fine
#
IF (WIN32)
  set(CMAKE_INSTALL_PREFIX D:/msys64/usr/local )
  install(TARGETS ${PROJECT_NAME}
          RUNTIME DESTINATION lib
         )
ELSE()
    install(TARGETS ${PROJECT_NAME}
            LIBRARY DESTINATION lib
           )
ENDIF()

install(DIRECTORY Sloppy DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )

#
# Unit tests
#
set(LIB_SOURCES_TST
    tests/unitTestMain.cpp
    tests/BasicTestClass.cpp
    tests/tstDateTime.cpp
    tests/tstTimestamps.cpp
    tests/tstGenericRange.cpp
    tests/tstTimeRanges.cpp
    tests/tstArrayView.cpp
    tests/tstManagedArray.cpp
    tests/tstDateRanges.cpp
    tests/tstRFC822.cpp
    tests/tstRFC2045.cpp
    tests/tstConfigParser.cpp
    tests/tstUtils.cpp
    tests/tstHTML.cpp
    tests/tstCrypto.cpp
    tests/tstBetterTemplateSys.cpp
    tests/tstSodium.cpp
    tests/tstManagedFileDescriptor.cpp
    #tests/tstTcpClientServer.cpp
    tests/tstNetFunc.cpp
    #tests/tstCryptoClientServer.cpp
    tests/tstPasswordProtectedSecret.cpp
    tests/tstPosixFileFunc.cpp
    tests/tstTimer.cpp
    tests/tstStrings.cpp
    tests/tstMiniCert.cpp
    tests/tstMemFile.cpp
    tests/tstCyclicThread.cpp
    tests/tstThreadSafeQueue.cpp
    tests/tstAsyncWorker.cpp
    tests/tstNamedType.cpp
    tests/tstCSV.cpp
    tests/tstSubprocess.cpp
    tests/tstWallclockTime.cpp
    tests/tstResultOrError.cpp
)

find_package(GTest)
message("Google Test found: ${GTEST_FOUND}")

if (GTEST_FOUND AND BUILD_TESTS)
    include_directories(${GTEST_INCLUDE_DIRS})

    #add_executable(${PROJECT_NAME}_Tests ${LIB_SOURCES} ${LIB_SOURCES_TST})
    add_executable(${PROJECT_NAME}_Tests ${LIB_SOURCES_TST})
    set_property(TARGET ${PROJECT_NAME}_Tests PROPERTY CXX_STANDARD 17)
    set_property(TARGET ${PROJECT_NAME}_Tests PROPERTY CXX_STANDARD_REQUIRED ON)

    set_target_properties(${PROJECT_NAME}_Tests PROPERTIES
      IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/lib${PROJECT_NAME}.so"
    )

    set(LIBS ${LIBS} ${PROJECT_NAME} curl)
    message("Using these libs: ${LIBS}")
    message("Writing unit test binary to: ${CMAKE_BINARY_DIR}")

    target_link_libraries(${PROJECT_NAME}_Tests ${LIBS} ${GTEST_BOTH_LIBRARIES})
endif (GTEST_FOUND AND BUILD_TESTS)
