#include "config.h"

#include <nlohmann/json.hpp>

appmap::configuration::configuration() noexcept
{
    instrument = std::getenv("APPMAP");
    method_list = std::getenv("APPMAP_METHOD_LIST");
    if (const char *path = std::getenv("APPMAP_OUTPUT_PATH"))
        output = path;
}

nlohmann::json appmap::configuration::metadata() const
{
    nlohmann::json j;
    return j;
}
