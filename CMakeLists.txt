cmake_minimum_required(VERSION 3.20)
project(printer)
set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(test_stl tests/test_stl.cpp src/mesh.cpp)
target_include_directories(test_stl PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(test_stl gtest_main)

include(GoogleTest)
gtest_discover_tests(test_stl)