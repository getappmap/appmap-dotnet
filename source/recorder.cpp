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
    thread_local std::vector<cor_value> arguments;

    const call_event *method_called(FunctionID id)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        if (spdlog::get_level() >= spdlog::level::trace) {
            const auto &method_info = method_infos.at(id);
            spdlog::trace("{}({}.{})", __FUNCTION__, method_info.defined_class, method_info.method_id);
        }
        auto event = std::make_unique<call_event>(id, std::exchange(arguments, {}));
        auto ptr = event.get();
        recorder::events.push_back(std::move(event));
        return ptr;
    }

    void method_returned_void(const call_event *call)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        if (spdlog::get_level() >= spdlog::level::trace && call) {
            const auto &method_info = method_infos.at(call->function);
            spdlog::trace("{}({}.{})", __FUNCTION__, method_info.defined_class, method_info.method_id);
        }
        recorder::events.push_back(std::make_unique<return_event>(call));
    }

    template <typename T>
    void method_returned(T return_value, const call_event *call)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        if (spdlog::get_level() >= spdlog::level::trace && call) {
            const auto &method_info = method_infos.at(call->function);
            spdlog::trace("{}({}, {}.{})", __FUNCTION__, return_value, method_info.defined_class, method_info.method_id);
        }
        recorder::events.push_back(std::make_unique<return_event>(call, return_value));
    }

    template <>
    void method_returned<const char *>(const char *return_value, const call_event *call)
    {
        std::lock_guard lock(appmap::recorder::mutex);
        if (spdlog::get_level() >= spdlog::level::trace && call) {
            const auto &method_info = method_infos.at(call->function);
            if (return_value == nullptr)
                spdlog::trace("{}({}, {}.{})", __FUNCTION__, "null", method_info.defined_class, method_info.method_id);
            else
                spdlog::trace("{}({}, {}.{})", __FUNCTION__, return_value, method_info.defined_class, method_info.method_id);
        }
        if (return_value == nullptr)
            recorder::events.push_back(std::make_unique<return_event>(call, nullptr));
        else
            recorder::events.push_back(std::make_unique<return_event>(call, std::string(return_value)));
    }

    TEST_CASE("method_returned()")
    {
        recorder::events.clear();
        SUBCASE("with a string argument") {
            method_returned("hello", nullptr);
            CHECK((*recorder::events.back() == return_event{nullptr, std::string("hello")}));
        }

        SUBCASE("with nullptr") {
            method_returned<const char *>(nullptr, nullptr);
            CHECK((*recorder::events.back() == return_event{nullptr, nullptr}));
        }
    }

    clrie::instruction_factory::instruction_sequence make_return(const instrumentation &instr, uint64_t call_event_local, clrie::type return_type)
    {
        const auto cor_type = return_type.cor_element_type();

        clrie::instruction_factory::instruction_sequence seq;
        if (cor_type != ELEMENT_TYPE_VOID) {
            seq += instr.create_instruction(Cee_Dup);
            seq += instr.capture_value(return_type);
        }

        seq += instr.create_load_local_instruction(call_event_local);

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

            case ELEMENT_TYPE_U1:
            case ELEMENT_TYPE_U2:
            case ELEMENT_TYPE_U4:
            case ELEMENT_TYPE_U8:
                seq += instr.make_call(method_returned<uint64_t>);
                break;

            default:
                seq += instr.make_call(method_returned<const char *>);
                break;
        }

        return seq;
    }

    template <typename T>
    void capture_argument(T value)
    {
        spdlog::trace("got argument: {}", value);
        arguments.push_back(value);
    }

    template <>
    void capture_argument<const char *>(const char *value)
    {
        if (value)
            arguments.push_back(std::string(value));
        else
            arguments.push_back(nullptr);
    }

    clrie::instruction_factory::instruction_sequence capture_argument(const instrumentation &instr, clrie::type type)
    {
        clrie::instruction_factory::instruction_sequence seq;

        seq += instr.capture_value(type);

        switch (type.cor_element_type()) {
            case ELEMENT_TYPE_VOID:
                throw std::logic_error("unexpected invalid void parameter");

            case ELEMENT_TYPE_I1:
            case ELEMENT_TYPE_I2:
            case ELEMENT_TYPE_I4:
            case ELEMENT_TYPE_I8:
                seq += instr.make_call(capture_argument<int64_t>);
                break;

            case ELEMENT_TYPE_BOOLEAN:
                seq += instr.make_call(capture_argument<bool>);
                break;

            case ELEMENT_TYPE_U1:
            case ELEMENT_TYPE_U2:
            case ELEMENT_TYPE_U4:
            case ELEMENT_TYPE_U8:
                seq += instr.make_call(capture_argument<uint64_t>);
                break;

            default:
                seq += instr.make_call(capture_argument<const char *>);
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
    instrumentation instr(method);

    FunctionID id = method.function_id();
    auto return_type = method.return_type();
    const auto is_static = method.is_static() || method.is_static_constructor();
    const auto call_event_local = instr.add_local<call_event *>();

    auto ins = code.first_instruction();

    const auto parameters = method.parameters();
    std::vector<std::string> parameter_types;
    parameter_types.reserve(parameters.size());

    uint idx = is_static ? 0 : 1;
    for (auto &p: parameters) {
        const clrie::type type = p.get(&IMethodParameter::GetType);
        parameter_types.push_back(type.name());
        code.insert_before(ins, instr.create_load_arg_instruction(idx++));
        code.insert_before(ins, capture_argument(instr, type));
    }

    method_infos[id] = {
        method.declaring_type().name(),
        method.name(),
        is_static,
        return_type.name(),
        std::move(parameter_types)
    };

    // prologue
    code.insert_before(ins, instr.load_constants(id));
    code.insert_before(ins, instr.make_call(&method_called));
    code.insert_before(ins, instr.create_store_local_instruction(call_event_local));

    // Look for returns and insert epilogue gadget before each.
    // (Note we could instead transfrom the method to have a single return point
    // at the end and insert one epilogue there.
    // This is what CLRIE's SingleRetDefaultInstrumentation is supposed to do,
    // but it's buggy and produces broken code in some cases.)
    while (++ins) {
        com::ptr<IInstruction> next;
        if (ins.get(&IInstruction::GetOpCode) == Cee_Ret) {
            if (is_tail(ins)) {
                spdlog::warn("Tail call detected in {} -- tail calls aren't fully supported yet, so your appmap might be incorrect.\n\tPlease report at https://github.com/applandinc/appmap-dotnet/issues", method.full_name());
                ins = ins.get(&IInstruction::GetPreviousInstruction);
            }

            code.insert_before_and_retarget_offsets(ins, make_return(instr, call_event_local, return_type));
        }
    }
}
