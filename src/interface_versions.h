#pragma once

// Steam interface versions we hook. ISteamApps v009 appends methods to v008
// (vtable-compatible), but SteamUser022 added GetAuthTicketForWebApi in the
// middle of the vtable and SteamClient021 removed 4 methods, so those need
// separate wrapper classes (see Hookey_SteamUser_Class21/23,
// Hookey_SteamClient_Class20/23 in steam_hooks.cpp).
#define STEAMAPPS_INTERFACE_VERSION_N008 "STEAMAPPS_INTERFACE_VERSION008"
#define STEAMAPPS_INTERFACE_VERSION_N009 "STEAMAPPS_INTERFACE_VERSION009"

#define STEAMUSER_INTERFACE_VERSION_020 "SteamUser020"
#define STEAMUSER_INTERFACE_VERSION_021 "SteamUser021"
#define STEAMUSER_INTERFACE_VERSION_022 "SteamUser022"
#define STEAMUSER_INTERFACE_VERSION_023 "SteamUser023"

#define STEAMCLIENT_INTERFACE_VERSION_017 "SteamClient017"
#define STEAMCLIENT_INTERFACE_VERSION_018 "SteamClient018"
#define STEAMCLIENT_INTERFACE_VERSION_019 "SteamClient019"
#define STEAMCLIENT_INTERFACE_VERSION_020 "SteamClient020"
#define STEAMCLIENT_INTERFACE_VERSION_021 "SteamClient021"
#define STEAMCLIENT_INTERFACE_VERSION_022 "SteamClient022"
#define STEAMCLIENT_INTERFACE_VERSION_023 "SteamClient023"
