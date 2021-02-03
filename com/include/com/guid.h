#pragma once

#include <pal_mstypes.h>
#include <ios>

namespace com {

template<class Interface>
constexpr GUID guid_of() noexcept;

namespace literals {
    namespace detail {
        constexpr char parse_hex(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            else if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            else return -1;
        }

        // This isn't too safe, don't try it at home.
        // No attempts at validating input. Don't use at runtime.
        template <typename T>
        constexpr T parse_hex(const char *&str)
        {
            T result = 0;
            for (unsigned int i = 0; i < sizeof(T) * 2; i++) {
                result <<= 4;
                char digit = 0;
                while ((digit = parse_hex(*(str++))) < 0);
                result |= digit;
            }
            return result;
        }
    }

    constexpr GUID operator ""_guid(const char *str, [[maybe_unused]] size_t length)
    {
        GUID result{};
        result.Data1 = detail::parse_hex<uint32_t>(str);
        result.Data2 = detail::parse_hex<uint16_t>(str);
        result.Data3 = detail::parse_hex<uint16_t>(str);
        for (int i = 0; i < 8; i++)
            result.Data4[i] = detail::parse_hex<uint8_t>(str);

        return result;
    }
}

template<>
constexpr GUID guid_of<IClassFactory>() noexcept {
    using namespace literals;
    return "00000001-0000-0000-C000-000000000046"_guid;
}

template<>
constexpr GUID guid_of<IUnknown>() noexcept {
    using namespace literals;
    return "00000000-0000-0000-C000-000000000046"_guid;
}

}

template <typename OStream>
OStream &operator<<(OStream &os, const GUID &guid) {
    return os << "{" << std::hex << guid.Data1 << "-" << guid.Data2 << "-" << guid.Data3 << "-" << 
        (guid.Data4[0] << 8 | guid.Data4[1]) << "-" << 
        (guid.Data4[2] << 8 | guid.Data4[3]) << 
        (guid.Data4[4] << 8 | guid.Data4[5]) << 
        (guid.Data4[6] << 8 | guid.Data4[7])
    << "}";
}
