# DAGFlow Project Guide

DAGFlow is a 10-day college algorithms project: a C++17 engine for dependency-aware workflow scheduling and deterministic simulation. Keep the implementation small, modular, and teachable. A student should be able to explain every data structure, algorithm, and metric in a code review or presentation.

## Scope and design

- Use standard C++17 and the STL. Keep external dependencies to a minimum; do not add one without a clear need and approval.
- Do not add AI/ML, databases, web backends, authentication, networking, or unrelated features. Do not silently expand or change the agreed scope.
- Prefer clear names, focused functions, simple interfaces, and comments that explain non-obvious decisions rather than restating code.
- Separate graph/model, scheduling, simulation, metrics, input/output, and tests into understandable modules as the project grows. Avoid premature frameworks and unnecessary abstraction.
- Before a major implementation or architectural change, describe the proposed design, key choices, and tradeoffs, then wait for the user's approval. Small fixes and documentation updates do not need this gate.
- Validate all external input and report invalid task identifiers, malformed values, negative durations, and invalid dependency references clearly. Do not continue with a partially valid workflow unless that behavior is explicitly designed and documented.

## Required algorithms

Implement the following with documented purpose and time/space complexity, including relevant assumptions (such as whether `V` is tasks and `E` is dependencies):

| Algorithm | Purpose | Expected complexity |
| --- | --- | --- |
| Adjacency-list DAG representation | Store task dependencies and successor edges efficiently. | `O(V + E)` storage; edge traversal `O(V + E)`. |
| Cycle detection | Reject cyclic dependencies before scheduling. | `O((V + E) log V)` time when using the deterministic min-priority-queue Kahn implementation, `O(V)` auxiliary space. |
| Kahn topological sort | Produce a valid dependency-respecting task order and detect cycles when not all vertices are emitted. | `O((V + E) log V)` time with a min-priority queue ordered by task ID, `O(V)` auxiliary space. |
| Critical path / bottom-level calculation | Compute each task's duration plus the longest successor path for priority decisions. | `O(V + E)` time after topological sorting, `O(V)` auxiliary space. |
| FCFS scheduler | Select ready tasks in first-come/ready order. | Document the implementation's queue/selection costs; target `O((V + E) log V)` or better overall. |
| SJF scheduler | Select the ready task with the shortest duration. | Document priority-queue costs; target `O((V + E) log V)` or better overall. |
| Critical Path First scheduler | Prioritize ready tasks by descending bottom-level/critical-path value. | Document priority-queue costs; target `O((V + E) log V)` or better overall. |
| Multi-worker discrete-event simulation | Advance time to the next completion and release newly ready work using a priority queue. | Document event and ready-queue costs; target `O((V + E) log V)` overall. |
| Performance metrics | Report useful measures such as makespan, worker utilization, and throughput with defined formulas and edge-case behavior. | Document complexity; aim for `O(V + W)` time and `O(W)` extra space for `W` workers. |

If an implementation differs from a target complexity, explain the actual complexity and why. Define deterministic tie-breaking for ready tasks and simultaneous events (for example, task ID, then worker ID); never rely on unspecified container iteration order. Simulation results must be repeatable for identical inputs and settings.

## Build and run

Support a normal `g++` C++17 build and, when using CMake, a straightforward CMake build. Keep the exact commands current in the README and in any changed build documentation. A simple single-target build may use:

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude src/*.cpp -o dagflow
```

For a CMake project:

```sh
cmake -S . -B build
cmake --build build
```

Use the test commands provided by the repository's actual test setup. If tests are registered with CTest:

```sh
ctest --test-dir build --output-on-failure
```

Do not claim a command works until it has been verified against the current project layout. Keep compiler warnings enabled and fix warnings introduced by changes.

## Testing requirements

Add or maintain tests for at least:

- Empty workflow / empty graph.
- Disconnected components.
- Multiple tasks completing at the same simulation time.
- Invalid dependency references and other malformed input.
- Cycles, including self-dependencies where supported by input validation.
- Ties in FCFS, SJF, and Critical Path First, confirming documented deterministic order.
- One-worker and multiple-worker simulation.
- Dependency constraints, task completion times, and metric edge cases (including zero work where applicable).

Tests should assert meaningful outputs and invariants, not implementation details. Do not invent benchmark results; label measured results with the environment and method, and otherwise report complexity analytically.

## Change expectations

- Preserve dependency correctness: a task cannot start until all predecessors complete.
- Keep scheduling policy separate from simulation mechanics so each can be understood and tested independently.
- Update complexity documentation, input rules, and build/test instructions when behavior changes.
- Explain material design decisions in plain language and keep every addition appropriate for a short student project.
