project(libws-tests C)

include(ExternalProject)

# CMocka
set(CMOCKA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/cmocka")
set(CMOCKA_INCLUDE_DIR "${CMOCKA_SOURCE_DIR}/include")
set(CMOCKA_BINARY_DIR "${CMOCKA_SOURCE_DIR}/build")

ExternalProject_Add(cmocka
  PREFIX cmocka
  SOURCE_DIR ${CMOCKA_SOURCE_DIR}
  BINARY_DIR ${CMOCKA_BINARY_DIR}
  INSTALL_COMMAND ""
  CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DHAVE_SNPRINTF=${HAVE_SNPRINTF}
        -DHAVE_VSNPRINTF=${HAVE_VSNPRINTF}
        -DWITH_STATIC_LIB=true
)
ExternalProject_Get_Property(cmocka source_dir binary_dir)

add_library(cmocka_vendor STATIC SHARED IMPORTED)

if(MSVC)
  set(CMOCKA_LIBRARY "${CMOCKA_BINARY_DIR}/src/${CMAKE_CFG_INTDIR}/cmocka-static.lib")
else()
  set(CMOCKA_LIBRARY "${CMOCKA_BINARY_DIR}/src/libcmocka-static.a")
endif()

add_dependencies(cmocka_vendor cmocka)

# libws tests
set(_TEST "test_ws")

message("CMOCKA_BINARY_DIR=${CMOCKA_BINARY_DIR}")
message("CMOCKA_LIBRARY=${CMOCKA_LIBRARY}")

include_directories(${CMOCKA_INCLUDE_DIR}
                    ${CMAKE_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${OPENSSL_INCLUDE_DIR}
                    ${LWS_INCLUDE_DIRS})

add_cmocka_test(libws-${_TEST}
                SOURCES ${CMAKE_SOURCE_DIR}/ws.c ${CMAKE_CURRENT_SOURCE_DIR}/${_TEST}.c
                COMPILE_OPTIONS ${DEFAULT_C_COMPILE_FLAGS}
                LINK_LIBRARIES ${CMOCKA_LIBRARY} ${LWS_LINKS} ${LIBWS_STATIC_LIB}
)
    