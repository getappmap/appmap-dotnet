#pragma once

#include <pal_mstypes.h>
#include <ios>

namespace com {

template<class Interface>
const GUID guid_of() noexcept;

template<>
const GUID guid_of<IClassFactory>() noexcept { return IID_IClassFactory; }
template<>
const GUID guid_of<IUnknown>() noexcept { return IID_IUnknown; }

template <typename OStream>
OStream &operator<<(OStream &os, const GUID &guid) {
    return os << "{" << std::hex << guid.Data1 << "-" << guid.Data2 << "-" << guid.Data3 << "-" << 
        (guid.Data4[0] << 8 | guid.Data4[1]) << "-" << 
        (guid.Data4[2] << 8 | guid.Data4[3]) << 
        (guid.Data4[4] << 8 | guid.Data4[5]) << 
        (guid.Data4[6] << 8 | guid.Data4[7])
    << "}";
}

}
