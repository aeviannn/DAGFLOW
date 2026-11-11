#pragma once

#include "dagflow/simulator.hpp"

#include <optional>

namespace dagflow {

struct PerformanceMetrics {
    Time makespan;                 // time units, copied from the simulation
    Time totalBusyTime;            // sum of task execution durations, in time units
    double workerUtilization;      // ratio in [0, 1] for a valid simulation
    double utilizationPercentage;  // workerUtilization * 100
    double throughput;             // completed tasks per time unit
};

class MetricsCalculator {
public:
    // Validates the simulation record structure before calculating metrics.
    // Time: O(V); auxiliary space: O(W) for last-finish validation.
    static PerformanceMetrics calculate(const SimulationResult& result);

    // Returns no value when either makespan is zero. The runs must use the same
    // workflow and policy; observable task IDs/durations, policy and worker counts
    // are checked. Dependency-graph provenance remains the caller's responsibility.
    static std::optional<double> speedup(const SimulationResult& singleWorkerBaseline,
                                        const SimulationResult& multiWorkerResult);
};

} // namespace dagflow
