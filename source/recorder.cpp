#include <doctest/doctest.h>
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
        if (spdlog::get_level() >= spdlog::level::trace) {
            const auto &method_info = method_infos.at(id);
            spdlog::trace("{}({}.{})", __FUNCTION__, method_info.defined_class, method_info.method_id);
        }
        recorder::events.push_back({event_kind::call, id});
    }

    void method_returned_void(FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        if (spdlog::get_level() >= spdlog::level::trace) {
            const auto &method_info = method_infos.at(id);
            spdlog::trace("{}({}.{})", __FUNCTION__, method_info.defined_class, method_info.method_id);
        }
        recorder::events.push_back({event_kind::ret, id});
    }

    template <typename T>
    void method_returned(T return_value, FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        if (spdlog::get_level() >= spdlog::level::trace) {
            const auto &method_info = method_infos.at(id);
            spdlog::trace("{}({}, {}.{})", __FUNCTION__, return_value, method_info.defined_class, method_info.method_id);
        }
        recorder::events.push_back({event_kind::ret, id, return_value});
    }

    template <>
    void method_returned<const char *>(const char *return_value, FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        if (spdlog::get_level() >= spdlog::level::trace) {
            const auto &method_info = method_infos.at(id);
            if (return_value == nullptr)
                spdlog::trace("{}({}, {}.{})", __FUNCTION__, "null", method_info.defined_class, method_info.method_id);
            else
                spdlog::trace("{}({}, {}.{})", __FUNCTION__, return_value, method_info.defined_class, method_info.method_id);
        }
        if (return_value == nullptr)
            recorder::events.push_back({event_kind::ret, id, nullptr});
        else
            recorder::events.push_back({event_kind::ret, id, std::string(return_value)});
    }

    TEST_CASE("method_returned()")
    {
        recorder::events.clear();
        SUBCASE("with a string argument") {
            method_returned("hello", 42);
            CHECK(recorder::events.back() == event{event_kind::ret, 42, std::string("hello")});
        }

        SUBCASE("with nullptr") {
            method_returned<const char *>(nullptr, 42);
            CHECK(recorder::events.back() == event{event_kind::ret, 42, nullptr});
        }
    }

    [[maybe_unused]]
    clrie::instruction_factory::instruction_sequence make_return(const instrumentation &instr, FunctionID id, com::ptr<IType> return_type)
    {
        const auto cor_type = return_type.get<CorElementType>(&IType::GetCorElementType);

        clrie::instruction_factory::instruction_sequence seq;
        if (cor_type != ELEMENT_TYPE_VOID)
            seq += instr.create_instruction(Cee_Dup);

        seq += instr.load_constants(id);

        switch (cor_type) {
            case ELEMENT_TYPE_VOID:
                seq += instr.make_call(method_returned_void);
                break;

            case ELEMENT_TYPE_I1:
            case ELEMENT_TYPE_I2:
            case ELEMENT_TYPE_I4:
            case ELEMENT_TYPE_I8:
                seq += instr.make_call(method_returned<int64_t>);
                break;

            case ELEMENT_TYPE_BOOLEAN:
                seq += instr.make_call(method_returned<bool>);
                break;

            default:
                {
                    spdlog::debug("capturing values of type {} unimplemented", std::string(return_type.get(&IType::GetName)));
                    auto to_string = instr.create_call_to_string(return_type);
                    seq.insert(seq.begin() + 1, to_string.begin(), to_string.end());
                }
                [[fallthrough]];

            case ELEMENT_TYPE_STRING:
                seq += instr.make_call(method_returned<const char *>);
                break;

            case ELEMENT_TYPE_U1:
            case ELEMENT_TYPE_U2:
            case ELEMENT_TYPE_U4:
            case ELEMENT_TYPE_U8:
                seq += instr.make_call(method_returned<uint64_t>);
                break;
        }

        return seq;
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

    code.insert_before_and_retarget_offsets(last, make_return(instr, id, return_type));
}
