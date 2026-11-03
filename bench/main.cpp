#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "deoxidene/result.hpp"

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t kIterations = 50'000'000;
constexpr std::array<std::string_view, 4> kInputs{"7", "42", "123456", "2147483647"};

enum class ParseError { kEmpty, kNotNumeric, kOverflow };

auto parse_raw(std::string_view text, int& out) -> bool {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        const int digit = c - '0';
        if (value > (std::numeric_limits<int>::max() - digit) / 10) {
            return false;
        }
        value = (value * 10) + digit;
    }
    out = value;
    return true;
}

auto parse_result(std::string_view text) -> deoxidene::Result<int, ParseError> {
    if (text.empty()) {
        return deoxidene::Fail<ParseError, int>(ParseError::kEmpty);
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return deoxidene::Fail<ParseError, int>(ParseError::kNotNumeric);
        }
        const int digit = c - '0';
        if (value > (std::numeric_limits<int>::max() - digit) / 10) {
            return deoxidene::Fail<ParseError, int>(ParseError::kOverflow);
        }
        value = (value * 10) + digit;
    }
    return deoxidene::Ok<int, ParseError>(value);
}

auto checksum_raw(std::string_view text) -> std::uint64_t {
    int value = 0;
    return parse_raw(text, value) ? static_cast<std::uint64_t>(value) : 0;
}

auto checksum_result(std::string_view text) -> std::uint64_t {
    const auto parsed = parse_result(text);
    return parsed.has_value() ? static_cast<std::uint64_t>(parsed.value()) : 0;
}

template <typename Checksum>
auto measure(Checksum checksum) -> std::pair<double, std::uint64_t> {
    std::uint64_t total = 0;
    const auto start = Clock::now();
    for (std::size_t i = 0; i < kIterations; ++i) {
        total += checksum(kInputs[i % kInputs.size()]);
    }
    const auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - start);
    return {elapsed.count() / static_cast<double>(kIterations), total};
}

}  // namespace

auto main() -> int {
    const auto [raw_ns, raw_total] = measure(checksum_raw);
    const auto [result_ns, result_total] = measure(checksum_result);
    std::printf("raw     %8.3f ns/op  checksum=%llu\n", raw_ns, static_cast<unsigned long long>(raw_total));
    std::printf("Result  %8.3f ns/op  checksum=%llu\n", result_ns, static_cast<unsigned long long>(result_total));
    std::printf("Result/raw %.3f\n", result_ns / raw_ns);
    return raw_total == result_total ? 0 : 1;
}
