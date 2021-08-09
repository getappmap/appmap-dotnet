#pragma once

#include <com/ptr.h>

#include <no_sal.h>
#include <cor.h>

#include <InstrumentationEngine.h>

template<>
constexpr GUID com::guid_of<ITokenType>() noexcept {
    using namespace com::literals;
    return "77655B33-1B29-4285-9F2D-FF9526E3A0AA"_guid;
}

template<>
constexpr GUID com::guid_of<ICompositeType>() noexcept {
    using namespace com::literals;
    return "06B9FD79-0386-4CF3-93DD-A23E95EBC225"_guid;
}

namespace clrie {
    struct type: public com::ptr<IType>
    {
        using ptr::ptr;
        using ptr::operator IType*;

        type(ptr &&t) noexcept: ptr(std::move(t)) {}

        void add_to_signature(com::ptr<ISignatureBuilder> signature_builder) const {
            com::hresult::check(ptr_->AddToSignature(signature_builder));
        }

        CorElementType cor_element_type() const {
            return get(&interface_type::GetCorElementType);
        }

        bool is_primitive() const {
            return get(&interface_type::IsPrimitive);
        }
        bool is_array() const {
            return get(&interface_type::IsArray);
        }
        bool is_class() const {
            return get(&interface_type::IsClass);
        }

        std::string name() const {
            return get(&interface_type::GetName);
        }
    };
}
