#pragma once

#include <variant>
#include <vector>

#include <pal_mstypes.h>
#include <cor.h>
#include <corhdr.h>

namespace appmap { namespace signature {

constexpr inline COR_SIGNATURE native_int = ELEMENT_TYPE_I;
constexpr inline COR_SIGNATURE object = ELEMENT_TYPE_OBJECT;
constexpr inline COR_SIGNATURE string = ELEMENT_TYPE_STRING;
constexpr inline COR_SIGNATURE Void = ELEMENT_TYPE_VOID;

struct value { mdTypeRef token; };

using type = std::variant<mdTypeRef, value, COR_SIGNATURE>;

using signature = std::vector<COR_SIGNATURE>;

signature field(type t);
signature method(type return_type, std::initializer_list<type> parameters);
signature static_method(type return_type, std::initializer_list<type> parameters);

}}
