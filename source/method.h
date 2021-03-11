#pragma once
#include <unordered_set>

#include "clrie/instrumentation_method.h"
#include "clrie/module_info.h"

#include "config.h"

namespace appmap {
    struct instrumentation_method : public clrie::instrumentation_method<instrumentation_method>
    {
        std::unordered_set<std::string> modules;
        appmap::config config;

        void on_module_loaded(clrie::module_info module);
        void on_shutdown();
    };
}
