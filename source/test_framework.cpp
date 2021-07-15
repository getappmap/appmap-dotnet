
#include <clrie/instruction_factory.h>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

#include "cil.h"
#include "config.h"
#include "generation.h"
#include "instrumentation.h"
#include "method.h"
#include "recorder.h"
#include "signature.h"

using namespace appmap;

namespace appmap { namespace test_framework {
    std::string case_name;

    void startCase(char *full_name) {
        std::lock_guard lock(appmap::recorder::mutex);
        spdlog::debug("Test case start: {}", full_name);
        case_name = full_name;
        appmap::recorder::events.clear();
    }

    void endCase() {
        namespace fs = std::filesystem;
        const config &c = appmap::config::instance();
        auto [stream, path] = c.appmap_output_stream(case_name);
        std::lock_guard lock(appmap::recorder::mutex);
        *stream << generate(appmap::recorder::events, c.generate_classmap) << std::endl;
        spdlog::info("Wrote {}", path.string());
    }

    auto start_case_recorder = add_hook(
        "Microsoft.VisualStudio.TestPlatform.CrossPlatEngine.Adapter.TestExecutionRecorder.RecordStart",
        [](const clrie::method_info &method)
    {
        spdlog::info("detected Visual Studio Test Platform, recording tests");

        instrumentation instr(method);
        clrie::instruction_graph code = method.instructions();
        const auto first = code.first_instruction();

        const auto TestCase = instr.type_reference(
            u"Microsoft.VisualStudio.TestPlatform.ObjectModel",
            u"Microsoft.VisualStudio.TestPlatform.ObjectModel.TestCase"
        );

        using namespace appmap::cil::ops;
        namespace sig = appmap::signature;
        code.insert_before(first, cil::compile({
            ldarg{1}, callvirt{instr.member_reference(TestCase,
                u"get_DisplayName", sig::method(sig::string, {}))},
            ldc{startCase}, calli{instr.native_type(startCase)}
        }, instr));

        return true;
    });

    auto end_case_recorder = add_hook(
        "Microsoft.VisualStudio.TestPlatform.CrossPlatEngine.Adapter.TestExecutionRecorder.RecordEnd",
        [](const clrie::method_info &method)
    {
        instrumentation instr(method);
        clrie::instruction_graph code = method.instructions();
        const auto first = code.first_instruction();
        code.insert_before(first, instr.make_call(endCase));

        return true;
    });

    auto xunit_runner = add_hook(
        "Xunit.Runner.VisualStudio.RunSettings.Parse",
        [](const clrie::method_info &method)
    {
        spdlog::info("detected Xunit, disabling test parallelization");

        instrumentation instr(method);
        clrie::instruction_graph code = method.instructions();
        const auto last = code.last_instruction();

        const auto RunSettings = method.declaring_type().as<ITokenType>()
            .get(&ITokenType::GetToken);

        namespace sig = appmap::signature;
        using namespace appmap::cil::ops;
        code.insert_before(last, cil::compile({
            dup, ldc{1}, callvirt{instr.member_reference(
                RunSettings,
                u"set_DisableParallelization",
                sig::method(sig::Void, {sig::boolean})
            )}
        }, instr));

        return true;
    });
} }
