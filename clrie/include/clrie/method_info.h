#pragma once

#include <no_sal.h>
#include <InstrumentationEngine.h>
#include "com/ptr.h"
#include "cor.h"
#include "clrie/instruction_graph.h"
#include "clrie/instruction_factory.h"
#include "clrie/module_info.h"

template<>
constexpr GUID com::guid_of<IMetaDataEmit>() noexcept {
    using namespace com::literals;
    return "{BA3FEE4C-ECB9-4e41-83B7-183FA41CD859}"_guid;
}

namespace clrie {
    struct method_info : public com::ptr<IMethodInfo> {
        method_info(IMethodInfo *info = nullptr) noexcept : com::ptr<IMethodInfo>(info) {}

        clrie::module_info module_info() const {
            return mut().get(&interface_type::GetModuleInfo);
        }

        std::string name() const {
            return mut().get(&interface_type::GetName);
        }
        std::string full_name() const {
            return mut().get(&interface_type::GetFullName);
        }

        // Obtain the graph of instructions.  Manipulating this graph will result in changes to the code
        // a the end of jit or rejit.
        instruction_graph instructions() const {
            return mut().get(&interface_type::GetInstructions);
        }

        // Create a local variable builder. Can be used to add new locals.
        com::ptr<ILocalVariableCollection> local_variables() const {
            return mut().get(&interface_type::GetLocalVariables);
        }

        ClassID class_id() const {
            return mut().get(&interface_type::GetClassId);
        }
        FunctionID function_id() const {
            return mut().get(&interface_type::GetFunctionId);
        }
        mdToken method_token() const {
            return mut().get(&interface_type::GetMethodToken);
        }
        unsigned int generic_parameter_count() const {
            return mut().get(&interface_type::GetGenericParameterCount);
        }
        bool is_static() const {
            return mut().get(&interface_type::GetIsStatic);
        }
        bool is_public() const {
            return mut().get(&interface_type::GetIsPublic);
        }
        bool is_private() const {
            return mut().get(&interface_type::GetIsPrivate);
        }
        bool is_property_getter() const {
            return mut().get(&interface_type::GetIsPropertyGetter);
        }
        bool is_property_setter() const {
            return mut().get(&interface_type::GetIsPropertySetter);
        }
        bool is_finalizer() const {
            return mut().get(&interface_type::GetIsFinalizer);
        }
        bool is_constructor() const {
            return mut().get(&interface_type::GetIsConstructor);
        }
        bool is_static_constructor() const {
            return mut().get(&interface_type::GetIsStaticConstructor);
        }

        com::ptr<IEnumMethodParameters> parameters() const {
            return mut().get(&interface_type::GetParameters);
        }
        com::ptr<IType> declaring_type() const {
            return mut().get(&interface_type::GetDeclaringType);
        }
        com::ptr<IType> return_type() const {
            return mut().get(&interface_type::GetReturnType);
        }

        // Get the cor signature for the method. Passing NULL for corSignature will return the required
        // buffer size in pcbSignature
        std::vector<COR_SIGNATURE> cor_signature() const {
            using com::hresult::check;
            DWORD size;
            std::vector<COR_SIGNATURE> result;
            check(ptr_->GetCorSignature(0, nullptr, &size));
            result.resize(size);
            check(ptr_->GetCorSignature(size, result.data(), &size));
            return result;
        }

        mdToken local_var_sig_token() const {
            return mut().get(&interface_type::GetLocalVarSigToken);
        }
        void set_local_var_sig_token(mdToken token) {
            com::hresult::check(ptr_->SetLocalVarSigToken(token));
        }

        /*CorMethodAttr*/ DWORD attributes() const {
            return mut().get(&interface_type::GetAttributes);
        }

        unsigned int rejit_code_gen_flags() const {
            return mut().get(&interface_type::GetRejitCodeGenFlags);
        }
        unsigned int code_rva() const {
            return mut().get(&interface_type::GetCodeRva);
        }
        /*CorMethodImpl*/ UINT method_impl_flags() const {
            return mut().get(&interface_type::MethodImplFlags);
        }

        // Allow callers to adjust optimizations during a rejit. For instance, disable all optimizations against a method
        void set_rejit_code_gen_flags(unsigned int dw_flags) {
            return com::hresult::check(ptr_->SetRejitCodeGenFlags(dw_flags));
        }

        com::ptr<IExceptionSection> exception_section() const {
            return mut().get(&interface_type::GetExceptionSection);
        }

        clrie::instruction_factory instruction_factory() const {
            return mut().get(&interface_type::GetInstructionFactory);
        }

        // Return the running count of the number of rejits for this methodinfo
        unsigned int rejit_count() const {
            return mut().get(&interface_type::GetRejitCount);
        }

        // Obtain the max stack value for the method. This is calculated using the instruction graph
        unsigned int max_stack() const {
            return mut().get(&interface_type::GetMaxStack);
        }

        // Get an instance of single return default transformation.
        // Ieally we need to have a generic method that will intantiate this class - it actually independent from the specific method
        com::ptr<ISingleRetDefaultInstrumentation> single_ret_default_instrumentation() const {
            return mut().get(&interface_type::GetSingleRetDefaultInstrumentation);
        }
    };
}
