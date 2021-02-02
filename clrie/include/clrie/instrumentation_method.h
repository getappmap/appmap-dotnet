#pragma once

#include <cor.h>
#include <corhdr.h>
#include <no_sal.h>
#include <InstrumentationEngine.h>
#include "com/base.h"
#include "com/hresult.h"
#include "com/ptr.h"

template<>
const GUID com::guid_of<IInstrumentationMethod>() noexcept { return IID_IInstrumentationMethod; }

namespace clrie {

/* CRTP base for an IInstrumentationMethod with modern C++ API */
template <typename T>
class instrumentation_method : public com::base<IInstrumentationMethod>
{    
public: 
    // You can reimplement the functions below in subclasses.
    
    void initialize([[maybe_unused]] com::ptr<IProfilerManager> profiler_manager) {}
    void on_app_domain_created([[maybe_unused]] com::ptr<IAppDomainInfo> app_domain_info) {}
    void on_app_domain_shutdown([[maybe_unused]] com::ptr<IAppDomainInfo> app_domain_info) {}
    void on_assembly_loaded([[maybe_unused]] com::ptr<IAssemblyInfo> assembly_info) {}
    void on_assembly_unloaded([[maybe_unused]] com::ptr<IAssemblyInfo> assembly_info) {}
    void on_module_loaded([[maybe_unused]] com::ptr<IModuleInfo> module_info) {}
    void on_module_unloaded([[maybe_unused]] com::ptr<IModuleInfo> module_info) {}
    void on_shutdown() {}

    // Instrumentation methods should return true if they want to instrument the method pointed to by IMethodInfo
    bool should_instrument_method([[maybe_unused]] com::ptr<IMethodInfo> method_info, [[maybe_unused]] bool is_rejit) { return false; }
    
    // Called on instrumentation methods that return true to ShouldInstrumentMethod. This gives them the opportunity
    // to replace the method body using the CreateBaseline api on the instruction graph. Only one instrumentation
    // method can create the baseline. BeforeInstrumentMethod is called on instrumentation methods in priority order.
    // Instrumentation methods should not perform any other instrumentation in this callback as a lower priority instrumentation
    // method may replace the method body later
    void before_instrument_method([[maybe_unused]] com::ptr<IMethodInfo> method_info, [[maybe_unused]] bool is_rejit) {}

    // Called for methods where the instrumentation method returned true during ShouldInstrumentMethod.
    // InstrumentMethod is called on instrumentation methods in priority order
    void instrument_method([[maybe_unused]] com::ptr<IMethodInfo> method_info, [[maybe_unused]] bool is_rejit) {}

    // Fires after all instrumentation is complete. This allows instrumentation methods to perform final
    // bookkeeping such as recording local variable signatures which are not known until the final process
    // is complete.
    void on_instrumentation_complete([[maybe_unused]] com::ptr<IMethodInfo> method_info, [[maybe_unused]] bool is_rejit) {}

    // Instrumentation engine asks each instrumentation method if inlining should be allowed
    bool allow_inline_site([[maybe_unused]] com::ptr<IMethodInfo> method_info_inlinee, [[maybe_unused]] com::ptr<IMethodInfo> method_info_caller) { return true; }

public:
    // IInstrumentationMethod interface implementation delegating to the functions above
    
    HRESULT Initialize(/*in*/ IProfilerManager* pProfilerManager) final {
        CATCH_HRESULT(static_cast<T *>(this)->initialize(pProfilerManager));
    }

    HRESULT OnAppDomainCreated(/*in*/ IAppDomainInfo* pAppDomainInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->on_app_domain_created(pAppDomainInfo));
    }
    HRESULT OnAppDomainShutdown(/*in*/ IAppDomainInfo* pAppDomainInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->on_app_domain_shutdown(pAppDomainInfo));
    }

    HRESULT OnAssemblyLoaded(/*in*/ IAssemblyInfo* pAssemblyInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->on_assembly_loaded(pAssemblyInfo));
    }
    HRESULT OnAssemblyUnloaded(/*in*/ IAssemblyInfo* pAssemblyInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->on_assembly_unloaded(pAssemblyInfo));
    }

    HRESULT OnModuleLoaded(/*in*/ IModuleInfo* pModuleInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->on_module_loaded(pModuleInfo));
    }
    HRESULT OnModuleUnloaded(/*in*/ IModuleInfo* pModuleInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->on_module_unloaded(pModuleInfo));
    }

    HRESULT OnShutdown() final {
        CATCH_HRESULT(static_cast<T *>(this)->on_shutdown());
    }

    HRESULT ShouldInstrumentMethod(/*in*/ IMethodInfo* pMethodInfo, /*in*/ BOOL isRejit, /*out*/ BOOL* pbInstrument) final {
        CATCH_HRESULT(*pbInstrument = static_cast<T *>(this)->should_instrument_method(pMethodInfo, isRejit));
    }

    HRESULT BeforeInstrumentMethod(/*in*/ IMethodInfo* pMethodInfo, /*in*/ BOOL isRejit) final {
        CATCH_HRESULT(static_cast<T *>(this)->before_instrument_method(pMethodInfo, isRejit));
    }

    HRESULT InstrumentMethod(/*in*/ IMethodInfo* pMethodInfo, /*in*/ BOOL isRejit) final {
        CATCH_HRESULT(static_cast<T *>(this)->instrument_method(pMethodInfo, isRejit));
    }

    HRESULT OnInstrumentationComplete(/*in*/ IMethodInfo* pMethodInfo, /*in*/ BOOL isRejit) final {
        CATCH_HRESULT(static_cast<T *>(this)->on_instrumentation_complete(pMethodInfo, isRejit));
    }

    HRESULT AllowInlineSite(/*in*/ IMethodInfo* pMethodInfoInlinee, /*in*/ IMethodInfo* pMethodInfoCaller, /*out*/ BOOL* pbAllowInline) final {
        CATCH_HRESULT(*pbAllowInline = static_cast<T *>(this)->allow_inline_site(pMethodInfoInlinee, pMethodInfoCaller));
    }
};

}
