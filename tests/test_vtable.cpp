// vtable layout test: every wrapper class must override its base interface's
// virtual methods at the SAME vtable slots, otherwise games calling through
// the interface would hit the wrong function (crash / wrong behaviour).
//
// On the Itanium C++ ABI (Linux) a pointer-to-member of a virtual function
// encodes the vtable slot offset; comparing the encoded values of a base
// method and the wrapper override tells us whether they share the slot.
//
// The wrapper classes live in steam_hooks.cpp, so this test compiles that
// translation unit directly and links config.cpp for the shared globals.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../src/config.h"
#include "../src/steam_hooks.h"

#include "../src/steam_hooks.cpp"  // wrapper class definitions

static int failures = 0;

// Itanium C++ ABI: a pointer-to-member of a virtual function starts with the
// vtable slot offset (the adjustment word follows). Reading the first word
// gives the slot; equal slots for base and wrapper mean a valid override.
template <typename M>
static uintptr_t member_slot(M member) {
    uintptr_t slot = 0;
    static_assert(sizeof(M) >= sizeof(uintptr_t), "member pointer too small");
    std::memcpy(&slot, &member, sizeof(uintptr_t));
    return slot;
}

#define CHECK_SLOT(Base, Wrap, method)                                        \
    do {                                                                      \
        uintptr_t base_slot = member_slot(&Base::method);                     \
        uintptr_t wrap_slot = member_slot(&Wrap::method);                     \
        if (base_slot != wrap_slot) {                                         \
            fprintf(stderr, "FAIL: %s::%s at different vtable slot "          \
                            "(base=%#lx wrap=%#lx)\n", #Base, #method,       \
                    (unsigned long)base_slot, (unsigned long)wrap_slot);      \
            failures++;                                                       \
        }                                                                     \
    } while (0)

// ISteamApps v009 vs the single wrapper serving v008/v009
#define CHECK_APPS(method) CHECK_SLOT(ISteamApps, Hookey_SteamApps_Class, method)

// ISteamUser v021 vs v023 wrappers
#define CHECK_USER21(method) \
    CHECK_SLOT(ISteamUser021, Hookey_SteamUser_Class21, method)
#define CHECK_USER23(method) \
    CHECK_SLOT(ISteamUser, Hookey_SteamUser_Class23, method)

// ISteamClient v020 vs v023 wrappers
#define CHECK_CLIENT20(method) \
    CHECK_SLOT(ISteamClient020, Hookey_SteamClient_Class20, method)
#define CHECK_CLIENT23(method) \
    CHECK_SLOT(ISteamClient, Hookey_SteamClient_Class23, method)

// SteamClient021+ moved several methods into a protected STEAM_PRIVATE_API
// section; expose them through a derived class so the slots are testable.
class PublicISteamClient : public ISteamClient {
public:
    using ISteamClient::RunFrame;
    using ISteamClient::DestroyAllInterfaces;
    using ISteamClient::DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess;
    using ISteamClient::DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess;
    using ISteamClient::Set_SteamAPI_CCheckCallbackRegisteredInProcess;
};

#define CHECK_CLIENT23_PUB(method) \
    CHECK_SLOT(PublicISteamClient, Hookey_SteamClient_Class23, method)

