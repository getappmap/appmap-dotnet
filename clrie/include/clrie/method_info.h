#pragma once

#include <InstrumentationEngine.h>
#include "com/ptr.h"
#include "cor.h"
#include "clrie/instruction_graph.h"
#include "clrie/instruction_factory.h"
#include "clrie/module_info.h"

namespace clrie {
    struct method_info : public com::ptr<IMethodInfo> {
        method_info(IMethodInfo *info = nullptr) noexcept : com::ptr<IMethodInfo>(info) {}

        clrie::module_info module_info() {
            return get(&interface_type::GetModuleInfo);
        }

        std::string name() {
            return get(&interface_type::GetName);
        }
        std::string full_name() {
            return get(&interface_type::GetFullName);
        }

        // Obtain the graph of instructions.  Manipulating this graph will result in changes to the code
        // a the end of jit or rejit.
        instruction_graph instructions() {
            return get(&interface_type::GetInstructions);
        }

        // Create a local variable builder. Can be used to add new locals.
        com::ptr<ILocalVariableCollection> local_variables() {
            return get(&interface_type::GetLocalVariables);
        }

        ClassID class_id() {
            return get(&interface_type::GetClassId);
        }
        FunctionID function_id() {
            return get(&interface_type::GetFunctionId);
        }
        mdToken method_token() {
            return get(&interface_type::GetMethodToken);
        }
        unsigned int generic_parameter_count() {
            return get(&interface_type::GetGenericParameterCount);
        }
        bool is_static() {
            return get(&interface_type::GetIsStatic);
        }
        bool is_public() {
            return get(&interface_type::GetIsPublic);
        }
        bool is_private() {
            return get(&interface_type::GetIsPrivate);
        }
        bool is_property_getter() {
            return get(&interface_type::GetIsPropertyGetter);
        }
        bool is_property_setter() {
            return get(&interface_type::GetIsPropertySetter);
        }
        bool is_finalizer() {
            return get(&interface_type::GetIsFinalizer);
        }
        bool is_constructor() {
            return get(&interface_type::GetIsConstructor);
        }
        bool is_static_constructor() {
            return get(&interface_type::GetIsStaticConstructor);
        }

        com::ptr<IEnumMethodParameters> parameters() {
            return get(&interface_type::GetParameters);
        }
        com::ptr<IType> declaring_type() {
            return get(&interface_type::GetDeclaringType);
        }
        com::ptr<IType> return_type() {
            return get(&interface_type::GetReturnType);
        }

        // Get the cor signature for the method. Passing NULL for corSignature will return the required
        // buffer size in pcbSignature
        std::vector<COR_SIGNATURE> cor_signature() {
            using com::hresult::check;
            DWORD size;
            std::vector<COR_SIGNATURE> result;
            check(ptr_->GetCorSignature(0, nullptr, &size));
            result.resize(size);
            check(ptr_->GetCorSignature(size, result.data(), &size));
            return result;
        }

        mdToken local_var_sig_token() {
            return get(&interface_type::GetLocalVarSigToken);
        }
        void set_local_var_sig_token(mdToken token) {
            com::hresult::check(ptr_->SetLocalVarSigToken(token));
        }

        /*CorMethodAttr*/ DWORD attributes() {
            return get(&interface_type::GetAttributes);
        }

        unsigned int rejit_code_gen_flags() {
            return get(&interface_type::GetRejitCodeGenFlags);
        }
        unsigned int code_rva() {
            return get(&interface_type::GetCodeRva);
        }
        /*CorMethodImpl*/ UINT method_impl_flags() {
            return get(&interface_type::MethodImplFlags);
        }

        // Allow callers to adjust optimizations during a rejit. For instance, disable all optimizations against a method
        void set_rejit_code_gen_flags(unsigned int dw_flags) {
            return com::hresult::check(ptr_->SetRejitCodeGenFlags(dw_flags));
        }

        com::ptr<IExceptionSection> exception_section() {
            return get(&interface_type::GetExceptionSection);
        }

        clrie::instruction_factory instruction_factory() {
            return get(&interface_type::GetInstructionFactory);
        }

        // Return the running count of the number of rejits for this methodinfo
        unsigned int rejit_count() {
            return get(&interface_type::GetRejitCount);
        }

        // Obtain the max stack value for the method. This is calculated using the instruction graph
        unsigned int max_stack() {
            return get(&interface_type::GetMaxStack);
        }

        // Get an instance of single return default transformation.
        // Ieally we need to have a generic method that will intantiate this class - it actually independent from the specific method
        com::ptr<ISingleRetDefaultInstrumentation> single_ret_default_instrumentation() {
            return get(&interface_type::GetSingleRetDefaultInstrumentation);
        }
    };
}
