#include <filesystem>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include "config.h"

namespace fs = std::filesystem;

namespace {
    std::optional<std::string> get_envar(const char *name) {
        const auto result = std::getenv(name);
        if (result) {
            return result;
        } else {
            return std::nullopt;
        }
    }

    std::optional<fs::path> find_file(const std::string name) {
        for (fs::path dir = fs::current_path(); dir != dir.root_path(); dir = dir.parent_path()) {
            const auto file = dir / name;
            if (fs::exists(file))
                return file;
        }

        spdlog::warn("no {} found in {}", name, fs::current_path().string());

        return std::nullopt;
    }
}

appmap::config::config()
: module_list_path{get_envar("APPMAP_LIST_MODULES")},
appmap_output_path{get_envar("APPMAP_OUTPUT_PATH")}
{
    spdlog::set_level(spdlog::level::debug);

    if (const auto config_path = find_file("appmap.yml")) {
        const auto config_file = YAML::LoadFile(*config_path);
        if (const auto &pkgs = config_file["packages"])
            packages = pkgs.as<decltype(packages)>();
    }
}
