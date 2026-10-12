// Config pages without code (cmfm 1.3, docs/design/config-pages.md): the
// description files in CMFM's pages folder, read once at al_plugin_init, and
// the game folder's files for them.
#pragma once

#include "al/al_plugin.h"

namespace cmfm::native {

// Reads <cmfm folder>\pages\*.json (through the game's virtual folder, so
// every MO2 mod's files are seen), checks each description and its config
// file, keeps the author's page over a community page for the same config,
// and adds the pages through cmfm.menu. A description that is refused is
// listed on the Overview with the reason (and logged).
void config_pages_load(const al_api_t* api, al_plugin_t* self, const wchar_t* cmfm_folder, const wchar_t* game_folder);

}  // namespace cmfm::native
