#include <spdlog/spdlog.h>

#include "method.h"
#include "resolver.h"
#include "signature.h"

namespace appmap::resolver {

using namespace appmap;
namespace sig = appmap::signature;

const char *resolve(const char *assembly_name)
{
    spdlog::info("trying to resolve {}", assembly_name);
    return nullptr;
}

bool hook_resolver = add_hook("AppMap.ResolveAssembly", [](const auto &method)
{
    instrumentation i(method);

    const auto SystemRuntime = i.assembly_reference(u"System.Runtime");
    const auto Assembly = i.type_reference(SystemRuntime, u"System.Reflection.Assembly");

    auto code = method.instructions();
    code.remove_all();
    auto ret = i.create_instruction(Cee_Ret);
    code.insert_after(nullptr, ret);

    code.insert_before(ret, i.create_load_arg_instruction(1));
    code.insert_before(ret, i.create_token_operand_instruction(Cee_Callvirt,
            i.member_reference(SystemRuntime, u"System.ResolveEventArgs",
                u"get_Name", sig::method(sig::string, {})))
    );
    code.insert_before(ret, i.make_call(resolve));
    code.insert_before(ret, {
        i.create_instruction(Cee_Dup),
        i.create_branch_instruction(Cee_Brfalse, ret),
        i.create_token_operand_instruction(Cee_Call,
            i.member_reference(Assembly, u"LoadFile", sig::static_method(
                Assembly, {sig::string})))
    });

    return true;
});

clrie::instruction_factory::instruction_sequence inject(instrumentation i)
{
    clrie::instruction_factory::instruction_sequence code;

    const auto SystemRuntime = i.assembly_reference(u"System.Runtime");
    const auto AppDomain = i.type_reference(SystemRuntime, u"System.AppDomain");
    const auto Assembly = i.type_reference(SystemRuntime, u"System.Reflection.Assembly");
    const auto ResolveEventArgs = i.type_reference(SystemRuntime, u"System.ResolveEventArgs");
    const auto ResolveEventHandler = i.type_reference(SystemRuntime, u"System.ResolveEventHandler");

    const auto sigResolver = sig::static_method(Assembly, {sig::object, ResolveEventArgs});
    const auto Resolver = i.metadata.get(&IMetaDataEmit::DefineMethod,
        mdTokenNil, u"AppMap.ResolveAssembly", mdPublic | mdStatic,
        sigResolver.data(), sigResolver.size(), i.method.code_rva(), miIL | miManaged);


    code += i.create_token_operand_instruction(Cee_Call, i.member_reference(
        AppDomain, u"get_CurrentDomain", sig::static_method(AppDomain, {})));
    code += i.create_instruction(Cee_Ldnull);
    code += i.create_token_operand_instruction(Cee_Ldftn, Resolver);
    code += i.create_token_operand_instruction(Cee_Newobj, i.member_reference(
        ResolveEventHandler, u".ctor", sig::method(sig::Void, { sig::object, sig::native_int })));
    code += i.create_token_operand_instruction(Cee_Callvirt, i.member_reference(
        AppDomain, u"add_AssemblyResolve", sig::method(sig::Void, {ResolveEventHandler})));

    return code;
}

}
