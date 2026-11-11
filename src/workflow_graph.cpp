#include "dagflow/workflow_graph.hpp"

#include <string>
#include <unordered_set>
#include <utility>

namespace dagflow {

WorkflowGraph::WorkflowGraph(
    std::vector<Task> tasks,
    std::vector<std::vector<TaskIndex>> successors,
    std::vector<std::size_t> indegrees,
    std::unordered_map<TaskId, TaskIndex> indexById)
    : tasks_(std::move(tasks)),
      successors_(std::move(successors)),
      indegrees_(std::move(indegrees)),
      indexById_(std::move(indexById)) {}

WorkflowGraph WorkflowGraph::fromSpecs(std::vector<TaskSpec> specs) {
    std::vector<Task> tasks;
    tasks.reserve(specs.size());
    std::unordered_map<TaskId, TaskIndex> indexById;
    indexById.reserve(specs.size());

    for (const TaskSpec& spec : specs) {
        if (spec.duration < 0) {
            throw std::invalid_argument("Task duration cannot be negative");
        }
        const TaskIndex index = tasks.size();
        if (!indexById.emplace(spec.id, index).second) {
            throw std::invalid_argument("Duplicate task ID: " + std::to_string(spec.id));
        }
        tasks.push_back(Task{spec.id, spec.duration});
    }

    std::vector<std::vector<TaskIndex>> successors(tasks.size());
    std::vector<std::size_t> indegrees(tasks.size(), 0);

    for (TaskIndex dependent = 0; dependent < specs.size(); ++dependent) {
        std::unordered_set<TaskId> seenDependencies;
        seenDependencies.reserve(specs[dependent].dependencies.size());

        for (const TaskId dependencyId : specs[dependent].dependencies) {
            if (dependencyId == specs[dependent].id) {
                throw CycleError("Task cannot depend on itself: " +
                                 std::to_string(dependencyId));
            }
            if (!seenDependencies.emplace(dependencyId).second) {
                throw std::invalid_argument("Duplicate dependency " +
                                            std::to_string(dependencyId) +
                                            " for task " +
                                            std::to_string(specs[dependent].id));
            }
            const auto prerequisite = indexById.find(dependencyId);
            if (prerequisite == indexById.end()) {
                throw std::invalid_argument("Unknown dependency ID: " +
                                            std::to_string(dependencyId));
            }
            successors[prerequisite->second].push_back(dependent);
            ++indegrees[dependent];
        }
    }

    return WorkflowGraph(std::move(tasks), std::move(successors),
                         std::move(indegrees), std::move(indexById));
}

std::size_t WorkflowGraph::size() const noexcept {
    return tasks_.size();
}

const Task& WorkflowGraph::task(TaskIndex index) const {
    return tasks_.at(index);
}

const std::vector<TaskIndex>& WorkflowGraph::successors(TaskIndex index) const {
    return successors_.at(index);
}

std::size_t WorkflowGraph::indegree(TaskIndex index) const {
    return indegrees_.at(index);
}

TaskIndex WorkflowGraph::indexOf(TaskId id) const {
    const auto found = indexById_.find(id);
    if (found == indexById_.end()) {
        throw std::invalid_argument("Unknown task ID: " + std::to_string(id));
    }
    return found->second;
}

} // namespace dagflow
