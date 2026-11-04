#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "deoxidene/containers.hpp"
#include "deoxidene/parse.hpp"

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t kIterations = 20'000'000;
constexpr std::size_t kRepeats = 7;
constexpr std::array<std::string_view, 4> kInputs{"7", "42", "123456", "2147483647"};

template <typename T>
inline void keep(T& value) {
#if defined(_MSC_VER)
    (void)value;
    _ReadWriteBarrier();
#else
    asm volatile("" : : "g"(&value) : "memory");
#endif
}

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

auto checksum_raw(std::string_view text) -> std::uint64_t {
    int value = 0;
    return parse_raw(text, value) ? static_cast<std::uint64_t>(value) : 0;
}

auto checksum_result(std::string_view text) -> std::uint64_t {
    const auto parsed = deoxidene::parse_positive_int(text);
    return parsed.has_value() ? static_cast<std::uint64_t>(parsed.value()) : 0;
}

auto spread(std::size_t i) -> int {
    return static_cast<int>((static_cast<std::uint32_t>(i) * 2654435761u) >> 29);
}

auto fill_std_vector(int base) -> std::uint64_t {
    std::vector<int> values;
    values.reserve(4);
    values.push_back(base);
    values.push_back(base + 1);
    values.push_back(base + 2);
    values.push_back(base + 3);
    keep(values);
    return static_cast<std::uint64_t>(values.front() + values.back());
}

auto fill_small_vec(int base) -> std::uint64_t {
    deoxidene::SmallVec<int, 8> values;
    values.push_back(base);
    values.push_back(base + 1);
    values.push_back(base + 2);
    values.push_back(base + 3);
    keep(values);
    return static_cast<std::uint64_t>(values.front() + values.back());
}

template <typename Body>
auto best_of(Body body) -> std::pair<double, std::uint64_t> {
    double best_ns = std::numeric_limits<double>::max();
    std::uint64_t total = 0;
    for (std::size_t repeat = 0; repeat < kRepeats; ++repeat) {
        std::uint64_t sum = 0;
        const auto start = Clock::now();
        for (std::size_t i = 0; i < kIterations; ++i) {
            sum += body(i);
        }
        const auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
        best_ns = std::min(best_ns, elapsed / static_cast<double>(kIterations));
        total = sum;
    }
    return {best_ns, total};
}

}  // namespace

auto main() -> int {
    const auto [raw_ns, raw_total] = best_of([](std::size_t i) { return checksum_raw(kInputs[i % kInputs.size()]); });
    const auto [result_ns, result_total] = best_of([](std::size_t i) { return checksum_result(kInputs[i % kInputs.size()]); });
    const auto [vector_ns, vector_total] = best_of([](std::size_t i) { return fill_std_vector(spread(i)); });
    const auto [small_ns, small_total] = best_of([](std::size_t i) { return fill_small_vec(spread(i)); });
    std::printf("parse raw            %8.3f ns/op  checksum=%llu\n", raw_ns, static_cast<unsigned long long>(raw_total));
    std::printf("parse Result         %8.3f ns/op  checksum=%llu\n", result_ns, static_cast<unsigned long long>(result_total));
    std::printf("collect std::vector  %8.3f ns/op  checksum=%llu\n", vector_ns, static_cast<unsigned long long>(vector_total));
    std::printf("collect SmallVec<8>  %8.3f ns/op  checksum=%llu\n", small_ns, static_cast<unsigned long long>(small_total));
    return (raw_total == result_total && vector_total == small_total) ? 0 : 1;
}
