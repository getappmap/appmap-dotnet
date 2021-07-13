#pragma once

#include <variant>

#include <gsl/gsl-lite.hpp>

#include <clrie/instruction_factory.h>

namespace appmap { namespace cil {

struct ldarg {
    int index;
};

struct op {
    ILOrdinalOpcode op;
};

struct token_op {
    ILOrdinalOpcode op;
    mdToken token;

};

struct long_op {
    ILOrdinalOpcode op;
    uint64_t value;
};

using instruction = std::variant<ldarg, op, token_op, long_op>;

namespace detail {

template <ILOrdinalOpcode Op>
struct token_opcode: token_op {
    token_opcode(mdToken token): token_op{Op, token} {}
};

}

namespace ops {
    using appmap::cil::ldarg;

    using ldfld = detail::token_opcode<Cee_Ldfld>;
    using stfld = detail::token_opcode<Cee_Stfld>;
    using ldftn = detail::token_opcode<Cee_Ldftn>;
    using call = detail::token_opcode<Cee_Call>;
    using calli = detail::token_opcode<Cee_Calli>;
    using callvirt = detail::token_opcode<Cee_Callvirt>;
    using newobj = detail::token_opcode<Cee_Newobj>;

    constexpr inline auto pop = op{Cee_Pop};

    struct ldc: long_op {
        ldc(uint64_t val): long_op{Cee_Ldc_I8, val} {}

        template <typename Ret, typename... Args>
        ldc(Ret(*fn)(Args...)): ldc(reinterpret_cast<uint64_t>(fn)) {}
    };
}

clrie::instruction_factory::instruction_sequence compile(const gsl::span<const instruction> code, const clrie::instruction_factory &factory);
clrie::instruction_factory::instruction_sequence compile(std::initializer_list<const instruction> code, const clrie::instruction_factory &factory);

}}
