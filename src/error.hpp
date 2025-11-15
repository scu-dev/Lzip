#pragma once
#include <string>

namespace Lzip::Util {
    using std::string;

    inline string errorMessage;

    [[nodiscard]] inline string getLastError() noexcept { return errorMessage; }
    inline void setError(const string& err) noexcept { errorMessage = err; }
}