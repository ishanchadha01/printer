cmake_minimum_required(VERSION 3.20)
project(printer)

# Make sure build is run with
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -std=c++20 \
    -stdlib=libc++")

enable_testing()

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

# make pybind11 use CMake FindPython3 instead of FindPythonLibs
set(PYBIND11_FINDPYTHON ON CACHE BOOL "" FORCE)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

add_subdirectory(extern/pybind11)

# add_executable(test_stl tests/test_stl.cpp src/mesh.cpp src/main.cpp)
# target_include_directories(test_stl PRIVATE ${PROJECT_SOURCE_DIR})
# target_link_libraries(test_stl gtest_main)
add_executable(test_stl tests/test_stl.cpp)
target_include_directories(test_stl PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(test_stl gtest_main)

add_executable(test_controller tests/test_controller.cpp src/path_plan.cpp src/mesh.cpp)
target_include_directories(test_controller PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(test_controller gtest_main)

include(GoogleTest)
gtest_discover_tests(test_stl)
gtest_discover_tests(test_controller)

pybind11_add_module(pathplan_bindings visualization/pathplan_bindings.cpp src/path_plan.cpp)
target_include_directories(pathplan_bindings PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(pathplan_bindings PRIVATE Boost::boost)
