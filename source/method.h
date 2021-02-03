#pragma once
#include "clrie/instrumentation_method.h"
#include "clrie/method_info.h"

namespace appmap {
    class instrumentation_method : public clrie::instrumentation_method<instrumentation_method>
    {
    public:
        void initialize([[maybe_unused]] com::ptr<IProfilerManager> profiler_manager);
        bool should_instrument_method([[maybe_unused]] clrie::method_info method_info, [[maybe_unused]] bool is_rejit);
        void instrument_method([[maybe_unused]] clrie::method_info method_info, [[maybe_unused]] bool is_rejit);
        bool allow_inline_site([[maybe_unused]] com::ptr<IMethodInfo> method_info_inlinee, [[maybe_unused]] com::ptr<IMethodInfo> method_info_caller) { return false; }
        
        void method_called(FunctionID fid);
        void method_returned(FunctionID fid);

    private:
        com::ptr<IProfilerManager> manager;
    };
}