int main() {
    // ---- ISteamApps ------------------------------------------------------
    CHECK_APPS(BIsSubscribed);
    CHECK_APPS(BIsLowViolence);
    CHECK_APPS(BIsCybercafe);
    CHECK_APPS(BIsVACBanned);
    CHECK_APPS(GetDLCCount);
    CHECK_APPS(BIsDlcInstalled);
    CHECK_APPS(BGetDLCDataByIndex);
    CHECK_APPS(GetCurrentGameLanguage);
    CHECK_APPS(GetAvailableGameLanguages);
    CHECK_APPS(GetAppOwner);
    CHECK_APPS(GetAppBuildId);
    CHECK_APPS(RequestAllProofOfPurchaseKeys);
    CHECK_APPS(BIsSubscribedFromFamilySharing);
    CHECK_APPS(BIsSubscribedFromFreeWeekend);
    CHECK_APPS(BIsSubscribedApp);
    CHECK_APPS(BIsAppInstalled);
    CHECK_APPS(GetEarliestPurchaseUnixTime);
    CHECK_APPS(InstallDLC);
    CHECK_APPS(UninstallDLC);
    CHECK_APPS(RequestAppProofOfPurchaseKey);
    CHECK_APPS(GetCurrentBetaName);
    CHECK_APPS(MarkContentCorrupt);
    CHECK_APPS(GetInstalledDepots);
    CHECK_APPS(GetAppInstallDir);
    CHECK_APPS(GetLaunchQueryParam);
    CHECK_APPS(GetDlcDownloadProgress);
    CHECK_APPS(GetFileDetails);
    CHECK_APPS(GetLaunchCommandLine);
    CHECK_APPS(BIsTimedTrial);
    CHECK_APPS(SetDlcContext);
    CHECK_APPS(GetNumBetas);
    CHECK_APPS(GetBetaInfo);
    CHECK_APPS(SetActiveBeta);
    CHECK_APPS(SetGamePerformanceSetting);
    CHECK_APPS(SetGameRenderResolution);

    // ---- ISteamUser v021 (legacy) ----------------------------------------
    CHECK_USER21(GetHSteamUser);
    CHECK_USER21(BLoggedOn);
    CHECK_USER21(GetSteamID);
    CHECK_USER21(InitiateGameConnection_DEPRECATED);
    CHECK_USER21(TerminateGameConnection_DEPRECATED);
    CHECK_USER21(TrackAppUsageEvent);
    CHECK_USER21(GetUserDataFolder);
    CHECK_USER21(StartVoiceRecording);
    CHECK_USER21(StopVoiceRecording);
    CHECK_USER21(GetAvailableVoice);
    CHECK_USER21(GetVoice);
    CHECK_USER21(DecompressVoice);
    CHECK_USER21(GetAuthSessionTicket);
    CHECK_USER21(BeginAuthSession);
    CHECK_USER21(EndAuthSession);
    CHECK_USER21(CancelAuthTicket);
    CHECK_USER21(UserHasLicenseForApp);
    CHECK_USER21(BIsBehindNAT);
    CHECK_USER21(AdvertiseGame);
    CHECK_USER21(RequestEncryptedAppTicket);
    CHECK_USER21(GetEncryptedAppTicket);
    CHECK_USER21(GetGameBadgeLevel);
    CHECK_USER21(GetPlayerSteamLevel);
    CHECK_USER21(RequestStoreAuthURL);
    CHECK_USER21(BIsPhoneVerified);
    CHECK_USER21(BIsTwoFactorEnabled);
    CHECK_USER21(BIsPhoneIdentifying);
    CHECK_USER21(BIsPhoneRequiringVerification);
    CHECK_USER21(GetMarketEligibility);
    CHECK_USER21(GetDurationControl);
    CHECK_USER21(BSetDurationControlOnlineState);

    // ---- ISteamUser v023 (modern) ----------------------------------------
    CHECK_USER23(GetHSteamUser);
    CHECK_USER23(BLoggedOn);
    CHECK_USER23(GetSteamID);
    CHECK_USER23(InitiateGameConnection_DEPRECATED);
    CHECK_USER23(TerminateGameConnection_DEPRECATED);
    CHECK_USER23(TrackAppUsageEvent);
    CHECK_USER23(GetUserDataFolder);
    CHECK_USER23(StartVoiceRecording);
    CHECK_USER23(StopVoiceRecording);
    CHECK_USER23(GetAvailableVoice);
    CHECK_USER23(GetVoice);
    CHECK_USER23(DecompressVoice);
    CHECK_USER23(GetAuthSessionTicket);
    CHECK_USER23(GetAuthTicketForWebApi);
    CHECK_USER23(BeginAuthSession);
    CHECK_USER23(EndAuthSession);
    CHECK_USER23(CancelAuthTicket);
    CHECK_USER23(UserHasLicenseForApp);
    CHECK_USER23(BIsBehindNAT);
    CHECK_USER23(AdvertiseGame);
    CHECK_USER23(RequestEncryptedAppTicket);
    CHECK_USER23(GetEncryptedAppTicket);
    CHECK_USER23(GetGameBadgeLevel);
    CHECK_USER23(GetPlayerSteamLevel);
    CHECK_USER23(RequestStoreAuthURL);
    CHECK_USER23(BIsPhoneVerified);
    CHECK_USER23(BIsTwoFactorEnabled);
    CHECK_USER23(BIsPhoneIdentifying);
    CHECK_USER23(BIsPhoneRequiringVerification);
    CHECK_USER23(GetMarketEligibility);
    CHECK_USER23(GetDurationControl);
    CHECK_USER23(BSetDurationControlOnlineState);

    // ---- ISteamClient v020 (legacy) --------------------------------------
    CHECK_CLIENT20(CreateSteamPipe);
    CHECK_CLIENT20(BReleaseSteamPipe);
    CHECK_CLIENT20(ConnectToGlobalUser);
    CHECK_CLIENT20(CreateLocalUser);
    CHECK_CLIENT20(ReleaseUser);
    CHECK_CLIENT20(GetISteamUser);
    CHECK_CLIENT20(GetISteamGameServer);
    CHECK_CLIENT20(SetLocalIPBinding);
    CHECK_CLIENT20(GetISteamFriends);
    CHECK_CLIENT20(GetISteamUtils);
    CHECK_CLIENT20(GetISteamMatchmaking);
    CHECK_CLIENT20(GetISteamMatchmakingServers);
    CHECK_CLIENT20(GetISteamGenericInterface);
    CHECK_CLIENT20(GetISteamUserStats);
    CHECK_CLIENT20(GetISteamGameServerStats);
    CHECK_CLIENT20(GetISteamApps);
    CHECK_CLIENT20(GetISteamNetworking);
    CHECK_CLIENT20(GetISteamRemoteStorage);
    CHECK_CLIENT20(GetISteamScreenshots);
    CHECK_CLIENT20(GetISteamGameSearch);
    CHECK_CLIENT20(RunFrame);
    CHECK_CLIENT20(SetWarningMessageHook);
    CHECK_CLIENT20(BShutdownIfAllPipesClosed);
    CHECK_CLIENT20(GetISteamHTTP);
    CHECK_CLIENT20(DEPRECATED_GetISteamUnifiedMessages);
    CHECK_CLIENT20(GetISteamController);
    CHECK_CLIENT20(GetISteamUGC);
    CHECK_CLIENT20(GetISteamAppList);
    CHECK_CLIENT20(GetISteamMusic);
    CHECK_CLIENT20(GetISteamMusicRemote);
    CHECK_CLIENT20(GetISteamHTMLSurface);
    CHECK_CLIENT20(DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess);
    CHECK_CLIENT20(DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess);
    CHECK_CLIENT20(Set_SteamAPI_CCheckCallbackRegisteredInProcess);
    CHECK_CLIENT20(GetISteamInventory);
    CHECK_CLIENT20(GetISteamVideo);
    CHECK_CLIENT20(GetISteamParentalSettings);
    CHECK_CLIENT20(GetISteamInput);
    CHECK_CLIENT20(GetISteamParties);
    CHECK_CLIENT20(GetISteamRemotePlay);
    CHECK_CLIENT20(DestroyAllInterfaces);
    CHECK_CLIENT20(GetIPCCallCount);

    // ---- ISteamClient v023 (modern) --------------------------------------
    CHECK_CLIENT23(CreateSteamPipe);
    CHECK_CLIENT23(BReleaseSteamPipe);
    CHECK_CLIENT23(ConnectToGlobalUser);
    CHECK_CLIENT23(CreateLocalUser);
    CHECK_CLIENT23(ReleaseUser);
    CHECK_CLIENT23(GetISteamUser);
    CHECK_CLIENT23(GetISteamGameServer);
    CHECK_CLIENT23(SetLocalIPBinding);
    CHECK_CLIENT23(GetISteamFriends);
    CHECK_CLIENT23(GetISteamUtils);
    CHECK_CLIENT23(GetISteamMatchmaking);
    CHECK_CLIENT23(GetISteamMatchmakingServers);
    CHECK_CLIENT23(GetISteamGenericInterface);
    CHECK_CLIENT23(GetISteamUserStats);
    CHECK_CLIENT23(GetISteamGameServerStats);
    CHECK_CLIENT23(GetISteamApps);
    CHECK_CLIENT23(GetISteamNetworking);
    CHECK_CLIENT23(GetISteamRemoteStorage);
    CHECK_CLIENT23(GetISteamScreenshots);
    CHECK_CLIENT23_PUB(RunFrame);
    CHECK_CLIENT23(SetWarningMessageHook);
    CHECK_CLIENT23(BShutdownIfAllPipesClosed);
    CHECK_CLIENT23(GetISteamHTTP);
    CHECK_CLIENT23(GetISteamController);
    CHECK_CLIENT23(GetISteamUGC);
    CHECK_CLIENT23(GetISteamMusic);
    CHECK_CLIENT23(GetISteamHTMLSurface);
    CHECK_CLIENT23_PUB(DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess);
    CHECK_CLIENT23_PUB(DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess);
    CHECK_CLIENT23_PUB(Set_SteamAPI_CCheckCallbackRegisteredInProcess);
    CHECK_CLIENT23(GetISteamInventory);
    CHECK_CLIENT23(GetISteamVideo);
    CHECK_CLIENT23(GetISteamParentalSettings);
    CHECK_CLIENT23(GetISteamInput);
    CHECK_CLIENT23(GetISteamParties);
    CHECK_CLIENT23(GetISteamRemotePlay);
    CHECK_CLIENT23_PUB(DestroyAllInterfaces);
    CHECK_CLIENT23(GetIPCCallCount);

    if (failures == 0) {
        printf("vtable layout OK (all wrapper overrides match base slots)\n");
        return 0;
    }
    printf("%d vtable mismatch(es)\n", failures);
    return 1;
}
