#pragma once

#include <InstrumentationEngine.h>
#include "com/ptr.h"
#include <cor.h>

template<>
constexpr GUID com::guid_of<IMetaDataEmit>() noexcept {
    using namespace com::literals;
    return "{BA3FEE4C-ECB9-4e41-83B7-183FA41CD859}"_guid;
}

template<>
constexpr GUID com::guid_of<IMetaDataAssemblyEmit>() noexcept {
    using namespace com::literals;
    return "{211EF15B-5317-4438-B196-DEC87B887693}"_guid;
}

template<>
constexpr GUID com::guid_of<IMetaDataImport>() noexcept {
    using namespace com::literals;
    return "{7DAC8207-D3AE-4c75-9B67-92801A497D44}"_guid;
}

template<>
constexpr GUID com::guid_of<IMetaDataAssemblyImport>() noexcept {
    using namespace com::literals;
    return "{EE62470B-E94B-424e-9B7C-2F00C9249F93}"_guid;
}

namespace clrie {
    struct module_info : public com::ptr<IModuleInfo>
    {
        using ptr::ptr;
        module_info(ptr &&info) : ptr(std::move(info)) {}

        std::string module_name() const {
            return get(&interface_type::GetModuleName);
        }
        std::string full_path() const {
            return get(&interface_type::GetFullPath);
        }

        // Get info about the assembly that contains this module
        com::ptr<IAssemblyInfo> assembly_info() const {
            return get(&interface_type::GetAssemblyInfo);
        }

        com::ptr<IAppDomainInfo> app_domain_info() const {
            return get(&interface_type::GetAppDomainInfo);
        }

        com::ptr<IMetaDataImport> meta_data_import() const {
            return get(&interface_type::GetMetaDataImport).as<IMetaDataImport>();
        }
        com::ptr<IMetaDataAssemblyImport> meta_data_assembly_import() const {
            return get(&interface_type::GetMetaDataAssemblyImport).as<IMetaDataAssemblyImport>();
        }

        // NOTE: It is expected that these fail for winmd files which do not support
        // metadata emit.
        com::ptr<IMetaDataEmit> meta_data_emit() const {
            return get(&interface_type::GetMetaDataEmit).as<IMetaDataEmit>();
        }
        com::ptr<IMetaDataAssemblyEmit> meta_data_assembly_emit() const {
            return get(&interface_type::GetMetaDataAssemblyEmit).as<IMetaDataAssemblyEmit>();
        }

        ModuleID module_id() const {
            return get(&interface_type::GetModuleID);
        }

        // Get the managed module version id
        GUID mvid() const {
            return get(&interface_type::GetMVID);
        }

        bool is_il_only() const {
            return get(&interface_type::GetIsILOnly);
        }
        bool is_mscorlib() const {
            return get(&interface_type::GetIsMscorlib);
        }

        // Gets a value indicating whether or not this module is a dynamic modules.
        // Note, dynamic modules have a base load address of 0, and any attempts
        // to resolve an address to an RVA in this module will result in an
        // error.
        bool is_dynamic() const {
            return get(&interface_type::GetIsDynamic);
        }
        bool is_ngen() const {
            return get(&interface_type::GetIsNgen);
        }
        bool is_win_rt() const {
            return get(&interface_type::GetIsWinRT);
        }
        bool is64bit() const {
            return get(&interface_type::GetIs64bit);
        }

        // Returns the image base for the loaded module. Note, if this module represents a dynamic module,
        // then the image base will be 0.
        LPCBYTE image_base();

        // Return the IMAGE_COR20_HEADER structure
        void cor_header();

        // Returns the modules entrypoint token. If the module has no entrypoint token, returns E_FAIL;
        unsigned int entrypoint_token() const {
            return get(&interface_type::GetEntrypointToken);
        }

        // Returns the VS_FIXEDFILEINFO for this module.
        void module_version();

        void request_rejit(mdToken method_token) const {
            com::hresult::check(ptr_->RequestRejit(method_token));
        }

        // Creates a type factory associated with this module.
        com::ptr<ITypeCreator> create_type_factory() const {
            return get(&interface_type::CreateTypeFactory);
        }

        // Allow instrumentation methods to receive an instance of IMethodInfo for a FunctionID.
        // This method info is only for obtaining information about the method and cannot be used
        // for instrumenation.
        com::ptr<IMethodInfo> method_info_by_id(FunctionID function_id);

        // Allow instrumentation methods to receive an instance of IMethodInfo for a token.
        // This method info is only for obtaining information about the method and cannot be used
        // for instrumenation.
        com::ptr<IMethodInfo> method_info_by_token(mdToken token);

        void import_module(IUnknown* pSourceModuleMetadataImport, LPCBYTE* pSourceImage);
    };
}
