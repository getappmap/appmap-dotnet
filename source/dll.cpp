#include <unknwn.h>
#include "com/base.h"

class Factory : public com::base<IClassFactory>
{
public:
    virtual ~Factory() {}
    
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown */*pUnkOuter*/, [[maybe_unused]] REFIID riid, [[maybe_unused]] void **ppvObject) override
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL /*fLock*/) override { return S_OK; } 
};

extern "C" HRESULT DllGetClassObject([[maybe_unused]] GUID &rclsid, GUID &riid, void **ppvObj)
{
    return (new Factory)->QueryInterface(riid, ppvObj);
}
