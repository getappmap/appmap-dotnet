#pragma once

#include <utility>
#include <pal_mstypes.h>
#include <utf8.h>

namespace com {

struct bstr {
    constexpr bstr() noexcept : ptr(nullptr) {}
    ~bstr() noexcept
    {
        if (ptr != nullptr)
            SysFreeString(ptr);
    }
    
    bstr(const bstr &other) = delete;
    bstr(bstr &&other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    
    bstr &operator=(const bstr &) = delete;
    bstr &operator=(bstr &&other)
    {
        BSTR temp = ptr;
        ptr = other.ptr;
        other.ptr = temp;
        return *this;
    }
    
    constexpr BSTR *operator &() noexcept
    {
        return &ptr;
    }
    
    operator std::string() const
    {
        return utf8::utf16to8(ptr);
    }
private:
    BSTR ptr;
};

}
