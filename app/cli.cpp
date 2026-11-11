#include "cli.hpp"

#include "report.hpp"
#include "workflows.hpp"

#include "dagflow/graph_analysis.hpp"
#include "dagflow/metrics.hpp"
#include "dagflow/simulator.hpp"
#include "dagflow/workflow_graph.hpp"

#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dagflow::app {
namespace {

std::optional<long long> readInteger(std::istream& input,
                                     std::ostream& output,
                                     const std::string& prompt) {
    while (true) {
        output << prompt;
        std::string line;
        if (!std::getline(input, line)) return std::nullopt;
        std::istringstream parser(line);
        long long value = 0;
        std::string extra;
        if ((parser >> value) && !(parser >> extra)) return value;
        output << "Invalid input. Enter one whole number.\n";
    }
}

std::optional<int> readMenuChoice(std::istream& input,
                                  std::ostream& output,
                                  const std::string& prompt,
                                  int minimum,
                                  int maximum) {
    while (true) {
        const auto value = readInteger(input, output, prompt);
        if (!value) return std::nullopt;
        if (*value >= minimum && *value <= maximum) return static_cast<int>(*value);
        output << "Choice must be between " << minimum << " and " << maximum << ".\n";
    }
}

std::optional<std::size_t> readWorkerCount(std::istream& input, std::ostream& output) {
    while (true) {
        const auto value = readInteger(input, output, "Worker count (positive integer): ");
        if (!value) return std::nullopt;
        if (*value <= 0 || static_cast<unsigned long long>(*value) >
                               std::numeric_limits<std::size_t>::max()) {
            output << "Worker count must be positive and fit this system.\n";
            continue;
        }
        return static_cast<std::size_t>(*value);
    }
}

void printWorkflowMenu(std::ostream& output,
                       const std::vector<examples::WorkflowPreset>& workflows) {
    output << "DAGFlow workflow presets\n";
    for (std::size_t i = 0; i < workflows.size(); ++i) {
        output << "  " << (i + 1) << ". " << workflows[i].name << " ("
               << workflows[i].tasks.size() << " tasks)\n";
    }
}

SchedulingPolicy policyFromChoice(int choice) {
    switch (choice) {
    case 1: return SchedulingPolicy::FCFS;
    case 2: return SchedulingPolicy::SJF;
    case 3: return SchedulingPolicy::CriticalPathFirst;
    default: throw std::invalid_argument("Invalid scheduling policy choice");
    }
}

} // namespace

int runCli(std::istream& input, std::ostream& output, std::ostream& errors) {
    const auto workflows = examples::presets();
    printWorkflowMenu(output, workflows);
    const auto workflowChoice = readMenuChoice(input, output, "Select workflow: ",
                                                1, static_cast<int>(workflows.size()));
    if (!workflowChoice) {
        output << "Input ended. Exiting.\n";
        return 0;
    }
    const auto& workflow = workflows[static_cast<std::size_t>(*workflowChoice - 1)];

    const auto workerCount = readWorkerCount(input, output);
    if (!workerCount) {
        output << "Input ended. Exiting.\n";
        return 0;
    }

    output << "Run mode\n  1. Individual simulation\n  2. Compare all policies\n";
    const auto mode = readMenuChoice(input, output, "Select mode: ", 1, 2);
    if (!mode) {
        output << "Input ended. Exiting.\n";
        return 0;
    }

    std::optional<SchedulingPolicy> selectedPolicy;
    if (*mode == 1) {
        output << "Scheduling policy\n  1. FCFS\n  2. SJF\n  3. Critical Path First\n";
        const auto choice = readMenuChoice(input, output, "Select policy: ", 1, 3);
        if (!choice) {
            output << "Input ended. Exiting.\n";
            return 0;
        }
        selectedPolicy = policyFromChoice(*choice);
    }

    try {
        const WorkflowGraph graph = WorkflowGraph::fromSpecs(workflow.tasks);
        const GraphAnalysis analysis = analyze(graph);
        if (selectedPolicy) {
            const SimulationResult result = Simulator::run(
                graph, analysis, *selectedPolicy, *workerCount);
            const PerformanceMetrics metrics = MetricsCalculator::calculate(result);
            printIndividualReport(output, workflow, graph, analysis, result, metrics);
        } else {
            const std::vector<SchedulingPolicy> policies{
                SchedulingPolicy::FCFS,
                SchedulingPolicy::SJF,
                SchedulingPolicy::CriticalPathFirst};
            std::vector<PolicyReport> reports;
            reports.reserve(policies.size());
            for (const SchedulingPolicy policy : policies) {
                SimulationResult result = Simulator::run(graph, analysis, policy, *workerCount);
                PerformanceMetrics metrics = MetricsCalculator::calculate(result);
                std::optional<double> speedup;
                if (*workerCount > 1) {
                    const SimulationResult baseline = Simulator::run(graph, analysis, policy, 1);
                    speedup = MetricsCalculator::speedup(baseline, result);
                }
                reports.push_back(PolicyReport{policy, std::move(result), metrics, speedup});
            }
            printComparisonReport(output, workflow, reports);
        }
    } catch (const std::exception& error) {
        errors << "DAGFlow error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

} // namespace dagflow::app
