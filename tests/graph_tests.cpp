#include "dagflow/workflow_graph.hpp"
#include "dagflow/graph_analysis.hpp"
#include "workflows.hpp"

#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

template <typename Exception, typename Function>
void checkThrows(Function&& function, const std::string& message) {
    try {
        function();
        check(false, message + " (no exception)");
    } catch (const Exception&) {
        check(true, message);
    } catch (...) {
        check(false, message + " (wrong exception type)");
    }
}

void testEmptyGraph() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({});
    check(graph.size() == 0, "empty workflow has no tasks");
}

void testGraphConstruction() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(dagflow::examples::forkAndJoin());
    check(graph.size() == 3, "fork/join example has three tasks");
    check(graph.task(graph.indexOf(3)).duration == 4, "task lookup returns duration");
    check(graph.indegree(graph.indexOf(3)) == 2, "join task has two prerequisites");
    check(graph.successors(graph.indexOf(1)).size() == 1,
          "first fork task has one successor");
    check(graph.successors(graph.indexOf(2)).size() == 1,
          "second fork task has one successor");
}

void testDisconnectedWorkflowAndIdMapping() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(
        {{30, 5, {10}}, {99, 0, {}}, {10, 2, {}}, {70, 1, {}}});
    check(graph.size() == 4, "disconnected workflow has four tasks");
    check(graph.indexOf(30) != graph.indexOf(10), "distinct IDs map to distinct indices");
    check(graph.task(graph.indexOf(30)).id == 30, "ID 30 maps back to its task");
    check(graph.task(graph.indexOf(99)).id == 99, "ID 99 maps back to its task");
    check(graph.task(graph.indexOf(10)).id == 10, "ID 10 maps back to its task");
    check(graph.task(graph.indexOf(70)).id == 70, "ID 70 maps back to its task");
    check(graph.indegree(graph.indexOf(10)) == 0, "independent task has zero indegree");
    check(graph.indegree(graph.indexOf(30)) == 1, "dependent task has correct indegree");
    check(graph.successors(graph.indexOf(10)).size() == 1,
          "disconnected component edge is stored");
}

void testPredefinedExamplesBuild() {
    const auto chain = dagflow::WorkflowGraph::fromSpecs(dagflow::examples::chain());
    const auto simultaneous = dagflow::WorkflowGraph::fromSpecs(
        dagflow::examples::simultaneousAndZeroDuration());
    check(chain.size() == 3, "in-memory chain example builds");
    check(simultaneous.task(simultaneous.indexOf(3)).duration == 0,
          "zero-duration example builds");
}

void testDuplicateTaskId() {
    checkThrows<std::invalid_argument>([] {
        dagflow::WorkflowGraph::fromSpecs({{7, 1, {}}, {7, 2, {}}});
    }, "duplicate task IDs are rejected");
}

void testUnknownDependency() {
    checkThrows<std::invalid_argument>([] {
        dagflow::WorkflowGraph::fromSpecs({{1, 1, {99}}});
    }, "unknown dependency IDs are rejected");
}

void testDuplicateDependencyEdge() {
    checkThrows<std::invalid_argument>([] {
        dagflow::WorkflowGraph::fromSpecs({{1, 1, {}}, {2, 1, {1, 1}}});
    }, "duplicate dependency edges are rejected");
}

void testSelfDependency() {
    checkThrows<dagflow::CycleError>([] {
        dagflow::WorkflowGraph::fromSpecs({{1, 1, {1}}});
    }, "self-dependency is reported as a cycle");
}

void testNegativeDuration() {
    checkThrows<std::invalid_argument>([] {
        dagflow::WorkflowGraph::fromSpecs({{1, -1, {}}});
    }, "negative duration is rejected");
}

void testMissingTaskLookup() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, 1, {}}});
    checkThrows<std::invalid_argument>([&graph] { graph.indexOf(42); },
                                       "unknown task lookup is rejected");
}

void testIndexBounds() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, 1, {}}});
    checkThrows<std::out_of_range>([&graph] { graph.task(graph.size()); },
                                   "task access checks index bounds");
    checkThrows<std::out_of_range>([&graph] { graph.successors(graph.size()); },
                                   "successor access checks index bounds");
    checkThrows<std::out_of_range>([&graph] { graph.indegree(graph.size()); },
                                   "indegree access checks index bounds");
}

void testFailedConstructionDoesNotAffectExistingGraph() {
    const auto valid = dagflow::WorkflowGraph::fromSpecs({{5, 2, {}}});
    checkThrows<std::invalid_argument>([] {
        dagflow::WorkflowGraph::fromSpecs({{1, 1, {}}, {1, 2, {}}});
    }, "invalid construction throws without returning a graph");
    check(valid.size() == 1 && valid.task(valid.indexOf(5)).duration == 2,
          "previously built graph remains usable after failed construction");
}

std::vector<dagflow::TaskId> taskIdsIn(const dagflow::WorkflowGraph& graph,
                                       const std::vector<dagflow::TaskIndex>& order) {
    std::vector<dagflow::TaskId> ids;
    for (const auto index : order) {
        ids.push_back(graph.task(index).id);
    }
    return ids;
}

void testAnalysisEmptyGraph() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({});
    const auto analysis = dagflow::analyze(graph);
    check(analysis.topologicalOrder.empty(), "empty graph has empty topological order");
    check(analysis.bottomLevel.empty(), "empty graph has no bottom levels");
    check(analysis.criticalPathLength == 0, "empty graph critical path is zero");
}

