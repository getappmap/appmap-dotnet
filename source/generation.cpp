#include <nlohmann/json.hpp>

#include "generation.h"

using namespace appmap;
using namespace nlohmann;

namespace appmap {
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(event, kind, function);
}

std::string appmap::generate(appmap::recording events)
{
    return json{ { "events", events } }.dump();
}
