#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ranges.h>

#include <algorithm>
#include <unordered_set>
#include <sstream>

#include "classmap.h"
#include "generation.h"
#include "method_info.h"

using namespace appmap;
using namespace nlohmann;

namespace {
    template <typename T>
    std::string code_object_type(const T &arg) {
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
        classmap::classmap map;

        std::unordered_set<size_t> functions;
        for (const auto &ev: rec)
            if (const auto event = dynamic_cast<const function_call_event *>(ev.get()); event != nullptr)
                functions.insert(event->function);

        for (const auto fun: functions) {
            const auto &method = method_infos.at(fun);
            classmap::code_container *code = &map;

            std::istringstream tokens(method.defined_class);
            std::string part;
            while (std::getline(tokens, part, '.')) {
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

    void to_json(json &j, const parameter_info &p) {
        j["name"] = p.name;
        j["class"] = p.type;
    }

    event::operator json() const
    {
        return {{ "thread_id", thread }};
    }

    function_call_event::operator json() const
    {
        auto j = event::operator json();
        const auto &method = method_infos.at(function);
        j.update(method);
        j["event"] = "call";

        bool capture_receiver = !method.is_static;
        auto param_it = method.parameters.begin();
        const auto &args = arguments;
        json params = json::array();
        for (const auto &arg: args) {
            json param = *(param_it++);
            std::visit([&param] (auto &&v) { param["value"] = v; }, arg);
            if (capture_receiver) {
                param.erase("name");
                j["receiver"] = param;
                capture_receiver = false;
            } else {
                params.push_back(param);
            }
        }

        if (!params.empty()) {
            j["parameters"] = params;
        }

        return j;
    }

    return_event::operator json() const
    {
        auto j = event::operator json();

        j["event"] = "return";
        if (value) {
            auto &rv = j["return_value"] = {};
            if (const auto call_fun = dynamic_cast<const function_call_event *>(call)) {
                rv["class"] = method_infos.at(call_fun->function).return_type;
            }
            std::visit([&rv] (auto &&arg) { rv["value"] = arg; }, *value);
        }

        return j;
    }

    http_request_event::operator json() const
    {
        auto j = event::operator json();
        j["event"] = "call";
        j["http_server_request"] = {
            { "request_method", meth },
            { "path_info", path }
        };
        return j;
    }

    http_response_event::operator json() const
    {
        auto j = return_event::operator json();
        j["http_server_response"] = {
            { "status_code", status }
        };
        return j;
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
            if (const auto call = dynamic_cast<const call_event *>(&ev)) {
                operator()(*call);
            } else if (const auto ret = dynamic_cast<const return_event *>(&ev)) {
                operator()(*ret);
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

namespace doctest {
    template<> struct StringMaker<json> {
        static String convert(const json& value) {
            return value.dump(4).c_str();
        }
    };
}

TEST_CASE("basic generation") {
    appmap::recording events;

    events.push_back(std::make_unique<function_call_event>(42, 0));
    events.push_back(std::make_unique<function_call_event>(42, 1));
    events.push_back(std::make_unique<return_event>(42, static_cast<function_call_event *>(events[1].get()), uint64_t{42}));
    events.push_back(std::make_unique<return_event>(42, static_cast<function_call_event *>(events[0].get()), int64_t{-31337}));

    method_infos.push_back({ "Some.Class", "Method", false, "I8" });
    method_infos.push_back({ "Some.Class", "OtherMethod", true, "U4" });

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
                            "name": "Method",
                            "type": "function",
                            "static": false
                        },
                        {
                            "name": "OtherMethod",
                            "type": "function",
                            "static": true
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
                    "static": false,
                    "thread_id": 42
                },
                {
                    "id": 2,
                    "event": "call",
                    "defined_class": "Some.Class",
                    "method_id": "OtherMethod",
                    "static": true,
                    "thread_id": 42
                },
                {
                    "id": 3,
                    "event": "return",
                    "parent_id": 2,
                    "return_value": {
                        "class": "U4",
                        "value": 42
                    },
                    "thread_id": 42
                },
                {
                    "id": 4,
                    "event": "return",
                    "parent_id": 1,
                    "return_value": {
                        "class": "I8",
                        "value": -31337
                    },
                    "thread_id": 42
                }
            ]
        })"_json);
}

TEST_CASE("http events generation") {
    appmap::recording events;
    events.push_back(std::make_unique<http_request_event>(42, "POST", "/test"));
    events.push_back(std::make_unique<http_response_event>(42, static_cast<call_event *>(events[0].get()), 409));
    CHECK(json::parse(generate(events, false)) == R"({"events": [
        {
            "id": 1,
            "event": "call",
            "http_server_request": {
                "path_info": "/test",
                "request_method": "POST"
            },
            "thread_id": 42
        },
        {
            "id": 2,
            "event": "return",
            "parent_id": 1,
            "http_server_response": {
                "status_code": 409
            },
            "thread_id": 42
        }
    ]})"_json);
}
