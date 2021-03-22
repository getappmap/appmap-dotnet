#include "classmap.h"

#include <clrie/method_info.h>

appmap::classmap::code_container::code_container(const appmap::classmap::code_container& other) : code_container_base()
{
    for (const auto &[k, v] : other) {
        emplace(k, std::make_unique<code_object>(*v));
    }
}

appmap::classmap::code_container::code_container(std::initializer_list<std::pair<std::string, code_object>> init)
{
    for (auto [k, v] : init) {
        emplace(k, std::make_unique<code_object>(v));
    }
}
