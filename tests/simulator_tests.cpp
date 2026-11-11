#include "dagflow/simulator.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        ++passed;
    } else {
        ++failed;
        std::cerr << "FAIL: " << description << '\n';
    }
}

template <typename Exception, typename Function>
void checkThrows(Function&& action, const std::string& description) {
    try {
        action();
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

const dagflow::TaskExecution& executionFor(const dagflow::SimulationResult& result,
                                            dagflow::TaskId id) {
    const auto found = std::find_if(result.executions.begin(), result.executions.end(),
                                    [id](const dagflow::TaskExecution& execution) {
                                        return execution.taskId == id;
                                    });
    if (found == result.executions.end()) {
        throw std::logic_error("Test task execution was not recorded");
    }
    return *found;
}

void testEmptyWorkflow() {
    const auto result = simulate({}, dagflow::SchedulingPolicy::FCFS, 2);
    check(result.executions.empty(), "empty workflow has no execution records");
    check(result.makespan == 0, "empty workflow makespan is zero");
    check(result.completedTaskCount == 0, "empty workflow completes zero tasks");
    check(result.workerCount == 2 && result.policy == dagflow::SchedulingPolicy::FCFS,
          "empty result retains configuration");
}

void testRejectsZeroWorkers() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({});
    const auto analysis = dagflow::analyze(graph);
    checkThrows<std::invalid_argument>([&] {
        (void)dagflow::Simulator::run(graph, analysis, dagflow::SchedulingPolicy::FCFS, 0);
    }, "zero workers rejected for empty workflow");
}

void testSingleTask() {
    const auto result = simulate({{7, 5, {}}}, dagflow::SchedulingPolicy::FCFS, 1);
    const auto& task = executionFor(result, 7);
    check(task.workerId == 0 && task.startTime == 0 && task.completionTime == 5,
          "single task receives worker zero and exact interval");
    check(result.makespan == 5 && result.completedTaskCount == 1,
          "single task updates completion count and makespan");
}

void testDependencyChainAndInvariant() {
    const std::vector<dagflow::TaskSpec> specs{{1, 2, {}}, {2, 3, {1}}, {3, 1, {2}}};
    const auto result = simulate(specs, dagflow::SchedulingPolicy::FCFS, 3);
    check(executionFor(result, 1).startTime == 0 && executionFor(result, 1).completionTime == 2,
          "chain root runs first");
    check(executionFor(result, 2).startTime == 2 && executionFor(result, 2).completionTime == 5,
          "chain second task waits for root");
    check(executionFor(result, 3).startTime == 5 && executionFor(result, 3).completionTime == 6,
          "chain sink waits for predecessor");
    check(result.makespan == 6 && result.completedTaskCount == 3,
          "chain completes with expected makespan and count");

    const auto graph = dagflow::WorkflowGraph::fromSpecs(specs);
    for (const auto& spec : specs) {
        const auto& dependent = executionFor(result, spec.id);
        for (const auto predecessorId : spec.dependencies) {
            check(executionFor(result, predecessorId).completionTime <= dependent.startTime,
                  "dependency completion never follows dependent start");
        }
    }
    (void)graph;
}

void testIndependentTasksAndLowestWorkerIds() {
    const auto result = simulate({{3, 4, {}}, {1, 2, {}}, {2, 1, {}}},
                                 dagflow::SchedulingPolicy::FCFS, 2);
    check(executionFor(result, 1).workerId == 0, "lowest worker gets first ID-ordered task");
    check(executionFor(result, 2).workerId == 1, "next available worker gets next task");
    check(executionFor(result, 3).workerId == 1,
          "lowest available worker is reused after earliest completion");
    check(executionFor(result, 3).startTime == 1, "third task starts when worker becomes free");
    check(result.completedTaskCount == 3 && result.makespan == 5,
          "independent tasks complete with expected makespan");
}

void testMoreTasksThanWorkers() {
    const auto result = simulate({{1, 2, {}}, {2, 2, {}}, {3, 1, {}}},
                                 dagflow::SchedulingPolicy::FCFS, 1);
    check(executionFor(result, 1).startTime == 0, "one-worker schedule starts first task at zero");
    check(executionFor(result, 2).startTime == 2, "one-worker FCFS starts second task next");
    check(executionFor(result, 3).startTime == 4, "one-worker FCFS starts third task last");
    check(result.makespan == 5 && result.completedTaskCount == 3,
          "more tasks than workers are all completed");
}

void testSchedulingPolicies() {
    const std::vector<dagflow::TaskSpec> specs{{3, 5, {}}, {1, 3, {}}, {2, 1, {}}};
    const auto fcfs = simulate(specs, dagflow::SchedulingPolicy::FCFS, 1);
    const auto sjf = simulate(specs, dagflow::SchedulingPolicy::SJF, 1);
    const auto cpf = simulate(specs, dagflow::SchedulingPolicy::CriticalPathFirst, 1);
    check(fcfs.executions[0].taskId == 1 && fcfs.executions[1].taskId == 2,
          "FCFS starts independent tasks by ascending ID");
    check(sjf.executions[0].taskId == 2 && sjf.executions[1].taskId == 1,
          "SJF selects shorter duration first");
    check(cpf.executions[0].taskId == 3 && cpf.executions[1].taskId == 1,
          "CPF selects highest bottom level and breaks tie by ID");
}

void testSimultaneousCompletionsReleaseAllSuccessors() {
    const std::vector<dagflow::TaskSpec> specs{
        {1, 2, {}}, {2, 2, {}}, {3, 1, {1}}, {4, 1, {2}}, {5, 1, {1, 2}}};
    const auto result = simulate(specs, dagflow::SchedulingPolicy::FCFS, 3);
    check(executionFor(result, 1).completionTime == 2 &&
              executionFor(result, 2).completionTime == 2,
          "both root tasks complete at same timestamp");
    check(executionFor(result, 3).startTime == 2 && executionFor(result, 4).startTime == 2 &&
              executionFor(result, 5).startTime == 2,
          "all successors including two-parent join release after the batch");
    check(executionFor(result, 3).workerId == 0 && executionFor(result, 4).workerId == 1 &&
              executionFor(result, 5).workerId == 2,
          "newly-ready batch is task-ID ordered and workers use lowest IDs");
    check(result.completedTaskCount == 5 && result.makespan == 3,
          "simultaneous completion workflow fully completes");
}

void testZeroAndMixedDurations() {
    const auto allZero = simulate({{1, 0, {}}, {2, 0, {1}}, {3, 0, {2}}},
                                   dagflow::SchedulingPolicy::FCFS, 1);
    check(allZero.completedTaskCount == 3 && allZero.makespan == 0,
          "zero-duration dependency chain finishes at current time");
    check(executionFor(allZero, 1).completionTime == 0 &&
              executionFor(allZero, 2).startTime == 0 &&
              executionFor(allZero, 3).completionTime == 0,
          "zero-duration completions do not advance time");

    const auto mixed = simulate({{1, 0, {}}, {2, 3, {1}}, {3, 0, {2}}},
                                 dagflow::SchedulingPolicy::FCFS, 1);
    check(executionFor(mixed, 1).completionTime == 0 &&
              executionFor(mixed, 2).startTime == 0 &&
              executionFor(mixed, 3).startTime == 3 &&
              executionFor(mixed, 3).completionTime == 3,
          "mixed zero and positive durations use exact timestamps");
    check(mixed.completedTaskCount == 3 && mixed.makespan == 3,
          "mixed-duration chain completes without stalling");
}

void testCycleRejectionAndStallDetection() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, 1, {2}}, {2, 1, {1}}});
    checkThrows<dagflow::CycleError>([&] { (void)dagflow::analyze(graph); },
                                     "graph analysis rejects a cyclic graph");

    // A cycle has no valid GraphAnalysis. Use a shape-compatible deliberately forged result
    // to exercise the simulator's no-event/no-ready stalled-state guard.
    const dagflow::GraphAnalysis forged{{0, 1}, {1, 1}, 1};
    checkThrows<dagflow::CycleError>([&] {
        (void)dagflow::Simulator::run(graph, forged, dagflow::SchedulingPolicy::FCFS, 1);
    }, "stalled cyclic simulation throws instead of looping");
}

