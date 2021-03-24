#pragma once
#include <unordered_set>

#include "clrie/instrumentation_method.h"
#include "clrie/method_info.h"
#include "clrie/module_info.h"

#include "config.h"
#include "recorder.h"
#include "test_framework.h"

namespace appmap {
    struct instrumentation_method : public clrie::instrumentation_method<instrumentation_method>
    {
        std::unordered_set<std::string> modules;
        appmap::config config = appmap::config::load();
        appmap::test_framework test_framework;
        com::ptr<IProfilerManager> profiler_manager;

        void initialize(com::ptr<IProfilerManager> manager);
        bool should_instrument_method(clrie::method_info method, bool is_rejit);
        void instrument_method(clrie::method_info method, bool is_rejit);
        void on_module_loaded(clrie::module_info module);
        void on_shutdown();
    };
}
