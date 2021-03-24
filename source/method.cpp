#include <spdlog/spdlog.h>
#include <string>

#include "generation.h"
#include "method.h"
#include "instrumentation.h"
#include <fstream>

using namespace appmap;

void appmap::instrumentation_method::initialize(com::ptr<IProfilerManager> manager)
{
    spdlog::debug("initialize()");
    profiler_manager = manager;
}

void appmap::instrumentation_method::on_module_loaded(clrie::module_info module)
{
    spdlog::trace("on_module_loaded({})", module.module_name());
    modules.insert(module.module_name());
}

void appmap::instrumentation_method::on_shutdown()
{
    spdlog::debug("on_shutdown()");
    if (config.module_list_path) {
        std::ofstream f(*config.module_list_path, std::ios_base::app);
        for (const auto &mod : modules) {
            f << mod << '\n';
        }
    }
    if (config.appmap_output_path) {
        std::ofstream(*config.appmap_output_path) << appmap::generate(recorder::events);
    }
}

bool appmap::instrumentation_method::should_instrument_method(clrie::method_info method, bool is_rejit)
{
    bool result = [&](){
        if (is_rejit) return false;

        return test_framework.should_instrument(method) || config.should_instrument(method);
    }();

    spdlog::trace("should_instrument_method({}, {}) -> {}", method.full_name(), is_rejit, result);

    return result;
}

void appmap::instrumentation_method::instrument_method(clrie::method_info method, bool is_rejit)
{
    spdlog::trace("instrument_method({}, {})", method.full_name(), is_rejit);

    if (test_framework.instrument(method))
        return;

    recorder::instrument(method);
}

// This creates a test registry so that a build with tests enabled
// can still be used as an instrumentation DLL, not only linked
// with the test runner.
#define DOCTEST_CONFIG_IMPLEMENTATION_IN_DLL
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
