#pragma once

#include <unordered_map>

#include <cor.h>
#include <corprof.h>
#include <clrie/method_info.h>

#include "event.h"

namespace appmap {
    using recording = std::vector<event>;

    struct recorder {
        void instrument(clrie::method_info method);
        appmap::recording events;

    protected:
        void method_called(FunctionID id);
        void method_returned(FunctionID id);

        std::unordered_map<FunctionID, std::string> method_names;
    };
}
