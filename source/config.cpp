#include "config.h"

appmap::configuration::configuration() noexcept
{
    instrument = std::getenv("APPMAP");
    method_list = std::getenv("APPMAP_METHOD_LIST");
    if (const char *path = std::getenv("APPMAP_OUTPUT_PATH"))
        output = path;
}
