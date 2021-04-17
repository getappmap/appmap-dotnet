#include "test_framework.h"

#include <clrie/instruction_factory.h>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

#include "config.h"
#include "generation.h"
#include "instrumentation.h"
#include "recorder.h"

using namespace appmap;
namespace fs = std::filesystem;

namespace {
    constexpr char StartCaseName[] = "AppMap.DataCollector.StartCase";
    constexpr char EndCaseName[] = "AppMap.DataCollector.EndCase";

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

bool appmap::test_framework::should_instrument(const clrie::method_info method)
{
    const std::string name = method.full_name();
    return (name == StartCaseName || name == EndCaseName);
}

bool appmap::test_framework::instrument(clrie::method_info method)
{
    auto module = method.module_info();
    if (module.module_name() != "appmap.collector.dll")
        return false;

    const std::string name = method.full_name();
    const instrumentation instr(method);
    clrie::instruction_graph code = method.instructions();
    const auto first = code.first_instruction();

    if (name == StartCaseName) {
        code.insert_before(first, instr.create_load_arg_instruction(0));
        code.insert_before(first, instr.make_call(startCase));
        return true;
    } else if (name == EndCaseName) {
        code.insert_before(first, instr.make_call(endCase));
        return true;
    }

    return false;
}
