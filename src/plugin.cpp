// CMFM skeleton: an Astera Loader plugin that loads, logs its version and
// reports ACTIVE. It asks AL for no services yet: asking for "al.input" or
// "al.d3d12" makes AL install those layers, so that waits until the menu
// uses them (docs/handoff-20261011.md).
#include "al/al_plugin.h"

#include <string>

namespace {

const al_plugin_info_t kInfo = {
    sizeof(al_plugin_info_t),
    AL_API_VERSION,
    "cmfm",       // name: also the folder and DLL name
    "CMFM",       // display name
    "0.1.0-dev",  // this plugin's version
    0,            // phases
    0,            // order
    nullptr,      // load_after
    nullptr,      // load_before
    nullptr,      // exe_sha256: any game exe
    nullptr,      // on_phase
    nullptr,      // dependencies
};

}  // namespace

extern "C" AL_PLUGIN_EXPORT const al_plugin_info_t* AL_CALL al_plugin_info(void) {
    return &kInfo;
}

extern "C" AL_PLUGIN_EXPORT al_result_t AL_CALL al_plugin_init(const al_api_t* api, const al_init_t* init) {
    const std::string line = std::string(kInfo.name) + " " + kInfo.version + " on Astera Loader API " +
                             std::to_string(AL_VERSION_MAJOR(api->version)) + "." +
                             std::to_string(AL_VERSION_MINOR(api->version)) + "; no menu yet";
    api->log(init->self, AL_LOG_INFO, line.c_str());
    api->set_status(init->self, AL_STATUS_ACTIVE, nullptr);
    return AL_OK;
}
