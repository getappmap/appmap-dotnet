#include "config.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

namespace {
    // find a file in the directory or one of its parents
    std::optional<fs::path> find_file(fs::path directory, const std::string name)
    {
        for (; directory != directory.root_path(); directory = directory.parent_path()) {
            const auto file = directory / name;
            if (fs::exists(file))
                return file;
        }
        return std::nullopt;
    }

    nlohmann::json load_metadata() {
        nlohmann::json metadata;

        const auto current = fs::canonical(fs::current_path());
        const auto config_path = find_file(current, "appmap.yml");
        const auto base_dir = config_path ? config_path->parent_path() : current;

        metadata["app"] = base_dir.filename();

        if (config_path) {
            const auto config = YAML::LoadFile(*config_path);
            if (config["name"]) {
                metadata["app"] = config["name"].as<std::string>();
            }
        }

        return metadata;
    }
}

appmap::configuration::configuration() noexcept
{
    instrument = std::getenv("APPMAP");
    method_list = std::getenv("APPMAP_METHOD_LIST");
    if (const char *path = std::getenv("APPMAP_OUTPUT_PATH"))
        output = path;

    metadata = load_metadata();
}

#ifndef DOCTEST_CONFIG_DISABLE

#include <cstdio>
#include <fstream>

using namespace appmap;
using namespace nlohmann;
using namespace std;
using namespace std::filesystem;

struct temp_directory : public path
{
    temp_directory() : path(std::tmpnam(nullptr)) {
        create_directories(*this);
    }

    ~temp_directory() {
        remove_all(*this);
    }
};

TEST_CASE("metadata") {
    const temp_directory test_dir;
    path working_dir = test_dir;

    SUBCASE("specified in config file") {
        std::ofstream(test_dir / "appmap.yml") << "name: appmap-test";

        SUBCASE("in the current directory") { }

        SUBCASE("in a parent directory") {
            working_dir /= "child";
            create_directory(working_dir);
        }
    }

    SUBCASE("guessed") {
        working_dir = test_dir / "appmap-test";
        create_directory(working_dir);

        SUBCASE("without a config file") {}
        SUBCASE("with a config file") {
            std::ofstream(working_dir / "appmap.yml") << "noname: true";

            SUBCASE("in the current directory") { }

            SUBCASE("in a parent directory") {
                working_dir /= "child";
                create_directory(working_dir);
            }
        }
    }

    CAPTURE(working_dir);
    current_path(working_dir);
    const configuration config;
    CHECK(config.metadata == json({ { "app", "appmap-test"} }));
}

#endif // DOCTEST_CONFIG_DISABLE
