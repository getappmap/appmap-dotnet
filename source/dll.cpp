#include <unknwn.h>
#include <atomic>

class Factory : IClassFactory
{
    std::atomic<int> ref_count = 0;
    
public:
    virtual ~Factory() {}
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        *ppvObject = nullptr;
        
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return std::atomic_fetch_add(&ref_count, 1) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        auto count = std::atomic_fetch_sub(&ref_count, 1) - 1;
        if (count <= 0)
            delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown */*pUnkOuter*/, REFIID /*riid*/, void **ppvObject) override
    {
        *ppvObject = nullptr;
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL /*fLock*/) override { return S_OK; } 
};

extern "C" HRESULT DllGetClassObject(GUID &/*rclsid*/, GUID &/*riid*/, void **/*ppvObj*/)
{
    return E_FAIL;
}
