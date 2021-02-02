#include "com/hresult.h"
#include <iostream>

using namespace com;

const std::error_category &hresult::category() noexcept
{
    static const struct hresult_category : public std::error_category {
        const char *name() const noexcept override {
            return "hresult";
        }
        std::string message(int /* condition */) const noexcept override {
            return "";
        }
    } category;
    return category;
}

HRESULT hresult::of_exception(std::exception_ptr ex, const std::experimental::source_location &loc)
{
    try {
        std::rethrow_exception(ex);
    } catch (const hresult::error &err) {
        return err.code().value();
    } catch (const std::exception &ex) {
        std::cerr << "Unhandled exception in " << loc.function_name() << "(): " << ex.what() << std::endl;
    } catch (...) {
        std::cerr << "Unhandled unknown exception in " << loc.function_name() << "()" << std::endl;
    }
    return (HRESULT)0x80004005L; // E_FAIL
}
