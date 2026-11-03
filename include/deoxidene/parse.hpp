#pragma once

#include <cstdint>
#include <limits>
#include <string_view>

#include "deoxidene/result.hpp"

namespace deoxidene {

enum class ParseError : std::uint8_t { kEmpty, kNotNumeric, kOverflow };

namespace detail {
inline constexpr int kDecimalBase = 10;
}  // namespace detail

[[nodiscard]] constexpr auto parse_positive_int(std::string_view text) -> Result<int, ParseError> {
    if (text.empty()) {
        return Fail<ParseError, int>(ParseError::kEmpty);
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return Fail<ParseError, int>(ParseError::kNotNumeric);
        }
        const int digit = c - '0';
        if (value > (std::numeric_limits<int>::max() - digit) / detail::kDecimalBase) {
            return Fail<ParseError, int>(ParseError::kOverflow);
        }
        value = (value * detail::kDecimalBase) + digit;
    }
    return Ok<int, ParseError>(value);
}

}  // namespace deoxidene
