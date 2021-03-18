#pragma once

#include <string>
#include <unordered_map>

#include <corprof.h>

namespace appmap {
    struct method_info {
        std::string defined_class;
        std::string method_id;
    };

    inline std::unordered_map<FunctionID, method_info> method_infos;
}