void testAnalysisChain() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(dagflow::examples::chain());
    const auto analysis = dagflow::analyze(graph);
    check(taskIdsIn(graph, analysis.topologicalOrder) ==
              std::vector<dagflow::TaskId>({1, 2, 3}),
          "chain has deterministic topological order");
    check(analysis.bottomLevel[graph.indexOf(1)] == 6, "chain root bottom level is 6");
    check(analysis.bottomLevel[graph.indexOf(2)] == 4, "chain middle bottom level is 4");
    check(analysis.bottomLevel[graph.indexOf(3)] == 1, "chain sink bottom level is 1");
    check(analysis.criticalPathLength == 6, "chain critical path is 6");
}

void testAnalysisForkJoin() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(dagflow::examples::forkAndJoin());
    const auto analysis = dagflow::analyze(graph);
    const auto order = taskIdsIn(graph, analysis.topologicalOrder);
    check(order == std::vector<dagflow::TaskId>({1, 2, 3}),
          "fork/join topological order respects dependencies");
    check(analysis.bottomLevel[graph.indexOf(1)] == 6, "fork left branch bottom level is 6");
    check(analysis.bottomLevel[graph.indexOf(2)] == 7, "fork right branch bottom level is 7");
    check(analysis.bottomLevel[graph.indexOf(3)] == 4, "join bottom level is its duration");
    check(analysis.criticalPathLength == 7, "fork/join global path is maximum bottom level");
}

void testAnalysisDisconnected() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(
        {{40, 1, {}}, {20, 3, {10}}, {10, 2, {}}, {99, 0, {}}});
    const auto analysis = dagflow::analyze(graph);
    const auto ids = taskIdsIn(graph, analysis.topologicalOrder);
    check(ids == std::vector<dagflow::TaskId>({10, 20, 40, 99}),
          "disconnected graph uses smallest available task ID");
    check(analysis.bottomLevel[graph.indexOf(10)] == 5,
          "longer disconnected component has bottom level 5");
    check(analysis.bottomLevel[graph.indexOf(40)] == 1,
          "shorter disconnected component has bottom level 1");
    check(analysis.criticalPathLength == 5,
          "global path is maximum over disconnected components");
}

void testAnalysisMultipleValidOrdersAndTieBreaking() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs(
        {{30, 2, {}}, {10, 2, {}}, {20, 2, {}}});
    const auto analysis = dagflow::analyze(graph);
    check(taskIdsIn(graph, analysis.topologicalOrder) ==
              std::vector<dagflow::TaskId>({10, 20, 30}),
          "equal-duration independent tasks break ties by ID");
}

void testAnalysisCycles() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, 1, {2}}, {2, 1, {1}}});
    try {
        (void)dagflow::analyze(graph);
        check(false, "multi-task cycle throws CycleError");
    } catch (const dagflow::CycleError& error) {
        check(std::string(error.what()).find("cycle") != std::string::npos,
              "cycle error message clearly identifies cycle");
    } catch (...) {
        check(false, "multi-task cycle throws the documented exception type");
    }
}

void testAnalysisZeroDurations() {
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, 0, {}}, {2, 0, {1}}});
    const auto analysis = dagflow::analyze(graph);
    check(analysis.bottomLevel[graph.indexOf(1)] == 0,
          "zero-duration chain root bottom level is zero");
    check(analysis.bottomLevel[graph.indexOf(2)] == 0,
          "zero-duration chain sink bottom level is zero");
    check(analysis.criticalPathLength == 0, "all-zero workflow path length is zero");
}

void testAnalysisLargeDuration() {
    const auto largest = std::numeric_limits<dagflow::Time>::max();
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, largest, {}}});
    const auto analysis = dagflow::analyze(graph);
    check(analysis.bottomLevel[graph.indexOf(1)] == largest,
          "maximum representable task duration is handled without overflow");
    check(analysis.criticalPathLength == largest,
          "maximum representable critical path is retained");
}

void testAnalysisCheckedOverflow() {
    const auto largest = std::numeric_limits<dagflow::Time>::max();
    const auto graph = dagflow::WorkflowGraph::fromSpecs({{1, largest, {}}, {2, 1, {1}}});
    checkThrows<std::overflow_error>([&graph] { (void)dagflow::analyze(graph); },
                                     "bottom-level addition detects overflow");
}

} // namespace

int main() {
    testEmptyGraph();
    testGraphConstruction();
    testDisconnectedWorkflowAndIdMapping();
    testPredefinedExamplesBuild();
    testDuplicateTaskId();
    testUnknownDependency();
    testDuplicateDependencyEdge();
    testSelfDependency();
    testNegativeDuration();
    testMissingTaskLookup();
    testIndexBounds();
    testFailedConstructionDoesNotAffectExistingGraph();
    testAnalysisEmptyGraph();
    testAnalysisChain();
    testAnalysisForkJoin();
    testAnalysisDisconnected();
    testAnalysisMultipleValidOrdersAndTieBreaking();
    testAnalysisCycles();
    testAnalysisZeroDurations();
    testAnalysisLargeDuration();
    testAnalysisCheckedOverflow();

    if (failures != 0) {
        std::cerr << checks - failures << " passed, " << failures
                  << " failed out of " << checks << " graph checks\n";
        return 1;
    }
    std::cout << checks << " graph checks passed, 0 failed\n";
    return 0;
}
