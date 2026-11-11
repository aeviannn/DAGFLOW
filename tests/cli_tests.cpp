#include "cli.hpp"
#include "report.hpp"
#include "dagflow/simulator.hpp"

#include <iostream>
#include <limits>
#include <sstream>
#include <string>

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

struct CliOutput {
    int status;
    std::string output;
    std::string errors;
};

CliOutput run(const std::string& inputText) {
    std::istringstream input(inputText);
    std::ostringstream output;
    std::ostringstream errors;
    const int status = dagflow::app::runCli(input, output, errors);
    return {status, output.str(), errors.str()};
}

void testEveryPreset() {
    const auto presets = dagflow::examples::presets();
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const int selection = static_cast<int>(i + 1);
        const CliOutput result = run(std::to_string(selection) + "\n2\n1\n1\n");
        check(result.status == 0 && result.errors.empty(),
              "preset " + presets[i].name + " runs without error");
        check(result.output.find("Workflow: " + presets[i].name) != std::string::npos,
              "preset name is displayed");
        check(result.output.find("Worker timeline") != std::string::npos,
              "preset generates a Gantt chart");
    }
}

void testEveryPolicyAndWorkerCounts() {
    const char* expected[] = {"FCFS", "SJF", "Critical Path First"};
    for (int policy = 1; policy <= 3; ++policy) {
        const CliOutput result = run("2\n3\n1\n" + std::to_string(policy) + "\n");
        check(result.status == 0 && result.output.find(std::string("Policy: ") + expected[policy - 1])
                                     != std::string::npos,
              "individual policy selection works");
    }
    const CliOutput oneWorker = run("3\n1\n1\n2\n");
    const CliOutput manyWorkers = run("3\n4\n1\n2\n");
    check(oneWorker.status == 0 && manyWorkers.status == 0,
          "multiple positive worker counts are accepted");
}

void testInvalidSelectionsAndWorkerCount() {
    const CliOutput invalidMenu = run("99\n1\n1\n1\n1\n");
    check(invalidMenu.status == 0 &&
              invalidMenu.output.find("Choice must be between") != std::string::npos,
          "invalid menu selection is reported and reprompted");

    const CliOutput invalidWorkers = run("1\n0\n2\n1\n1\n");
    check(invalidWorkers.status == 0 &&
              invalidWorkers.output.find("Worker count must be positive") != std::string::npos,
          "zero worker count is reported and reprompted");

    const CliOutput eof = run("");
    check(eof.status == 0 && eof.output.find("Input ended") != std::string::npos,
          "EOF exits cleanly without an input loop");
}

void testPolicyComparison() {
    const CliOutput result = run("5\n3\n2\n");
    check(result.status == 0 && result.errors.empty(), "comparison mode completes");
    check(result.output.find("Policy comparison: Software build pipeline") != std::string::npos,
          "comparison names the selected workflow");
    check(result.output.find("Speedup") != std::string::npos &&
              result.output.find("Critical Path First") != std::string::npos,
          "comparison table includes all policies and speedup");
    check(result.output.find("Gantt chart") != std::string::npos,
          "comparison generates policy Gantt charts");

    const CliOutput oneWorker = run("2\n1\n2\n");
    check(oneWorker.output.find("N/A") != std::string::npos,
          "comparison displays N/A speedup for one selected worker");
}

void testLargeMakespanGanttIsBounded() {
    const auto large = std::numeric_limits<dagflow::Time>::max();
    const dagflow::SimulationResult result{
        {{7, 0, 0, large}}, large, 1, dagflow::SchedulingPolicy::FCFS, 1, {}};
    std::ostringstream output;
    dagflow::app::printGanttChart(output, result);
    check(output.str().size() < 2000, "large makespan chart remains a bounded width");
    check(output.str().find("T7 [0, 9223372036854775807)") != std::string::npos,
          "Gantt annotation preserves exact large task interval");
}

} // namespace

int main() {
    testEveryPreset();
    testEveryPolicyAndWorkerCounts();
    testInvalidSelectionsAndWorkerCount();
    testPolicyComparison();
    testLargeMakespanGanttIsBounded();
    std::cout << passed << " CLI checks passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
