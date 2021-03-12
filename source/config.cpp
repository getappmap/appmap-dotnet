#include <spdlog/spdlog.h>

#include "config.h"

namespace {
    std::optional<std::string> get_envar(const char *name) {
        const auto result = std::getenv(name);
        if (result) {
            return result;
        } else {
            return std::nullopt;
        }
    }
}

appmap::config::config()
: module_list_path{get_envar("APPMAP_LIST_MODULES")}
{
    spdlog::set_level(spdlog::level::debug);
}