void testCompletionTimeOverflow() {
    const auto largest = std::numeric_limits<dagflow::Time>::max();
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, largest, {}}, {2, 1, {}}});
    const auto analysis = dagflow::analyze(graph);
    checkThrows<std::overflow_error>([&] {
        (void)dagflow::Simulator::run(graph, analysis, dagflow::SchedulingPolicy::FCFS, 1);
    }, "sequential completion time overflow is detected");
}

void testResultPolicyAndWorkerCount() {
    const auto result = simulate({{1, 1, {}}}, dagflow::SchedulingPolicy::SJF, 4);
    check(result.policy == dagflow::SchedulingPolicy::SJF && result.workerCount == 4,
          "result records selected policy and worker count");
}

} // namespace

int main() {
    testEmptyWorkflow();
    testRejectsZeroWorkers();
    testSingleTask();
    testDependencyChainAndInvariant();
    testIndependentTasksAndLowestWorkerIds();
    testMoreTasksThanWorkers();
    testSchedulingPolicies();
    testSimultaneousCompletionsReleaseAllSuccessors();
    testZeroAndMixedDurations();
    testCycleRejectionAndStallDetection();
    testCompletionTimeOverflow();
    testResultPolicyAndWorkerCount();

    std::cout << passed << " simulator checks passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
