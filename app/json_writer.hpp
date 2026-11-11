#pragma once

#include "report.hpp"
#include "workflows.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace dagflow::app {

void writeJsonString(std::ostream& output, const std::string& value);
void writeWorkflowCatalogJson(std::ostream& output,
                              const std::vector<examples::WorkflowPreset>& workflows);
void writeSimulationJson(std::ostream& output,
                         const examples::WorkflowPreset& workflow,
                         const WorkflowGraph& graph,
                         const GraphAnalysis& analysis,
                         const SimulationResult& simulation,
                         const PerformanceMetrics& metrics,
                         const std::optional<double>& speedup);
void writeComparisonJson(std::ostream& output,
                         const examples::WorkflowPreset& workflow,
                         const WorkflowGraph& graph,
                         const GraphAnalysis& analysis,
                         const std::vector<PolicyReport>& reports);

} // namespace dagflow::app
