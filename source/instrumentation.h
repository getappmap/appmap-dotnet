#pragma once
#include <array>
#include <variant>
#include <gsl/gsl-lite.hpp>

#include <clrie/method_info.h>
#include <clrie/instruction_factory.h>

#include <corhdr.h>

namespace appmap {
    template <typename T>
    constexpr COR_SIGNATURE type_signature;

    template <>
    constexpr COR_SIGNATURE type_signature<void> = ELEMENT_TYPE_VOID;

    template <>
    constexpr COR_SIGNATURE type_signature<int32_t> = ELEMENT_TYPE_I4;

    template <>
    constexpr COR_SIGNATURE type_signature<int64_t> = ELEMENT_TYPE_I8;

    template <>
    constexpr COR_SIGNATURE type_signature<uint32_t> = ELEMENT_TYPE_U4;

    template <>
    constexpr COR_SIGNATURE type_signature<uint64_t> = ELEMENT_TYPE_U8;

    template <>
    constexpr COR_SIGNATURE type_signature<char *> = ELEMENT_TYPE_STRING;

    template <>
    constexpr COR_SIGNATURE type_signature<const char *> = ELEMENT_TYPE_STRING;

    static_assert(sizeof(void *) == 8);
    template <typename T>
    constexpr COR_SIGNATURE type_signature<T *> = ELEMENT_TYPE_I8;

    template <>
    constexpr COR_SIGNATURE type_signature<bool> = ELEMENT_TYPE_BOOLEAN;

    template <typename F>
    struct func_traits;

    template <typename Ret, typename... Args>
    struct func_traits<Ret(*)(Args...)>
    {
        static constexpr std::size_t arity = sizeof...(Args);
        static constexpr std::array<COR_SIGNATURE, 3 + arity> signature = {
            IMAGE_CEE_UNMANAGED_CALLCONV_STDCALL,
            arity,
            type_signature<Ret>,
            type_signature<Args>...
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
        instruction_sequence capture_value(const clrie::type &type) const noexcept;

        template <typename T>
        uint64_t add_local()
        {
            auto t = type_factory.get(&ITypeCreator::FromCorElement, static_cast<CorElementType>(type_signature<T>));
            return locals.get(&ILocalVariableCollection::AddLocal, t);
        }

    protected:
        instruction_sequence make_call_sig(void *fn, gsl::span<const COR_SIGNATURE> signature) const;
    };
}
