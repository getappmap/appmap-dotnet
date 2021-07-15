#pragma once

#include <variant>
#include <vector>

#include <pal_mstypes.h>
#include <cor.h>
#include <corhdr.h>

namespace appmap { namespace signature {

constexpr inline COR_SIGNATURE boolean = ELEMENT_TYPE_BOOLEAN;
constexpr inline COR_SIGNATURE int32 = ELEMENT_TYPE_I4;
constexpr inline COR_SIGNATURE native_int = ELEMENT_TYPE_I;
constexpr inline COR_SIGNATURE object = ELEMENT_TYPE_OBJECT;
constexpr inline COR_SIGNATURE string = ELEMENT_TYPE_STRING;
constexpr inline COR_SIGNATURE Void = ELEMENT_TYPE_VOID;

struct value { mdTypeRef token; };

using signature = std::vector<COR_SIGNATURE>;

using type = std::variant<mdTypeRef, value, COR_SIGNATURE, signature>;

signature field(type t);
signature method(type return_type, std::initializer_list<type> parameters);
signature static_method(type return_type, std::initializer_list<type> parameters);
signature locals(std::initializer_list<type> types);
signature generic(type typeRef, std::initializer_list<type> params);

}}
