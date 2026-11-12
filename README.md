# DAGFlow

DAGFlow is a small C++17 project for studying dependency-aware scheduling and deterministic workflow simulation. It models tasks as a directed acyclic graph, applies three ready-task policies, simulates work across a chosen number of workers, and reports schedule performance. The project is designed to stay understandable as a short college algorithms project.

## Features

- Validated adjacency-list workflow graphs.
- Deterministic Kahn topological sorting and cycle detection.
- Bottom-level and global critical path calculations.
- FCFS, Shortest Job First (SJF), and Critical Path First (CPF) policies.
- Deterministic multi-worker discrete-event simulation, including simultaneous and zero-duration completions.
- Makespan, total busy time, utilization, throughput, and optional same-policy speedup comparison.
- Interactive console app with five in-memory workflow presets, a policy comparison table, and bounded-width ASCII worker timelines.

## Console application

Start the app and follow its numbered menus. The workflow list includes an empty graph for edge-case demonstration.

```powershell
.\build\dagflow.exe
```

Example scripted input: select the fork-and-join workflow, use three workers, run an individual simulation, and select SJF.

```powershell
"3`n3`n1`n2`n" | .\build\dagflow.exe
```

The report includes a topological order, critical path length, task start/completion records, metrics, and a worker timeline. For this preset, the report begins approximately as follows; task times are calculated by the simulator:

```text
Workflow: Fork-and-join workflow
Policy: SJF
Workers: 3
Topological order: 1 2 3
Critical path length: 7 time units
```

Choose comparison mode to run FCFS, SJF, and CPF on the same workflow and worker count. For each policy, the app runs a one-worker baseline and reports speedup when the comparison uses at least two workers. With one selected worker, speedup is shown as N/A.

## Workflow presets

1. Empty workflow.
2. Simple three-task dependency chain.
3. Fork-and-join workflow.
4. Simultaneous completion with a zero-duration join.
5. Ten-task software build pipeline: checkout, configuration, source generation, parallel compile branches, linking, tests, and packaging.

Presets are defined in `examples/workflows.hpp`. External workflow-file parsing is not part of this version.

## Architecture

Core code is independent of console input and formatting.

| Area | Responsibility |
| --- | --- |
| `task.hpp`, `workflow_graph.*` | Task types, validated graph storage, and canonical workflow identity. |
| `graph_analysis.*` | Deterministic topological order, cycle detection, bottom levels, and critical path length. |
| `scheduler.*` | ReadyQueue interface and FCFS, SJF, and CPF task selection. |
| `simulator.*` | Discrete-event multi-worker execution and execution records. |
| `metrics.*` | Validated metrics and speedup comparison. |
| `examples/workflows.hpp` | Reusable in-memory workflow presets. |
| `app/cli.*` | Menu input, validation, and orchestration of core modules. |
| `app/report.*` | Report tables and ASCII Gantt-style timeline formatting. |
| `app/main.cpp` | Console entry point. |

## Algorithms and complexity

Here `V` is the task count, `E` the dependency edge count, and `W` the worker count.

- Graph storage and edge traversal: `O(V + E)` space and traversal.
- Deterministic Kahn sort with task-ID min-priority queue: `O((V + E) log V)` time, `O(V)` auxiliary space.
- Bottom levels after topological sorting: `O(V + E)` time, `O(V)` space.
- FCFS ready-queue push/pop: `O(1)` each; SJF and CPF: `O(log V)` each.
- Simulator: `O(W + (V + E) log V + V log W)` time and `O(V + W)` space, including worker-heap setup and assignment.
- Metrics validation/calculation: expected `O(V)` time and `O(V)` auxiliary space.
- Workflow signature construction: `O(V log V + E log V)` time and `O(V + E)` storage; equality compares all task IDs, durations, and edges exactly.

The Gantt display maps simulated time to a fixed 60-column timeline. Idle periods use `.`, running intervals use `=`, and zero-duration work uses `|`. Each worker row is followed by exact task IDs and `[start, completion)` intervals, so compressed segments retain precise timings. Time labels and chart width do not grow with makespan.

## Build

Requirements: a C++17 compiler and CMake 3.16 or newer. No third-party libraries are used.

```powershell
cmake -S . -B build
cmake --build build
```

