cmake_minimum_required(VERSION 3.13)

project(ChaosMod)

add_subdirectory(${PROJECT_SOURCE_DIR}/../vendor/shv shv)
add_subdirectory(${PROJECT_SOURCE_DIR}/../vendor/minhook minhook)
add_subdirectory(${PROJECT_SOURCE_DIR}/../vendor/websocketpp websocketpp)
add_subdirectory(${PROJECT_SOURCE_DIR}/../vendor/boost-cmake boost)
add_subdirectory(${PROJECT_SOURCE_DIR}/../vendor/openssl-cmake openssl)

file(GLOB ROOT_SRC ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE SRC ${PROJECT_SOURCE_DIR}/Components/*.cpp ${PROJECT_SOURCE_DIR}/Effects/*.cpp
    ${PROJECT_SOURCE_DIR}/Memory/*.cpp ${PROJECT_SOURCE_DIR}/Util/*.cpp)
file(GLOB PATTERNS_SRC ${PROJECT_SOURCE_DIR}/../vendor/Patterns/Patterns.cpp)
add_library(ChaosMod MODULE ${ROOT_SRC} ${SRC} ${PATTERNS_SRC} ChaosMod.rc)

set_property(TARGET ChaosMod PROPERTY CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(ChaosMod PRIVATE -DUNICODE -D_UNICODE -DNOMINMAX /Zi)
    target_link_options(ChaosMod PRIVATE /DEBUG /OPT:REF /OPT:ICF)
else()
    target_compile_options(ChaosMod PRIVATE -municode -fpermissive)
endif()

target_include_directories(ChaosMod PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/../vendor ${PROJECT_SOURCE_DIR}/../vendor/websocketpp)

target_link_directories(ChaosMod PRIVATE ${PROJECT_SOURCE_DIR}/../vendor ${PROJECT_SOURCE_DIR}/../vendor/lua)
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_link_options(ChaosMod PRIVATE -static)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(ChaosMod PRIVATE shv minhook lua54 winmm d3dcompiler Boost::thread OpenSSL::SSL OpenSSL::Crypto wsock32 ws2_32)
else()
    target_link_libraries(ChaosMod PRIVATE shv minhook lua54 winmm d3dcompiler Boost::thread ssl crypto wsock32 ws2_32)
endif()

target_precompile_headers(ChaosMod PRIVATE stdafx.cpp)

target_compile_definitions(ChaosMod PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
set_target_properties(ChaosMod PROPERTIES SUFFIX ".asi")
