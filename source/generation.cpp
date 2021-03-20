#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "generation.h"
#include "method_info.h"

using namespace appmap;
using namespace nlohmann;

namespace appmap {
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(method_info, defined_class, method_id)

    NLOHMANN_JSON_SERIALIZE_ENUM(event_kind, {
        { event_kind::call, "call" },
        { event_kind::ret, "return" }
    })

    void to_json(json &j, const appmap::event &event)
    {
        j = method_infos.at(event.function);
        j["event"] = event.kind;
    }

    void to_json(json &j, const appmap::recording &events)
    {
        uint id = 1;
        struct stack_entry { FunctionID fid; uint ev; };
        std::vector<stack_entry> stack;
        j = json::array();
        for (const auto &ev : events) {
            json jev;
            if (ev.kind == event_kind::call) {
                jev = ev;
                stack.push_back({ev.function, id});
            } else if (ev.kind == event_kind::ret) {
                assert(!stack.empty());
                const auto &parent = stack.back();
                if (parent.fid != ev.function) {
                    spdlog::error("Function mismatch on return; your appmap will be incorrect.\n\tPlease report at https://github.com/applandinc/appmap-dotnet/issues");
                    continue;
                }
                jev = { { "event", "return" }, { "parent_id", parent.ev } };
                stack.pop_back();
            }
            jev["id"] = id++;
            j.push_back(jev);
        }
    }
}

std::string appmap::generate(appmap::recording events)
{
    return json{ { "events", events } }.dump();
}

TEST_CASE("basic generation") {
    const appmap::recording events = {
        { event_kind::call, 42 },
        { event_kind::call, 43 },
        { event_kind::ret, 43 },
        { event_kind::ret, 42 },
    };
    method_infos[42] = { "Some.Class", "Method" };
    method_infos[43] = { "Some.Class", "OtherMethod" };

    CHECK(json::parse(generate(events)) == R"(
        {
            "events": [
                {
                    "id": 1,
                    "event": "call",
                    "defined_class": "Some.Class",
                    "method_id": "Method"
                },
                {
                    "id": 2,
                    "event": "call",
                    "defined_class": "Some.Class",
                    "method_id": "OtherMethod"
                },
                {
                    "id": 3,
                    "event": "return",
                    "parent_id": 2
                },
                {
                    "id": 4,
                    "event": "return",
                    "parent_id": 1
                }
            ]
        })"_json);
}
