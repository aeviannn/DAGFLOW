#include "report.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace dagflow::app {
namespace {

const char* policyName(SchedulingPolicy policy) {
    switch (policy) {
    case SchedulingPolicy::FCFS: return "FCFS";
    case SchedulingPolicy::SJF: return "SJF";
    case SchedulingPolicy::CriticalPathFirst: return "Critical Path First";
    }
    return "Unknown";
}

std::size_t timeColumn(Time time, Time makespan, std::size_t width) {
    if (makespan <= 0) return 0;
    const long double position = static_cast<long double>(time) /
                                 static_cast<long double>(makespan) *
                                 static_cast<long double>(width);
    return std::min(width, static_cast<std::size_t>(std::floor(position + 0.5L)));
}

std::string formatTime(Time time) {
    return std::to_string(time);
}

void printMetrics(std::ostream& output, const PerformanceMetrics& metrics) {
    output << "Makespan: " << metrics.makespan << " time units\n"
           << "Total busy time: " << metrics.totalBusyTime << " time units\n"
           << "Worker utilization: " << std::fixed << std::setprecision(1)
           << metrics.utilizationPercentage << "%\n"
           << "Throughput: " << std::setprecision(3) << metrics.throughput
           << " tasks/time unit\n";
}

} // namespace

void printGanttChart(std::ostream& output, const SimulationResult& simulation) {
    constexpr std::size_t chartWidth = 60;
    output << "\nWorker timeline ('.' idle, '=' running, '|' zero-duration task)\n";
    output << "Time: 0";
    if (simulation.makespan > 0) {
        output << std::string(chartWidth - 1, ' ') << simulation.makespan;
    }
    output << "\n";

    std::string scale(chartWidth + 1, ' ');
    if (simulation.makespan == 0) {
        scale[0] = '|';
    } else {
        for (int tick = 0; tick <= 4; ++tick) {
            const std::size_t column = static_cast<std::size_t>(tick) * chartWidth / 4;
            scale[column] = '|';
        }
    }
    output << scale << "\n";

    for (std::size_t worker = 0; worker < simulation.workerCount; ++worker) {
        std::string cells(chartWidth + 1, '.');
        for (const TaskExecution& execution : simulation.executions) {
            if (execution.workerId != worker) continue;
            const std::size_t start = timeColumn(execution.startTime, simulation.makespan, chartWidth);
            std::size_t finish = timeColumn(execution.completionTime,
                                            simulation.makespan, chartWidth);
            if (execution.startTime == execution.completionTime) {
                cells[start] = '|';
            } else {
                if (finish <= start) finish = std::min(chartWidth, start + 1);
                for (std::size_t column = start; column < finish; ++column) cells[column] = '=';
            }
        }
        output << "W" << worker << " |" << cells << "|\n";
        output << "     ";
        bool found = false;
        for (const TaskExecution& execution : simulation.executions) {
            if (execution.workerId != worker) continue;
            if (found) output << "  ";
            output << "T" << execution.taskId << " [" << formatTime(execution.startTime)
                   << ", " << formatTime(execution.completionTime) << ")";
            found = true;
        }
        if (!found) output << "(idle)";
        output << "\n";
    }

    output << "Scale: 0";
    if (simulation.makespan > 0) {
        const Time quarter = simulation.makespan / 4;
        const Time threeQuarters = quarter * 3 + (simulation.makespan % 4) * 3 / 4;
        output << " | " << quarter
               << " | " << simulation.makespan / 2
               << " | " << threeQuarters
               << " | " << simulation.makespan;
    }
    output << " time units\n";
}

void printIndividualReport(std::ostream& output,
                           const examples::WorkflowPreset& workflow,
                           const WorkflowGraph& graph,
                           const GraphAnalysis& analysis,
                           const SimulationResult& simulation,
                           const PerformanceMetrics& metrics) {
    output << "\nWorkflow: " << workflow.name << "\n"
           << "Policy: " << policyName(simulation.policy) << "\n"
           << "Workers: " << simulation.workerCount << "\n"
           << "Topological order:";
    for (const TaskIndex task : analysis.topologicalOrder) {
        output << ' ' << graph.task(task).id;
    }
    output << "\nCritical path length: " << analysis.criticalPathLength << " time units\n\n"
           << "Task executions (task, worker, start, completion)\n";
    for (const TaskExecution& execution : simulation.executions) {
        output << "  " << execution.taskId << "\tW" << execution.workerId << "\t"
               << execution.startTime << "\t" << execution.completionTime << "\n";
    }
    printMetrics(output, metrics);
    printGanttChart(output, simulation);
}

void printComparisonReport(std::ostream& output,
                           const examples::WorkflowPreset& workflow,
                           const std::vector<PolicyReport>& reports) {
    output << "\nPolicy comparison: " << workflow.name << "\n";
    output << "Policy                 Workers  Makespan  Busy time  Utilization  Throughput  Speedup\n";
    output << "--------------------------------------------------------------------------------------\n";
    for (const PolicyReport& report : reports) {
        output << std::left << std::setw(23) << policyName(report.policy)
               << std::right << std::setw(7) << report.simulation.workerCount
               << std::setw(10) << report.metrics.makespan
               << std::setw(11) << report.metrics.totalBusyTime
               << std::setw(11) << std::fixed << std::setprecision(1)
               << report.metrics.utilizationPercentage << "%"
               << std::setw(12) << std::setprecision(3) << report.metrics.throughput
               << std::setw(9);
        if (report.speedup) output << std::setprecision(3) << *report.speedup;
        else output << "N/A";
        output << "\n";
    }
    output << "\n";
    for (const PolicyReport& report : reports) {
        output << "--- " << policyName(report.policy) << " Gantt chart ---\n";
        printGanttChart(output, report.simulation);
    }
}

} // namespace dagflow::app
