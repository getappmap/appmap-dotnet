#pragma once

#include <system_error>
#include <pal_mstypes.h>
#include <experimental/source_location>

namespace com {
    namespace hresult {
        const std::error_category &category() noexcept;
        
        struct error : public std::system_error {
            error(HRESULT code) : std::system_error(code, category()) {}
        };

        inline void check(HRESULT result) {
            if (result < 0)
                throw error(result);
        }

        // Transform an exception to a HRESULT.
        // If the exception is not a system error from a hresult category, log it, too.
        HRESULT of_exception(std::exception_ptr ex, const std::experimental::source_location &loc = std::experimental::source_location::current());
        
        template <typename F>
        HRESULT catch_errors(F f, const std::experimental::source_location &loc = std::experimental::source_location::current()) {
            try {
                f();
                return 0; // S_OK
            } catch (...) {
                return com::hresult::of_exception(std::current_exception(), loc);
            }
        }
    }
}

#define CATCH_HRESULT(expression) \
    try {\
        expression;\
        return S_OK;\
    } catch (...) { return com::hresult::of_exception(std::current_exception()); }
