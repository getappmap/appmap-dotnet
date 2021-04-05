#pragma once

#include <string>
#include <unordered_map>

#include <cor.h>
#include <corprof.h>
#undef __valid

namespace appmap {
    struct method_info {
        std::string defined_class;
        std::string method_id;
        bool is_static;
        std::string return_type;
    };

    inline std::unordered_map<FunctionID, method_info> method_infos;
}
