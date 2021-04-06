#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ranges.h>
#include <utf8.h>
#include "instrumentation.h"

using namespace appmap;

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
    mdAssemblyRef find_assembly_ref(com::ptr<IMetaDataAssemblyImport> import, std::u16string name)
    {
        HCORENUM corenum = nullptr;
        std::vector<mdAssemblyRef> assemblies(50);
        ULONG count;
        com::hresult::check(import->EnumAssemblyRefs(&corenum, assemblies.data(), assemblies.size(), &count));
        import->CloseEnum(corenum);
        assert(count < assemblies.size());
        assemblies.resize(count);

        for (auto assembly: assemblies) {
            char16_t assembly_name[50];
            com::hresult::check(import->GetAssemblyRefProps(assembly, nullptr, nullptr, assembly_name, 50, &count, nullptr, nullptr, nullptr, nullptr));
            assert(count < 50);
            if (name == assembly_name)
                return assembly;
        }

        return mdAssemblyRefNil;
    }
}

clrie::instruction_factory::instruction appmap::instrumentation::create_call_to_string() const
{
    static std::unordered_map<ModuleID, mdMemberRef> object_to_string_refs;

    if (object_to_string_refs.find(module_id) == object_to_string_refs.end()) {
        auto system_runtime = find_assembly_ref(module.meta_data_assembly_import(), u"System.Runtime");
        auto import = module.meta_data_import();
        auto system_object = import.get<mdTypeRef>(&IMetaDataImport::FindTypeRef, system_runtime, u"System.Object");
        object_to_string_refs[module_id] = import.get<mdMemberRef>(&IMetaDataImport::FindMemberRef, system_object, u"ToString", nullptr, 0);
    }

    return create_token_operand_instruction(Cee_Callvirt, object_to_string_refs[module_id]);
}
