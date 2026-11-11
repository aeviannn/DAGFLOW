#pragma once

#include "dagflow/scheduler.hpp"

#include <cstddef>
#include <vector>

namespace dagflow {

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
    // Borrows the immutable graph and its analysis for this call.
    // Time: O(W + (V + E) log V + V log W); extra space: O(V + W).
    static SimulationResult run(const WorkflowGraph& graph,
                                const GraphAnalysis& analysis,
                                SchedulingPolicy policy,
                                std::size_t workerCount);
};

} // namespace dagflow
