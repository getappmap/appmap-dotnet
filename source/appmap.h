#pragma once
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

namespace appmap {
    struct event {
        enum class kind {
            call,
            return_
        };

        unsigned int id;
        kind event;
        unsigned int thread_id;
        
        struct call_info {
            std::string defined_class;
            std::string method_id;
            bool static_;
        };

        struct return_info {
            unsigned int parent_id;
        };
        
        std::variant<call_info, return_info> info;
    };
    
    struct map
    {
        std::vector<event> events;
    };
    
    NLOHMANN_JSON_SERIALIZE_ENUM(event::kind, {
        {event::kind::call, "call"},
        {event::kind::return_, "return"}
    })
    
    void to_json(nlohmann::json &j, const event &event) {
        j["id"] = event.id;
        j["event"] = event.event;
        j["thread_id"] = event.thread_id;
        
        if (event.event == event::kind::call) {
            const auto &info = std::get<event::call_info>(event.info);
            j["defined_class"] = info.defined_class;
            j["method_id"] = info.method_id;
            j["static"] = info.static_;
        } else {
            j["parent_id"] = std::get<event::return_info>(event.info).parent_id;
        }
    }
    
    void to_json(nlohmann::json &j, const map &map) {
        j["events"] = map.events;
    }
}
