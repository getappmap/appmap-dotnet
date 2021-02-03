#include <iostream>

#include "method.h"
#include "clrie/instruction_graph.h"

void appmap::instrumentation_method::initialize([[maybe_unused]] com::ptr<IProfilerManager> profiler_manager)
{
    std::cout << "initialize" << std::endl;
}

bool appmap::instrumentation_method::should_instrument_method(clrie::method_info method_info, bool is_rejit)
{
    std::cout << "should_instrument_method(" << method_info.full_name() << ", " << is_rejit << std::endl;
    return true;
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

void appmap::instrumentation_method::instrument_method(clrie::method_info method_info, bool is_rejit)
{
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
    
    // Note: we should perform the single return transformation before this, 
    // but for some reason it leads to random segfaults. Let's revisit later.
    auto last = graph.last_instruction();
    graph.insert_before(last, factory.create_load_const_instruction(this));
    graph.insert_before(last, factory.create_load_const_instruction(method_info.function_id()));
    graph.insert_before(last, factory.create_load_const_instruction(&::method_returned));
    graph.insert_before(last, factory.create_token_operand_instruction(Cee_Calli, token));

    std::cout << "instrument_method(" << method_info.full_name() << "(" << method_info.function_id() << "), " << is_rejit << std::endl;
}

void appmap::instrumentation_method::method_called(FunctionID fid)
{
    std::cout << "method_called(" << fid << std::endl;
}

void appmap::instrumentation_method::method_returned(FunctionID fid)
{
    std::cout << "method_returned(" << fid << std::endl;
}
