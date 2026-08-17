#include <cstdio>
#include <cstring>
#include <string>

#include "deoxidene/ownership.hpp"
#include "deoxidene/result.hpp"

namespace {

enum class ParseError { kEmpty, kNotNumeric };

auto parse_positive_int(deoxidene::NotNull<const char*> text) -> deoxidene::Result<int, ParseError> {
    if (std::strlen(text.get()) == 0) {
        return deoxidene::Fail<ParseError, int>(ParseError::kEmpty);
    }
    int value = 0;
    for (const char* p = text.get(); *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return deoxidene::Fail<ParseError, int>(ParseError::kNotNumeric);
        }
        value = (value * 10) + (*p - '0');
    }
    return deoxidene::Ok<int, ParseError>(value);
}

auto describe_error(ParseError err) -> const char* {
    switch (err) {
        case ParseError::kEmpty:
            return "empty input";
        case ParseError::kNotNumeric:
            return "not numeric";
    }
    return "unknown";
}

// Deliberate injected out-of-bounds write behind --inject-bug, for ASan/UBSan demonstration.
void inject_bug() {
    auto buffer = deoxidene::make_box<int[]>(4);
    buffer[4] = 1;  // one past the end: ASan heap-buffer-overflow
    std::printf("unreachable if ASan is active: %d\n", buffer[4]);
}

}  // namespace

auto main(int argc, char** argv) -> int {
    if (argc > 1 && std::strcmp(argv[1], "--inject-bug") == 0) {
        inject_bug();
        return 0;
    }

    const char* input = argc > 1 ? argv[1] : "42";
    auto result = parse_positive_int(deoxidene::NotNull<const char*>{input});

    if (!result.has_value()) {
        std::fprintf(stderr, "parse failed: %s\n", describe_error(result.error()));
        return 1;
    }

    auto owned = deoxidene::make_box<int>(result.value());
    std::printf("parsed value: %d\n", *owned);
    return 0;
}
