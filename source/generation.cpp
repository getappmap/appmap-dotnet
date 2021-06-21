#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ranges.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/iterator/operations.hpp>
#include <range/v3/view/remove_if.hpp>
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
            | remove_if([](const auto &ev) { return typeid(*ev) != typeid(call_event); })
            | transform([](const auto &ev) { return static_cast<const call_event &>(*ev).function; }) | unique
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

    void to_json(json &j, const call_event &ev)
    {
        const auto &method = method_infos.at(ev.function);
        j = method;
        j["event"] = "call";
        const auto &args = ev.arguments;
        if (args.empty()) return;

        json params = json::array();
        auto type = method.parameters.begin();
        for (const auto &arg: args) {
            json param;
            std::visit([&param] (auto &&v) { param["value"] = v; }, arg);
            param["class"] = *(type++);
            params.push_back(param);
        }
        j["parameters"] = params;
    }

    void to_json([[maybe_unused]] json &j, [[maybe_unused]] const appmap::return_event &ev)
    {
        j["event"] = "return";
        if (ev.value) {
            const auto &method = method_infos.at(ev.call->function);
            auto &rv = j["return_value"] = { { "class", method.return_type } };
            std::visit([&rv] (auto &&arg) { rv["value"] = arg; }, *ev.value);
        }
    }

    struct generation_visitor {
        json &events;
        using id_t = uint;

        id_t id = 1;

        std::unordered_map<const call_event *, id_t> calls{};

        void operator()(const call_event &ev) {
            calls[&ev] = id;
            push(ev);
        }

        void operator()(const return_event &ev) {
            json jev(ev);
            jev["parent_id"] = calls.at(ev.call);
            calls.erase(ev.call);
            push(std::move(jev));
        }

        void operator()(const event &ev) {
            const auto &t = typeid(ev);
            if (t == typeid(call_event)) {
                operator()(static_cast<const call_event &>(ev));
            } else if (t == typeid(return_event)) {
                operator()(static_cast<const return_event &>(ev));
            } else {
                assert(false && "unexpected event type");
            }
        }

    private:
        void push(json &&event) {
            event["id"] = id++;
            events.push_back(std::move(event));
        }
    };

    void to_json(json &j, const appmap::recording &events)
    {
        generation_visitor v{j};
        for (const auto &ev : events) {
            v(*ev);
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

std::string appmap::generate(const appmap::recording &events, bool generate_classmap)
{
    json result = { { "events", events } };
    if (generate_classmap)
        result["classMap"] = classmap_of_recording(events);
    return result.dump(2);
}

TEST_CASE("basic generation") {
    appmap::recording events;

    events.push_back(std::make_unique<call_event>(42));
    events.push_back(std::make_unique<call_event>(43));
    events.push_back(std::make_unique<return_event>(static_cast<call_event *>(events[1].get()), uint64_t{42}));
    events.push_back(std::make_unique<return_event>(static_cast<call_event *>(events[0].get()), int64_t{-31337}));

    method_infos[42] = { "Some.Class", "Method", false, "I8" };
    method_infos[43] = { "Some.Class", "OtherMethod", true, "U4" };

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
                    "parent_id": 2,
                    "return_value": {
                        "class": "U4",
                        "value": 42
                    }
                },
                {
                    "id": 4,
                    "event": "return",
                    "parent_id": 1,
                    "return_value": {
                        "class": "I8",
                        "value": -31337
                    }
                }
            ]
        })"_json);
}
