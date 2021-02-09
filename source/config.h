#pragma once

#include <optional>
#include <string>

namespace appmap {
    struct configuration {
        configuration() noexcept;

        // whether to instrument methods and produce an appmap
        bool instrument;

        // whether to produce a method list
        bool method_list;

        std::optional<std::string> output;
    };
}
