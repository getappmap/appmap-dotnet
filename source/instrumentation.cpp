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

    std::vector<COR_SIGNATURE> signature_of_type(com::ptr<IType> type)
    {
        auto &builder = instrumentation::signature_builder;
        com::hresult::check(builder->Clear());
        com::hresult::check(type->AddToSignature(builder));
        const COR_SIGNATURE *signature = builder.get(&ISignatureBuilder::GetCorSignaturePtr);
        return std::vector<COR_SIGNATURE>(signature, signature + builder.get(&ISignatureBuilder::GetSize));
    }
}

clrie::instruction_factory::instruction_sequence appmap::instrumentation::create_call_to_string(com::ptr<IType> type) const noexcept
{
    static std::unordered_map<ModuleID, mdMemberRef> object_to_string_refs;

    mdTypeSpec type_token;

    const auto signature = signature_of_type(type);

    spdlog::trace("return type signature: {}", signature);

    try {
        type_token = type.as<ITokenType>().get(&ITokenType::GetToken);
    } catch (const std::system_error &) {
        type_token = metadata.get(&IMetaDataEmit::GetTokenFromTypeSpec, signature.data(), signature.size());
    }

    if (object_to_string_refs.find(module_id) == object_to_string_refs.end()) {
        auto system_runtime = find_assembly_ref(module.meta_data_assembly_import(), u"System.Runtime");
        auto import = module.meta_data_import();
        auto system_object = metadata.get(&IMetaDataEmit::DefineTypeRefByName, system_runtime, u"System.Object");
        constexpr COR_SIGNATURE to_string_sig[] = { IMAGE_CEE_CS_CALLCONV_DEFAULT_HASTHIS, 0, ELEMENT_TYPE_STRING };
        object_to_string_refs[module_id] = metadata.get(&IMetaDataEmit::DefineMemberRef, system_object, u"ToString", to_string_sig, sizeof(to_string_sig));
    }

    instruction_sequence result;

    const bool is_ref = signature[0] == ELEMENT_TYPE_CLASS
        || (signature[0] == ELEMENT_TYPE_GENERICINST && signature[1] == ELEMENT_TYPE_CLASS);

    const bool is_nullable = !is_ref && [&type, &signature]() {
        if (signature[0] != ELEMENT_TYPE_GENERICINST) return false;
        auto rel_type = type.as<ICompositeType>().get(&ICompositeType::GetRelatedType);
        return std::string(rel_type.get(&IType::GetName)) == "System.Nullable`1";
    }();

    const bool is_mvar = signature[0] == ELEMENT_TYPE_MVAR;

    auto end = create_instruction(Cee_Nop); // just to have a place to branch to

    if (is_nullable || is_ref || is_mvar) { // add check for null
        if (!is_ref) // boxing a Nullable will automatically pull the null ref
            result += create_token_operand_instruction(Cee_Box, type_token);
        result += {
            create_instruction(Cee_Dup),
            create_instruction(Cee_Ldnull),
            create_branch_instruction(Cee_Beq_S, end),
        };
    } else {
        auto locals = method.get(&IMethodInfo::GetLocalVariables);
        auto local = locals.get(&ILocalVariableCollection::AddLocal, type);
        result += {
            create_store_local_instruction(local),
            create_load_local_address_instruction(local),
            create_token_operand_instruction(Cee_Constrained, type_token)
        };
    }

    result += create_token_operand_instruction(Cee_Callvirt, object_to_string_refs[module_id]);

    if (is_nullable || is_ref || is_mvar)
        result += end;

    return result;
}
