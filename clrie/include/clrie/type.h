#pragma once

#include <InstrumentationEngine.h>

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
