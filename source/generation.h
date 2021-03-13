#pragma once

#include "recorder.h"

namespace appmap {
    std::string generate(recording events, com::ptr<IAppDomainCollection> app);
}
