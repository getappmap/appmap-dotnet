#pragma once

#include <string>

#include <cor.h>
#include <corprof.h>
#undef __valid

namespace appmap {
    struct method_info {
        std::string defined_class;
        std::string method_id;
        bool is_static;
        std::string return_type;
        std::vector<std::string> parameters{};
    };

    inline std::vector<method_info> method_infos;
}
