#include "signature.h"


#include <doctest/doctest.h>
#ifndef DOCTEST_CONFIG_DISABLE
#include <iomanip>
#include <sstream>

namespace doctest {
    template<> struct StringMaker<appmap::signature::signature> {
        static String convert(const appmap::signature::signature& sig) {
            std::ostringstream out;
            out << "SIG: " << std::setfill('0');
            for (const auto v: sig) {
                out << std::hex << std::setw(2) << static_cast<unsigned int>(v) << " ";
            }
            return String(std::move(out.str()).c_str());
        }
    };
}
#endif

namespace appmap::signature {

struct push {
    signature &sig;
    void operator()(COR_SIGNATURE t) {
        sig.push_back(t);
    }

    void operator()(mdToken token) {
        sig.push_back(ELEMENT_TYPE_CLASS);
        COR_SIGNATURE tok[4];
        const auto len = CorSigCompressToken(token, tok);
        for (size_t i = 0; i < len; i++)
            sig.push_back(tok[i]);
    }
};

signature build(COR_SIGNATURE call_convention, type return_type, std::initializer_list<type> parameters)
{
    signature sig;
    sig.reserve(2 + 5 * (parameters.size() + 1));
    sig.push_back(call_convention);
    sig.push_back(parameters.size());

    push pusher{sig};
    std::visit(pusher, return_type);
    for (const auto &param: parameters)
        std::visit(pusher, param);

    return std::move(sig);
}

signature method(type return_type, std::initializer_list<type> parameters)
{
    return build(IMAGE_CEE_CS_CALLCONV_DEFAULT_HASTHIS, return_type, std::move(parameters));
}

signature static_method(type return_type, std::initializer_list<type> parameters)
{
    return build(IMAGE_CEE_CS_CALLCONV_DEFAULT, return_type, std::move(parameters));
}


TEST_CASE("building signatures") {
    CHECK(static_method(Void, {}) == signature{ IMAGE_CEE_CS_CALLCONV_DEFAULT, 0, ELEMENT_TYPE_VOID });
    CHECK(method(Void, {string}) == signature{ IMAGE_CEE_CS_CALLCONV_DEFAULT_HASTHIS, 1, ELEMENT_TYPE_VOID, ELEMENT_TYPE_STRING });
    CHECK(static_method(mdTypeRef{0x01000012}, {object, mdTypeRef{0x01000013}}) == signature{ 0, 2, 0x12, 0x49, 0x1c, 0x12, 0x4d });
}

}
