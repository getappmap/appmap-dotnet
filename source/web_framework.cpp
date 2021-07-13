#include "cil.h"
#include "event.h"
#include "instrumentation.h"
#include "method.h"
#include "recorder.h"
#include "signature.h"

#include <spdlog/spdlog.h>

namespace sig = appmap::signature;
namespace appmap { namespace web_framework {
    void hello() {
        spdlog::trace("hello ctor");
    }

    auto request(const char *method, const char *path_info) {
        spdlog::trace("request({}, {})", method, path_info);
        auto call = std::make_unique<http_request_event>(method, path_info);
        call_event *ptr = call.get();
        recorder::events.push_back(std::move(call));
        return ptr;
    }

    auto asp_net_build = add_hook(
        "Microsoft.AspNetCore.Builder.ApplicationBuilder.Build", "Microsoft.AspNetCore.Http.dll",
        [](const auto &method) {
            using namespace appmap::cil::ops;

            static bool applied = false;
            if (applied) return false;

            instrumentation instr(method);

            const auto HttpAbstractions = instr.assembly_reference(u"Microsoft.AspNetCore.Http.Abstractions");

            const auto HttpRequest = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.HttpRequest");
            const auto HttpContext = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.HttpContext");
            const auto HttpContext_get_Request = instr.member_reference(HttpContext, u"get_Request", sig::method(HttpRequest, {}));

            const auto PathString = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.PathString");
            const auto RequestDelegate = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.RequestDelegate");

            const auto Task = instr.type_reference(u"System.Runtime", u"System.Threading.Tasks.Task");

            const auto RequestWrapper = instr.define_type(u"AppMap.AspNetCore.RequestWrapper");
            const auto RequestWrapperNext = instr.define_field(RequestWrapper, u"next", sig::field(RequestDelegate));

            const auto RequestWrapperCtor = instr.define_method(RequestWrapper,
                u".ctor", sig::method(sig::Void, {RequestDelegate}),
                { ldarg{0}, ldarg{1}, stfld{RequestWrapperNext} });

            const auto RequestWrapperInvoke = instr.define_method(RequestWrapper,
                u"Invoke", sig::method(Task, {HttpContext}),
                {
                    ldarg{1}, callvirt{HttpContext_get_Request},
                    callvirt{instr.member_reference(HttpRequest, u"get_Method", sig::method(sig::string, {}))},

                    ldarg{1}, callvirt{HttpContext_get_Request},
                    callvirt{instr.member_reference(HttpRequest, u"get_Path", sig::method(sig::value{PathString}, {}))},

                    ldc{request}, calli{instr.native_type(request)},
                    pop,

                    ldarg{0}, ldfld{RequestWrapperNext}, ldarg{1},
                    callvirt{instr.member_reference(RequestDelegate, u"Invoke", sig::method(Task, {HttpContext}))}
                });

            spdlog::trace("request wrappers defined");

            auto code = method.instructions();
            const auto last = code.last_instruction();

            code.insert_before(last, cil::compile({
                newobj{RequestWrapperCtor},
                ldftn{RequestWrapperInvoke},
                newobj{instr.member_reference(RequestDelegate, u".ctor", sig::method(sig::Void, {sig::object, sig::native_int}))}
            }, instr));
            spdlog::trace("request wrappers installed");

            applied = true;
            return true;
        });

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

    void response(const call_event *parent, int code) {
        recorder::events.push_back(std::make_unique<http_response_event>(parent, code));
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
