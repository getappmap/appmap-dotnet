#pragma once
#include <array>
#include <gsl/gsl-lite.hpp>

#include <clrie/instruction_factory.h>

#include <corhdr.h>

namespace {
    template <typename T>
    constexpr COR_SIGNATURE type_signature() noexcept;

    template <>
    constexpr COR_SIGNATURE type_signature<void>() noexcept {
        return ELEMENT_TYPE_VOID;
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
}

namespace appmap {
    struct instrumentation {
        mutable clrie::instruction_factory factory;
        mutable com::ptr<IMetaDataEmit> metadata;

        using instruction = com::ptr<IInstruction>;
        using instruction_sequence = std::vector<instruction>;

        template <class F>
        instruction_sequence make_call(F f) const {
            return make_call(reinterpret_cast<void *>(f), gsl::make_span(func_traits<F>::signature));
        }

    protected:
        instruction_sequence make_call(void *fn, gsl::span<const COR_SIGNATURE> signature) const;
    };
}
