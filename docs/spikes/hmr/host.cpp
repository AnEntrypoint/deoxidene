#include <dlfcn.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include "api.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using Ms = std::chrono::duration<double, std::milli>;

namespace {

constexpr const char* kZig = "zig";

struct Loaded {
    void* handle = nullptr;
    const DeoxPluginApi* api = nullptr;
};

auto build(int gen) -> double {
    const auto start = Clock::now();
    const std::string cmd = std::string(kZig) + " c++ -shared -fPIC -O1 -std=c++23 plugin.cpp -o plugin.gen" +
                            std::to_string(gen) + ".so";
    if (std::system(cmd.c_str()) != 0) {
        std::fprintf(stderr, "build failed for gen %d\n", gen);
        std::exit(2);
    }
    return Ms(Clock::now() - start).count();
}

auto load(int gen) -> std::pair<Loaded, double> {
    const auto start = Clock::now();
    Loaded out;
    out.handle = dlopen(("./plugin.gen" + std::to_string(gen) + ".so").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (out.handle == nullptr) {
        std::fprintf(stderr, "dlopen: %s\n", dlerror());
        std::exit(3);
    }
    const auto get = reinterpret_cast<DeoxPluginApiFn>(dlsym(out.handle, "deox_plugin_api"));
    out.api = get();
    return {out, Ms(Clock::now() - start).count()};
}

}  // namespace

auto main() -> int {
    DeoxState state{};
    int gen = 0;
    const double cold_ms = build(gen);
    auto [live, first_load_ms] = load(gen);
    std::printf("cold build %.1f ms, first load %.3f ms\n", cold_ms, first_load_ms);

    fs::file_time_type seen = fs::last_write_time("plugin.cpp");
    std::optional<Clock::time_point> edit_at;
    const char* variants[] = {"plugin_v1.cpp", "plugin_v2.cpp", "plugin_v3.cpp"};
    int next_variant = 0;

    for (std::uint64_t frame = 0; frame < 3000; ++frame) {
        live.api->update(&state);

        if (frame == 500 || frame == 1500 || frame == 2500) {
            edit_at = Clock::now();
            fs::copy_file(variants[next_variant++], "plugin.cpp", fs::copy_options::overwrite_existing);
        }
        if (edit_at && fs::last_write_time("plugin.cpp") != seen) {
            seen = fs::last_write_time("plugin.cpp");
            const double build_ms = build(++gen);
            auto [next, load_ms] = load(gen);
            Loaded old = live;
            live = next;
            if (old.handle != nullptr) {
                dlclose(old.handle);
            }
            const double edit_to_live_ms = Ms(Clock::now() - *edit_at).count();
            std::printf("reload gen %d: build %.1f ms, load %.3f ms, edit-to-live %.1f ms\n", gen, build_ms, load_ms,
                        edit_to_live_ms);
            edit_at.reset();
        }
    }
    std::printf("frames %llu total %.1f\n", static_cast<unsigned long long>(state.frames), state.total);
    return 0;
}
