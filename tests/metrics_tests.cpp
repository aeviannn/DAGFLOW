#include "dagflow/metrics.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& description) {
    if (condition) ++passed;
    else {
        ++failed;
        std::cerr << "FAIL: " << description << '\n';
    }
}

template <typename Exception, typename Function>
void checkThrows(Function&& function, const std::string& description) {
    try {
        function();
        check(false, description + " (no exception)");
    } catch (const Exception&) {
        check(true, description);
    } catch (...) {
        check(false, description + " (wrong exception type)");
    }
}

dagflow::SimulationResult simulate(const std::vector<dagflow::TaskSpec>& specs,
                                   dagflow::SchedulingPolicy policy,
                                   std::size_t workers) {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(specs);
    const auto analysis = dagflow::analyze(graph);
    return dagflow::Simulator::run(graph, analysis, policy, workers);
}

void testEmptyWorkflow() {
    const auto result = simulate({}, dagflow::SchedulingPolicy::FCFS, 2);
    const auto metrics = dagflow::MetricsCalculator::calculate(result);
    check(metrics.makespan == 0 && metrics.totalBusyTime == 0,
          "empty workflow has zero makespan and busy time");
    check(metrics.workerUtilization == 0.0 && metrics.utilizationPercentage == 0.0,
          "empty workflow utilization is zero");
    check(metrics.throughput == 0.0, "empty workflow throughput is zero");
}

void testSingleTask() {
    const auto result = simulate({{1, 5, {}}}, dagflow::SchedulingPolicy::FCFS, 1);
    const auto metrics = dagflow::MetricsCalculator::calculate(result);
    check(metrics.makespan == 5 && metrics.totalBusyTime == 5,
          "single task makespan and busy time equal duration");
    check(metrics.workerUtilization == 1.0 && metrics.utilizationPercentage == 100.0,
          "single worker is fully utilized");
    check(metrics.throughput == 0.2, "single task throughput is tasks per time unit");
}

void testIdleAndFullyUtilizedWorkers() {
    const auto idle = simulate({{1, 3, {}}}, dagflow::SchedulingPolicy::FCFS, 2);
    const auto idleMetrics = dagflow::MetricsCalculator::calculate(idle);
    check(idleMetrics.workerUtilization == 0.5,
          "one busy worker out of two has 50 percent utilization");

    const auto full = simulate({{1, 3, {}}, {2, 3, {}}},
                               dagflow::SchedulingPolicy::FCFS, 2);
    const auto fullMetrics = dagflow::MetricsCalculator::calculate(full);
    check(fullMetrics.totalBusyTime == 6 && fullMetrics.makespan == 3,
          "parallel task busy time sums across workers");
    check(fullMetrics.workerUtilization == 1.0 && fullMetrics.utilizationPercentage == 100.0,
          "both workers fully utilized");
}

void testThroughputAndZeroDuration() {
    const auto result = simulate({{1, 1, {}}, {2, 2, {}}},
                                 dagflow::SchedulingPolicy::FCFS, 1);
    const auto metrics = dagflow::MetricsCalculator::calculate(result);
    check(metrics.throughput == 2.0 / 3.0, "throughput uses completed tasks divided by makespan");

    const auto zero = simulate({{1, 0, {}}, {2, 0, {1}}},
                               dagflow::SchedulingPolicy::FCFS, 1);
    const auto zeroMetrics = dagflow::MetricsCalculator::calculate(zero);
    check(zeroMetrics.makespan == 0 && zeroMetrics.totalBusyTime == 0,
          "zero-duration tasks contribute no busy time");
    check(zeroMetrics.workerUtilization == 0 && zeroMetrics.throughput == 0,
          "zero makespan avoids division by zero");
}

void testSpeedup() {
    const std::vector<dagflow::TaskSpec> specs{{1, 2, {}}, {2, 2, {}}};
    const auto baseline = simulate(specs, dagflow::SchedulingPolicy::SJF, 1);
    const auto parallel = simulate(specs, dagflow::SchedulingPolicy::SJF, 2);
    const auto speedup = dagflow::MetricsCalculator::speedup(baseline, parallel);
    check(speedup.has_value() && *speedup == 2.0,
          "speedup is single-worker makespan divided by parallel makespan");

    const auto zeroBaseline = simulate({{1, 0, {}}}, dagflow::SchedulingPolicy::FCFS, 1);
    const auto zeroParallel = simulate({{1, 0, {}}}, dagflow::SchedulingPolicy::FCFS, 2);
    check(!dagflow::MetricsCalculator::speedup(zeroBaseline, zeroParallel).has_value(),
          "speedup is undefined when either makespan is zero");
}

