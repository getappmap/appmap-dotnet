#pragma once

#include <functional>
#include <utility>

#include "com/bstr.h"
#include "com/guid.h"
#include "com/hresult.h"

namespace com {

namespace detail {
    template <typename T>
    struct tag {
        using type = T;
    };

    template <typename... Ts>
    using last_t = typename decltype((tag<Ts>{}, ...))::type;
}

template <class Interface>
requires std::is_base_of_v<IUnknown, Interface>
struct ptr
{
    ptr() noexcept {}
    using interface_type = Interface;

    ptr(Interface* pointer) noexcept : ptr_(pointer) {
        if (ptr_ != nullptr)
            ptr_->AddRef();
    }

    ~ptr() noexcept {
        if (ptr_ != nullptr)
            ptr_->Release();
    }

    ptr(const ptr& other) noexcept : ptr(other.ptr_) {}
    ptr(ptr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}

    ptr& operator=(const ptr& other) noexcept {
        return *this = ptr(other);
    }

    template <class Other>
    ptr& operator=(const ptr<Other> other) {
        hresult::check(other->QueryInterface(guid_of<Interface>(), reinterpret_cast<void**>(&*this)));
        return *this;
    }

    ptr& operator=(ptr&& other) noexcept {
        std::swap(ptr_, other.ptr_);
        return *this;
    }

    constexpr operator Interface*() const noexcept {
        return ptr_;
    }

    Interface* operator->() const noexcept {
        return ptr_;
    }

    Interface** operator&() noexcept {
        assert(ptr_ == nullptr);
        return &ptr_;
    }

    bstr get(HRESULT (Interface::*fun)(BSTR *)) const
    {
        bstr result;
        hresult::check(std::invoke(fun, *ptr_, &result));
        return result;
    }

    template <typename T>
    T get(HRESULT (Interface::*fun)(T *)) const
    {
        T result;
        hresult::check(std::invoke(fun, *ptr_, &result));
        return result;
    }

    template <typename... Params, typename... Args>
    auto get(HRESULT (Interface::*fun)(Params...), Args&&... args) const
    {
        std::remove_pointer_t<detail::last_t<Params...>> result;
        hresult::check(std::invoke(fun, *ptr_, std::forward<Args>(args)..., &result));
        return result;
    }

    template <class Other>
    requires std::is_base_of_v<IUnknown, Other>
    ptr<Other> get(HRESULT (Interface::*fun)(Other **)) const
    {
        ptr<Other> result;
        hresult::check(std::invoke(fun, *ptr_, &result));
        return result;
    }

    template <class Other, class... Params>
    requires std::is_base_of_v<IUnknown, Other>
    ptr<Other> get(HRESULT (Interface::*fun)(Params..., Other **), Params&&...params)
    {
        ptr<Other> result;
        hresult::check(std::invoke(fun, *ptr_, std::forward<Params>(params)..., &result));
        return result;
    }

    template <class Other>
    ptr<Other> as() const
    {
        ptr<Other> result;
        result = *this;
        return result;
    }

protected:
    Interface *ptr_ = nullptr;
};

}
