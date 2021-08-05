#include "cil.h"
#include "event.h"
#include "instrumentation.h"
#include "method.h"
#include "recorder.h"
#include "signature.h"

#include <spdlog/spdlog.h>

using namespace appmap;

namespace sig = appmap::signature;
namespace appmap { namespace web_framework {
    auto request(const char *method, const char *path_info) {
        spdlog::trace("request({}, {})", method, path_info);
        auto call = std::make_unique<http_request_event>(current_thread_id(), method, path_info);
        call_event *ptr = call.get();
        recorder::events.push_back(std::move(call));
        return ptr;
    }

    void response(const call_event *parent, int code) {
        spdlog::trace("response({})", code);
        recorder::events.push_back(std::make_unique<http_response_event>(current_thread_id(), parent, code));
    }

    auto asp_net_build = add_hook(
        "Microsoft.AspNetCore.Builder.ApplicationBuilder.Build", "Microsoft.AspNetCore.Http.dll",
        [](const auto &method) {
            using namespace appmap::cil::ops;

            static bool applied = false;
            if (applied) return false;

            instrumentation instr(method);

            const auto SystemRuntime = instr.assembly_reference(u"System.Runtime");
            const auto HttpAbstractions = instr.assembly_reference(u"Microsoft.AspNetCore.Http.Abstractions");

            const auto HttpRequest = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.HttpRequest");
            const auto HttpResponse = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.HttpResponse");
            const auto HttpContext = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.HttpContext");
            const auto HttpContext_get_Request = instr.member_reference(HttpContext, u"get_Request", sig::method(HttpRequest, {}));

            const auto PathString = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.PathString");
            const auto RequestDelegate = instr.type_reference(HttpAbstractions, u"Microsoft.AspNetCore.Http.RequestDelegate");

            const auto Task = instr.type_reference(SystemRuntime, u"System.Threading.Tasks.Task");
            const auto Action = instr.type_reference(SystemRuntime, u"System.Action`1");
            const auto ActionTask = instr.type_token(sig::generic(Action, {Task}));

            const auto ResponseWrapper = instr.define_type(u"AppMap.AspNetCore.ResponseWrapper");
            const auto ResponseWrapperCall = instr.define_field(ResponseWrapper, u"call", sig::field(sig::native_int));
            const auto ResponseWrapperContext = instr.define_field(ResponseWrapper, u"context", sig::field(HttpContext));
            const auto ResponseWrapperCtor = instr.define_method(ResponseWrapper,
                u".ctor", sig::method(sig::Void, { sig::native_int, HttpContext }),
                {
                    ldarg{0}, ldarg{1}, stfld{ResponseWrapperCall},
                    ldarg{0}, ldarg{2}, stfld{ResponseWrapperContext}
                });

            const auto ResponseWrapperInvoke = instr.define_method(ResponseWrapper,
                u"Invoke", sig::method(sig::Void, {Task}),
                {
                    ldarg{0}, ldfld{ResponseWrapperCall},
                    ldarg{0}, ldfld{ResponseWrapperContext},
                    callvirt{instr.member_reference(HttpContext, u"get_Response", sig::method(HttpResponse, {}))},
                    callvirt{instr.member_reference(HttpResponse, u"get_StatusCode", sig::method(sig::int32, {}))},
                    ldc{response}, calli{instr.native_type(response)}
                });

            const auto RequestWrapper = instr.define_type(u"AppMap.AspNetCore.RequestWrapper");
            const auto RequestWrapperNext = instr.define_field(RequestWrapper, u"next", sig::field(RequestDelegate));

            const auto RequestWrapperCtor = instr.define_method(RequestWrapper,
                u".ctor", sig::method(sig::Void, {RequestDelegate}),
                { ldarg{0}, ldarg{1}, stfld{RequestWrapperNext} });

            const auto RequestWrapperInvoke = instr.define_method(RequestWrapper,
                u"Invoke", sig::method(Task, {HttpContext}), {sig::native_int},
                {
                    ldarg{1}, callvirt{HttpContext_get_Request},
                    callvirt{instr.member_reference(HttpRequest, u"get_Method", sig::method(sig::string, {}))},

                    ldarg{1}, callvirt{HttpContext_get_Request},
                    callvirt{instr.member_reference(HttpRequest, u"get_Path", sig::method(sig::value{PathString}, {}))},

                    ldc{request}, calli{instr.native_type(request)}, stloc{0},

                    ldarg{0}, ldfld{RequestWrapperNext}, ldarg{1},
                    callvirt{instr.member_reference(RequestDelegate, u"Invoke", sig::method(Task, {HttpContext}))},

                    ldloc{0}, ldarg{1}, newobj{ResponseWrapperCtor},
                    ldftn{ResponseWrapperInvoke},
                    newobj{instr.member_reference(ActionTask, u".ctor", sig::method(sig::Void, {sig::object, sig::native_int}))},

                    ldc(0x80000), // TaskContinuationOptions.ExecuteSynchronously
                    callvirt{instr.member_reference(Task, u"ContinueWith",
                        sig::method(Task, {
                            sig::generic(Action, {Task}),
                            sig::value{instr.type_reference(SystemRuntime, u"System.Threading.Tasks.TaskContinuationOptions")}
                        }))}
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
}}
