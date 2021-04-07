#pragma once
#include <optional>
#include <string>
#include <variant>

#include <corprof.h>

namespace appmap {
    using cor_value = std::variant<std::string, uint64_t, int64_t, bool, nullptr_t>;

    enum class event_kind {
        call, ret
    };

    struct event {
        event_kind kind;
        FunctionID function;
        std::optional<cor_value> return_value = std::nullopt;

        bool operator==(const event &r) const
        {
            return kind == r.kind && function == r.function && return_value == r.return_value;
        }
    };
}
