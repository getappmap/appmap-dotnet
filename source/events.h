#pragma once

#include <vector>

#include <cor.h>
#include <corprof.h>
#include <nlohmann/json_fwd.hpp>

#include "clrie/method_info.h"

namespace appmap {
    struct event {
        FunctionID function;
        enum class kind { call, return_ };
        kind kind;
        ThreadID thread;
        using id = unsigned int;
    };

    nlohmann::json to_json(const std::vector<event> &events, const std::unordered_map<FunctionID, clrie::method_info> &methods);
}
