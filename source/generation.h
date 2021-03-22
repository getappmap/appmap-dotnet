#pragma once

#include "classmap.h"
#include "method_info.h"
#include "recorder.h"

namespace appmap {
    std::string generate(recording events);
    std::string generate(const classmap::classmap &map);
}
