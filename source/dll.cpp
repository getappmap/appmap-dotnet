#include <unknwn.h>
#include "com_base.h"

class Factory : public com_base<IClassFactory>
{
public:
    virtual ~Factory() {}
    
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown */*pUnkOuter*/, REFIID /*riid*/, void **ppvObject) override
    {
        *ppvObject = nullptr;
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL /*fLock*/) override { return S_OK; } 
};

extern "C" HRESULT DllGetClassObject([[maybe_unused]] GUID &rclsid, GUID &riid, void **ppvObj)
{
    return (new Factory)->QueryInterface(riid, ppvObj);
}
