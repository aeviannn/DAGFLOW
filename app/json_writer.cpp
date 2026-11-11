#include "json_writer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace dagflow::app {
namespace {

const char* policyId(SchedulingPolicy policy) {
    switch (policy) {
    case SchedulingPolicy::FCFS: return "fcfs";
    case SchedulingPolicy::SJF: return "sjf";
    case SchedulingPolicy::CriticalPathFirst: return "cpf";
    }
    return "unknown";
}

void writeTasks(std::ostream& output, const examples::WorkflowPreset& workflow) {
    output << "[";
    for (std::size_t i = 0; i < workflow.tasks.size(); ++i) {
        if (i != 0) output << ",";
        const TaskSpec& task = workflow.tasks[i];
        output << "{\"id\":" << task.id << ",\"duration\":" << task.duration
               << ",\"dependencies\":[";
        for (std::size_t j = 0; j < task.dependencies.size(); ++j) {
            if (j != 0) output << ",";
            output << task.dependencies[j];
        }
        output << "]}";
    }
    output << "]";
}

void writeAnalysis(std::ostream& output, const examples::WorkflowPreset& workflow,
                   const WorkflowGraph& graph, const GraphAnalysis& analysis) {
    output << "\"topological_order\":[";
    for (std::size_t i = 0; i < analysis.topologicalOrder.size(); ++i) {
        if (i != 0) output << ",";
        output << graph.task(analysis.topologicalOrder[i]).id;
    }
    output << "],\"bottom_levels\":[";
    for (std::size_t i = 0; i < graph.size(); ++i) {
        if (i != 0) output << ",";
        output << "{\"task_id\":" << graph.task(i).id
               << ",\"value\":" << analysis.bottomLevel.at(i) << "}";
    }
    output << "],\"critical_path_length\":" << analysis.criticalPathLength
           << ",\"critical_path_task_ids\":[";
    std::unordered_map<TaskId, const TaskSpec*> taskSpecs;
    taskSpecs.reserve(workflow.tasks.size());
    for (const TaskSpec& task : workflow.tasks) taskSpecs.emplace(task.id, &task);
    std::vector<Time> longestPrefix(graph.size(), 0);
    bool first = true;
    for (const TaskIndex index : analysis.topologicalOrder) {
        const Task& task = graph.task(index);
        const TaskSpec& spec = *taskSpecs.at(task.id);
        Time prefixBeforeTask = 0;
        for (const TaskId dependency : spec.dependencies) {
            prefixBeforeTask = std::max(prefixBeforeTask, longestPrefix.at(graph.indexOf(dependency)));
        }
        if (task.duration > std::numeric_limits<Time>::max() - prefixBeforeTask) {
            throw std::overflow_error("Critical-path prefix exceeds supported Time range");
        }
        longestPrefix[index] = prefixBeforeTask + task.duration;
        const Time suffixAfterTask = analysis.bottomLevel.at(index) - task.duration;
        if (suffixAfterTask > std::numeric_limits<Time>::max() - longestPrefix[index]) {
            throw std::overflow_error("Critical-path total exceeds supported Time range");
        }
        if (longestPrefix[index] + suffixAfterTask == analysis.criticalPathLength) {
            if (!first) output << ",";
            output << task.id;
            first = false;
        }
    }
    output << "],\"tasks\":";
    writeTasks(output, workflow);
}

void writeExecutions(std::ostream& output, const WorkflowGraph& graph,
                     const SimulationResult& simulation) {
    output << "[";
    for (std::size_t i = 0; i < simulation.executions.size(); ++i) {
        if (i != 0) output << ",";
        const TaskExecution& execution = simulation.executions[i];
        const Time duration = graph.task(graph.indexOf(execution.taskId)).duration;
        output << "{\"task_id\":" << execution.taskId
               << ",\"worker_id\":" << execution.workerId
               << ",\"start_time\":" << execution.startTime
               << ",\"completion_time\":" << execution.completionTime
               << ",\"duration\":" << duration << "}";
    }
    output << "]";
}

void writeMetrics(std::ostream& output, const PerformanceMetrics& metrics,
                  const std::optional<double>& speedup) {
    output << "{\"makespan\":" << metrics.makespan
           << ",\"total_busy_time\":" << metrics.totalBusyTime
           << ",\"worker_utilization\":" << std::setprecision(17)
           << metrics.workerUtilization << ",\"utilization_percentage\":"
           << metrics.utilizationPercentage << ",\"throughput\":" << metrics.throughput
           << ",\"speedup\":";
    if (speedup && std::isfinite(*speedup)) output << *speedup;
    else output << "null";
    output << "}";
}

} // namespace

void writeJsonString(std::ostream& output, const std::string& value) {
    static const char hex[] = "0123456789abcdef";
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) {
                output << "\\u00" << hex[character >> 4] << hex[character & 0x0f];
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    output << '"';
}

void writeWorkflowCatalogJson(std::ostream& output,
                              const std::vector<examples::WorkflowPreset>& workflows) {
    output << "{\"workflows\":[";
    for (std::size_t i = 0; i < workflows.size(); ++i) {
        if (i != 0) output << ",";
        output << "{\"id\":";
        writeJsonString(output, workflows[i].id);
        output << ",\"name\":";
        writeJsonString(output, workflows[i].name);
        output << ",\"task_count\":" << workflows[i].tasks.size() << "}";
    }
    output << "]}";
}

void writeSimulationJson(std::ostream& output, const examples::WorkflowPreset& workflow,
                         const WorkflowGraph& graph, const GraphAnalysis& analysis,
                         const SimulationResult& simulation,
                         const PerformanceMetrics& metrics,
                         const std::optional<double>& speedup) {
    output << "{\"workflow\":{\"id\":";
    writeJsonString(output, workflow.id);
    output << ",\"name\":";
    writeJsonString(output, workflow.name);
    output << ",\"task_count\":" << workflow.tasks.size() << "},\"worker_count\":"
           << simulation.workerCount << ",\"policy\":";
    writeJsonString(output, policyId(simulation.policy));
    output << ",\"analysis\":{";
    writeAnalysis(output, workflow, graph, analysis);
    output << "},\"completed_task_count\":" << simulation.completedTaskCount
           << ",\"executions\":";
    writeExecutions(output, graph, simulation);
    output << ",\"metrics\":";
    writeMetrics(output, metrics, speedup);
    output << "}";
}

void writeComparisonJson(std::ostream& output, const examples::WorkflowPreset& workflow,
                         const WorkflowGraph& graph, const GraphAnalysis& analysis,
                         const std::vector<PolicyReport>& reports) {
    output << "{\"workflow\":{\"id\":";
    writeJsonString(output, workflow.id);
    output << ",\"name\":";
    writeJsonString(output, workflow.name);
    output << ",\"task_count\":" << workflow.tasks.size() << "},\"worker_count\":";
    output << (reports.empty() ? 0 : reports.front().simulation.workerCount)
           << ",\"analysis\":{";
    writeAnalysis(output, workflow, graph, analysis);
    output << "},\"results\":[";
    for (std::size_t i = 0; i < reports.size(); ++i) {
        if (i != 0) output << ",";
        const PolicyReport& report = reports[i];
        output << "{\"policy\":";
        writeJsonString(output, policyId(report.policy));
        output << ",\"metrics\":";
        writeMetrics(output, report.metrics, report.speedup);
        output << ",\"executions\":";
        writeExecutions(output, graph, report.simulation);
        output << "}";
    }
    output << "]}";
}

} // namespace dagflow::app
