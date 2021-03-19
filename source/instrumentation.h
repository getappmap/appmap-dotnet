#pragma once
#include <array>
#include <gsl/gsl-lite.hpp>

#include <clrie/instruction_factory.h>

#include <corhdr.h>

namespace {
    template <typename T>
    constexpr COR_SIGNATURE type_signature;

    template <>
    constexpr COR_SIGNATURE type_signature<void> = ELEMENT_TYPE_VOID;

    template <>
    constexpr COR_SIGNATURE type_signature<int32_t> = ELEMENT_TYPE_I4;

    template <>
    constexpr COR_SIGNATURE type_signature<uint32_t> = ELEMENT_TYPE_U4;

    template <>
    constexpr COR_SIGNATURE type_signature<char *> = ELEMENT_TYPE_STRING;

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
}

namespace appmap {
    struct instrumentation {
        mutable clrie::instruction_factory factory;
        mutable com::ptr<IMetaDataEmit> metadata;

        using instruction = com::ptr<IInstruction>;
        using instruction_sequence = std::vector<instruction>;

        template <class F>
        instruction_sequence make_call(F f) const {
            return make_call_sig(reinterpret_cast<void *>(f), gsl::make_span(func_traits<F>::signature));
        }

    protected:
        instruction_sequence make_call_sig(void *fn, gsl::span<const COR_SIGNATURE> signature) const;
    };
}
