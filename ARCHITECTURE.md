# DAGFlow Architecture

## Purpose and constraints

DAGFlow is a small, teachable C++17 engine for dependency-aware task scheduling and deterministic multi-worker simulation. It uses the standard library only. The input/output boundary stays thin; the core works with an in-memory workflow. No input file format is prescribed here because the requirements do not define one.

This document is a design proposal. Implementation begins only after the user approves this refined design.

## Shared types and validation rules

```cpp
using TaskId = std::uint32_t;
using TaskIndex = std::size_t;
using Time = std::int64_t; // integer time units

struct TaskSpec {
    TaskId id;
    Time duration;
    std::vector<TaskId> dependencies; // predecessor task IDs
};
```

Durations must be nonnegative. Task IDs must be unique; every dependency must name an existing task; duplicate dependency edges are rejected. A self-dependency is reported as a cycle. Invalid input raises `std::invalid_argument`; detected cycles raise a dedicated `CycleError` derived from `std::runtime_error`; arithmetic overflow raises `std::overflow_error`. Nothing is silently dropped or partially accepted.

Internally, tasks receive dense indices. The graph owns task records, an ID-to-index lookup, successor adjacency lists, and initial indegree counts. It does not depend on hash-map iteration order. Any order that affects output is explicitly sorted or selected by a documented comparator.

## Module boundaries, APIs, and ownership

Suggested layout:

```text
include/dagflow/task.hpp
include/dagflow/workflow_graph.hpp
include/dagflow/graph_analysis.hpp
include/dagflow/scheduler.hpp
include/dagflow/simulator.hpp
include/dagflow/metrics.hpp
include/dagflow/input.hpp       # only if a CLI/input format is agreed
include/dagflow/output.hpp      # only if needed by the chosen interface
src/...
tests/...
```

### Task model and graph (`task`, `workflow_graph`)

`WorkflowGraph` is immutable after construction and owns all task and edge storage. `TaskIndex` values are valid only for the graph that produced them.

```cpp
struct Task {
    TaskId id;
    Time duration;
};

class WorkflowGraph {
public:
    static WorkflowGraph fromSpecs(std::vector<TaskSpec> specs);
    std::size_t size() const noexcept;
    const Task& task(TaskIndex index) const;
    const std::vector<TaskIndex>& successors(TaskIndex index) const;
    std::size_t indegree(TaskIndex index) const;
    TaskIndex indexOf(TaskId id) const; // throws invalid_argument if absent
};
```

The graph owns its vectors and lookup. `fromSpecs` validates IDs, durations, and dependency references before returning a graph. The parser, if one is added, owns parsing and syntax errors and passes validated-shaped `TaskSpec` records to this factory; it does not own or mutate the graph.

Graph construction and storage target expected `O(V + E)` time and `O(V + E)` space using an ID lookup. If an implementation sorts input records or edges, document the additional sorting cost. Successor iteration order has no semantic effect; newly-ready tasks are sorted before scheduler insertion.

### Graph analysis (`graph_analysis`)

```cpp
struct GraphAnalysis {
    std::vector<TaskIndex> topologicalOrder;
    std::vector<Time> bottomLevel; // indexed by TaskIndex
    Time criticalPathLength;       // maximum bottom level, or 0 for V == 0
};

GraphAnalysis analyze(const WorkflowGraph& graph); // throws CycleError on a cycle
```

`analyze` borrows the immutable graph for the duration of the call and returns owned vectors. Kahn's algorithm uses a min-priority queue ordered by task ID for deterministic topological output. If fewer than `V` tasks are emitted, it throws `CycleError`.

Bottom level is **task-specific**: `duration(v) + max(bottomLevel(s))` over successors `s`, or `duration(v)` for a sink. Global critical path length is **one workflow-wide value**: the maximum bottom level among all tasks, or zero for an empty graph. Bottom levels rank Critical Path First choices; the global value describes the longest dependency path in the workflow. These are not interchangeable.

