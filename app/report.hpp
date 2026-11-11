#pragma once

#include "dagflow/metrics.hpp"
#include "workflows.hpp"

#include <iosfwd>
#include <optional>
#include <vector>

namespace dagflow::app {

struct PolicyReport {
    SchedulingPolicy policy;
    SimulationResult simulation;
    PerformanceMetrics metrics;
    std::optional<double> speedup;
};

void printIndividualReport(std::ostream& output,
                           const examples::WorkflowPreset& workflow,
                           const WorkflowGraph& graph,
                           const GraphAnalysis& analysis,
                           const SimulationResult& simulation,
                           const PerformanceMetrics& metrics);

void printComparisonReport(std::ostream& output,
                           const examples::WorkflowPreset& workflow,
                           const std::vector<PolicyReport>& reports);

void printGanttChart(std::ostream& output, const SimulationResult& simulation);

} // namespace dagflow::app
