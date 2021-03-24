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

    std::optional<fs::path> config_file_path() {
        const auto from_env = get_envar("APPMAP_CONFIG");
        if (from_env)
            return from_env;
        else
            return find_file("appmap.yml");
    }

    void load_config(appmap::config &c, const YAML::Node &config_file) {
        if (const auto &pkgs = config_file["packages"])
            c.classes = pkgs.as<decltype(c.classes)>();
    }
}

appmap::config appmap::config::load()
{
    config c;
    c.module_list_path = get_envar("APPMAP_LIST_MODULES");
    c.appmap_output_path = get_envar("APPMAP_OUTPUT_PATH");

    // it's probably not the best place for this, but it'll do
    spdlog::set_level(spdlog::level::debug);

    if (const auto config_path = config_file_path())
        load_config(c, YAML::LoadFile(*config_path));

    return c;
}

bool appmap::config::should_instrument(clrie::method_info method)
{
    const auto name = method.full_name();
    for (const auto &cls : classes) {
        if (name.rfind(cls, 0) == 0) {
            return true;
        }
    }
    return false;
}
