#include "command_line.hpp"
#include "json_writer.hpp"
#include "workflows.hpp"

#include <iostream>
#include <sstream>
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

struct Result {
    int status;
    std::string output;
    std::string errors;
};

Result run(std::initializer_list<const char*> arguments) {
    std::vector<const char*> argv(arguments);
    std::ostringstream output;
    std::ostringstream errors;
    const int status = dagflow::app::runCommandLine(
        static_cast<int>(argv.size()), argv.data(), output, errors);
    return {status, output.str(), errors.str()};
}

void testCatalog() {
    const Result result = run({"dagflow", "--list-workflows", "--json"});
    check(result.status == 0 && result.errors.empty(), "catalog command succeeds cleanly");
    const char* ids[] = {"empty", "chain", "fork-join", "simultaneous", "software-build-pipeline"};
    const char* names[] = {"Empty workflow", "Simple dependency chain", "Fork-and-join workflow",
                           "Simultaneous completion", "Software build pipeline"};
    const std::size_t taskCounts[] = {0, 3, 3, 3, 10};
    for (std::size_t i = 0; i < 5; ++i) {
        check(result.output.find(std::string("\"id\":\"") + ids[i] + "\"") != std::string::npos,
              "catalog includes stable workflow ID");
        check(result.output.find(std::string("\"name\":\"") + names[i] + "\"") != std::string::npos,
              "catalog retains display name");
        check(result.output.find(std::string("\"task_count\":") +
                                 std::to_string(taskCounts[i])) != std::string::npos,
              "catalog reports preset task count");
    }
}

void testSimulationPoliciesAndAnalysis() {
    const char* policies[] = {"fcfs", "sjf", "cpf"};
    for (const char* policy : policies) {
        const Result result = run({"dagflow", "--simulate", "--workflow", "fork-join",
                                   "--workers", "4", "--policy", policy, "--json"});
        check(result.status == 0 && result.errors.empty(), "policy simulation succeeds");
        check(result.output.find(std::string("\"policy\":\"") + policy + "\"") != std::string::npos,
              "simulation includes selected policy");
        check(result.output.find("\"worker_count\":4") != std::string::npos,
              "simulation includes worker count");
        check(result.output.find("\"topological_order\":[1,2,3]") != std::string::npos,
              "simulation includes deterministic topological order");
        check(result.output.find("\"critical_path_length\":7") != std::string::npos,
              "simulation includes global critical path length");
        check(result.output.find("\"critical_path_task_ids\":[2,3]") != std::string::npos,
              "critical path IDs include every task on the global critical path");
        check(result.output.find("\"dependencies\":[1,2]") != std::string::npos,
              "simulation includes dependency IDs");
        check(result.output.find("\"duration\":4") != std::string::npos &&
                  result.output.find("\"completion_time\"") != std::string::npos,
              "execution records include duration and completion time");
    }
}

void testEmptyWorkflowAndComparison() {
    const Result empty = run({"dagflow", "--simulate", "--workflow", "empty", "--workers", "2",
                              "--policy", "cpf", "--json"});
    check(empty.status == 0 && empty.output.find("\"tasks\":[]") != std::string::npos,
          "empty workflow serializes successfully");
    check(empty.output.find("\"speedup\":null") != std::string::npos,
          "undefined speedup is JSON null");

    const Result comparison = run({"dagflow", "--compare", "--workflow", "chain", "--workers", "4",
                                   "--json"});
    check(comparison.status == 0 && comparison.errors.empty(), "policy comparison succeeds cleanly");
    for (const char* policy : {"fcfs", "sjf", "cpf"}) {
        check(comparison.output.find(std::string("\"policy\":\"") + policy + "\"") !=
                  std::string::npos,
              "comparison includes each policy");
    }
    check(comparison.output.find("\"speedup\":1") != std::string::npos,
          "comparison includes single-worker baseline speedup");
}

void testSingleAndZeroDurationWorkflows() {
    const dagflow::examples::WorkflowPreset single{
        "single", "Single task", {{77, 5, {}}}};
    const dagflow::WorkflowGraph graph = dagflow::WorkflowGraph::fromSpecs(single.tasks);
    const dagflow::GraphAnalysis analysis = dagflow::analyze(graph);
    const dagflow::SimulationResult simulation = dagflow::Simulator::run(
        graph, analysis, dagflow::SchedulingPolicy::FCFS, 1);
    const dagflow::PerformanceMetrics metrics = dagflow::MetricsCalculator::calculate(simulation);
    std::ostringstream json;
    dagflow::app::writeSimulationJson(json, single, graph, analysis, simulation, metrics,
                                      std::nullopt);
    check(json.str().find("\"task_id\":77") != std::string::npos &&
              json.str().find("\"completion_time\":5") != std::string::npos,
          "single-task workflow is serialized from actual engine results");

    const Result zero = run({"dagflow", "--simulate", "--workflow", "simultaneous",
                             "--workers", "3", "--policy", "fcfs", "--json"});
    check(zero.status == 0 && zero.output.find("\"duration\":0") != std::string::npos,
          "zero-duration task is represented in JSON output");
}

void testArgumentErrorsAndEscaping() {
    const Result invalidPolicy = run({"dagflow", "--simulate", "--workflow", "chain", "--workers", "1",
                                      "--policy", "bad", "--json"});
    check(invalidPolicy.status != 0 && invalidPolicy.output.empty() &&
              invalidPolicy.errors.find("Unknown scheduling policy") != std::string::npos,
          "invalid policy reports only to stderr with nonzero exit");
    const Result invalidWorkflow = run({"dagflow", "--simulate", "--workflow", "missing",
                                        "--workers", "1", "--policy", "fcfs", "--json"});
    check(invalidWorkflow.status != 0 && invalidWorkflow.output.empty(),
          "unknown workflow is rejected cleanly");
    const Result zeroWorkers = run({"dagflow", "--simulate", "--workflow", "chain", "--workers", "0",
                                    "--policy", "fcfs", "--json"});
    check(zeroWorkers.status != 0 && zeroWorkers.output.empty(), "zero workers are rejected");
    const Result malformed = run({"dagflow", "--compare", "--workflow", "chain", "--workers", "2",
                                  "--policy", "fcfs", "--json"});
    check(malformed.status != 0 && malformed.output.empty(),
          "unsupported or incomplete argument combinations are rejected");

    std::ostringstream escaped;
    dagflow::app::writeJsonString(escaped, "quote\" slash\\ line\n tab\t control\x01");
    check(escaped.str() == "\"quote\\\" slash\\\\ line\\n tab\\t control\\u0001\"",
          "JSON string writer escapes quotes, slashes, whitespace, and control characters");
}

} // namespace

int main() {
    testCatalog();
    testSimulationPoliciesAndAnalysis();
    testEmptyWorkflowAndComparison();
    testSingleAndZeroDurationWorkflows();
    testArgumentErrorsAndEscaping();
    std::cout << "JSON CLI tests: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