Kahn sorting costs `O((V + E) log V)` time and `O(V)` auxiliary space with the specified min-heap tie rule. A simple implementation could use a FIFO queue for `O(V + E)`, but would have a different deterministic tie order and must say so. Bottom-level calculation is `O(V + E)` after sorting and `O(V)` additional space. Checked addition detects path-length overflow.

### Ready-task policies (`scheduler`)

```cpp
enum class SchedulingPolicy { FCFS, SJF, CriticalPathFirst };

class ReadyQueue {
public:
    virtual ~ReadyQueue() = default;
    virtual void push(TaskIndex task) = 0;
    virtual TaskIndex pop() = 0; // precondition: not empty
    virtual bool empty() const noexcept = 0;
};

std::unique_ptr<ReadyQueue> makeReadyQueue(
    SchedulingPolicy policy,
    const WorkflowGraph& graph,
    const GraphAnalysis& analysis);
```

The factory returns a queue owned by the caller. Queue implementations borrow the graph and analysis, which must outlive the queue. `Simulator::run` creates and owns its policy queue for one run; callers do not share queue state between simulations.

- **FCFS:** FIFO insertion order. Initially-ready tasks are inserted by ascending task ID. At each completion timestamp, the newly-ready batch is inserted by ascending task ID. Tasks already waiting remain ahead of that batch. Once inserted, FIFO order is preserved.
- **SJF:** smallest duration first, then ascending task ID.
- **Critical Path First:** largest task-specific bottom level first, then ascending task ID.

Each policy inserts and removes each task at most once. FCFS queue operations are `O(1)`; SJF and Critical Path First operations are `O(log V)`. Ties are explicit, so results do not depend on STL iteration order.

### Discrete-event simulation (`simulator`)

```cpp
struct TaskExecution {
    TaskId taskId;
    std::size_t workerId;
    Time startTime;
    Time completionTime;
};

struct SimulationResult {
    std::vector<TaskExecution> executions; // ordered by task dispatch
    Time makespan;
    std::size_t completedTaskCount;
    SchedulingPolicy policy;
    std::size_t workerCount;
};

class Simulator {
public:
    static SimulationResult run(const WorkflowGraph& graph,
                                const GraphAnalysis& analysis,
                                SchedulingPolicy policy,
                                std::size_t workerCount); // workerCount must be positive
};
```

`Simulator::run` borrows the graph and analysis, owns all per-run state and its ready queue, and returns dispatch-ordered execution records. It rejects `workerCount == 0` with `std::invalid_argument`. A completed empty workflow has makespan zero and zero completed tasks. Metrics remain a separate module and can derive busy time from execution intervals.

Internal state consists of remaining indegrees, completion count, a min-priority queue of free worker IDs, and a min-priority queue of events `(finishTime, workerId, taskIndex)`. Events are ordered by finish time, then worker ID, then task ID. Worker allocation always uses the lowest available ID. Events at the same time are handled as one completion batch; the event tie order makes processing reproducible, while batching ensures dependents see the complete set of simultaneous completions before dispatch.

#### Exact event order

1. Validate positive worker count. Initialize time to zero, remaining indegrees from the graph, free workers as IDs `0..W-1`, and the selected ready queue.
2. Insert all initially-ready tasks by ascending task ID.
3. While unfinished tasks remain, dispatch while both a free worker and a ready task exist. Choose the worker with the lowest ID and the next task under the selected policy. Record its start time and push an event at `currentTime + duration`.
4. If work remains, take the smallest event time and set `currentTime` to it. Remove every event with that same time, in `(workerId, taskId)` order. Mark all those tasks complete and return all their workers to the free-worker queue.
5. For every completed task, decrement each successor's remaining indegree. Collect each successor that reaches zero exactly once. Sort this newly-ready batch by ascending task ID and insert it into the ready queue. Existing ready tasks stay ahead under FCFS.
6. Return to dispatch. If tasks remain but there are no events and no dispatchable work, report an internal logic error; a validated DAG should make this unreachable.

