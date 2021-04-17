#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <clrie/method_info.h>

namespace appmap {
    struct config {
        std::optional<std::filesystem::path> module_list_path;
        std::optional<std::filesystem::path> appmap_output_path;
        std::filesystem::path base_path = std::filesystem::current_path();
        std::filesystem::path appmap_output_dir() const noexcept;

        std::vector<std::string> classes;
        std::vector<std::string> modules;
        std::vector<std::filesystem::path> paths;

        bool generate_classmap = false;

        static config &instance();
        bool should_instrument(clrie::method_info method);

        std::unique_ptr<std::ostream> module_list_stream() const;
        std::unique_ptr<std::ostream> appmap_output_stream() const;
        std::pair<std::unique_ptr<std::ostream>, std::filesystem::path> appmap_output_stream(const std::string &name) const;

    private:
        mutable std::optional<std::filesystem::path> output_dir;
    };
}
