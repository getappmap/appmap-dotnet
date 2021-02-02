#include <unknwn.h>
#include "com/base.h"
#include "clrie/instrumentation_method.h"
#include <iostream>

class imethod : public clrie::instrumentation_method<imethod>
{
public:
    void initialize([[maybe_unused]] com::ptr<IProfilerManager> profiler_manager) {
        std::cout << "hello instrumentation method" << std::endl;
    }
    void on_shutdown() {
        std::cout << "goodbye instrumentation method" << std::endl;
    }
};

class Factory : public com::base<IClassFactory>
{
public:
    virtual ~Factory() {}
    
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown */*pUnkOuter*/, [[maybe_unused]] REFIID riid, [[maybe_unused]] void **ppvObject) override
    {
        return (new imethod)->QueryInterface(riid, ppvObject);
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL /*fLock*/) override { return S_OK; } 
};

extern "C" HRESULT DllGetClassObject([[maybe_unused]] GUID &rclsid, GUID &riid, void **ppvObj)
{
    return (new Factory)->QueryInterface(riid, ppvObj);
}
