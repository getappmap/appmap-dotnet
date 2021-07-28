#pragma once
#include <array>
#include <variant>
#include <gsl/gsl-lite.hpp>

#include <clrie/method_info.h>
#include <clrie/instruction_factory.h>

#include <corhdr.h>

#include "cil.h"
#include "signature.h"

namespace appmap {
    template <typename T>
    constexpr COR_SIGNATURE type_signature() {
        if constexpr (std::is_same_v<T, void>) {
            return ELEMENT_TYPE_VOID;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return ELEMENT_TYPE_I4;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return ELEMENT_TYPE_I8;
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return ELEMENT_TYPE_U4;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return ELEMENT_TYPE_U8;
        } else if constexpr (std::is_same_v<T, char *> || std::is_same_v<T, const char *>) {
            return ELEMENT_TYPE_STRING;
        } else if constexpr (std::is_pointer_v<T>) {
            static_assert(sizeof(void *) == 8);
            return ELEMENT_TYPE_I8;
        } else if constexpr (std::is_same_v<T, bool>) {
            return ELEMENT_TYPE_BOOLEAN;
        } else {
            static_assert(std::is_same_v<T, void>, "unhandled native type");
        }
    }

    template <typename F>
    struct func_traits;

    template <typename Ret, typename... Args>
    struct func_traits<Ret(*)(Args...)>
    {
        static constexpr std::size_t arity = sizeof...(Args);
        static constexpr std::array<COR_SIGNATURE, 3 + arity> signature = {
            IMAGE_CEE_UNMANAGED_CALLCONV_STDCALL,
            arity,
            type_signature<Ret>(),
            type_signature<Args>()...
        };
    };

    template <typename C, typename Ret, typename... Args>
    struct func_traits<Ret(C::*)(Args...)>
    {
        using instance_type = C;
        static constexpr std::size_t arity = sizeof...(Args);
        static constexpr std::array<COR_SIGNATURE, 4 + arity> signature = {
            IMAGE_CEE_UNMANAGED_CALLCONV_STDCALL,
            arity + 1,
            type_signature<Ret>,
            type_signature<C*>,
            type_signature<Args>...
        };
    };

    struct instrumentation : public clrie::instruction_factory {
        instrumentation(clrie::method_info method_info) :
            clrie::instruction_factory(method_info.instruction_factory()),
            method(method_info),
            module(method.module_info()),
            module_id(module.module_id()),
            metadata(module.meta_data_emit()),
            type_factory(module.create_type_factory()),
            locals(method.get(&IMethodInfo::GetLocalVariables))
        {}

        clrie::method_info method;
        clrie::module_info module;
        const ModuleID module_id;
        com::ptr<IMetaDataEmit> metadata;
        com::ptr<ITypeCreator> type_factory;
        com::ptr<ILocalVariableCollection> locals;

        static com::ptr<ISignatureBuilder> signature_builder;

        template <class F>
        instruction_sequence make_call(F f) const {
            return make_call_sig(reinterpret_cast<void *>(f), gsl::make_span(func_traits<F>::signature));
        }

        instruction_sequence create_call_to_string(const clrie::type &type) const noexcept;

        // Note capture_value takes a reference; in case of a composite type, it dereferences
        // it to a primitive that can be then passed onto the correct native function.
        // The argument is updated to reflect the resulting simple type.
        instruction_sequence capture_value(clrie::type &type) const noexcept;

        template <typename T>
        uint64_t add_local()
        {
            auto t = type_factory.get(&ITypeCreator::FromCorElement, static_cast<CorElementType>(type_signature<T>()));
            return locals.get(&ILocalVariableCollection::AddLocal, t);
        }

        // emit metadata and return reference tokens

        mdAssemblyRef assembly_reference(const char16_t *assembly);

        mdTypeRef type_reference(mdAssemblyRef assembly, const char16_t *type);
        mdTypeRef type_reference(const char16_t *assembly, const char16_t *type) {
            return type_reference(assembly_reference(assembly), type);
        }

        mdMemberRef member_reference(mdTypeRef type, const char16_t *member,
            gsl::span<const COR_SIGNATURE> signature);

        mdMemberRef member_reference(mdAssemblyRef assembly, const char16_t *type,
            const char16_t *member, gsl::span<const COR_SIGNATURE> signature) {
            return member_reference(type_reference(assembly, type), member, signature);
        }

        mdMemberRef member_reference(const char16_t *assembly, const char16_t *type,
            const char16_t *member, gsl::span<const COR_SIGNATURE> signature) {
            return member_reference(type_reference(assembly, type), member, signature);
        }

        mdTypeDef define_type(const char16_t *name);
        mdFieldDef define_field(mdTypeDef type, const char16_t *name, gsl::span<const COR_SIGNATURE> signature);

        mdMethodDef define_method(
            mdTypeDef type,
            const char16_t *name,
            gsl::span<const COR_SIGNATURE> signature,
            std::initializer_list<appmap::signature::type> locals,
            std::vector<cil::instruction> code
        );

        mdMethodDef define_method(mdTypeDef type, const char16_t *name, gsl::span<const COR_SIGNATURE> signature, std::vector<cil::instruction> code) {
            return define_method(type, name, signature, {}, std::move(code));
        }

        template <class Ret, class... Args>
        mdTypeSpec native_type(Ret (*)(Args...)) {
            constexpr auto sig = func_traits<Ret(*)(Args...)>::signature;
            return metadata.get(&IMetaDataEmit::GetTokenFromSig, sig.data(), sig.size());
        }

        mdTypeSpec type_token(gsl::span<const COR_SIGNATURE> sig) {
            return metadata.get(&IMetaDataEmit::GetTokenFromTypeSpec, sig.data(), sig.size());
        }

    protected:
        instruction_sequence make_call_sig(void *fn, gsl::span<const COR_SIGNATURE> signature) const;
    };
}
