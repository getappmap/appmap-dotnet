#pragma once

#include <cor.h>
#include <corhdr.h>
#include <no_sal.h>
#include <InstrumentationEngine.h>
#include "com/base.h"
#include "com/hresult.h"
#include "com/ptr.h"

template<>
constexpr GUID com::guid_of<IInstrumentationMethod>() noexcept {
    using namespace com::literals;
    return "{0D92A8D9-6645-4803-B94B-06A1C4F4E633}"_guid;
}

template<>
constexpr GUID com::guid_of<IInstrumentationMethodExceptionEvents>() noexcept {
    using namespace com::literals;
    return "{8310B758-6642-46AD-9423-DDA5F9E278AE}"_guid;
}

template<>
constexpr GUID com::guid_of<ICorProfilerInfo>() noexcept {
    using namespace com::literals;
    return "28B5557D-3F3F-48b4-90B2-5F9EEA2F6C48"_guid;
}

namespace clrie {

/* CRTP base for an IInstrumentationMethod with modern C++ API */
template <typename T>
class instrumentation_method : public com::base<IInstrumentationMethod, IInstrumentationMethodExceptionEvents>
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

    // Interface implemented by instrumentation methods that want events about exceptions. The instrumentation engine
    // must also set the COR_PRF_MONITOR_EXCEPTIONS flags on the instrumentation flags during initialize as the
    // exception callbacks are not enabled by default.
    void exception_catcher_enter([[maybe_unused]] com::ptr<IMethodInfo> method_info, [[maybe_unused]] UINT_PTR object_id) {}
    void exception_catcher_leave() {}
    void exception_search_catcher_found([[maybe_unused]] com::ptr<IMethodInfo> method_info) {}
    void exception_search_filter_enter([[maybe_unused]] com::ptr<IMethodInfo> method_info) {}
    void exception_search_filter_leave() {}
    void exception_search_function_enter([[maybe_unused]] com::ptr<IMethodInfo> method_info) {}
    void exception_search_function_leave() {}
    void exception_thrown([[maybe_unused]] UINT_PTR thrown_object_id) {}
    void exception_unwind_finally_enter([[maybe_unused]] com::ptr<IMethodInfo> method_info) {}
    void exception_unwind_finally_leave() {}
    void exception_unwind_function_enter([[maybe_unused]] com::ptr<IMethodInfo> method_info) {}
    void exception_unwind_function_leave() {}

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
    
    // IInstrumentationMethodExceptionEvents

    HRESULT ExceptionCatcherEnter(/*in*/ IMethodInfo* pMethodInfo, /*in*/ UINT_PTR objectId) final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_catcher_enter(pMethodInfo, objectId));
    }

    HRESULT ExceptionCatcherLeave() final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_catcher_leave());
    }

    HRESULT ExceptionSearchCatcherFound(/*in*/ IMethodInfo* pMethodInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_search_catcher_found(pMethodInfo));
    }

    HRESULT ExceptionSearchFilterEnter(/*in*/ IMethodInfo* pMethodInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_search_filter_enter(pMethodInfo));
    }

    HRESULT ExceptionSearchFilterLeave() final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_search_filter_leave());
    }

    HRESULT ExceptionSearchFunctionEnter(/*in*/ IMethodInfo* pMethodInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_search_function_enter(pMethodInfo));
    }

    HRESULT ExceptionSearchFunctionLeave() final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_search_function_leave());
    }

    HRESULT ExceptionThrown(/*in*/ UINT_PTR thrownObjectId) final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_thrown(thrownObjectId));
    }

    HRESULT ExceptionUnwindFinallyEnter(/*in*/ IMethodInfo* pMethodInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_unwind_finally_enter(pMethodInfo));
    }

    HRESULT ExceptionUnwindFinallyLeave() final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_unwind_finally_leave());
    }

    HRESULT ExceptionUnwindFunctionEnter(/*in*/ IMethodInfo* pMethodInfo) final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_unwind_function_enter(pMethodInfo));
    }

    HRESULT ExceptionUnwindFunctionLeave() final {
        CATCH_HRESULT(static_cast<T *>(this)->exception_unwind_function_leave());
    }
};

}
