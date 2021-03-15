#pragma once

#include <clrie/method_info.h>

namespace appmap {
    struct test_framework {
        bool should_instrument(const clrie::method_info method);
        bool instrument(clrie::method_info method);
    };
}
