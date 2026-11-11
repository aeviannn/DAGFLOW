#pragma once

#include "dagflow/task.hpp"

#include <vector>

namespace dagflow::examples {

inline std::vector<TaskSpec> chain() {
    return {{1, 2, {}}, {2, 3, {1}}, {3, 1, {2}}};
}

inline std::vector<TaskSpec> forkAndJoin() {
    return {{1, 2, {}}, {2, 3, {}}, {3, 4, {1, 2}}};
}

inline std::vector<TaskSpec> simultaneousAndZeroDuration() {
    return {{1, 2, {}}, {2, 2, {}}, {3, 0, {1, 2}}};
}

} // namespace dagflow::examples
