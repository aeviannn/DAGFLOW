#include "dagflow/scheduler.hpp"

#include <deque>
#include <functional>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace dagflow {
namespace {

class FifoReadyQueue final : public ReadyQueue {
public:
    explicit FifoReadyQueue(const WorkflowGraph& graph) : graph_(graph) {}

    void push(TaskIndex task) override {
        (void)graph_.task(task); // Validate the borrowed graph index.
        tasks_.push_back(task);
    }

    TaskIndex pop() override {
        if (tasks_.empty()) {
            throw std::out_of_range("Cannot pop from an empty ready queue");
        }
        const TaskIndex task = tasks_.front();
        tasks_.pop_front();
        return task;
    }

    bool empty() const noexcept override { return tasks_.empty(); }

private:
    const WorkflowGraph& graph_;
    std::deque<TaskIndex> tasks_;
};

class ShortestJobQueue final : public ReadyQueue {
public:
    explicit ShortestJobQueue(const WorkflowGraph& graph) : graph_(graph) {}

    void push(TaskIndex task) override {
        const Task& value = graph_.task(task);
        tasks_.emplace(value.duration, value.id, task);
    }

    TaskIndex pop() override {
        if (tasks_.empty()) {
            throw std::out_of_range("Cannot pop from an empty ready queue");
        }
        const TaskIndex task = std::get<2>(tasks_.top());
        tasks_.pop();
        return task;
    }

    bool empty() const noexcept override { return tasks_.empty(); }

private:
    const WorkflowGraph& graph_;
    using Entry = std::tuple<Time, TaskId, TaskIndex>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> tasks_;
};

struct CriticalPathEntry {
    Time bottomLevel;
    TaskId id;
    TaskIndex index;
};

struct CriticalPathPriority {
    bool operator()(const CriticalPathEntry& left,
                    const CriticalPathEntry& right) const noexcept {
        if (left.bottomLevel != right.bottomLevel) {
            return left.bottomLevel < right.bottomLevel; // Largest bottom level first.
        }
        return left.id > right.id; // Smallest task ID first for ties.
    }
};

class CriticalPathQueue final : public ReadyQueue {
public:
    CriticalPathQueue(const WorkflowGraph& graph, const GraphAnalysis& analysis)
        : graph_(graph), analysis_(analysis) {}

    void push(TaskIndex task) override {
        const Task& value = graph_.task(task);
        tasks_.push(CriticalPathEntry{analysis_.bottomLevel.at(task), value.id, task});
    }

    TaskIndex pop() override {
        if (tasks_.empty()) {
            throw std::out_of_range("Cannot pop from an empty ready queue");
        }
        const TaskIndex task = tasks_.top().index;
        tasks_.pop();
        return task;
    }

    bool empty() const noexcept override { return tasks_.empty(); }

private:
    const WorkflowGraph& graph_;
    const GraphAnalysis& analysis_;
    std::priority_queue<CriticalPathEntry,
                        std::vector<CriticalPathEntry>,
                        CriticalPathPriority> tasks_;
};

} // namespace

std::unique_ptr<ReadyQueue> makeReadyQueue(
    SchedulingPolicy policy,
    const WorkflowGraph& graph,
    const GraphAnalysis& analysis) {
    switch (policy) {
    case SchedulingPolicy::FCFS:
        return std::make_unique<FifoReadyQueue>(graph);
    case SchedulingPolicy::SJF:
        return std::make_unique<ShortestJobQueue>(graph);
    case SchedulingPolicy::CriticalPathFirst:
        if (analysis.bottomLevel.size() != graph.size()) {
            throw std::invalid_argument("Graph analysis does not match workflow size");
        }
        return std::make_unique<CriticalPathQueue>(graph, analysis);
    }
    throw std::invalid_argument("Unknown scheduling policy");
}

} // namespace dagflow
