
#include <clrie/instruction_factory.h>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

#include "config.h"
#include "generation.h"
#include "instrumentation.h"
#include "method.h"
#include "recorder.h"

using namespace appmap;
namespace fs = std::filesystem;

namespace {
    std::string case_name;

    void startCase(char *full_name) {
        std::lock_guard lock(appmap::recorder::mutex);
        spdlog::debug("Test case start: {}", full_name);
        case_name = full_name;
        appmap::recorder::events.clear();
    }

    void endCase() {
        const config &c = appmap::config::instance();
        auto [stream, path] = c.appmap_output_stream(case_name);
        std::lock_guard lock(appmap::recorder::mutex);
        *stream << generate(appmap::recorder::events, c.generate_classmap) << std::endl;
        spdlog::info("Wrote {}", path.string());
    }
}

namespace appmap { namespace test_framework {
    auto start = add_hook("AppMap.DataCollector.StartCase", [](const auto &method){
        const instrumentation instr(method);
        clrie::instruction_graph code = method.instructions();
        const auto first = code.first_instruction();

        code.insert_before(first, instr.create_load_arg_instruction(0));
        code.insert_before(first, instr.make_call(startCase));

        return true;
    });

    auto end_case = add_hook("AppMap.DataCollector.EndCase", [](const auto &method){
        const instrumentation instr(method);
        clrie::instruction_graph code = method.instructions();
        const auto first = code.first_instruction();

        code.insert_before(first, instr.make_call(endCase));
        return true;
    });
} }
