#pragma once
#include <InstrumentationEngine.h>

namespace clrie { namespace detail {

template <typename Enum>
class element_of_enum {
    template <typename Elt>
    static auto element(HRESULT (Enum::*fun)(ULONG, Elt**, ULONG*)) -> Elt*;

public:
    using type = std::remove_pointer_t<decltype(element(&Enum::Next))>;
};

template <typename Enum, typename Elt = typename element_of_enum<Enum>::type>
requires requires(Enum enu, Elt *elt, DWORD dw) {
    { enu.GetCount(&dw) } -> HRESULT;
    { enu.Next(dw, &elt, &dw) } -> HRESULT;
}
auto to_vector(com::ptr<Enum> enu)
{
    auto len = enu.get(&Enum::GetCount);
    std::vector<com::ptr<Elt>> result(len);

    if (len > 0)
        com::hresult::check(enu->Next(len, &result[0], &len));

    return result;
}

}}
