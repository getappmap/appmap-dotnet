#include "test_framework.h"

#include <clrie/instruction_factory.h>
#include <spdlog/spdlog.h>

#include "instrumentation.h"

namespace {
    constexpr char StartCaseName[] = "AppMap.DataCollector.StartCase";
    constexpr char EndCaseName[] = "AppMap.DataCollector.EndCase";

    void startCase(char *full_name) {
        spdlog::info("Test case start: {}", full_name);
    }

    void endCase() {
        spdlog::info("Test case end");
    }
}

using namespace appmap;

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
    const auto factory = method.instruction_factory();
    const instrumentation instr{factory, module.meta_data_emit().as<IMetaDataEmit>()};
    clrie::instruction_graph code = method.instructions();
    const auto first = code.first_instruction();

    if (name == StartCaseName) {
        code.insert_before(first, factory.create_load_arg_instruction(0));
        code.insert_before(first, instr.make_call(startCase));
        return true;
    } else if (name == EndCaseName) {
        code.insert_before(first, instr.make_call(endCase));
        return true;
    }

    return false;
}
