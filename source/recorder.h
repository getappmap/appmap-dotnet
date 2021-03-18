#pragma once

#include <mutex>
#include <unordered_map>

#include <cor.h>
#include <corprof.h>
#include <clrie/method_info.h>

#include "event.h"

namespace appmap {
    using recording = std::vector<event>;

    namespace recorder {
        extern appmap::recording events;
        inline std::mutex mutex;
        void instrument(clrie::method_info method);
    }
}
