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

        bool generate_classmap = false;

        static config &instance();
        bool should_instrument(clrie::method_info method);

        std::unique_ptr<std::ostream> module_list_stream() const;
        std::unique_ptr<std::ostream> appmap_output_stream() const;
        std::pair<std::unique_ptr<std::ostream>, std::filesystem::path> appmap_output_stream(const std::string &name) const;


        struct instrumentation_filter;
        using filter_list = std::vector<std::unique_ptr<instrumentation_filter>>;
        filter_list filters;

    private:
        mutable std::optional<std::filesystem::path> output_dir;
    };
}
