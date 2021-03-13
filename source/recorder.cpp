#include <spdlog/spdlog.h>

#include "recorder.h"

#include "instrumentation.h"

using namespace appmap;

void recorder::instrument(clrie::method_info method)
{
    const auto f = ELEMENT_TYPE_U8;
    clrie::instruction_graph code = method.instructions();
    auto factory = method.instruction_factory();
    const instrumentation instr{factory, method.module_info().meta_data_emit().as<IMetaDataEmit>()};

    // prologue
    const auto first = code.first_instruction();
    code.insert_before(first, factory.load_constants(this, method.function_id()));
    code.insert_before(first, instr.make_call(&recorder::method_called));

    // epilogue
    auto ret = code.last_instruction();
    code.insert_before(first, factory.load_constants(this, method.function_id()));
    code.insert_before(ret, instr.make_call(&recorder::method_returned));
}

void recorder::method_called(FunctionID id)
{
    spdlog::debug("{}({})", __FUNCTION__, id);
    events.push_back({id, event::kind::call});
}

void recorder::method_returned(FunctionID id)
{
    spdlog::debug("{}({})", __FUNCTION__, id);
    events.push_back({id, event::kind::ret});
}