The executable is `build/dagflow.exe` on Windows and `build/dagflow` on Unix-like systems.

## Tests

Run every registered test suite with CTest:

```powershell
ctest --test-dir build --output-on-failure
```

Suites cover graph construction/analysis, scheduler policies, simulation behavior, metrics, and CLI smoke flows. CLI checks exercise every preset, all policies, worker-count choices, invalid menu/worker input, comparison output, empty workflow, and bounded Gantt formatting.

If CMake is unavailable, compile each suite directly (PowerShell examples):

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -Iexamples src/workflow_graph.cpp src/graph_analysis.cpp src/scheduler.cpp src/simulator.cpp src/metrics.cpp tests/graph_tests.cpp -o graph_tests.exe
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/workflow_graph.cpp src/graph_analysis.cpp src/scheduler.cpp src/simulator.cpp src/metrics.cpp tests/scheduler_tests.cpp -o scheduler_tests.exe
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/workflow_graph.cpp src/graph_analysis.cpp src/scheduler.cpp src/simulator.cpp src/metrics.cpp tests/simulator_tests.cpp -o simulator_tests.exe
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/workflow_graph.cpp src/graph_analysis.cpp src/scheduler.cpp src/simulator.cpp src/metrics.cpp tests/metrics_tests.cpp -o metrics_tests.exe
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -Iapp -Iexamples src/workflow_graph.cpp src/graph_analysis.cpp src/scheduler.cpp src/simulator.cpp src/metrics.cpp app/main.cpp app/cli.cpp app/report.cpp -o dagflow.exe
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -Iapp -Iexamples src/workflow_graph.cpp src/graph_analysis.cpp src/scheduler.cpp src/simulator.cpp src/metrics.cpp app/cli.cpp app/report.cpp tests/cli_tests.cpp -o cli_tests.exe
```

Run each generated test executable (`graph_tests.exe`, `scheduler_tests.exe`, `simulator_tests.exe`, `metrics_tests.exe`, and `cli_tests.exe`).

## Known limitations

- Workflows are selected from built-in examples; users cannot load custom workflow files.
- Gantt intervals are drawn on a compact fixed-width scale, so very short tasks may occupy a single visual column. Exact intervals are printed beneath each worker row.
- Speedup is undefined and displayed as N/A when the measured makespan is zero or when the selected comparison worker count is one.
- The console app is text-only; it does not provide a graphical interface or export reports.

## Local desktop-style web interface

The React interface visualizes completed schedules returned by the existing C++17 executable. A small Node.js/Express bridge validates requests and launches that executable with its JSON CLI; JavaScript does not implement scheduling or recalculate metrics.

Requirements: Node.js 24 or compatible, npm, and a built `build/dagflow.exe`. Install frontend packages from the repository root:

```powershell
npm.cmd install
```

Start the API bridge and Vite development server in separate PowerShell terminals:

```powershell
npm.cmd run dev:api
npm.cmd run dev:ui
```

Open `http://127.0.0.1:5173`. Vite proxies `/api` to Express on `127.0.0.1:4174`. The bridge defaults to `build/dagflow.exe`; set `DAGFLOW_ENGINE_PATH` before starting the bridge if your executable is elsewhere. The API binds only to loopback. For a production-style local preview, run `npm.cmd run build` and then `npm.cmd run dev:api`; Express serves `frontend/dist` and the API from port 4174.

API routes are `GET /api/health`, `GET /api/workflows`, `POST /api/simulate`, and `POST /api/compare`. Simulation requests contain `workflow`, `workers`, and `policy`; comparison requests contain `workflow` and `workers`. Workflow IDs are fetched from the C++ catalog. The Gantt chart and dependency graph use the engine's returned task and execution records.

Run the Node bridge integration tests against the built C++ executable with:

```powershell
npm.cmd run test:api
```

These tests exercise the local HTTP API and therefore need permission to bind and connect to a loopback port in restricted shells.

Run the Chromium browser tests with:

```powershell
npx.cmd playwright install chromium --only-shell
npm.cmd run test:e2e
```

Playwright starts the Express bridge and Vite development server when they are not already running. The browser suite covers workflow selection, simulation requests, configuration invalidation, overlapping requests, policy comparison, empty workflows, and recoverable API and engine errors. These tests also require loopback access.
