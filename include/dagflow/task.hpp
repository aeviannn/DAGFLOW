#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dagflow {

using TaskId = std::uint32_t;
using TaskIndex = std::size_t;
using Time = std::int64_t;

struct Task {
    TaskId id;
    Time duration;
};

struct TaskSpec {
    TaskId id;
    Time duration;
    std::vector<TaskId> dependencies;
};

} // namespace dagflow