Zero-duration tasks create events at the current time. The algorithm processes them through the same event-batch path without advancing time. Each processed event completes one task, so same-time processing makes finite progress and cannot spin indefinitely. A zero-duration successor released by a completion can run and complete at that same timestamp. Simultaneous completions release dependencies as one batch before any new dispatch.

The simulation processes at most `V` task completions and `E` dependency edges. With a heapified initial worker-ID queue, the implementation costs `O(W + (V + E) log V + V log W)` time: worker initialization is `O(W)`, ready batches and events are `O((V + E) log V)`, and worker heap operations are `O(V log W)`. Extra space is `O(V + W)`.

### Metrics and optional speedup (`metrics`)

```cpp
struct ComparisonResult {
    SimulationResult parallel;
    std::optional<SimulationResult> singleWorkerBaseline;
};

class ComparisonRunner {
public:
    static ComparisonResult run(
        const WorkflowGraph& graph,
        const GraphAnalysis& analysis,
        SchedulingPolicy policy,
        std::size_t workerCount,
        bool includeSpeedup);
};

struct Metrics {
    Time totalWork;
    Time makespan;
    double utilization;
    double throughput;
    std::optional<double> speedup;
};

class MetricsCalculator {
public:
    static Metrics calculate(
        const WorkflowGraph& graph,
        const ComparisonResult& comparison);
};
```

`ComparisonRunner` borrows the immutable graph and analysis and owns both returned results. It runs the selected policy with the requested worker count. If `includeSpeedup` is true, it also runs the same graph and policy with one worker; when the requested count is already one, it reuses that run as the baseline. `MetricsCalculator` borrows the graph and comparison and owns no data. It validates result vector sizes, policy equality, and that any baseline has one worker; a mismatch raises `std::invalid_argument`. Graph provenance is a caller contract: both runs in a `ComparisonResult` must come from the same graph and analysis.

- `totalWork = sum(task durations)`.
- `makespan = SimulationResult::makespan`.
- `utilization = totalWork / (workerCount * makespan)`. It is `0` when makespan is zero.
- `throughput = V / makespan` tasks per time unit. It is `0` when makespan is zero.
- Optional `speedup = singleWorkerMakespan / parallelMakespan`. It is `std::nullopt` if no baseline was requested or either makespan is zero. Otherwise it is a finite positive ratio. For one worker with positive makespan, speedup is `1`.

Sum durations with checked integer addition. Do not multiply `workerCount * makespan` in integer arithmetic; convert operands to `long double` before multiplication/division, then store the reported ratio as `double`. Metrics take `O(V + W)` time and `O(1)` extra space beyond the result vectors.

### Input/output and executable (`input`, `output`, `main`)

These modules own parsing and presentation only. They must report malformed input clearly and must not implement graph algorithms or scheduling rules. Once a simple input format is agreed, the interfaces are:

```cpp
std::vector<TaskSpec> parseWorkflow(std::istream& input);
void writeReport(std::ostream& output,
                 const GraphAnalysis& analysis,
                 const ComparisonResult& comparison,
                 const Metrics& metrics);
```

The parser borrows the input stream and returns owned task specs; it raises `std::invalid_argument` for malformed content and `std::runtime_error` for stream failures. The reporter borrows its stream and result objects and does not retain references. `main` owns the streams, parsed specs, graph, analysis, comparison results, and metrics. No network, database, web service, or third-party framework is part of the design.

## Overflow and failure behavior

All time values use signed 64-bit integer units. A shared helper performs checked addition before computing finish times, bottom levels, total work, or per-worker busy time: for nonnegative `b`, reject when `a > max(Time) - b`. Throw `std::overflow_error` before storing a wrapped value. Ratios use floating-point conversion only after integer totals have been checked. This also makes event-time equality exact and deterministic.

## Hand-calculated examples

Assume IDs shown are the numeric task IDs. A notation `A(2) <- []` means task A has duration 2 and no predecessors. FCFS initial and same-time ready batches are task-ID ordered; workers are allocated by lowest ID.

