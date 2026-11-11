#include "command_line.hpp"

#include "json_writer.hpp"
#include "workflows.hpp"

#include "dagflow/graph_analysis.hpp"
#include "dagflow/metrics.hpp"
#include "dagflow/simulator.hpp"
#include "dagflow/workflow_graph.hpp"

#include <charconv>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dagflow::app {
namespace {

enum class Mode { List, Simulate, Compare };

struct Options {
    Mode mode;
    std::string workflow;
    std::size_t workers = 0;
    SchedulingPolicy policy = SchedulingPolicy::FCFS;
};

std::size_t parseWorkerCount(const std::string& value) {
    unsigned long long parsed = 0;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed == 0 ||
        parsed > static_cast<unsigned long long>(static_cast<std::size_t>(-1))) {
        throw std::invalid_argument("--workers must be a positive integer that fits this system");
    }
    return static_cast<std::size_t>(parsed);
}

SchedulingPolicy parsePolicy(const std::string& value) {
    if (value == "fcfs") return SchedulingPolicy::FCFS;
    if (value == "sjf") return SchedulingPolicy::SJF;
    if (value == "cpf") return SchedulingPolicy::CriticalPathFirst;
    throw std::invalid_argument("Unknown scheduling policy: " + value);
}

Options parseOptions(int argc, const char* const argv[]) {
    if (argc < 2) throw std::invalid_argument("Missing machine command");
    const std::string command = argv[1];
    if (command == "--list-workflows") {
        if (argc != 3 || std::string(argv[2]) != "--json") {
            throw std::invalid_argument("Usage: dagflow --list-workflows --json");
        }
        return Options{Mode::List, {}, 0, SchedulingPolicy::FCFS};
    }

    Mode mode;
    if (command == "--simulate") mode = Mode::Simulate;
    else if (command == "--compare") mode = Mode::Compare;
    else throw std::invalid_argument("Unknown command: " + command);

    std::string workflow;
    std::string policyValue;
    std::string workerValue;
    bool haveWorkflow = false;
    bool havePolicy = false;
    bool haveWorkers = false;
    bool haveJson = false;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--json") {
            if (haveJson) throw std::invalid_argument("Duplicate --json argument");
            haveJson = true;
            continue;
        }
        if (argument != "--workflow" && argument != "--workers" && argument != "--policy") {
            throw std::invalid_argument("Unknown argument: " + argument);
        }
        if (index + 1 >= argc) throw std::invalid_argument("Missing value for " + argument);
        const std::string value = argv[++index];
        if (argument == "--workflow") {
            if (haveWorkflow) throw std::invalid_argument("Duplicate --workflow argument");
            workflow = value;
            haveWorkflow = true;
        } else if (argument == "--workers") {
            if (haveWorkers) throw std::invalid_argument("Duplicate --workers argument");
            workerValue = value;
            haveWorkers = true;
        } else {
            if (havePolicy) throw std::invalid_argument("Duplicate --policy argument");
            policyValue = value;
            havePolicy = true;
        }
    }
    if (!haveJson || !haveWorkflow || !haveWorkers) {
        throw std::invalid_argument("Command requires --workflow, --workers, and --json");
    }
    if ((mode == Mode::Simulate) != havePolicy) {
        throw std::invalid_argument(mode == Mode::Simulate
            ? "Simulation requires --policy"
            : "Comparison does not accept --policy");
    }
    Options options{mode, {}, 0, SchedulingPolicy::FCFS};
    options.workflow = workflow;
    options.workers = parseWorkerCount(workerValue);
    if (havePolicy) options.policy = parsePolicy(policyValue);
    return options;
}

const examples::WorkflowPreset& findWorkflow(
    const std::vector<examples::WorkflowPreset>& workflows, const std::string& id) {
    for (const auto& workflow : workflows) {
        if (workflow.id == id) return workflow;
    }
    throw std::invalid_argument("Unknown workflow: " + id);
}

std::optional<double> calculateSpeedup(const WorkflowGraph& graph,
                                       const GraphAnalysis& analysis,
                                       SchedulingPolicy policy,
                                       std::size_t workers,
                                       const SimulationResult& result) {
    if (workers < 2) return std::nullopt;
    const SimulationResult baseline = Simulator::run(graph, analysis, policy, 1);
    return MetricsCalculator::speedup(baseline, result);
}

int execute(const Options& options, std::ostream& output) {
    const auto workflows = examples::presets();
    if (options.mode == Mode::List) {
        writeWorkflowCatalogJson(output, workflows);
        output << '\n';
        return 0;
    }

    const auto& workflow = findWorkflow(workflows, options.workflow);
    const WorkflowGraph graph = WorkflowGraph::fromSpecs(workflow.tasks);
    const GraphAnalysis analysis = analyze(graph);
    if (options.mode == Mode::Simulate) {
        const SimulationResult result = Simulator::run(
            graph, analysis, options.policy, options.workers);
        const PerformanceMetrics metrics = MetricsCalculator::calculate(result);
        const auto speedup = calculateSpeedup(
            graph, analysis, options.policy, options.workers, result);
        writeSimulationJson(output, workflow, graph, analysis, result, metrics, speedup);
        output << '\n';
        return 0;
    }

    const std::vector<SchedulingPolicy> policies{
        SchedulingPolicy::FCFS,
        SchedulingPolicy::SJF,
        SchedulingPolicy::CriticalPathFirst};
    std::vector<PolicyReport> reports;
    reports.reserve(policies.size());
    for (const SchedulingPolicy policy : policies) {
        SimulationResult result = Simulator::run(graph, analysis, policy, options.workers);
        PerformanceMetrics metrics = MetricsCalculator::calculate(result);
        auto speedup = calculateSpeedup(graph, analysis, policy, options.workers, result);
        reports.push_back(PolicyReport{policy, std::move(result), metrics, speedup});
    }
    writeComparisonJson(output, workflow, graph, analysis, reports);
    output << '\n';
    return 0;
}

} // namespace

int runCommandLine(int argc, const char* const argv[],
                   std::ostream& output, std::ostream& errors) {
    if (argc == 1) return -1;
    try {
        return execute(parseOptions(argc, argv), output);
    } catch (const std::exception& error) {
        errors << "DAGFlow error: " << error.what() << '\n';
        return 2;
    }
}

} // namespace dagflow::app
