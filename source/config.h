#pragma once

#include <optional>
#include <string>

namespace appmap {
    struct config {
        std::optional<std::string> module_list_path;

        config();
    };
}
