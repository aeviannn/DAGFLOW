cmake_minimum_required(VERSION 3.16)
project(DAGFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dagflow_graph
    src/graph_analysis.cpp
    src/workflow_graph.cpp
)
target_include_directories(dagflow_graph PUBLIC include)
if(MSVC)
    target_compile_options(dagflow_graph PRIVATE /W4)
else()
    target_compile_options(dagflow_graph PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(graph_tests tests/graph_tests.cpp)
target_link_libraries(graph_tests PRIVATE dagflow_graph)
target_include_directories(graph_tests PRIVATE examples)
if(MSVC)
    target_compile_options(graph_tests PRIVATE /W4)
else()
    target_compile_options(graph_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME graph_tests COMMAND graph_tests)
