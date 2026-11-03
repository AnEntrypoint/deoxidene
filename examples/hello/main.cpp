#include <cstdio>
#include <cstring>

#include "deoxidene/ownership.hpp"
#include "deoxidene/parse.hpp"

namespace {

auto describe_error(deoxidene::ParseError err) -> const char* {
    switch (err) {
        case deoxidene::ParseError::kEmpty:
            return "empty input";
        case deoxidene::ParseError::kNotNumeric:
            return "not numeric";
        case deoxidene::ParseError::kOverflow:
            return "value overflows int";
    }
    return "unknown";
}

void inject_heap_overflow() {
    auto buffer = deoxidene::make_box<int[]>(4);
    buffer[4] = 1;
    std::printf("unreachable if ASan is active: %d\n", buffer[4]);
}

}  // namespace

auto main(int argc, char** argv) -> int {
    if (argc > 1 && std::strcmp(argv[1], "--inject-bug") == 0) {
        inject_heap_overflow();
        return 0;
    }

    const char* input = argc > 1 ? argv[1] : "42";
    auto result = deoxidene::parse_positive_int(input);

    if (!result.has_value()) {
        std::fprintf(stderr, "parse failed: %s\n", describe_error(result.error()));
        return 1;
    }

    auto owned = deoxidene::make_box<int>(result.value());
    std::printf("parsed value: %d\n", *owned);
    return 0;
}
