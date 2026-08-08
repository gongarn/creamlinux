# My version, use previous version at own risk
CMAKE_MINIMUM_REQUIRED(VERSION 3.9)

PROJECT(Creamlinux)
SET(CMAKE_CXX_STANDARD 14) # no explicit compiler flags if possible
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

link_directories(${CMAKE_SOURCE_DIR}/lib)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

ADD_LIBRARY(Creamlinux SHARED
	${CMAKE_SOURCE_DIR}/src/config.cpp
	${CMAKE_SOURCE_DIR}/src/steam_hooks.cpp
	${CMAKE_SOURCE_DIR}/src/export_hooks.cpp
)
message("CMAKE_SIZEOF_VOID_P is ${CMAKE_SIZEOF_VOID_P}")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # 64 bits
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    # 32 bits
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib32")
endif()

# --- tests -----------------------------------------------------------------
enable_testing()
ADD_EXECUTABLE(creamlinux_tests
    ${CMAKE_SOURCE_DIR}/tests/test_config.cpp
    ${CMAKE_SOURCE_DIR}/src/config.cpp
)
add_test(NAME config COMMAND creamlinux_tests)

ADD_EXECUTABLE(creamlinux_vtable_test
    ${CMAKE_SOURCE_DIR}/tests/test_vtable.cpp
    ${CMAKE_SOURCE_DIR}/src/config.cpp
)
add_test(NAME vtable COMMAND creamlinux_vtable_test)

add_custom_command(
        TARGET Creamlinux POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/package/
                ${CMAKE_CURRENT_BINARY_DIR}/lib/)


# If you want to build the testapp, you'll need to find libsteam_api and copy it to lib/libsteam_api.so
#ADD_EXECUTABLE(Testapp ${CMAKE_SOURCE_DIR}/test.cpp)
#TARGET_LINK_LIBRARIES(Testapp libsteam_api.so)
