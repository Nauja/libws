cmake_minimum_required(VERSION 3.3)
 
if (MSVC)
list(APPEND LIBWS_COMPILE_FLAGS /W4 /WX)
else()
list(APPEND LIBWS_COMPILE_FLAGS -Wall -Wextra -pedantic -Werror)
endif()
 
# Specify search path for CMake modules to be loaded by include() 
# and find_package()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

include(ExternalProject)

set(LIBWS_VERSION_MAJOR 0)
set(LIBWS_VERSION_MINOR 1)
set(LIBWS_VERSION_PATCH 0)
set(LIBWS_VERSION "${LIBWS_VERSION_MAJOR}.${LIBWS_VERSION_MINOR}.${LIBWS_VERSION_PATCH}")

#find_package(Git)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ERROR_VARIABLE RESULT_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(LENGTH "${RESULT_STRING}" LENGTH_RESULT_STRING)

    if(${LENGTH_RESULT_STRING} EQUAL 0)

        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 --format=%H
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        string(SUBSTRING ${GIT_COMMIT_HASH} 0 7 GIT_COMMIT_HASH)
        set(VERSION_HASH ${GIT_COMMIT_HASH} )

        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list HEAD --count
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE VERSION_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

    endif()
endif()

project(libws VERSION ${LIBWS_VERSION} LANGUAGES C)

include(ConfigureChecks)

option(LIBWS_STATIC "Build a static library" ON)
option(LIBWS_UNIT_TESTING "Unit Tests Enabled" ON)
option(LIBWS_DOXYGEN "Docs Enabled" OFF)

# disallow in-source build
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out of source build. Please create a separate build directory and run 'cmake /path/to/${PROJECT_NAME} [options]' there.")

message("Building for target : ${CMAKE_SYSTEM_NAME}")
message("PROJECT_VERSION: ${LIBWS_VERSION}")
message("VERSION_HASH: ${VERSION_HASH}")
configure_file(ws.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/ws.h)

# libwebsockets

set(LIBWEBSOCKETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/libwebsockets")
set(LIBWEBSOCKETS_INCLUDE_DIR "${LIBWEBSOCKETS_SOURCE_DIR}/include")
set(LIBWEBSOCKETS_BINARY_DIR "${LIBWEBSOCKETS_SOURCE_DIR}/build")

ExternalProject_Add(libwebsockets
PREFIX libwebsockets
SOURCE_DIR ${LIBWEBSOCKETS_SOURCE_DIR}
BINARY_DIR ${LIBWEBSOCKETS_BINARY_DIR}
INSTALL_COMMAND ""
CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DLWS_WITH_STATIC=On
        -DLWS_WITH_SHARED=Off
        -DLWS_WITHOUT_TESTAPPS=On
        -DLWS_WITHOUT_TEST_SERVER=On
        -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=On
        -DLWS_WITHOUT_TEST_PING=On
        -DLWS_WITHOUT_TEST_CLIENT=On
        -DLWS_WITH_MINIMAL_EXAMPLES=Off
)

add_library(libwebsockets_vendor STATIC SHARED IMPORTED)

if(MSVC)
set(LIBWEBSOCKETS_LIBRARY "${LIBWEBSOCKETS_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/libwebsockets.lib")
else()
set(LIBWEBSOCKETS_LIBRARY "${LIBWEBSOCKETS_BINARY_DIR}/lib/libwebsockets_static.a")
endif()

add_dependencies(libwebsockets_vendor libwebsockets)

# libws
set(LIBWS_LIB libws)

file(GLOB HEADERS ws.h)
set(SOURCES ws.c)

set(LWS_LINKS libssl wsock32 ws2_32 ssl crypto crypt32)
set(LWS_INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIR} ${LIBWEBSOCKETS_BINARY_DIR})
set(LWS_DEPENDENCIES libwebsockets)

add_library(${LIBWS_LIB} SHARED "${HEADERS}" "${SOURCES}")
set_target_properties(${LIBWS_LIB} PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

target_compile_options(${LIBWS_LIB}
                        PRIVATE
                            ${LIBWS_COMPILE_FLAGS}
                            -DHAVE_CONFIG_H)
                   

target_link_libraries(${LIBWS_LIB} PRIVATE ${LWS_LINKS})

target_include_directories(${LIBWS_LIB}
                            SYSTEM PRIVATE
                                ${CMAKE_CURRENT_BINARY_DIR}
                                ${LWS_INCLUDE_DIRS}
                            PUBLIC
                                ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(${LIBWS_LIB} libwebsockets)

if (LIBWS_STATIC)
    
    set(LIBWS_STATIC_LIB "${LIBWS_LIB}-static")

    add_library(${LIBWS_STATIC_LIB} STATIC "${HEADERS}" "${SOURCES}")
    set_target_properties(${LIBWS_STATIC_LIB} PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            OUTPUT_NAME "${LIBWS_LIB}"
            PREFIX "")

    target_compile_options(${LIBWS_STATIC_LIB}
                           PRIVATE
                               ${LIBWS_COMPILE_FLAGS}
                               -DHAVE_CONFIG_H
                               -DLIBWS_HIDE_SYMBOLS)
                              
    target_link_libraries(${LIBWS_STATIC_LIB} PRIVATE ${LWS_LINKS})

    target_include_directories(${LIBWS_STATIC_LIB}
    SYSTEM
                               PRIVATE
                                   ${CMAKE_CURRENT_BINARY_DIR}
                                   ${LWS_INCLUDE_DIRS}
                               PUBLIC
                                    ${CMAKE_CURRENT_SOURCE_DIR})

    add_dependencies(${LIBWS_STATIC_LIB} libwebsockets)

endif(LIBWS_STATIC)
