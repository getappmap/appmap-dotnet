#pragma once

#include <nlohmann/json_fwd.hpp>

namespace appmap {

struct metadata
{
    metadata() {}
    operator nlohmann::json() const;

    static nlohmann::json common;
};

}
