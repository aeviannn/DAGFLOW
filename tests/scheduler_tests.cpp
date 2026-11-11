#include "dagflow/scheduler.hpp"

#include <iostream>
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

dagflow::TaskIndex indexFor(const dagflow::WorkflowGraph& graph, dagflow::TaskId id) {
    return graph.indexOf(id);
}

void testFcfsInsertionOrder() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{3, 1, {}}, {1, 1, {}}, {2, 1, {}}});
    const auto analysis = dagflow::analyze(graph);
    auto ready = dagflow::makeReadyQueue(dagflow::SchedulingPolicy::FCFS, graph, analysis);
    ready->push(indexFor(graph, 2));
    ready->push(indexFor(graph, 1));
    ready->push(indexFor(graph, 3));
    check(graph.task(ready->pop()).id == 2, "FCFS selects earliest insertion");
    check(graph.task(ready->pop()).id == 1, "FCFS preserves second insertion");
    check(graph.task(ready->pop()).id == 3, "FCFS preserves third insertion");
}

void testSimultaneouslyReadyOrder() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{30, 1, {}}, {10, 1, {}}, {20, 1, {}}});
    const auto analysis = dagflow::analyze(graph);
    auto ready = dagflow::makeReadyQueue(dagflow::SchedulingPolicy::FCFS, graph, analysis);
    // The caller inserts a same-time ready batch in ascending ID order.
    ready->push(indexFor(graph, 10));
    ready->push(indexFor(graph, 20));
    ready->push(indexFor(graph, 30));
    check(graph.task(ready->pop()).id == 10, "simultaneously-ready FCFS batch starts by ID");
    check(graph.task(ready->pop()).id == 20, "simultaneously-ready FCFS batch preserves ID order");
    check(graph.task(ready->pop()).id == 30, "simultaneously-ready FCFS batch ends by ID");
}

void testSjfDurationAndTieBreak() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(
        {{40, 4, {}}, {30, 2, {}}, {10, 2, {}}, {20, 3, {}}});
    const auto analysis = dagflow::analyze(graph);
    auto ready = dagflow::makeReadyQueue(dagflow::SchedulingPolicy::SJF, graph, analysis);
    for (const auto id : {40u, 20u, 30u, 10u}) ready->push(indexFor(graph, id));
    check(graph.task(ready->pop()).id == 10, "SJF chooses shortest duration and smallest ID tie");
    check(graph.task(ready->pop()).id == 30, "SJF chooses next equal shortest duration");
    check(graph.task(ready->pop()).id == 20, "SJF chooses next duration");
    check(graph.task(ready->pop()).id == 40, "SJF chooses longest duration last");
}

void testCpfBottomLevelAndTieBreak() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(
        {{50, 1, {}}, {40, 1, {}}, {30, 1, {}}, {10, 1, {}}, {20, 1, {}}});
    const auto analysis = dagflow::analyze(graph);
    // Override the analysis values to isolate policy ordering? No: use actual DAG-derived levels.
    auto ready = dagflow::makeReadyQueue(
        dagflow::SchedulingPolicy::CriticalPathFirst, graph, analysis);
    for (const auto id : {50u, 40u, 30u, 10u, 20u}) ready->push(indexFor(graph, id));
    // All are sinks with equal duration, so equal bottom levels are resolved by ID.
    check(graph.task(ready->pop()).id == 10, "CPF equal bottom levels choose smallest ID");
    check(graph.task(ready->pop()).id == 20, "CPF retains ascending-ID tie order");
    check(graph.task(ready->pop()).id == 30, "CPF retains ascending-ID tie order");
    check(graph.task(ready->pop()).id == 40, "CPF retains ascending-ID tie order");
    check(graph.task(ready->pop()).id == 50, "CPF retains ascending-ID tie order");

    const auto weightedGraph = dagflow::WorkflowGraph::fromSpecs(
        {{1, 2, {}}, {2, 1, {}}, {3, 5, {1}}});
    const auto weightedAnalysis = dagflow::analyze(weightedGraph);
    auto weightedReady = dagflow::makeReadyQueue(
        dagflow::SchedulingPolicy::CriticalPathFirst, weightedGraph, weightedAnalysis);
    weightedReady->push(indexFor(weightedGraph, 1));
    weightedReady->push(indexFor(weightedGraph, 2));
    check(weightedGraph.task(weightedReady->pop()).id == 1,
          "CPF prioritizes bottom level over task duration or ID");
}

void testEmptyQueueBehavior() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, 1, {}}});
    const auto analysis = dagflow::analyze(graph);
    auto ready = dagflow::makeReadyQueue(dagflow::SchedulingPolicy::FCFS, graph, analysis);
    check(ready->empty(), "new ready queue is empty");
    try {
        (void)ready->pop();
        check(false, "pop on empty queue throws out_of_range");
    } catch (const std::out_of_range&) {
        check(true, "pop on empty queue throws out_of_range");
    }
}

void testTasksReadyAfterEarlierSelection() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(
        {{1, 1, {}}, {2, 1, {}}, {3, 1, {1}}, {4, 1, {2}}});
    const auto analysis = dagflow::analyze(graph);
    auto ready = dagflow::makeReadyQueue(dagflow::SchedulingPolicy::FCFS, graph, analysis);
    ready->push(indexFor(graph, 1));
    ready->push(indexFor(graph, 2));
    check(graph.task(ready->pop()).id == 1, "first initial task selected");
    ready->push(indexFor(graph, 3));
    check(graph.task(ready->pop()).id == 2, "waiting task remains ahead of later-ready task");
    ready->push(indexFor(graph, 4));
    check(graph.task(ready->pop()).id == 3, "first later-ready task follows prior queue order");
    check(graph.task(ready->pop()).id == 4, "second later-ready task follows first");
}

void testSingleReadyTaskAcrossPolicies() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{8, 7, {}}});
    const auto analysis = dagflow::analyze(graph);
    for (const auto policy : {dagflow::SchedulingPolicy::FCFS,
                              dagflow::SchedulingPolicy::SJF,
                              dagflow::SchedulingPolicy::CriticalPathFirst}) {
        auto ready = dagflow::makeReadyQueue(policy, graph, analysis);
        ready->push(indexFor(graph, 8));
        check(graph.task(ready->pop()).id == 8,
              "all policies select the only ready task");
    }
}

} // namespace

int main() {
    testFcfsInsertionOrder();
    testSimultaneouslyReadyOrder();
    testSjfDurationAndTieBreak();
    testCpfBottomLevelAndTieBreak();
    testEmptyQueueBehavior();
    testTasksReadyAfterEarlierSelection();
    testSingleReadyTaskAcrossPolicies();

    std::cout << passed << " scheduler checks passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
