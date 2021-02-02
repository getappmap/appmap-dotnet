#pragma once

#include <atomic>
#include "guid.h"

namespace com {

inline namespace detail {
    template <typename Interface, typename T>
    inline void *query(T *obj, const GUID &iid) {
        if (iid == guid_of<Interface>())
            return static_cast<Interface *>(obj);
        else
            return nullptr;
    }

    template <typename T, typename... Interfaces>
    inline void *query(T *obj, const GUID &iid) {
        void *result = nullptr;
        (void) (... || (result = query<Interfaces>(obj, iid)));
        return result;
    }
}

template <typename... Interfaces>
class base : public Interfaces... {
public:
    virtual ~base() {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final
    {
        *ppvObject = detail::query<base, IUnknown, Interfaces...>(this, riid);

        if (*ppvObject) {
            AddRef();
            return S_OK;
        }
        
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() final {
        return std::atomic_fetch_add(&ref_count, 1) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() final {
        auto count = std::atomic_fetch_sub(&ref_count, 1) - 1;
        if (count <= 0)
            delete this;
        return count;
    }

private:
    std::atomic<int> ref_count = 0;
};

}
