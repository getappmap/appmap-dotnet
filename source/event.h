#pragma once

#include <corprof.h>

namespace appmap {
    struct event {
        enum class kind {
            call, ret
        };

        FunctionID function;
        event::kind kind;
    };
}
