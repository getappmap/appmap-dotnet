#include "metadata.h"

#include <nlohmann/json.hpp>

using namespace appmap;

nlohmann::json metadata::common = {{ "client", {
    { "name", "appmap-dotnet" },
    { "url", "https://github.com/applandinc/appmap-dotnet/" }
}}};

metadata::operator nlohmann::json() const
{
    return common;
}
