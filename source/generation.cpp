#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

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
        uint id = 0;
        j = json::array();
        for (const auto &ev : events) {
            json jev = ev;
            jev["id"] = ++id;
            j.push_back(jev);
        }
    }
}

std::string appmap::generate(appmap::recording events)
{
    return json{ { "events", events } }.dump();
}

TEST_CASE("basic generation") {
    const appmap::recording events = { { event_kind::call, 42 }, { event_kind::call, 43 } };
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
                }
            ]
        })"_json);
}
