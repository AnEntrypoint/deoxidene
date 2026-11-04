#pragma once

#include <cstdint>

struct DeoxState {
    std::uint64_t frames;
    double total;
};

struct DeoxPluginApi {
    std::uint32_t abi;
    void (*update)(DeoxState*);
};

extern "C" typedef const DeoxPluginApi* (*DeoxPluginApiFn)();
