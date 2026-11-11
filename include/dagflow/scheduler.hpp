#pragma once

#include "dagflow/graph_analysis.hpp"

#include <memory>
#include <vector>

namespace dagflow {

enum class SchedulingPolicy { FCFS, SJF, CriticalPathFirst };

// A policy-specific ready set. Inputs are task indices belonging to the borrowed graph.
// Calling pop on an empty queue throws std::out_of_range.
class ReadyQueue {
public:
    virtual ~ReadyQueue() = default;
    virtual void push(TaskIndex task) = 0;
    virtual TaskIndex pop() = 0;
    virtual bool empty() const noexcept = 0;
};

// Returns an owned policy queue. Implementations borrow graph and analysis; both must
// outlive the returned queue. For FCFS, callers push each simultaneously-ready batch
// in ascending task-ID order; FIFO order is preserved between batches.
// Push/pop costs: FCFS O(1), SJF/CPF O(log V). Storage: O(V) maximum queued tasks.
std::unique_ptr<ReadyQueue> makeReadyQueue(
    SchedulingPolicy policy,
    const WorkflowGraph& graph,
    const GraphAnalysis& analysis);

} // namespace dagflow