### Example 1: chain, two workers

`1(2) <- []`, `2(3) <- [1]`, `3(1) <- [2]`.

The only possible schedule is task 1 at `[0,2)`, task 2 at `[2,5)`, task 3 at `[5,6)`. Makespan is `6`; total work is `6`; bottom levels are `6, 4, 1`; global critical path length is `6`. With two workers utilization is `6 / (2*6) = 0.5`; throughput is `3/6 = 0.5`. A one-worker baseline also takes `6`, so optional speedup is `1`.

### Example 2: fork and join, two workers

`1(2) <- []`, `2(3) <- []`, `3(4) <- [1,2]`.

At time 0, task 1 runs on worker 0 and task 2 on worker 1. Both predecessors are complete at time 3 (task 1 completed at 2, task 2 at 3), so task 3 runs on worker 0 at `[3,7)`. Makespan is `7`; total work is `9`; bottom levels are `6, 7, 4`; global critical path length is `7`. Utilization is `9/14`; throughput is `3/7`. One-worker FCFS runs tasks 1, 2, 3 consecutively and takes `9`, so speedup is `9/7`.

### Example 3: simultaneous completion and zero-duration join

`1(2) <- []`, `2(2) <- []`, `3(0) <- [1,2]`, two workers.

Tasks 1 and 2 run on workers 0 and 1 during `[0,2)`. At time 2 both events are collected before dependency release; task 3 then becomes ready and runs on worker 0 at `[2,2)`, completing immediately at time 2. Makespan is `2`; total work is `4`; utilization is `4/(2*2) = 1`; throughput is `3/2`; bottom levels are `2, 2, 0`; global critical path length is `2`. The one-worker baseline completes tasks 1 and 2 by time 4, then task 3 at time 4; speedup is `4/2 = 2`. The zero-duration event still removes a task from the unfinished count, demonstrating same-time progress.

## Test plan

Use small unit tests for graph construction/analysis, each ready policy, simulation, metrics, and any parser; add a few end-to-end workflows. Assert outputs and dependency invariants rather than private implementation details.

- Empty graph: analysis returns no tasks and critical path length 0; simulation has makespan 0; utilization and throughput are 0; speedup is absent when either comparison makespan is 0.
- Disconnected graph: verify each independent component is scheduled, and global critical path is the maximum component path.
- Invalid inputs: duplicate IDs, unknown dependency IDs, duplicate edges, negative duration, invalid task lookup, zero workers, and malformed parser values if a parser exists.
- Cycles: two-node cycle and self-dependency; `analyze` rejects before simulation.
- Topological correctness and bottom levels: check dependency order, task-specific values, and global longest-path value separately.
- Policy ties: FCFS initial and same-timestamp insertion order by ID while preserving FIFO; SJF equal durations by ID; Critical Path First equal bottom levels by ID.
- Completion batches: two or more events at one time; a successor requiring multiple tasks finishing in that batch is released once, only after all its predecessors finish.
- Zero durations: empty-at-time-zero workflow, zero task followed by zero task, zero task among positive tasks, and positive task releasing a zero-duration task; assert finite completion and exact timestamps.
- Workers: one worker and multiple workers, including lowest-worker-ID reuse when several workers become free together.
- Metrics: expected makespan, total work, utilization, throughput, optional speedup for the same policy, zero-makespan conventions, and invalid baseline rejection.
- Overflow: maximum representable duration/path/time plus one where appropriate; verify `std::overflow_error` and no wrapped result.

## Build and complexity documentation

Keep the build dependency-free. With CMake, the documented baseline is:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The simple single-command alternative in `AGENTS.md` is a starting point and must match the actual source layout before it is advertised as working. Compile as C++17 with warnings enabled (`-Wall -Wextra -Wpedantic`). Document purpose and actual time/space complexity next to each public algorithm. Report performance analytically unless real benchmarks are run and their environment and method are recorded.
