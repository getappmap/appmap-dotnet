#include <range/v3/algorithm/move.hpp>
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

    void method_returned_void(FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        recorder::events.push_back({event_kind::ret, id});
    }

    template <typename T>
    void method_returned(T return_value, FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        recorder::events.push_back({event_kind::ret, id, return_value});
    }

    clrie::instruction_factory::instruction_sequence make_return(const instrumentation &instr, com::ptr<IType> return_type)
    {
        switch (return_type.get<CorElementType>(&IType::GetCorElementType)) {
            case ELEMENT_TYPE_VOID:
                return instr.make_call(method_returned_void);

            case ELEMENT_TYPE_I1:
            case ELEMENT_TYPE_I2:
            case ELEMENT_TYPE_I4:
            case ELEMENT_TYPE_I8:
                return instr.make_call(method_returned<int64_t>);

            case ELEMENT_TYPE_BOOLEAN:
                return instr.make_call(method_returned<bool>);

            default:
                spdlog::warn("capturing values of type {} unimplemented", std::string(return_type.get(&IType::GetName)));
                [[fallthrough]];

            case ELEMENT_TYPE_U1:
            case ELEMENT_TYPE_U2:
            case ELEMENT_TYPE_U4:
            case ELEMENT_TYPE_U8:
                return instr.make_call(method_returned<uint64_t>);
        }
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
    const instrumentation instr(method);

    FunctionID id = method.function_id();
    auto return_type = method.return_type();
    method_infos[id] = {
        method.declaring_type().get(&IType::GetName),
        method.name(),
        (method.is_static() || method.is_static_constructor()),
        return_type.get(&IType::GetName)
    };

    // prologue
    const auto first = code.first_instruction();
    code.insert_before(first, instr.load_constants(id));
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

    clrie::instruction_factory::instruction_sequence epilogue;

    if (return_type.get<CorElementType>(&IType::GetCorElementType) != ELEMENT_TYPE_VOID) {
        epilogue += instr.create_instruction(Cee_Dup);
    }

    epilogue += instr.load_constants(id);
    epilogue += make_return(instr, return_type);

    code.insert_before_and_retarget_offsets(last, epilogue);
}
