#pragma once

#include <corprof.h>

namespace appmap {
    enum class event_kind {
        call, ret
    };
    struct event {
        event_kind kind;
        FunctionID function;
    };
}
