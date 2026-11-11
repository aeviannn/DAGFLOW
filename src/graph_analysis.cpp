#include "dagflow/graph_analysis.hpp"

#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace dagflow {
namespace {

Time checkedAdd(Time left, Time right) {
    if (right > 0 && left > std::numeric_limits<Time>::max() - right) {
        throw std::overflow_error("Bottom-level duration exceeds supported Time range");
    }
    return left + right;
}

} // namespace

GraphAnalysis analyze(const WorkflowGraph& graph) {
    const std::size_t taskCount = graph.size();
    std::vector<std::size_t> remainingIndegrees(taskCount);
    using ReadyEntry = std::pair<TaskId, TaskIndex>;
    std::priority_queue<ReadyEntry, std::vector<ReadyEntry>, std::greater<ReadyEntry>> ready;

    for (TaskIndex index = 0; index < taskCount; ++index) {
        remainingIndegrees[index] = graph.indegree(index);
        if (remainingIndegrees[index] == 0) {
            ready.emplace(graph.task(index).id, index);
        }
    }

    GraphAnalysis result;
    result.topologicalOrder.reserve(taskCount);
    result.bottomLevel.assign(taskCount, 0);

    while (!ready.empty()) {
        const TaskIndex current = ready.top().second;
        ready.pop();
        result.topologicalOrder.push_back(current);

        for (const TaskIndex successor : graph.successors(current)) {
            if (remainingIndegrees[successor] == 0) {
                throw std::logic_error("Graph indegree data is inconsistent");
            }
            --remainingIndegrees[successor];
            if (remainingIndegrees[successor] == 0) {
                ready.emplace(graph.task(successor).id, successor);
            }
        }
    }

    if (result.topologicalOrder.size() != taskCount) {
        throw CycleError("Workflow contains a dependency cycle");
    }

    for (auto position = result.topologicalOrder.rbegin();
         position != result.topologicalOrder.rend(); ++position) {
        const TaskIndex current = *position;
        Time longestSuccessorLevel = 0;
        for (const TaskIndex successor : graph.successors(current)) {
            if (result.bottomLevel[successor] > longestSuccessorLevel) {
                longestSuccessorLevel = result.bottomLevel[successor];
            }
        }
        result.bottomLevel[current] = checkedAdd(
            graph.task(current).duration, longestSuccessorLevel);
    }

    result.criticalPathLength = 0;
    for (const Time level : result.bottomLevel) {
        if (level > result.criticalPathLength) {
            result.criticalPathLength = level;
        }
    }
    return result;
}

} // namespace dagflow
