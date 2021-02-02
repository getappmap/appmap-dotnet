#pragma once

#include <functional>
#include <utility>

#include "com/bstr.h"
#include "com/guid.h"
#include "com/hresult.h"

namespace com {

template <class Interface>
struct ptr
{
    ptr() noexcept {}
    
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
    
    ptr& operator=(ptr&& other) noexcept {
        std::swap(ptr_, other.ptr_);
        return *this;
    }
    
    constexpr operator Interface*() noexcept {
        return ptr_;
    }
    
    Interface* operator->() noexcept {
        return ptr_;
    }
    
    Interface** operator&() noexcept {
        assert(ptr_ == nullptr);
        return &ptr_;
    }
    
    bstr get(HRESULT (Interface::*fun)(BSTR *))
    {
        bstr result;
        check_hresult(std::invoke(fun, *ptr_, &result));
        return result;
    }
    
    template <typename T>
    T get(HRESULT (Interface::*fun)(T *))
    {
        T result;
        check_hresult(std::invoke(fun, *ptr_, &result));
        return result;
    }

    template <class Other>
    ptr<Other> get(HRESULT (Interface::*fun)(Other **))
    {
        ptr<Other> result;
        check_hresult(std::invoke(fun, *ptr_, &result));
        return result;
    }

    template <class Other, class... Params>
    ptr<Other> get(HRESULT (Interface::*fun)(Params..., Other **), Params&&...params)
    {
        ptr<Other> result;
        check_hresult(std::invoke(fun, *ptr_, std::forward<Params...>(params)..., &result));
        return result;
    }
    
    template <class Other>
    ptr<Other> as()
    {
        ptr<Other> result;
        check_hresult(ptr_->QueryInterface(guid_of<Other>(), reinterpret_cast<void**>(&result)));
        return result;
    }
    
    ptr &mut() const {
        return const_cast<ptr &>(*this);
    }

private:
    Interface *ptr_ = nullptr;
};

}
