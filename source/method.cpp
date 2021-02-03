#include <iostream>
#include <algorithm>

#include "appmap.h"
#include "method.h"
#include "clrie/instruction_graph.h"

void appmap::instrumentation_method::initialize(com::ptr<IProfilerManager> profiler_manager)
{
    manager = profiler_manager;
    profiler = manager.get(&IProfilerManager::GetCorProfilerInfo).as<ICorProfilerInfo>();
    
    DWORD eventMask;
    com::hresult::check(profiler->GetEventMask(&eventMask));
    eventMask |= COR_PRF_MONITOR_EXCEPTIONS;
    com::hresult::check(profiler->SetEventMask(eventMask));
}

bool appmap::instrumentation_method::should_instrument_method([[maybe_unused]] clrie::method_info method_info, [[maybe_unused]] bool is_rejit)
{
    // don't instrument system methods, it's more trouble than it's worth
    return method_info.full_name().rfind("System.", 0) != 0;
}

[[maybe_unused]] static void method_called(appmap::instrumentation_method *ptr, FunctionID fid)
{
    ptr->method_called(fid);
}

[[maybe_unused]] static void method_returned(appmap::instrumentation_method *ptr, FunctionID fid)
{
    ptr->method_returned(fid);
}

template<>
constexpr GUID com::guid_of<IMetaDataEmit>() noexcept {
    using namespace com::literals;
    return "{BA3FEE4C-ECB9-4e41-83B7-183FA41CD859}"_guid;
}

void appmap::instrumentation_method::instrument_method(clrie::method_info method_info, [[maybe_unused]] bool is_rejit)
{
    methods[method_info.function_id()] = method_info;
    auto graph = method_info.instructions();
    auto factory = method_info.instruction_factory();

    auto module = method_info.module_info();
    auto metadata = module.meta_data_emit().as<IMetaDataEmit>();
    
    const COR_SIGNATURE sig[] = { IMAGE_CEE_UNMANAGED_CALLCONV_C, 2, ELEMENT_TYPE_VOID, ELEMENT_TYPE_I8, ELEMENT_TYPE_I8 };
    auto token = metadata.get<mdToken>(&IMetaDataEmit::GetTokenFromSig, sig, sizeof(sig));
    
    auto first = graph.first_instruction();
    graph.insert_before(first, factory.create_load_const_instruction(this));
    graph.insert_before(first, factory.create_load_const_instruction(method_info.function_id()));
    graph.insert_before(first, factory.create_load_const_instruction(&::method_called));
    graph.insert_before(first, factory.create_token_operand_instruction(Cee_Calli, token));
    
    auto instr = method_info.single_ret_default_instrumentation();
    instr->Initialize(graph);
    instr->ApplySingleRetDefaultInstrumentation();

    auto last = graph.last_instruction();
    graph.insert_before(last, factory.create_load_const_instruction(this));
    graph.insert_before(last, factory.create_load_const_instruction(method_info.function_id()));
    graph.insert_before(last, factory.create_load_const_instruction(&::method_returned));
    graph.insert_before(last, factory.create_token_operand_instruction(Cee_Calli, token));
}

void appmap::instrumentation_method::method_called(FunctionID fid)
{
    events.push_back({fid, event::kind::call, profiler.get(&ICorProfilerInfo::GetCurrentThreadID)});
}

void appmap::instrumentation_method::method_returned(FunctionID fid)
{
    events.push_back({fid, event::kind::return_, profiler.get(&ICorProfilerInfo::GetCurrentThreadID)});
}

void appmap::instrumentation_method::on_shutdown()
{
    auto to_appmap = [](auto kind) { 
        if (kind == appmap::instrumentation_method::event::kind::call)
            return appmap::event::kind::call;
        else
            return appmap::event::kind::return_;
    };
    
    auto basic_call_info = [this](auto id) {
        static std::unordered_map<FunctionID, appmap::event::call_info> infos;
        auto it = infos.find(id);
        if (it != infos.end())
            return it->second;
        
        auto &method = methods[id];
        
        appmap::event::call_info info;
        info.defined_class = method.declaring_type().get(&IType::GetName);
        info.method_id = method.name();
        info.static_ = method.is_static();
        
        return infos[id] = info;
    };

    map appmap;

    for (unsigned int i = 0; i < events.size(); ++i) {
        const event &event = events[i];
        
        appmap::event ev{ i, to_appmap(event.kind), event.thread, {} };
        
        if (event.kind == event::kind::call) {
            appmap::event::call_info info = basic_call_info(event.function);
            ev.info = info;
        } else {
            unsigned int call_idx = i;
            for (; call_idx > 0; call_idx--) {
                const auto &other = events[call_idx];
                if (other.function == event.function && other.thread == event.thread && other.kind == event::kind::call)
                    break;
            }
            ev.info = appmap::event::return_info { call_idx };
        }
        
        appmap.events.push_back(ev);
    }
    
    std::cout << nlohmann::json(appmap).dump(4) << std::endl;
}

void appmap::instrumentation_method::exception_catcher_enter(clrie::method_info method_info, [[maybe_unused]] UINT_PTR object_id)
{
    if (methods.find(method_info.function_id()) != methods.end()) {
        // An exception has been caught inside a function we're instrumenting.
        // This means the return recorded in exception_unwind_function_enter() wasn't actually a return,
        // as the stack unwinding stopped.
        events.pop_back();
    }
}

void appmap::instrumentation_method::exception_unwind_function_enter(clrie::method_info method_info)
{
    const FunctionID fid = method_info.function_id();

    // If we're here an exception has been thrown and the stack is being unwound.
    // If it's a function we're instrumenting, record a return.
    if (methods.find(fid) != methods.end())
        events.push_back({fid, event::kind::return_, profiler.get(&ICorProfilerInfo::GetCurrentThreadID)});
}
