#include "instrumentation.h"
#include "method.h"

#include <spdlog/spdlog.h>

namespace {
    constexpr USHORT any_version = -1;
    constexpr ASSEMBLYMETADATA any_metadata = {
        any_version, any_version, any_version, any_version,
        nullptr, 0, nullptr, 0, nullptr, 0
    };
}

namespace appmap { namespace web_framework {
    auto asp_net_build = add_hook(
        "Microsoft.AspNetCore.Builder.ApplicationBuilder.Build", "Microsoft.AspNetCore.Http.dll",
        [](const auto &method) {
            static bool applied = false;
            if (applied) return false;

            instrumentation instr(method);

            const auto httpRequestDelegate = instr.type_reference(u"Microsoft.AspNetCore.Http.Abstractions", u"Microsoft.AspNetCore.Http.RequestDelegate");

            COR_SIGNATURE sig[12] = { IMAGE_CEE_CS_CALLCONV_DEFAULT, 1, ELEMENT_TYPE_CLASS };
            COR_SIGNATURE *pSig = sig + 3;
            pSig += CorSigCompressToken(httpRequestDelegate, pSig);
            *(pSig++) = ELEMENT_TYPE_CLASS;
            pSig += CorSigCompressToken(httpRequestDelegate, pSig);

            const auto methodRef = instr.member_reference(u"AppMap.AspNetCore", u"AppMap.AspNetCore", u"Wrap", gsl::span(sig, pSig - sig));

            auto code = method.instructions();
            const auto last = code.last_instruction();
            code.insert_before(last, instr.create_token_operand_instruction(Cee_Call, methodRef));

            applied = true;
            return true;
        });

    int request(const char *method, const char *path_info) {
        spdlog::info("request({}, {})", method, path_info);
        return 31337;
    }

    auto request_hook = add_hook(
        "AppMap.AspNetCore.HttpRequest",
        [](const auto &method) {
            instrumentation instr(method);
            auto code = method.instructions();

            code.remove_all();
            const auto last = instr.create_instruction(Cee_Ret);
            code.insert_after(nullptr, last);

            code.insert_before(last, instr.create_load_arg_instruction(0));
            code.insert_before(last, instr.create_load_arg_instruction(1));
            code.insert_before(last, instr.make_call(request));

            return true;
        });

    void response(int parent, int code) {
        spdlog::info("response({}, {})", parent, code);
    }

    auto response_hook = add_hook(
        "AppMap.AspNetCore.HttpResponse",
        [](const auto &method) {
            instrumentation instr(method);
            auto code = method.instructions();

            code.remove_all();
            const auto last = instr.create_instruction(Cee_Ret);
            code.insert_after(nullptr, last);

            code.insert_before(last, instr.create_load_arg_instruction(0));
            code.insert_before(last, instr.create_load_arg_instruction(1));
            code.insert_before(last, instr.make_call(response));

            return true;
        });
}}