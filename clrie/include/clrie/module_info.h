#pragma once

#include <InstrumentationEngine.h>
#include "com/ptr.h"
#include <cor.h>

namespace clrie {
    struct module_info : public com::ptr<IModuleInfo>
    {
        using ptr::ptr;

        std::string module_name() {
            return get(&interface_type::GetModuleName);
        }
        std::string full_path() {
            return get(&interface_type::GetFullPath);
        }

        // Get info about the assembly that contains this module
        com::ptr<IAssemblyInfo> assembly_info() {
            return get(&interface_type::GetAssemblyInfo);
        }

        com::ptr<IAppDomainInfo> app_domain_info() {
            return get(&interface_type::GetAppDomainInfo);
        }

        com::ptr<IUnknown> meta_data_import() {
            return get(&interface_type::GetMetaDataImport);
        }
        com::ptr<IUnknown> meta_data_assembly_import() {
            return get(&interface_type::GetMetaDataAssemblyImport);
        }

        // NOTE: It is expected that these fail for winmd files which do not support
        // metadata emit.
        com::ptr<IUnknown> meta_data_emit() {
            return get(&interface_type::GetMetaDataEmit);
        }
        com::ptr<IUnknown> meta_data_assembly_emit() {
            return get(&interface_type::GetMetaDataAssemblyEmit);
        }

        ModuleID module_id() {
            return get(&interface_type::GetModuleID);
        }

        // Get the managed module version id
        GUID mvid() {
            return get(&interface_type::GetMVID);
        }

        bool is_il_only() {
            return get(&interface_type::GetIsILOnly);
        }
        bool is_mscorlib() {
            return get(&interface_type::GetIsMscorlib);
        }

        // Gets a value indicating whether or not this module is a dynamic modules.
        // Note, dynamic modules have a base load address of 0, and any attempts
        // to resolve an address to an RVA in this module will result in an
        // error.
        bool is_dynamic() {
            return get(&interface_type::GetIsDynamic);
        }
        bool is_ngen() {
            return get(&interface_type::GetIsNgen);
        }
        bool is_win_rt() {
            return get(&interface_type::GetIsWinRT);
        }
        bool is64bit() {
            return get(&interface_type::GetIs64bit);
        }

        // Returns the image base for the loaded module. Note, if this module represents a dynamic module,
        // then the image base will be 0.
        LPCBYTE image_base();

        // Return the IMAGE_COR20_HEADER structure
        void cor_header();

        // Returns the modules entrypoint token. If the module has no entrypoint token, returns E_FAIL;
        unsigned int entrypoint_token() {
            return get(&interface_type::GetEntrypointToken);
        }

        // Returns the VS_FIXEDFILEINFO for this module.
        void module_version();

        void request_rejit(mdToken method_token) {
            com::hresult::check(ptr_->RequestRejit(method_token));
        }

        // Creates a type factory associated with this module.
        com::ptr<ITypeCreator> create_type_factory() {
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
