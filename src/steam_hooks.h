#pragma once

#include "ext/steam/isteamapps.h"
#include "ext/steam/isteamclient.h"
#include "ext/steam/isteamclient020.h"
#include "ext/steam/isteamuser.h"
#include "ext/steam/isteamuser021.h"

// Wrapper factories. Dispatch on the requested interface version (see
// export_hooks.cpp): SteamApps has a single v009 wrapper serving v008/v009;
// SteamUser needs v021 (SteamUser020/021) and v023 (SteamUser022/023)
// wrappers; SteamClient needs v020 (SteamClient017-020) and v023
// (SteamClient021-023) wrappers.
ISteamApps* Hookey_SteamApps(ISteamApps* real_steamApps);
ISteamUser021* Hookey_SteamUser21(ISteamUser021* real_steamUser);
ISteamUser* Hookey_SteamUser23(ISteamUser* real_steamUser);
ISteamClient020* Hookey_SteamClient20(ISteamClient020* real_steamClient);
ISteamClient* Hookey_SteamClient23(ISteamClient* real_steamClient);
