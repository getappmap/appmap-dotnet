#pragma once

#include "instrumentation.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace appmap { namespace resolver {

clrie::instruction_factory::instruction_sequence inject(instrumentation);

}}
