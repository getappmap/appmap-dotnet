#pragma once
#include <optional>
#include <variant>

#include <corprof.h>

namespace appmap {
    using cor_value = std::variant<uint64_t, int64_t, bool>;

    enum class event_kind {
        call, ret
    };

    struct event {
        event_kind kind;
        FunctionID function;
        std::optional<cor_value> return_value = std::nullopt;
    };
}
