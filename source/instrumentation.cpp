#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ranges.h>
#include "instrumentation.h"

using namespace appmap;

appmap::instrumentation::instruction_sequence appmap::instrumentation::make_call_sig(void *fn, gsl::span<const COR_SIGNATURE> signature) const
{
    mdToken token;
    com::hresult::check(metadata->GetTokenFromSig(signature.data(), signature.size(), &token));
    return {
        factory.create_long_operand_instruction(Cee_Ldc_I8, reinterpret_cast<int64_t>(fn)),
        factory.create_token_operand_instruction(Cee_Calli, token)
    };
}
