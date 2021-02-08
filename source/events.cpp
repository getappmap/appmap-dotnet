#include "events.h"

#include <iostream>

#include <nlohmann/json.hpp>

namespace appmap {
    NLOHMANN_JSON_SERIALIZE_ENUM(typename event::kind, {
        { event::kind::call, "call" },
        { event::kind::return_, "return" }
    })
}

using namespace appmap;
using namespace nlohmann;

json appmap::to_json([[maybe_unused]] const std::vector<event> &events, [[maybe_unused]] const std::unordered_map<FunctionID, clrie::method_info> &methods)
{
    json j;
    std::vector<std::pair<FunctionID, event::id>> stack;

    event::id id = 0;

    struct method_info {
        const std::string defined_class;
        const std::string method_id;
        const bool static_;
    };

    auto info = [&methods](FunctionID id) -> const method_info & {
        static std::unordered_map<FunctionID, method_info> infos;
        const auto &it = infos.find(id);
        if (it != infos.end())
            return it->second;
        const auto &method = methods.at(id);
        return infos.emplace(id, method_info{
            method.declaring_type().get(&IType::GetName),
            method.name(),
            method.is_static()
        }).first->second;
    };

    for (const auto &ev : events) {
        json event = { { "event", ev.kind }, { "id", ++id }, { "thread_id", ev.thread } };
        switch (ev.kind) {
            case event::kind::call: {
                const auto &method = info(ev.function);
                stack.push_back({ev.function, id});
                event["defined_class"] = method.defined_class;
                event["method_id"] = method.method_id;
                event["static"] = method.static_;
                break;
            }
            case event::kind::return_:
                while (!stack.empty()) {
                    const auto [fun, pid] = stack.back();
                    stack.pop_back();
                    event["parent_id"] = pid;
                    if (fun != ev.function) {
                        std::cerr << "warning: unmatched call for " << info(fun).method_id << std::endl;
                        j.push_back(json(event));
                        event["id"] = ++id;
                    } else break;
                }
                break;
        }
        j.push_back(event);
    }

    return j;
}
