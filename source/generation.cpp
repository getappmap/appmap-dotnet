#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ranges.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/iterator/operations.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/unique.hpp>

#include <algorithm>
#include <unordered_set>

#include "classmap.h"
#include "generation.h"
#include "method_info.h"

using namespace appmap;
using namespace nlohmann;

namespace {
    template <typename T>
    constexpr std::string code_object_type(const T &arg) {
        using namespace appmap::classmap;
        if constexpr (std::is_same_v<T, function>)
            return "function";
        else if constexpr (std::is_same_v<T, code_container>) {
            if (arg.kind == code_container::package)
                return "package";
            else
                return "class";
        }
    }

    classmap::classmap classmap_of_recording(const recording &rec) {
        using namespace ranges::views;
        classmap::classmap map;

        for (const method_info &method: rec
            | transform([](const auto &ev) { return ev.function; }) | unique
            | transform([](const FunctionID fun) { return method_infos.at(fun); }))
        {
            classmap::code_container *code = &map;
            for (std::string &&part: std::string_view(method.defined_class) | split('.')
                | transform([](auto &&rng) { return std::string(&*rng.begin(), ranges::distance(rng)); })
            ) {
                auto [it, inserted] = code->try_emplace(part, std::make_unique<classmap::code_object>(classmap::code_container()));
                code = &std::get<classmap::code_container>(*(it->second));
                if (inserted)
                    code->kind = classmap::code_container::package;
            }

            code->kind = classmap::code_container::klass;
            code->try_emplace(method.method_id, std::make_unique<classmap::code_object>(classmap::function{method.is_static}));
        }

        return map;
    }
}


namespace appmap {
    void to_json(json &j, const method_info &m) {
        j["defined_class"] = m.defined_class;
        j["method_id"] = m.method_id;
        j["static"] = m.is_static;
    }

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
                if (stack.empty()) {
                    // known bug
                    spdlog::warn("stack empty on return, ignoring");
                    continue;
                }
                if (stack.back().fid != ev.function) {
                    // known bug
                    spdlog::warn("function mismatch detected on return; function: {}, stack: {}", method_infos.at(ev.function).method_id, stack | ranges::views::transform([](auto &ev){ return method_infos.at(ev.fid).method_id; }));
                    if (ranges::any_of(stack, [fid = ev.function](const auto &ev) { return ev.fid == fid; })) {
                        // try to do out best by generating the missing returns
                        while (stack.back().fid != ev.function) {
                            j.push_back({ { "event", "return" }, { "parent_id", stack.back().ev }, { "id", id++ } });
                            stack.pop_back();
                        }
                    } else {
                        // ok, this is super weird, did we miss the call? do nothing
                    }
                }
                jev = { { "event", "return" }, { "parent_id", stack.back().ev } };
                stack.pop_back();
            }
            jev["id"] = id++;
            j.push_back(jev);
        }
    }

    namespace classmap {
        void to_json(json &j, const code_container &co);
        void to_json(json &j, const code_object &co) {
            std::visit([&j](const auto &arg) {
                using t = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<t, function>) {
                    j["static"] = arg.is_static;
                } else {
                    j["children"] = arg;
                }
                j["type"] = code_object_type(arg);
            }, co);
        }

        void to_json(json &j, const code_object_ptr &p) {
            to_json(j, *p);
        }

        void to_json(json &j, const code_container &co) {
            j = json::array();
            for (const auto &[k, v]: co) {
                json el = *v;
                el["name"] = k;
                j.push_back(el);
            }
        }
    }
}

std::string appmap::generate(appmap::recording events, bool generate_classmap)
{
    json result = { { "events", events } };
    if (generate_classmap)
        result["classMap"] = classmap_of_recording(events);
    return result.dump(2);
}

TEST_CASE("basic generation") {
    const appmap::recording events = {
        { event_kind::call, 42 },
        { event_kind::call, 43 },
        { event_kind::ret, 43 },
        { event_kind::ret, 42 },
    };
    method_infos[42] = { "Some.Class", "Method", false };
    method_infos[43] = { "Some.Class", "OtherMethod", true };

    CHECK(json::parse(generate(events, true)) == R"(
        {
            "classMap": [{
                "name": "Some",
                "type": "package",
                "children": [{
                    "name": "Class",
                    "type": "class",
                    "children": [
                        {
                            "name": "OtherMethod",
                            "type": "function",
                            "static": true
                        },
                        {
                            "name": "Method",
                            "type": "function",
                            "static": false
                        }
                    ]
                }]
            }],
            "events": [
                {
                    "id": 1,
                    "event": "call",
                    "defined_class": "Some.Class",
                    "method_id": "Method",
                    "static": false
                },
                {
                    "id": 2,
                    "event": "call",
                    "defined_class": "Some.Class",
                    "method_id": "OtherMethod",
                    "static": true
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
