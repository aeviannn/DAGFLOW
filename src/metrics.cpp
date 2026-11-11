#include "dagflow/metrics.hpp"

#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace dagflow {
namespace {

using TaskDurations = std::unordered_map<TaskId, Time>;

TaskDurations validate(const SimulationResult& result) {
    if (result.workerCount == 0) {
        throw std::invalid_argument("Simulation result must have a positive worker count");
    }
    if (result.makespan < 0) {
        throw std::invalid_argument("Simulation makespan cannot be negative");
    }
    if (result.completedTaskCount != result.executions.size()) {
        throw std::invalid_argument("Completed task count does not match execution records");
    }

    TaskDurations durations;
    durations.reserve(result.executions.size());
    std::unordered_map<std::size_t, Time> lastFinishByWorker;
    lastFinishByWorker.reserve(result.executions.size());
    Time maximumCompletion = 0;

    for (const TaskExecution& execution : result.executions) {
        if (execution.workerId >= result.workerCount) {
            throw std::invalid_argument("Execution references a worker outside worker count");
        }
        if (execution.startTime < 0 || execution.completionTime < execution.startTime) {
            throw std::invalid_argument("Execution interval has invalid time values");
        }
        if (!durations.emplace(execution.taskId,
                               execution.completionTime - execution.startTime).second) {
            throw std::invalid_argument("Task appears more than once in execution records");
        }

        const auto previous = lastFinishByWorker.find(execution.workerId);
        if (previous != lastFinishByWorker.end() && execution.startTime < previous->second) {
            throw std::invalid_argument("Worker has overlapping task executions");
        }
        lastFinishByWorker[execution.workerId] = execution.completionTime;
        if (execution.completionTime > maximumCompletion) {
            maximumCompletion = execution.completionTime;
        }
    }

    if (maximumCompletion != result.makespan) {
        throw std::invalid_argument("Simulation makespan does not match task completion times");
    }
    return durations;
}

Time checkedBusyTimeAdd(Time total, Time duration) {
    if (duration > std::numeric_limits<Time>::max() - total) {
        throw std::overflow_error("Total task busy time exceeds supported Time range");
    }
    return total + duration;
}

} // namespace

PerformanceMetrics MetricsCalculator::calculate(const SimulationResult& result) {
    (void)validate(result);

    Time totalBusyTime = 0;
    for (const TaskExecution& execution : result.executions) {
        totalBusyTime = checkedBusyTimeAdd(
            totalBusyTime, execution.completionTime - execution.startTime);
    }

    if (result.makespan == 0) {
        return PerformanceMetrics{0, totalBusyTime, 0.0, 0.0, 0.0};
    }

    const long double denominator = static_cast<long double>(result.workerCount) *
                                    static_cast<long double>(result.makespan);
    const double utilization = static_cast<double>(
        static_cast<long double>(totalBusyTime) / denominator);
    const double throughput = static_cast<double>(
        static_cast<long double>(result.completedTaskCount) /
        static_cast<long double>(result.makespan));
    return PerformanceMetrics{result.makespan, totalBusyTime, utilization,
                              utilization * 100.0, throughput};
}

std::optional<double> MetricsCalculator::speedup(
    const SimulationResult& singleWorkerBaseline,
    const SimulationResult& multiWorkerResult) {
    const TaskDurations baselineTasks = validate(singleWorkerBaseline);
    const TaskDurations parallelTasks = validate(multiWorkerResult);

    if (singleWorkerBaseline.workerCount != 1 || multiWorkerResult.workerCount < 2) {
        throw std::invalid_argument("Speedup requires a one-worker baseline and a multi-worker run");
    }
    if (singleWorkerBaseline.policy != multiWorkerResult.policy) {
        throw std::invalid_argument("Speedup runs must use the same scheduling policy");
    }
    if (baselineTasks != parallelTasks) {
        throw std::invalid_argument("Speedup runs must contain the same task IDs and durations");
    }
    if (singleWorkerBaseline.makespan == 0 || multiWorkerResult.makespan == 0) {
        return std::nullopt;
    }

    return static_cast<double>(singleWorkerBaseline.makespan) /
           static_cast<double>(multiWorkerResult.makespan);
}

} // namespace dagflow