void testBusyTimeOverflow() {
    const auto largest = std::numeric_limits<dagflow::Time>::max();
    const dagflow::SimulationResult result{
        {{1, 0, 0, largest}, {2, 1, 0, largest}},
        largest, 2, dagflow::SchedulingPolicy::FCFS, 2, {}};
    checkThrows<std::overflow_error>([&] {
        (void)dagflow::MetricsCalculator::calculate(result);
    }, "busy-time sum detects signed integer overflow");
}

void testInvalidResults() {
    dagflow::SimulationResult mismatch{{{1, 0, 0, 2}}, 2, 0,
                                       dagflow::SchedulingPolicy::FCFS, 1, {}};
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::MetricsCalculator::calculate(mismatch);
    }, "completed-count mismatch is rejected");

    dagflow::SimulationResult invalidWorker{{{1, 2, 0, 1}}, 1, 1,
                                            dagflow::SchedulingPolicy::FCFS, 1, {}};
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::MetricsCalculator::calculate(invalidWorker);
    }, "out-of-range worker ID is rejected");

    dagflow::SimulationResult overlap{{{1, 0, 0, 3}, {2, 0, 2, 4}}, 4, 2,
                                       dagflow::SchedulingPolicy::FCFS, 1, {}};
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::MetricsCalculator::calculate(overlap);
    }, "overlapping executions on one worker are rejected");

    dagflow::SimulationResult badMakespan{{{1, 0, 0, 3}}, 4, 1,
                                          dagflow::SchedulingPolicy::FCFS, 1, {}};
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::MetricsCalculator::calculate(badMakespan);
    }, "inconsistent makespan is rejected");
}

void testSpeedupValidation() {
    const auto baseline = simulate({{1, 2, {}}}, dagflow::SchedulingPolicy::FCFS, 1);
    const auto differentPolicy = simulate({{1, 2, {}}}, dagflow::SchedulingPolicy::SJF, 2);
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::MetricsCalculator::speedup(baseline, differentPolicy);
    }, "speedup rejects mismatched policies");

    const auto differentTasks = simulate({{2, 2, {}}}, dagflow::SchedulingPolicy::FCFS, 2);
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::MetricsCalculator::speedup(baseline, differentTasks);
    }, "speedup rejects mismatched task sets");

    const auto differentDurations = simulate({{1, 3, {}}}, dagflow::SchedulingPolicy::FCFS, 2);
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::MetricsCalculator::speedup(baseline, differentDurations);
    }, "speedup rejects different task durations");

    const std::vector<dagflow::TaskSpec> noEdges{{1, 1, {}}, {2, 1, {}}};
    const std::vector<dagflow::TaskSpec> withEdge{{1, 1, {}}, {2, 1, {1}}};
    const auto independent = simulate(noEdges, dagflow::SchedulingPolicy::FCFS, 1);
    const auto dependent = simulate(withEdge, dagflow::SchedulingPolicy::FCFS, 2);
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::MetricsCalculator::speedup(independent, dependent);
    }, "speedup rejects different dependency edges");

    const auto emptyBaseline = simulate({}, dagflow::SchedulingPolicy::FCFS, 1);
    const auto emptyMulti = simulate({}, dagflow::SchedulingPolicy::FCFS, 2);
    check(!dagflow::MetricsCalculator::speedup(emptyBaseline, emptyMulti).has_value(),
          "identical empty workflows with different worker counts are safely compared");
}

} // namespace

int main() {
    testEmptyWorkflow();
    testSingleTask();
    testIdleAndFullyUtilizedWorkers();
    testThroughputAndZeroDuration();
    testSpeedup();
    testBusyTimeOverflow();
    testInvalidResults();
    testSpeedupValidation();
    std::cout << passed << " metrics checks passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
