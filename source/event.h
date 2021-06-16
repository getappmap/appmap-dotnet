#pragma once
#include <optional>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <variant>

#include <corprof.h>

namespace appmap {
    using cor_value = std::variant<std::string, uint64_t, int64_t, bool, nullptr_t>;

    struct event {
        virtual bool operator==(const event &other) const noexcept {
            return typeid(*this) == typeid(other);
        }
        virtual ~event() {}
    };

    struct call_event: event {
        FunctionID function;

        call_event(FunctionID fun): function(fun) {}

        bool operator==(const event &other) const noexcept override {
            if (!event::operator==(other))
                return false;

            return function == static_cast<const call_event &>(other).function;
        }
    };

    struct return_event: event {
        const call_event *call;
        std::optional<cor_value> value = std::nullopt;

        return_event(const call_event *call_ev, std::optional<cor_value> return_value = std::nullopt):
            call(call_ev), value(return_value) {}

        bool operator==(const event &other) const noexcept override {
            if (!event::operator==(other))
                return false;

            const auto &other_ret = static_cast<const return_event &>(other);
            return call == other_ret.call && value == other_ret.value;
        }
    };
}
