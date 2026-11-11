#pragma once

#include "dagflow/task.hpp"

#include <string>
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

struct WorkflowPreset {
    std::string id;
    std::string name;
    std::vector<TaskSpec> tasks;
};

inline std::vector<TaskSpec> softwareBuildPipeline() {
    return {
        {100, 2, {}},          // checkout source
        {110, 1, {100}},       // configure build
        {120, 3, {110}},       // generate sources
        {130, 5, {120}},       // compile core library
        {140, 4, {120}},       // compile application objects
        {150, 4, {120}},       // compile test objects
        {160, 2, {130, 140}},  // link application
        {170, 2, {130, 150}},  // link test executable
        {180, 4, {170}},       // run unit tests
        {190, 1, {160, 180}}   // package release
    };
}

inline std::vector<WorkflowPreset> presets() {
    return {
        {"empty", "Empty workflow", {}},
        {"chain", "Simple dependency chain", chain()},
        {"fork-join", "Fork-and-join workflow", forkAndJoin()},
        {"simultaneous", "Simultaneous completion", simultaneousAndZeroDuration()},
        {"software-build-pipeline", "Software build pipeline", softwareBuildPipeline()}
    };
}

} // namespace dagflow::examples
