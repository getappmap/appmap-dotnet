#pragma once

#include <variant>

#include <gsl/gsl-lite.hpp>

#include <clrie/instruction_factory.h>

namespace appmap { namespace cil {

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

template <ILOrdinalOpcode Op>
struct token_opcode: token_op {
    token_opcode(mdToken token): token_op{Op, token} {}
};

namespace ops {
    struct ldarg { int index; };
    struct ldloc { int index; };
    struct stloc { int index; };


    using ldfld = token_opcode<Cee_Ldfld>;
    using stfld = token_opcode<Cee_Stfld>;
    using ldftn = token_opcode<Cee_Ldftn>;
    using call = token_opcode<Cee_Call>;
    using calli = token_opcode<Cee_Calli>;
    using callvirt = token_opcode<Cee_Callvirt>;
    using newobj = token_opcode<Cee_Newobj>;

    constexpr inline auto ldnull = op{Cee_Ldnull};
    constexpr inline auto pop = op{Cee_Pop};

    struct ldc: long_op {
        ldc(uint64_t val): long_op{Cee_Ldc_I8, val} {}

        template <typename Ret, typename... Args>
        ldc(Ret(*fn)(Args...)): ldc(reinterpret_cast<uint64_t>(fn)) {}
    };
}

using instruction = std::variant<ops::ldarg, ops::ldloc, ops::stloc, op, token_op, long_op>;

clrie::instruction_factory::instruction_sequence compile(const gsl::span<const instruction> code, const clrie::instruction_factory &factory);
clrie::instruction_factory::instruction_sequence compile(std::initializer_list<const instruction> code, const clrie::instruction_factory &factory);

}}
