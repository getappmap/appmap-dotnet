#include "cil.h"

namespace appmap::cil {

using inseq = clrie::instruction_factory::instruction_sequence;

struct compiler {
    const clrie::instruction_factory &factory;
    inseq insns;

    compiler(const clrie::instruction_factory &factory): factory(factory) {}

    compiler &operator()(const gsl::span<const instruction> code) {
        for (const auto &i: code)
            std::visit(*this, i);
        return *this;
    }

    void operator()(ldarg arg) {
        insns += factory.create_load_arg_instruction(arg.index);
    }

    void operator()(token_op op) {
        insns += factory.create_token_operand_instruction(op.op, op.token);
    }

    void operator()(long_op op) {
        insns += factory.create_long_operand_instruction(op.op, op.value);
    }

    void operator()(op code) {
        insns += factory.create_instruction(code.op);
    }

    operator inseq() && {
        return std::move(insns);
    }
};

inseq compile(const gsl::span<const instruction> code, const clrie::instruction_factory &factory)
{
    return std::move(compiler(factory)(code));
}

clrie::instruction_factory::instruction_sequence compile(std::initializer_list<const instruction> code, const clrie::instruction_factory &factory) {
    return compile(gsl::make_span(code), factory);
}

}
