cmake_minimum_required(VERSION 3.13)

include(FetchContent)
include(ExternalProject)

include(ProcessorCount)
ProcessorCount(nproc)

project(ChaosWebServer)

add_subdirectory(${PROJECT_SOURCE_DIR}/../vendor/websocketpp websocketpp)

file(GLOB ROOT_SRC ${PROJECT_SOURCE_DIR}/*.cpp ${PROJECT_SOURCE_DIR}/*.h)
add_executable(ChaosWebServer ${ROOT_SRC})

set_property(TARGET ChaosWebServer PROPERTY CXX_STANDARD 20)

target_include_directories(ChaosWebServer PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/../vendor ${PROJECT_SOURCE_DIR}/../vendor/websocketpp)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED)

find_program(MAKE_EXECUTABLE NAMES make)

ExternalProject_Add(libmicrohttpd PREFIX prefix
    URL https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.75.tar.gz
    UPDATE_DISCONNECTED true
    BUILD_IN_SOURCE true
    CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}
    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${nproc}
    INSTALL_COMMAND ${MAKE_EXECUTABLE} install
)
add_dependencies(ChaosWebServer libmicrohttpd)

ExternalProject_Add(libhttpserver PREFIX prefix
    GIT_REPOSITORY https://github.com/etr/libhttpserver.git
    GIT_TAG acc3e6a3ad0b6193286886015109839d642f1ef6
    UPDATE_DISCONNECTED true
    BUILD_IN_SOURCE true
    CONFIGURE_COMMAND ./bootstrap COMMAND mkdir -p build COMMAND cd build
        COMMAND env CXXFLAGS=-I${CMAKE_CURRENT_BINARY_DIR}/include/\ -L${CMAKE_CURRENT_BINARY_DIR}/lib ../configure --prefix=${CMAKE_CURRENT_BINARY_DIR} --disable-examples # One of the examples fails to build
    BUILD_COMMAND cd build COMMAND ${MAKE_EXECUTABLE} -j${nproc}
    INSTALL_COMMAND cd build COMMAND ${MAKE_EXECUTABLE} install
)
add_dependencies(libhttpserver libmicrohttpd)
add_dependencies(ChaosWebServer libhttpserver)

target_link_directories(ChaosWebServer BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/lib)
target_include_directories(ChaosWebServer BEFORE PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/include
    )

target_link_libraries(ChaosWebServer PRIVATE ${Boost_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto microhttpd httpserver)
