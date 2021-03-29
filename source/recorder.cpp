#include <spdlog/spdlog.h>

#include "recorder.h"

#include "instrumentation.h"
#include "method_info.h"

using namespace appmap;

namespace appmap::recorder {
    appmap::recording events;
}

namespace {
    void method_called(FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        recorder::events.push_back({event_kind::call, id});
    }

    void method_returned(FunctionID id, [[maybe_unused]] bool is_tail)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        recorder::events.push_back({event_kind::ret, id});
    }

    bool is_tail(com::ptr<IInstruction> inst) {
        try {
            com::ptr<IInstruction> prev = inst.get(&IInstruction::GetPreviousInstruction);
            return prev.get(&IInstruction::GetOpCode) == Cee_Tailcall;
        } catch (const std::system_error &) {
            return false;
        }
    }
}

void recorder::instrument(clrie::method_info method)
{
    clrie::instruction_graph code = method.instructions();
    auto factory = method.instruction_factory();
    const instrumentation instr{factory, method.module_info().meta_data_emit().as<IMetaDataEmit>()};

    FunctionID id = method.function_id();
    method_infos[id] = { method.declaring_type().get(&IType::GetName), method.name(), (method.is_static() || method.is_static_constructor()) };

    // prologue
    const auto first = code.first_instruction();
    code.insert_before(first, factory.load_constants(id));
    code.insert_before(first, instr.make_call(&method_called));

    auto srdi = method.single_ret_default_instrumentation();
    srdi->Initialize(code);
    com::hresult::check(srdi->ApplySingleRetDefaultInstrumentation());
    // epilogue
    auto last = code.last_instruction();
    bool tail = is_tail(last);
    if (tail) {
        spdlog::warn("Tail call detected in {} -- tail calls aren't fully supported yet, so your appmap might be incorrect.\n\tPlease report at https://github.com/applandinc/appmap-dotnet/issues", method.full_name());
        last = last.get(&IInstruction::GetPreviousInstruction);
    }
    code.insert_before_and_retarget_offsets(last, factory.load_constants(id, tail));
    code.insert_before(last, instr.make_call(&method_returned));
}
