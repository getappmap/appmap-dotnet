#include <spdlog/spdlog.h>

#include "recorder.h"

#include "instrumentation.h"

using namespace appmap;

namespace appmap::recorder {
    appmap::recording events;
}

namespace {
    std::unordered_map<FunctionID, std::string> method_names;

    void method_called(FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        spdlog::debug("{}({})", __FUNCTION__, method_names[id]);
        recorder::events.push_back({event_kind::call, id});
    }

    void method_returned(FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        spdlog::debug("{}({})", __FUNCTION__, method_names[id]);
        recorder::events.push_back({event_kind::ret, id});
    }
}

void recorder::instrument(clrie::method_info method)
{
    clrie::instruction_graph code = method.instructions();
    auto factory = method.instruction_factory();
    const instrumentation instr{factory, method.module_info().meta_data_emit().as<IMetaDataEmit>()};

    FunctionID id = method.function_id();
    method_names[id] = method.full_name();

    // prologue
    const auto first = code.first_instruction();
    code.insert_before(first, factory.load_constants(id));
    code.insert_before(first, instr.make_call(&method_called));

    return;
    // epilogue
    auto ret = code.last_instruction();
    code.insert_before(first, factory.load_constants(id));
    code.insert_before(ret, instr.make_call(&method_returned));
}
