#pragma once

#include "dagflow/task.hpp"

#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace dagflow {

class CycleError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct WorkflowTaskIdentity {
    TaskId id;
    Time duration;
    std::vector<TaskId> dependencies;

    bool operator==(const WorkflowTaskIdentity& other) const noexcept {
        return id == other.id && duration == other.duration &&
               dependencies == other.dependencies;
    }
};

struct WorkflowSignature {
    // Canonical task-ID order; each dependency list is also sorted by task ID.
    std::vector<WorkflowTaskIdentity> tasks;

    bool operator==(const WorkflowSignature& other) const noexcept {
        return tasks == other.tasks;
    }
    bool operator!=(const WorkflowSignature& other) const noexcept {
        return !(*this == other);
    }
};

class WorkflowGraph {
public:
    static WorkflowGraph fromSpecs(std::vector<TaskSpec> specs);

    std::size_t size() const noexcept;
    const Task& task(TaskIndex index) const;
    const std::vector<TaskIndex>& successors(TaskIndex index) const;
    std::size_t indegree(TaskIndex index) const;
    TaskIndex indexOf(TaskId id) const;
    WorkflowSignature signature() const;

private:
    WorkflowGraph(std::vector<Task> tasks,
                  std::vector<std::vector<TaskIndex>> successors,
                  std::vector<std::size_t> indegrees,
                  std::unordered_map<TaskId, TaskIndex> indexById);

    std::vector<Task> tasks_;
    std::vector<std::vector<TaskIndex>> successors_;
    std::vector<std::size_t> indegrees_;
    std::unordered_map<TaskId, TaskIndex> indexById_;
};

} // namespace dagflow
