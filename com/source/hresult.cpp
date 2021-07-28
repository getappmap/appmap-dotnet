#include "com/hresult.h"
#include <iostream>
#include <winerror.h>

using namespace com;

const std::error_category &hresult::category() noexcept
{
    static const struct hresult_category : public std::error_category {
        const char *name() const noexcept override {
            return "hresult";
        }
        std::string message(int condition) const noexcept override {
            switch (condition) {
                case E_NOTIMPL:
                    return "E_NOTIMPL not implemented";
                default:
                    return std::string("HRESULT ") + std::to_string(condition);
            }
        }
    } category;
    return category;
}

HRESULT hresult::of_exception(std::exception_ptr ex)
{
    try {
        std::rethrow_exception(ex);
    } catch (const hresult::error &err) {
        return err.code().value();
    } catch (const std::exception &ex) {
        std::cerr << "Unhandled exception: " << ex.what() << std::endl;
    } catch (...) {
        std::cerr << "Unhandled unknown exception" << std::endl;
    }
    return (HRESULT)0x80004005L; // E_FAIL
}
