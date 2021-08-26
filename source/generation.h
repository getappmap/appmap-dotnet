#pragma once

#include "classmap.h"
#include "metadata.h"
#include "method_info.h"
#include "recorder.h"

namespace appmap {
    std::string generate(const recording &events, bool generate_classmap, const metadata &metadata = {});
}
