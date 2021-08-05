#include <spdlog/spdlog.h>
#include <string>

#include <utf8.h>

#include "generation.h"
#include "method.h"
#include "instrumentation.h"
#include <fstream>

using namespace appmap;

com::ptr<ICorProfilerInfo> appmap::instrumentation_method::profiler_info = nullptr;

void appmap::instrumentation_method::initialize(com::ptr<IProfilerManager> manager)
{
    spdlog::debug("initialize()");
    assert(profiler_info == nullptr);
    profiler_info = manager.get(&IProfilerManager::GetCorProfilerInfo);
    instrumentation::signature_builder = manager.get(&IProfilerManager::CreateSignatureBuilder);
}

void appmap::instrumentation_method::on_shutdown()
{
    spdlog::debug("on_shutdown()");
    if (auto f = config.module_list_stream()) {
        for (const auto &mod : modules) {
            *f << mod << '\n';
        }
    }
    if (auto f = config.appmap_output_stream()) {
        *f << appmap::generate(recorder::events, config.generate_classmap) << std::endl;
    }
}

namespace {
    using method_ref = std::pair<mdMethodDef, ModuleID>;
    using hook_point = std::variant<std::string, method_ref>;

    auto &hooks() {
        static std::map<hook_point, hook> hooks;
        return hooks;
    }

    auto &requested_rejits() {
        static std::map<std::string, std::vector<std::string>> rejits;
        return rejits;
    }

    std::optional<hook> find_hook(clrie::method_info &method)
    {
        const auto &hs = hooks();
        const auto name = method.full_name();

        if (hs.count(name))
            return hs.at(name);

        const method_ref ref{method.method_token(), method.module_info().module_id()};

        if (hs.count(ref))
            return hs.at(ref);

        return {};
    }
}

bool appmap::instrumentation_method::should_instrument_method(clrie::method_info method, [[maybe_unused]] bool is_rejit)
{
    return find_hook(method) || config.should_instrument(method);
}

void appmap::instrumentation_method::instrument_method(clrie::method_info method, [[maybe_unused]] bool is_rejit)
{
    const auto &name = method.full_name();

    spdlog::trace("instrument_method({}, {})", name, is_rejit);

    if (const auto hook = find_hook(method))
        if ((*hook)(method))
            return;

    recorder::instrument(method);
    spdlog::trace("instrument_method({}, {}) finished", method.full_name(), is_rejit);
}

void appmap::instrumentation_method::on_module_loaded(clrie::module_info module)
{
    const auto name = module.module_name();
    modules.insert(name);

    if (const auto &rejits = requested_rejits(); rejits.count(name)) {
        const auto md = module.meta_data_import();
        for (const auto &method: rejits.at(name)) {
            auto dot = method.find_last_of('.');
            while (dot > 0 && method[dot - 1] == '.') dot--; // find the right dot in .ctor and the likes

            mdToken type;
            if (md->FindTypeDefByName(utf8::utf8to16(method.substr(0, dot)).c_str(), 0, &type) != S_OK) {
                spdlog::warn("type {} not found in {}", method.substr(0, dot), name);
                continue;
            }

            mdToken function;
            if (md->FindMethod(type, utf8::utf8to16(method.substr(dot + 1)).c_str(), nullptr, 0, &function) != S_OK) {
                spdlog::warn("method {} not found in {}", method, name);
                continue;
            }

            spdlog::debug("requesting rejit of {} in {}", method, name);
            module.request_rejit(function);
        }
    }
}

hook appmap::add_hook(const std::string &method_name, hook handler)
{
    return hooks()[method_name] = handler;
}

hook appmap::add_hook(const std::string &method_name, const std::string &module_name, hook handler)
{
    requested_rejits()[module_name].push_back(method_name);
    return hooks()[method_name] = handler;
}

hook appmap::add_hook(mdMethodDef method, ModuleID module, hook handler)
{
    return hooks()[method_ref{method, module}] = handler;
}

uint64_t appmap::current_thread_id()
{
    if (instrumentation_method::profiler_info != nullptr)
        return instrumentation_method::profiler_info.get(&ICorProfilerInfo::GetCurrentThreadID);
    else // mostly for testing
        return 42;
}
// This creates a test registry so that a build with tests enabled
// can still be used as an instrumentation DLL, not only linked
// with the test runner.
#define DOCTEST_CONFIG_IMPLEMENTATION_IN_DLL
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
