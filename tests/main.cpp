#include <cstdio>

#include "deoxidene/containers.hpp"
#include "deoxidene/ownership.hpp"
#include "deoxidene/parse.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAILED: %s\n", what);
    }
}

void test_parse() {
    using deoxidene::ParseError;
    using deoxidene::parse_positive_int;
    check(parse_positive_int("42").value() == 42, "parse 42");
    check(parse_positive_int("2147483647").value() == 2147483647, "parse INT_MAX");
    check(parse_positive_int("2147483648").error() == ParseError::kOverflow, "overflow");
    check(parse_positive_int("").error() == ParseError::kEmpty, "empty");
    check(parse_positive_int("-1").error() == ParseError::kNotNumeric, "negative");
    check(parse_positive_int("+1").error() == ParseError::kNotNumeric, "plus sign");
    check(parse_positive_int("12a").error() == ParseError::kNotNumeric, "trailing junk");
}

void test_small_vec() {
    deoxidene::SmallVec<int, 4> inline_only;
    inline_only.push_back(1);
    inline_only.push_back(2);
    check(inline_only.size() == 2 && inline_only.front() == 1 && inline_only.back() == 2, "inline storage");

    deoxidene::SmallVec<int, 4> spilled;
    for (int i = 0; i < 10; ++i) {
        spilled.push_back(i * 3);
    }
    bool ordered = spilled.size() == 10;
    for (int i = 0; ordered && i < 10; ++i) {
        ordered = spilled[static_cast<std::size_t>(i)] == i * 3;
    }
    check(ordered && spilled.back() == 27, "spill keeps order");
}

void test_pointers() {
    int value = 9;
    deoxidene::NotNull<int*> pointer{&value};
    check(*pointer == 9 && pointer.get() == &value, "NotNull dereference");
    auto boxed = deoxidene::make_box<int>(7);
    check(*boxed == 7, "make_box");
}

}  // namespace

auto main() -> int {
    test_parse();
    test_small_vec();
    test_pointers();
    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("all checks passed");
    return 0;
}
