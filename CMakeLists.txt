cmake_minimum_required(VERSION 3.16)
project(DAGFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dagflow_graph
    src/graph_analysis.cpp
    src/metrics.cpp
    src/scheduler.cpp
    src/simulator.cpp
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

add_executable(scheduler_tests tests/scheduler_tests.cpp)
target_link_libraries(scheduler_tests PRIVATE dagflow_graph)
if(MSVC)
    target_compile_options(scheduler_tests PRIVATE /W4)
else()
    target_compile_options(scheduler_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME scheduler_tests COMMAND scheduler_tests)

add_executable(simulator_tests tests/simulator_tests.cpp)
target_link_libraries(simulator_tests PRIVATE dagflow_graph)
if(MSVC)
    target_compile_options(simulator_tests PRIVATE /W4)
else()
    target_compile_options(simulator_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME simulator_tests COMMAND simulator_tests)

add_executable(metrics_tests tests/metrics_tests.cpp)
target_link_libraries(metrics_tests PRIVATE dagflow_graph)
if(MSVC)
    target_compile_options(metrics_tests PRIVATE /W4)
else()
    target_compile_options(metrics_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME metrics_tests COMMAND metrics_tests)

add_library(dagflow_console
    app/cli.cpp
    app/command_line.cpp
    app/json_writer.cpp
    app/report.cpp
)
target_include_directories(dagflow_console PUBLIC app examples)
target_link_libraries(dagflow_console PUBLIC dagflow_graph)
if(MSVC)
    target_compile_options(dagflow_console PRIVATE /W4)
else()
    target_compile_options(dagflow_console PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(dagflow app/main.cpp)
target_link_libraries(dagflow PRIVATE dagflow_console)
if(MSVC)
    target_compile_options(dagflow PRIVATE /W4)
else()
    target_compile_options(dagflow PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cli_tests tests/cli_tests.cpp)
target_link_libraries(cli_tests PRIVATE dagflow_console)
if(MSVC)
    target_compile_options(cli_tests PRIVATE /W4)
else()
    target_compile_options(cli_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME cli_tests COMMAND cli_tests)

add_executable(json_cli_tests tests/json_cli_tests.cpp)
target_link_libraries(json_cli_tests PRIVATE dagflow_console)
if(MSVC)
    target_compile_options(json_cli_tests PRIVATE /W4)
else()
    target_compile_options(json_cli_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME json_cli_tests COMMAND json_cli_tests)
