#include "dagflow/simulator.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace dagflow {
namespace {

enum class TaskState { Pending, Ready, Running, Completed };

struct CompletionEvent {
    Time completionTime;
    std::size_t workerId;
    TaskId taskId;
    TaskIndex taskIndex;
};

struct EventLater {
    bool operator()(const CompletionEvent& left, const CompletionEvent& right) const noexcept {
        return std::tie(left.completionTime, left.workerId, left.taskId) >
               std::tie(right.completionTime, right.workerId, right.taskId);
    }
};

Time checkedFinishTime(Time currentTime, Time duration) {
    if (duration < 0 || currentTime < 0 ||
        currentTime > std::numeric_limits<Time>::max() - duration) {
        throw std::overflow_error("Task completion time exceeds supported Time range");
    }
    return currentTime + duration;
}

} // namespace

SimulationResult Simulator::run(const WorkflowGraph& graph,
                                const GraphAnalysis& analysis,
                                SchedulingPolicy policy,
                                std::size_t workerCount) {
    if (workerCount == 0) {
        throw std::invalid_argument("Worker count must be positive");
    }
    if (analysis.topologicalOrder.size() != graph.size() ||
        analysis.bottomLevel.size() != graph.size()) {
        throw std::invalid_argument("Graph analysis does not match workflow size");
    }

    auto readyQueue = makeReadyQueue(policy, graph, analysis);
    SimulationResult result{{}, 0, 0, policy, workerCount};
    result.executions.reserve(graph.size());

    const std::size_t taskCount = graph.size();
    std::vector<TaskState> states(taskCount, TaskState::Pending);
    std::vector<std::size_t> remainingDependencies(taskCount);
    std::vector<std::optional<TaskIndex>> runningOnWorker(workerCount);
    std::vector<std::size_t> workerIds(workerCount);
    std::iota(workerIds.begin(), workerIds.end(), 0);
    std::priority_queue<std::size_t,
                        std::vector<std::size_t>,
                        std::greater<std::size_t>> freeWorkers(
                            std::greater<std::size_t>{}, std::move(workerIds));
    std::priority_queue<CompletionEvent,
                        std::vector<CompletionEvent>,
                        EventLater> completionEvents;

    std::vector<TaskIndex> initialReady;
    initialReady.reserve(taskCount);
    for (TaskIndex task = 0; task < taskCount; ++task) {
        remainingDependencies[task] = graph.indegree(task);
        if (remainingDependencies[task] == 0) {
            initialReady.push_back(task);
        }
    }
    std::sort(initialReady.begin(), initialReady.end(), [&graph](TaskIndex left, TaskIndex right) {
        return graph.task(left).id < graph.task(right).id;
    });
    for (const TaskIndex task : initialReady) {
        states[task] = TaskState::Ready;
        readyQueue->push(task);
    }

    Time currentTime = 0;
    while (result.completedTaskCount < taskCount) {
        while (!freeWorkers.empty() && !readyQueue->empty()) {
            const std::size_t worker = freeWorkers.top();
            freeWorkers.pop();
            const TaskIndex task = readyQueue->pop();
            if (states[task] != TaskState::Ready) {
                throw std::logic_error("Ready queue contains a task that is not ready");
            }

            const Time finish = checkedFinishTime(currentTime, graph.task(task).duration);
            states[task] = TaskState::Running;
            runningOnWorker[worker] = task;
            result.executions.push_back(
                TaskExecution{graph.task(task).id, worker, currentTime, finish});
            completionEvents.push(CompletionEvent{finish, worker, graph.task(task).id, task});
        }

        if (result.completedTaskCount == taskCount) {
            break;
        }
        if (completionEvents.empty()) {
            if (readyQueue->empty()) {
                throw CycleError("Workflow is cyclic or has tasks that cannot become ready");
            }
            throw std::logic_error("Simulation stalled with ready tasks but no available workers or events");
        }

        currentTime = completionEvents.top().completionTime;
        std::vector<CompletionEvent> completedAtTime;
        while (!completionEvents.empty() &&
               completionEvents.top().completionTime == currentTime) {
            completedAtTime.push_back(completionEvents.top());
            completionEvents.pop();
        }

        // Complete and free every worker at this timestamp before releasing successors.
        for (const CompletionEvent& event : completedAtTime) {
            if (states[event.taskIndex] != TaskState::Running ||
                !runningOnWorker[event.workerId] ||
                *runningOnWorker[event.workerId] != event.taskIndex) {
                throw std::logic_error("Completion event does not match running task state");
            }
            states[event.taskIndex] = TaskState::Completed;
            runningOnWorker[event.workerId].reset();
            freeWorkers.push(event.workerId);
            ++result.completedTaskCount;
        }

        std::vector<TaskIndex> newlyReady;
        for (const CompletionEvent& event : completedAtTime) {
            for (const TaskIndex successor : graph.successors(event.taskIndex)) {
                if (remainingDependencies[successor] == 0) {
                    throw std::logic_error("Dependency count underflow during simulation");
                }
                --remainingDependencies[successor];
                if (remainingDependencies[successor] == 0) {
                    if (states[successor] != TaskState::Pending) {
                        throw std::logic_error("Task became ready more than once");
                    }
                    states[successor] = TaskState::Ready;
                    newlyReady.push_back(successor);
                }
            }
        }
        std::sort(newlyReady.begin(), newlyReady.end(), [&graph](TaskIndex left, TaskIndex right) {
            return graph.task(left).id < graph.task(right).id;
        });
        for (const TaskIndex task : newlyReady) {
            readyQueue->push(task);
        }
    }

    result.makespan = currentTime;
    return result;
}

} // namespace dagflow
