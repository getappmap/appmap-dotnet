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

template <
    typename Enum, typename Elt = typename element_of_enum<Enum>::type,
    typename = std::enable_if_t<
        std::is_same_v<decltype(static_cast<Enum *>(nullptr)->GetCount(nullptr)), HRESULT>
        && std::is_same_v<decltype(static_cast<Enum *>(nullptr)->Next(0, nullptr, nullptr)), HRESULT>
    >
>
auto to_vector(com::ptr<Enum> enu)
{
    const auto len = enu.get(&Enum::GetCount);
    std::vector<com::ptr<Elt>> result(len);

    for (unsigned int i = 0; i < len; i++) {
        DWORD count;
        // For some reason trying to get more than one at a time
        // leads to wrong results.
        com::hresult::check(enu->Next(1, &result[i], &count));
    }

    return result;
}

}}
