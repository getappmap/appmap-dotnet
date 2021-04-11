#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ranges.h>
#include <utf8.h>
#include "instrumentation.h"

using namespace appmap;

com::ptr<ISignatureBuilder> appmap::instrumentation::signature_builder = nullptr;

appmap::instrumentation::instruction_sequence appmap::instrumentation::make_call_sig(void *fn, gsl::span<const COR_SIGNATURE> signature) const
{
    mdToken token;
    com::hresult::check(metadata->GetTokenFromSig(signature.data(), signature.size(), &token));
    return {
        create_long_operand_instruction(Cee_Ldc_I8, reinterpret_cast<int64_t>(fn)),
        create_token_operand_instruction(Cee_Calli, token)
    };
}

namespace {
    template <typename F>
    struct [[maybe_unused]] scope_guard {
        scope_guard(F fun) : f(fun) {}
        ~scope_guard() { f(); }

    private:
        F f;
    };

    mdAssemblyRef find_assembly_ref(com::ptr<IMetaDataAssemblyImport> import, std::u16string name)
    {
        HCORENUM corenum = nullptr;
        scope_guard enum_closer{ [&corenum, &import]() { if (corenum) import->CloseEnum(corenum); }};

        constexpr auto max_asms = 50;
        ULONG count = max_asms;
        while (count == max_asms) {
            std::vector<mdAssemblyRef> assemblies(max_asms);
            com::hresult::check(import->EnumAssemblyRefs(&corenum, assemblies.data(), assemblies.size(), &count));
            assemblies.resize(count);

            for (auto assembly: assemblies) {
                char16_t assembly_name[50];
                com::hresult::check(import->GetAssemblyRefProps(assembly, nullptr, nullptr, assembly_name, 50, &count, nullptr, nullptr, nullptr, nullptr));
                assert(count < 50);
                if (name == assembly_name)
                    return assembly;
            }
        }

        return mdAssemblyRefNil;
    }
}

clrie::instruction_factory::instruction_sequence appmap::instrumentation::create_call_to_string(com::ptr<IType> type) const noexcept
{
    static std::unordered_map<ModuleID, mdMemberRef> object_to_string_refs;

    mdTypeSpec type_token;

    try {
        type_token = type.as<ITokenType>().get(&ITokenType::GetToken);
    } catch (const std::system_error &) {
        // not a token type, we have to generate a signature and emit metadata token
        com::hresult::check(signature_builder->Clear());
        com::hresult::check(type->AddToSignature(signature_builder));
        type_token = metadata.get<mdTypeSpec>(
            &IMetaDataEmit::GetTokenFromTypeSpec,
            signature_builder.get<const COR_SIGNATURE *>(&ISignatureBuilder::GetCorSignaturePtr),
            signature_builder.get(&ISignatureBuilder::GetSize)
        );
    }

    if (object_to_string_refs.find(module_id) == object_to_string_refs.end()) {
        auto system_runtime = find_assembly_ref(module.meta_data_assembly_import(), u"System.Runtime");
        auto import = module.meta_data_import();
        auto system_object = metadata.get<mdTypeRef>(&IMetaDataEmit::DefineTypeRefByName, system_runtime, u"System.Object");
        constexpr COR_SIGNATURE to_string_sig[] = { IMAGE_CEE_CS_CALLCONV_DEFAULT_HASTHIS, 0, ELEMENT_TYPE_STRING };
        object_to_string_refs[module_id] = metadata.get<mdMemberRef>(&IMetaDataEmit::DefineMemberRef, system_object, u"ToString", to_string_sig, sizeof(to_string_sig));
    }

    com::ptr<ILocalVariableCollection> local_variables = method.get(&IMethodInfo::GetLocalVariables);
    DWORD local = local_variables.get<DWORD>(&ILocalVariableCollection::AddLocal, type);

    return {
        create_store_local_instruction(local),
        create_load_local_address_instruction(local),
        create_token_operand_instruction(Cee_Constrained, type_token),
        create_token_operand_instruction(Cee_Callvirt, object_to_string_refs[module_id])
    };
}
