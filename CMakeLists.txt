cmake_minimum_required(VERSION 3.3)
 
if (MSVC)
list(APPEND LIBWS_COMPILE_FLAGS /W4 /WX)
else()
list(APPEND LIBWS_COMPILE_FLAGS -Wall -Wextra -pedantic -Werror)
endif()
 
# Specify search path for CMake modules to be loaded by include() 
# and find_package()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
message("${CMAKE_ARGS}")

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
option(LIBWS_SHARED "Build a shared library" ON)
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

find_package(LibWebSockets REQUIRED)
message("LIBWEBSOCKETS_FOUND=${LIBWEBSOCKETS_FOUND}")
message("LIBWEBSOCKETS_INCLUDE_DIR=${LIBWEBSOCKETS_INCLUDE_DIR}")
set(LWS_LINKS ${LIBWEBSOCKETS_LIBRARIES})
set(LWS_INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIR})

# libws
set(LIBWS_LIB libws)

file(GLOB HEADERS ws.h)
set(SOURCES ws.c)

if (LIBWS_SHARED)

    add_library(${LIBWS_LIB} SHARED "${HEADERS}" "${SOURCES}")
    set_target_properties(${LIBWS_LIB} PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED ON
            CXX_STANDARD_REQUIRED ON)

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

endif(LIBWS_SHARED)

if (LIBWS_STATIC)
    
    set(LIBWS_STATIC_LIB "${LIBWS_LIB}-static")

    add_library(${LIBWS_STATIC_LIB} STATIC "${HEADERS}" "${SOURCES}")
    set_target_properties(${LIBWS_STATIC_LIB} PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED ON
            CXX_STANDARD_REQUIRED ON
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

endif(LIBWS_STATIC)

# include cmocka
if (LIBWS_UNIT_TESTING)
    if (NOT LIBWS_STATIC)
        message("Skip unit testing because LIBWS_STATIC option is off")

    else()
        include(AddCMockaTest) 
        add_subdirectory(tests)

    endif(NOT LIBWS_STATIC)
endif (LIBWS_UNIT_TESTING)

if (LIBWS_DOXYGEN)
    add_subdirectory ("docs")
endif (LIBWS_DOXYGEN)
