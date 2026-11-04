#include "api.h"

namespace {

void update(DeoxState* s) {
    s->frames += 1;
    s->total += 3.0;
}

const DeoxPluginApi kApi{1, update};

}  // namespace

extern "C" const DeoxPluginApi* deox_plugin_api() {
    return &kApi;
}
