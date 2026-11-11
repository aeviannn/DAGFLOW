#pragma once

#include "dagflow/workflow_graph.hpp"

#include <vector>

namespace dagflow {

struct GraphAnalysis {
    // Task indices in deterministic Kahn order, with task ID as the ready-set tie-breaker.
    std::vector<TaskIndex> topologicalOrder;
    // Longest weighted path starting at each task; indexed by TaskIndex.
    std::vector<Time> bottomLevel;
    // Maximum bottom level over all tasks; zero for an empty graph.
    Time criticalPathLength;
};

// Runs deterministic Kahn sorting and reverse-order bottom-level dynamic programming.
// Time: O((V + E) log V); auxiliary space: O(V), excluding the returned vectors.
// Throws CycleError if the graph is cyclic and overflow_error if a path sum exceeds Time.
GraphAnalysis analyze(const WorkflowGraph& graph);

} // namespace dagflow
