#include <cstddef>
#include <iostream>
#include <fstream>
#include "ext/steam/isteamapps.h"
#include "ext/steam/isteamclient.h"
#include "ext/steam/isteamclient020.h"
#include "ext/steam/isteamuser.h"
#include "ext/steam/isteamuser021.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "ext/ini.h"
#include <string>
#include <cassert>
#include <vector>
#include <map>
#include <unistd.h>

using namespace std;

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <link.h>
#include <stdio.h>

vector<tuple<int, string>> dlcs;

// Returns true if the given appID is present in the cream_api.ini [dlc] list
static bool is_dlc_unlocked(AppId_t appID) {
    return std::find_if(
        std::begin(dlcs),
        std::end(dlcs),
        [&](const tuple<int, string>& a) { return std::get<0>(a) == appID; }) != std::end(dlcs);
}

// Steam interface versions we hook. ISteamApps v009 appends methods to v008 (vtable-compatible),
// but SteamUser022 added GetAuthTicketForWebApi in the middle of the vtable and SteamClient021
// removed 4 methods, so those need separate wrapper classes (see Hookey_SteamUser_Class21/23,
// Hookey_SteamClient_Class20/23).
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

void* (*real_dlsym)(void *handle, const char *name);

mINI::INIStructure ini;

//TODO: move this somewhere else (immediately on lib load?)
//TODO: hook dlvsym as well
void ensure_realdlsym() {
    if (real_dlsym == NULL) {
        *(void **)(&real_dlsym) = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
    }
}

class Hookey_SteamApps_Class : public ISteamApps {
public:
    bool BIsSubscribed() { return true; }
    bool BIsLowViolence() { return false; }
    bool BIsCybercafe() { return false; }
    bool BIsVACBanned() { return false; }
    int GetDLCCount() {
        spdlog::info("ISteamApps->GetDLCCount called PID {0}", getpid());
        auto count = dlcs.size();
        auto content = dlcs;
        return dlcs.size();
    }
    bool BIsDlcInstalled(AppId_t appID) {
        spdlog::info("ISteamApps->BIsDlcInstalled called");
        if (is_dlc_unlocked(appID)) {
            spdlog::info("BIsDlcInstalled unlocked {}", appID);
            return true;
        }
        return false;
    }
    bool BGetDLCDataByIndex(int iDLC, AppId_t* pAppID, bool* pbAvailable, char* pchName, int cchNameBufferSize) {
        spdlog::info("ISteamApps->BGetDLCDataByIndex called");
        if ((size_t)iDLC >= dlcs.size()) {
            return false;
        }

        *pAppID = std::get<0>(dlcs[iDLC]);
        *pbAvailable = true;

        const char* name = std::get<1>(dlcs[iDLC]).c_str();
        size_t slen = std::min((size_t)cchNameBufferSize - 1, std::get<1>(dlcs[iDLC]).size());
        memcpy((void*)pchName, (void*)name, slen);
        *(pchName + slen) = 0x0;

        return true;
    }

    const char* GetCurrentGameLanguage() { return real_steamApps->GetCurrentGameLanguage(); }
    const char* GetAvailableGameLanguages() { return real_steamApps->GetAvailableGameLanguages(); }
    CSteamID GetAppOwner() { return real_steamApps->GetAppOwner(); }
    int GetAppBuildId() { return real_steamApps->GetAppBuildId(); }
    void RequestAllProofOfPurchaseKeys() {
        spdlog::info("ISteamApps->RequestAllProofOfPurchaseKeys called");
        return real_steamApps->RequestAllProofOfPurchaseKeys();
    }
    bool BIsSubscribedFromFamilySharing() { return real_steamApps->BIsSubscribedFromFamilySharing(); }
    bool BIsSubscribedFromFreeWeekend() { return real_steamApps->BIsSubscribedFromFreeWeekend(); }
    bool BIsSubscribedApp(AppId_t appID) { 
        spdlog::info("ISteamApps->BIsSubscribedApp called");
        if (ini["methods"]["disable_steamapps_issubscribedapp"] == "true") {
            spdlog::info("BIsSubscribedApp function override disabled");
            return real_steamApps->BIsSubscribedApp(appID); 
        } else {
            spdlog::info("BIsSubscribedApp creamified called");
            if (is_dlc_unlocked(appID)) {
                spdlog::info("BIsSubscribedApp unlocked {}", appID);
                return true;
            } else {
                if (ini["config"]["issubscribedapp_on_false_use_real"] == "true") {
                    return real_steamApps->BIsSubscribedApp(appID); 
                }
                return false;
            }
        }
    }
    bool BIsAppInstalled(AppId_t appID) {
         spdlog::info("ISteamApps->BIsAppInstalled {0} called", appID);
        return real_steamApps->BIsAppInstalled(appID);
    }
    uint32 GetEarliestPurchaseUnixTime(AppId_t appID) { return real_steamApps->GetEarliestPurchaseUnixTime(appID); }
    void InstallDLC(AppId_t appID) { real_steamApps->InstallDLC(appID); }
    void UninstallDLC(AppId_t appID) { real_steamApps->UninstallDLC(appID); }
    void RequestAppProofOfPurchaseKey(AppId_t appID) { real_steamApps->RequestAppProofOfPurchaseKey(appID); }
    bool GetCurrentBetaName(char* pchName, int cchNameBufferSize) { return real_steamApps->GetCurrentBetaName(pchName, cchNameBufferSize); }
    bool MarkContentCorrupt(bool bMissingFilesOnly) { return real_steamApps->MarkContentCorrupt(bMissingFilesOnly); }

    uint32 GetInstalledDepots(AppId_t appID, DepotId_t* pvecDepots, uint32 cMaxDepots) { return real_steamApps->GetInstalledDepots(appID, pvecDepots, cMaxDepots); }
    uint32 GetAppInstallDir(AppId_t appID, char* pchFolder, uint32 cchFolderBufferSize) { return real_steamApps->GetAppInstallDir(appID, pchFolder, cchFolderBufferSize); }
    const char* GetLaunchQueryParam(const char* pchKey) { return real_steamApps->GetLaunchQueryParam(pchKey); }
    bool GetDlcDownloadProgress(AppId_t nAppID, uint64* punBytesDownloaded, uint64* punBytesTotal) { return real_steamApps->GetDlcDownloadProgress(nAppID, punBytesDownloaded, punBytesTotal); }
    SteamAPICall_t GetFileDetails(const char* pszFileName) { return real_steamApps->GetFileDetails(pszFileName); }
    int GetLaunchCommandLine(char* pszCommandLine, int cubCommandLine) { return real_steamApps->GetLaunchCommandLine(pszCommandLine, cubCommandLine); }
	virtual bool BIsTimedTrial( uint32* punSecondsAllowed, uint32* punSecondsPlayed ) { return real_steamApps->BIsTimedTrial(punSecondsAllowed, punSecondsPlayed); } 
    // ---- ISteamApps v009 methods (STEAMAPPS_INTERFACE_VERSION009) ----
    bool SetDlcContext( AppId_t nAppID ) { return real_steamApps->SetDlcContext(nAppID); }
    int GetNumBetas( int *pnAvailable, int *pnPrivate ) { return real_steamApps->GetNumBetas(pnAvailable, pnPrivate); }
    bool GetBetaInfo( int iBetaIndex, uint32 *punFlags, uint32 *punBuildID, char *pchBetaName, int cchBetaName, char *pchDescription, int cchDescription, uint32 *punLastUpdated ) {
        return real_steamApps->GetBetaInfo(iBetaIndex, punFlags, punBuildID, pchBetaName, cchBetaName, pchDescription, cchDescription, punLastUpdated);
    }
    bool SetActiveBeta( const char *pchBetaName ) { return real_steamApps->SetActiveBeta(pchBetaName); }
    void SetGamePerformanceSetting( EGamePerformanceSetting setting ) { return real_steamApps->SetGamePerformanceSetting(setting); }
    void SetGameRenderResolution( uint32 unWidth, uint32 unHeight ) { return real_steamApps->SetGameRenderResolution(unWidth, unHeight); }
    ISteamApps* real_steamApps;
};

class Hookey_SteamUser_Class21 : public ISteamUser021 {
public:
	HSteamUser GetHSteamUser() {
        return real_steamUser->GetHSteamUser();
    };
	bool BLoggedOn() {
        return real_steamUser->BLoggedOn();
    };
	CSteamID GetSteamID() {
        return real_steamUser->GetSteamID();
    };
	int InitiateGameConnection_DEPRECATED( void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure ) {
        return real_steamUser->InitiateGameConnection_DEPRECATED(pAuthBlob, cbMaxAuthBlob, steamIDGameServer, unIPServer, usPortServer, bSecure);
    };
	void TerminateGameConnection_DEPRECATED( uint32 unIPServer, uint16 usPortServer ) {
        return real_steamUser->TerminateGameConnection_DEPRECATED(unIPServer, usPortServer);
    };
    int InitiateGameConnection( void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure ) {
        return real_steamUser->InitiateGameConnection_DEPRECATED(pAuthBlob, cbMaxAuthBlob, steamIDGameServer, unIPServer, usPortServer, bSecure);
    };
	void TerminateGameConnection( uint32 unIPServer, uint16 usPortServer ) {
        return real_steamUser->TerminateGameConnection_DEPRECATED(unIPServer, usPortServer);
    };
	void TrackAppUsageEvent( CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo = "" ) {
        return real_steamUser->TrackAppUsageEvent(gameID, eAppUsageEvent, pchExtraInfo);
    };
	bool GetUserDataFolder( char *pchBuffer, int cubBuffer ) {
        return real_steamUser->GetUserDataFolder(pchBuffer, cubBuffer);
    };
	void StartVoiceRecording( ) {
        return real_steamUser->StartVoiceRecording();
    };
	void StopVoiceRecording( ) {
        return real_steamUser->StopVoiceRecording();
    };
	EVoiceResult GetAvailableVoice( uint32 *pcbCompressed, uint32 *pcbUncompressed_Deprecated, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated) {
        return real_steamUser->GetAvailableVoice(pcbCompressed, pcbUncompressed_Deprecated, nUncompressedVoiceDesiredSampleRate_Deprecated);
    };
	EVoiceResult GetVoice( bool bWantCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, bool bWantUncompressed_Deprecated, void *pUncompressedDestBuffer_Deprecated, uint32 cbUncompressedDestBufferSize_Deprecated, uint32 *nUncompressBytesWritten_Deprecated, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated ) {
        return real_steamUser->GetVoice(bWantCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, bWantUncompressed_Deprecated, pUncompressedDestBuffer_Deprecated, cbUncompressedDestBufferSize_Deprecated, nUncompressBytesWritten_Deprecated, nUncompressedVoiceDesiredSampleRate_Deprecated);
    };
	EVoiceResult DecompressVoice( const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, uint32 nDesiredSampleRate ) {
        return real_steamUser->DecompressVoice(pCompressed, cbCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, nDesiredSampleRate);
    };
	uint32 GetVoiceOptimalSampleRate() {
        return real_steamUser->GetVoiceOptimalSampleRate();
    };
	HAuthTicket GetAuthSessionTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket ) {
        return real_steamUser->GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket);
    };
	EBeginAuthSessionResult BeginAuthSession( const void *pAuthTicket, int cbAuthTicket, CSteamID steamID ) {
        return real_steamUser->BeginAuthSession(pAuthTicket, cbAuthTicket, steamID);
    };
	void EndAuthSession( CSteamID steamID ) {
        return real_steamUser->EndAuthSession(steamID);
    };
	void CancelAuthTicket( HAuthTicket hAuthTicket ) {
        return real_steamUser->CancelAuthTicket(hAuthTicket);
    };
	EUserHasLicenseForAppResult UserHasLicenseForApp( CSteamID steamID, AppId_t appID ) {
        spdlog::info("ISteamUser->UserHasLicenseForApp {} called", appID);
        if (is_dlc_unlocked(appID)) {
            spdlog::info("ISteamUser_UserHasLicenseForApp result: owned");
            return (EUserHasLicenseForAppResult)0;
        } else {
            spdlog::info("ISteamUser_UserHasLicenseForApp result: not owned");
            return (EUserHasLicenseForAppResult)2;
        }
    };
	bool BIsBehindNAT() {
        return real_steamUser->BIsBehindNAT();
    };
	void AdvertiseGame( CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer ) {
        return real_steamUser->AdvertiseGame(steamIDGameServer, unIPServer, usPortServer);
    };
	SteamAPICall_t RequestEncryptedAppTicket( void *pDataToInclude, int cbDataToInclude ) {
        return real_steamUser->RequestEncryptedAppTicket(pDataToInclude, cbDataToInclude);
    };
	bool GetEncryptedAppTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket ) {
        return real_steamUser->GetEncryptedAppTicket(pTicket, cbMaxTicket, pcbTicket);
    };
	int GetGameBadgeLevel( int nSeries, bool bFoil ) {
        return real_steamUser->GetGameBadgeLevel(nSeries, bFoil);
    };
	int GetPlayerSteamLevel() {
        return real_steamUser->GetPlayerSteamLevel();
    };
	SteamAPICall_t RequestStoreAuthURL( const char *pchRedirectURL ) {
        return real_steamUser->RequestStoreAuthURL(pchRedirectURL);
    };
	bool BIsPhoneVerified() {
        return real_steamUser->BIsPhoneVerified();
    };
	bool BIsTwoFactorEnabled() {
        return real_steamUser->BIsTwoFactorEnabled();
    };
	bool BIsPhoneIdentifying() {
        return real_steamUser->BIsPhoneIdentifying();
    };
	bool BIsPhoneRequiringVerification() {
        return real_steamUser->BIsPhoneRequiringVerification();
    };
	SteamAPICall_t GetMarketEligibility() {
        return real_steamUser->GetMarketEligibility();
    };
	virtual SteamAPICall_t GetDurationControl() {
        return real_steamUser->GetDurationControl();
    }
	virtual bool BSetDurationControlOnlineState( EDurationControlOnlineState eNewState ) {
        return real_steamUser->BSetDurationControlOnlineState(eNewState);
    };
    ISteamUser021* real_steamUser;
};
static std::shared_ptr<Hookey_SteamApps_Class> steamapps_instance;

ISteamApps* Hookey_SteamApps(ISteamApps* real_steamApps) {
    if (steamapps_instance != NULL) {
        ISteamApps* ptraccess = steamapps_instance.get();
        auto debg = ptraccess->GetDLCCount();
        return steamapps_instance.get();
    } else {
        Hookey_SteamApps_Class nhooky;
        nhooky.real_steamApps = real_steamApps;
        steamapps_instance = std::make_shared<Hookey_SteamApps_Class>(nhooky);
        return Hookey_SteamApps(real_steamApps);
    }
}

// ISteamUser v022/v023 wrapper: SteamUser022 added GetAuthTicketForWebApi in the middle of the
// vtable, shifting every method after it. Games requesting SteamUser022/023 MUST get this class;
// games requesting SteamUser020/021 get Hookey_SteamUser_Class21 (vtable-compatible).
class Hookey_SteamUser_Class23 : public ISteamUser {
public:
	HSteamUser GetHSteamUser() { return real_steamUser->GetHSteamUser(); };
	bool BLoggedOn() { return real_steamUser->BLoggedOn(); };
	CSteamID GetSteamID() { return real_steamUser->GetSteamID(); };
	int InitiateGameConnection_DEPRECATED( void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure ) {
        return real_steamUser->InitiateGameConnection_DEPRECATED(pAuthBlob, cbMaxAuthBlob, steamIDGameServer, unIPServer, usPortServer, bSecure);
    };
	void TerminateGameConnection_DEPRECATED( uint32 unIPServer, uint16 usPortServer ) {
        return real_steamUser->TerminateGameConnection_DEPRECATED(unIPServer, usPortServer);
    };
	void TrackAppUsageEvent( CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo = "" ) {
        return real_steamUser->TrackAppUsageEvent(gameID, eAppUsageEvent, pchExtraInfo);
    };
	bool GetUserDataFolder( char *pchBuffer, int cubBuffer ) {
        return real_steamUser->GetUserDataFolder(pchBuffer, cubBuffer);
    };
	void StartVoiceRecording( ) { return real_steamUser->StartVoiceRecording(); };
	void StopVoiceRecording( ) { return real_steamUser->StopVoiceRecording(); };
	EVoiceResult GetAvailableVoice( uint32 *pcbCompressed, uint32 *pcbUncompressed_Deprecated, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated) {
        return real_steamUser->GetAvailableVoice(pcbCompressed, pcbUncompressed_Deprecated, nUncompressedVoiceDesiredSampleRate_Deprecated);
    };
	EVoiceResult GetVoice( bool bWantCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, bool bWantUncompressed_Deprecated, void *pUncompressedDestBuffer_Deprecated, uint32 cbUncompressedDestBufferSize_Deprecated, uint32 *nUncompressBytesWritten_Deprecated, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated ) {
        return real_steamUser->GetVoice(bWantCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, bWantUncompressed_Deprecated, pUncompressedDestBuffer_Deprecated, cbUncompressedDestBufferSize_Deprecated, nUncompressBytesWritten_Deprecated, nUncompressedVoiceDesiredSampleRate_Deprecated);
    };
	EVoiceResult DecompressVoice( const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, uint32 nDesiredSampleRate ) {
        return real_steamUser->DecompressVoice(pCompressed, cbCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, nDesiredSampleRate);
    };
	uint32 GetVoiceOptimalSampleRate() { return real_steamUser->GetVoiceOptimalSampleRate(); };
	HAuthTicket GetAuthSessionTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket, const SteamNetworkingIdentity *pSteamNetworkingIdentity ) {
        return real_steamUser->GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket, pSteamNetworkingIdentity);
    };
	HAuthTicket GetAuthTicketForWebApi( const char *pchIdentity ) {
        return real_steamUser->GetAuthTicketForWebApi(pchIdentity);
    };
	EBeginAuthSessionResult BeginAuthSession( const void *pAuthTicket, int cbAuthTicket, CSteamID steamID ) {
        return real_steamUser->BeginAuthSession(pAuthTicket, cbAuthTicket, steamID);
    };
	void EndAuthSession( CSteamID steamID ) { return real_steamUser->EndAuthSession(steamID); };
	void CancelAuthTicket( HAuthTicket hAuthTicket ) { return real_steamUser->CancelAuthTicket(hAuthTicket); };
	EUserHasLicenseForAppResult UserHasLicenseForApp( CSteamID steamID, AppId_t appID ) {
        spdlog::info("ISteamUser->UserHasLicenseForApp {} called", appID);
        if (is_dlc_unlocked(appID)) {
            spdlog::info("ISteamUser_UserHasLicenseForApp result: owned");
            return (EUserHasLicenseForAppResult)0;
        } else {
            spdlog::info("ISteamUser_UserHasLicenseForApp result: not owned");
            return (EUserHasLicenseForAppResult)2;
        }
    };
	bool BIsBehindNAT() { return real_steamUser->BIsBehindNAT(); };
	void AdvertiseGame( CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer ) {
        return real_steamUser->AdvertiseGame(steamIDGameServer, unIPServer, usPortServer);
    };
	SteamAPICall_t RequestEncryptedAppTicket( void *pDataToInclude, int cbDataToInclude ) {
        return real_steamUser->RequestEncryptedAppTicket(pDataToInclude, cbDataToInclude);
    };
	bool GetEncryptedAppTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket ) {
        return real_steamUser->GetEncryptedAppTicket(pTicket, cbMaxTicket, pcbTicket);
    };
	int GetGameBadgeLevel( int nSeries, bool bFoil ) { return real_steamUser->GetGameBadgeLevel(nSeries, bFoil); };
	int GetPlayerSteamLevel() { return real_steamUser->GetPlayerSteamLevel(); };
	SteamAPICall_t RequestStoreAuthURL( const char *pchRedirectURL ) {
        return real_steamUser->RequestStoreAuthURL(pchRedirectURL);
    };
	bool BIsPhoneVerified() { return real_steamUser->BIsPhoneVerified(); };
	bool BIsTwoFactorEnabled() { return real_steamUser->BIsTwoFactorEnabled(); };
	bool BIsPhoneIdentifying() { return real_steamUser->BIsPhoneIdentifying(); };
	bool BIsPhoneRequiringVerification() { return real_steamUser->BIsPhoneRequiringVerification(); };
	SteamAPICall_t GetMarketEligibility() { return real_steamUser->GetMarketEligibility(); };
	virtual SteamAPICall_t GetDurationControl() { return real_steamUser->GetDurationControl(); }
	virtual bool BSetDurationControlOnlineState( EDurationControlOnlineState eNewState ) {
        return real_steamUser->BSetDurationControlOnlineState(eNewState);
    };
    ISteamUser* real_steamUser;
};
static std::shared_ptr<Hookey_SteamUser_Class21> steamuser21_instance;
static std::shared_ptr<Hookey_SteamUser_Class23> steamuser23_instance;

ISteamUser021* Hookey_SteamUser21(ISteamUser021* real_steamUser) {
    if (steamuser21_instance != NULL) {
        return steamuser21_instance.get();
    } else {
        Hookey_SteamUser_Class21 nhooky;
        nhooky.real_steamUser = real_steamUser;
        steamuser21_instance = std::make_shared<Hookey_SteamUser_Class21>(nhooky);
        return Hookey_SteamUser21(real_steamUser);
    }
}

ISteamUser* Hookey_SteamUser23(ISteamUser* real_steamUser) {
    if (steamuser23_instance != NULL) {
        return steamuser23_instance.get();
    } else {
        Hookey_SteamUser_Class23 nhooky;
        nhooky.real_steamUser = real_steamUser;
        steamuser23_instance = std::make_shared<Hookey_SteamUser_Class23>(nhooky);
        return Hookey_SteamUser23(real_steamUser);
    }
}

class Hookey_SteamClient_Class20 : public ISteamClient020 {
public:
	HSteamPipe CreateSteamPipe() {
        return real_steamClient->CreateSteamPipe();
    }
	bool BReleaseSteamPipe( HSteamPipe hSteamPipe ) {
        return real_steamClient->BReleaseSteamPipe(hSteamPipe);
    }
	HSteamUser ConnectToGlobalUser( HSteamPipe hSteamPipe ) {
        return real_steamClient->ConnectToGlobalUser(hSteamPipe);
    }
	HSteamUser CreateLocalUser( HSteamPipe *phSteamPipe, EAccountType eAccountType ) {
        return real_steamClient->CreateLocalUser(phSteamPipe, eAccountType);
    }
	void ReleaseUser( HSteamPipe hSteamPipe, HSteamUser hUser ) {
        return real_steamClient->ReleaseUser(hSteamPipe, hUser);
    }
	ISteamUser *GetISteamUser( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamUser {0} called (hooked)", pchVersion);
        ISteamUser* real_user = real_steamClient->GetISteamUser(hSteamUser, hSteamPipe, pchVersion);
        if (strstr(pchVersion, STEAMUSER_INTERFACE_VERSION_022) == pchVersion ||
            strstr(pchVersion, STEAMUSER_INTERFACE_VERSION_023) == pchVersion) {
            return Hookey_SteamUser23(real_user);
        }
        return (ISteamUser*)Hookey_SteamUser21((ISteamUser021*)real_user);
    }
    ISteamGameServer *GetISteamGameServer( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamGameServer(hSteamUser, hSteamPipe, pchVersion);
    }
	void SetLocalIPBinding( const SteamIPAddress_t &unIP, uint16 usPort ) {
        return real_steamClient->SetLocalIPBinding(unIP, usPort);
    }
	ISteamFriends *GetISteamFriends( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion )  {
        return real_steamClient->GetISteamFriends(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamUtils *GetISteamUtils( HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamUtils called");
        return real_steamClient->GetISteamUtils(hSteamPipe, pchVersion);
    }
	ISteamMatchmaking *GetISteamMatchmaking( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamMatchmaking(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamMatchmakingServers *GetISteamMatchmakingServers( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamMatchmakingServers(hSteamUser, hSteamPipe, pchVersion);
    }
	void *GetISteamGenericInterface( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamGenericInterface {0} called (you're in for a wild ride)", pchVersion);
        return real_steamClient->GetISteamGenericInterface(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamUserStats *GetISteamUserStats( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamUserStats(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamGameServerStats *GetISteamGameServerStats( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamGameServerStats(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamApps *GetISteamApps( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamApps {0} called (hooked)", pchVersion);
         return Hookey_SteamApps(real_steamClient->GetISteamApps(hSteamUser, hSteamPipe, pchVersion));
        //return real_steamClient->GetISteamApps(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamNetworking *GetISteamNetworking( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamNetworking(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamRemoteStorage *GetISteamRemoteStorage( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamRemoteStorage(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamScreenshots *GetISteamScreenshots( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamScreenshots(hSteamuser, hSteamPipe, pchVersion);
    }
	void *GetISteamGameSearch( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamGameSearch(hSteamuser, hSteamPipe, pchVersion);
    }
	uint32 GetIPCCallCount() {
        return real_steamClient->GetIPCCallCount();
    }
	void SetWarningMessageHook( SteamAPIWarningMessageHook_t pFunction ) {
        return real_steamClient->SetWarningMessageHook(pFunction);
    }
	bool BShutdownIfAllPipesClosed() {
        return real_steamClient->BShutdownIfAllPipesClosed();
    }
	ISteamHTTP *GetISteamHTTP( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamHTTP(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamController *GetISteamController( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamController(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamUGC *GetISteamUGC( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamUGC(hSteamUser, hSteamPipe, pchVersion);
    }
	void *GetISteamAppList( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamAppList called");
        return real_steamClient->GetISteamAppList(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamMusic *GetISteamMusic( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamMusic(hSteamuser, hSteamPipe, pchVersion);
    }
	void *GetISteamMusicRemote(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
        return real_steamClient->GetISteamMusicRemote(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamHTMLSurface *GetISteamHTMLSurface(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
        return real_steamClient->GetISteamHTMLSurface(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamInventory *GetISteamInventory( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamInventory(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamVideo *GetISteamVideo( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamVideo(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamParentalSettings *GetISteamParentalSettings( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamParentalSettings(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamInput *GetISteamInput( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamInput(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamParties *GetISteamParties( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamParties(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamRemotePlay *GetISteamRemotePlay( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamRemotePlay(hSteamUser, hSteamPipe, pchVersion);
    }
    void RunFrame() {
         return real_steamClient->RunFrame();
    }
    void *DEPRECATED_GetISteamUnifiedMessages( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
         return real_steamClient->DEPRECATED_GetISteamUnifiedMessages(hSteamuser, hSteamPipe, pchVersion);
    }
    void DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess( void (*)()  ) {
         return real_steamClient->DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess(NULL);
    }
	void DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess( void (*)() ) {
         return real_steamClient->DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess(NULL);
    }
	void Set_SteamAPI_CCheckCallbackRegisteredInProcess( SteamAPI_CheckCallbackRegistered_t func ) {
         return real_steamClient->Set_SteamAPI_CCheckCallbackRegisteredInProcess(func);
    }
    void DestroyAllInterfaces() {
         return real_steamClient->DestroyAllInterfaces();
    }
    ISteamClient020* real_steamClient;
};

// ISteamClient v021/v022/v023 wrapper: SteamClient021 removed GetISteamGameSearch,
// DEPRECATED_GetISteamUnifiedMessages, GetISteamAppList and GetISteamMusicRemote from the
// middle of the vtable, so this class is NOT vtable-compatible with SteamClient020 and older.
// Games requesting SteamClient017-020 get Hookey_SteamClient_Class20 instead.
class Hookey_SteamClient_Class23 : public ISteamClient {
public:
	HSteamPipe CreateSteamPipe() { return real_steamClient->CreateSteamPipe(); }
	bool BReleaseSteamPipe( HSteamPipe hSteamPipe ) { return real_steamClient->BReleaseSteamPipe(hSteamPipe); }
	HSteamUser ConnectToGlobalUser( HSteamPipe hSteamPipe ) { return real_steamClient->ConnectToGlobalUser(hSteamPipe); }
	HSteamUser CreateLocalUser( HSteamPipe *phSteamPipe, EAccountType eAccountType ) { return real_steamClient->CreateLocalUser(phSteamPipe, eAccountType); }
	void ReleaseUser( HSteamPipe hSteamPipe, HSteamUser hUser ) { return real_steamClient->ReleaseUser(hSteamPipe, hUser); }
	ISteamUser *GetISteamUser( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamUser {0} called (hooked)", pchVersion);
        return Hookey_SteamUser23(real_steamClient->GetISteamUser(hSteamUser, hSteamPipe, pchVersion));
    }
	ISteamGameServer *GetISteamGameServer( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamGameServer(hSteamUser, hSteamPipe, pchVersion);
    }
	void SetLocalIPBinding( const SteamIPAddress_t &unIP, uint16 usPort ) { return real_steamClient->SetLocalIPBinding(unIP, usPort); }
	ISteamFriends *GetISteamFriends( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamFriends(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamUtils *GetISteamUtils( HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamUtils called");
        return real_steamClient->GetISteamUtils(hSteamPipe, pchVersion);
    }
	ISteamMatchmaking *GetISteamMatchmaking( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamMatchmaking(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamMatchmakingServers *GetISteamMatchmakingServers( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamMatchmakingServers(hSteamUser, hSteamPipe, pchVersion);
    }
	void *GetISteamGenericInterface( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamGenericInterface {0} called (you're in for a wild ride)", pchVersion);
        return real_steamClient->GetISteamGenericInterface(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamUserStats *GetISteamUserStats( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamUserStats(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamGameServerStats *GetISteamGameServerStats( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamGameServerStats(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamApps *GetISteamApps( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        spdlog::info("ISteamClient->GetISteamApps {0} called (hooked)", pchVersion);
        return Hookey_SteamApps(real_steamClient->GetISteamApps(hSteamUser, hSteamPipe, pchVersion));
    }
	ISteamNetworking *GetISteamNetworking( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamNetworking(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamRemoteStorage *GetISteamRemoteStorage( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamRemoteStorage(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamScreenshots *GetISteamScreenshots( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamScreenshots(hSteamuser, hSteamPipe, pchVersion);
    }
	void RunFrame() { /* real ISteamClient::RunFrame is protected in SteamClient021+; safe no-op */ }
	void SetWarningMessageHook( SteamAPIWarningMessageHook_t pFunction ) { return real_steamClient->SetWarningMessageHook(pFunction); }
	bool BShutdownIfAllPipesClosed() { return real_steamClient->BShutdownIfAllPipesClosed(); }
	uint32 GetIPCCallCount() { return real_steamClient->GetIPCCallCount(); }
	ISteamHTTP *GetISteamHTTP( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamHTTP(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamController *GetISteamController( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamController(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamUGC *GetISteamUGC( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamUGC(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamMusic *GetISteamMusic( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamMusic(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamHTMLSurface *GetISteamHTMLSurface(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
        return real_steamClient->GetISteamHTMLSurface(hSteamuser, hSteamPipe, pchVersion);
    }
    // SteamClient021+ moved these to a protected STEAM_PRIVATE_API section; we must override
    // the pure virtuals but cannot delegate. They are deprecated/internal, so no-op is safe.
    void DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess( void (*)() ) { }
    void DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess( void (*)() ) { }
    void Set_SteamAPI_CCheckCallbackRegisteredInProcess( SteamAPI_CheckCallbackRegistered_t func ) { }
	ISteamInventory *GetISteamInventory( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamInventory(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamVideo *GetISteamVideo( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamVideo(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamParentalSettings *GetISteamParentalSettings( HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamParentalSettings(hSteamuser, hSteamPipe, pchVersion);
    }
	ISteamInput *GetISteamInput( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamInput(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamParties *GetISteamParties( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamParties(hSteamUser, hSteamPipe, pchVersion);
    }
	ISteamRemotePlay *GetISteamRemotePlay( HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion ) {
        return real_steamClient->GetISteamRemotePlay(hSteamUser, hSteamPipe, pchVersion);
    }
    void DestroyAllInterfaces() { /* real ISteamClient::DestroyAllInterfaces is protected in SteamClient021+; safe no-op (SteamAPI_Shutdown handles cleanup) */ }
    ISteamClient* real_steamClient;
};

static std::shared_ptr<Hookey_SteamClient_Class20> steamclient20_instance;
static std::shared_ptr<Hookey_SteamClient_Class23> steamclient23_instance;

ISteamClient020* Hookey_SteamClient20(ISteamClient020* real_steamClient) {
    if (steamclient20_instance != NULL) {
        return steamclient20_instance.get();
    } else {
        Hookey_SteamClient_Class20 nhooky;
        nhooky.real_steamClient = real_steamClient;
        steamclient20_instance = std::make_shared<Hookey_SteamClient_Class20>(nhooky);
        return Hookey_SteamClient20(real_steamClient);
    }
}

ISteamClient* Hookey_SteamClient23(ISteamClient* real_steamClient) {
    if (steamclient23_instance != NULL) {
        return steamclient23_instance.get();
    } else {
        Hookey_SteamClient_Class23 nhooky;
        nhooky.real_steamClient = real_steamClient;
        steamclient23_instance = std::make_shared<Hookey_SteamClient_Class23>(nhooky);
        return Hookey_SteamClient23(real_steamClient);
    }
}

#define STEAMCLIENT_INTERFACE_VERSION_023 "SteamClient023"

extern "C" void* CreateInterface(const char *pName, int *pReturnCode) {
    ensure_realdlsym();
    void *S_CALLTYPE (*real)(const char *pName, int *pReturnCode);
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "CreateInterface");
    spdlog::info("CreateInterface called pszVersion: {}", pName);
    return real(pName, pReturnCode);
}

extern "C" void Steam_LogOn(HSteamUser hUser, HSteamPipe hSteamPipe, uint64 ulSteamID) {
    ensure_realdlsym();
    void* S_CALLTYPE (*real)(HSteamUser hUser, HSteamPipe hSteamPipe, uint64 ulSteamID);
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamAPI_LogOn");
    spdlog::info("SteamAPI_LogOn called {0} {1} {2}",hUser, hSteamPipe, ulSteamID );
    real(hUser, hSteamPipe, ulSteamID);
}

// ---- Flat API hooks (steam_api_flat.h) ----
// Some games (e.g. Unity titles) call SteamAPI_ISteamApps_* and SteamAPI_ISteamUser_*
// directly instead of going through the C++ interface accessors. Since creamlinux is
// LD_PRELOADed before libsteam_api.so, these symbols interpose the real ones.
// NOTE: the real flat API passes `self` as the first argument; it must stay in the
// signature or 32-bit builds will read the wrong stack slot.

extern "C" int SteamAPI_ISteamApps_GetDLCCount(ISteamApps* self) {
    spdlog::info("SteamAPI_ISteamApps_GetDLCCount called (flat API)");
    return (int)dlcs.size();
}

extern "C" bool SteamAPI_ISteamApps_BIsDlcInstalled(ISteamApps* self, AppId_t appID) {
    spdlog::info("SteamAPI_ISteamApps_BIsDlcInstalled called (flat API) appID {}", appID);
    if (is_dlc_unlocked(appID)) {
        spdlog::info("SteamAPI_ISteamApps_BIsDlcInstalled unlocked {}", appID);
        return true;
    }
    return false;
}

extern "C" bool SteamAPI_ISteamApps_BIsSubscribedApp(ISteamApps* self, AppId_t appID) {
    spdlog::info("SteamAPI_ISteamApps_BIsSubscribedApp called (flat API) appID {}", appID);
    if (ini["methods"]["disable_steamapps_issubscribedapp"] == "true") {
        return self->BIsSubscribedApp(appID);
    }
    if (is_dlc_unlocked(appID)) {
        spdlog::info("SteamAPI_ISteamApps_BIsSubscribedApp unlocked {}", appID);
        return true;
    }
    return false;
}

extern "C" bool SteamAPI_ISteamApps_BGetDLCDataByIndex(ISteamApps* self, int iDLC, AppId_t* pAppID, bool* pbAvailable, char* pchName, int cchNameBufferSize) {
        spdlog::info("SteamAPI_ISteamApps_BGetDLCDataByIndex called (flat API)");
        if ((size_t)iDLC >= dlcs.size()) {
            return false;
        }

        *pAppID = std::get<0>(dlcs[iDLC]);
        *pbAvailable = true;

        const char* name = std::get<1>(dlcs[iDLC]).c_str();
        size_t slen = std::min((size_t)cchNameBufferSize - 1, std::get<1>(dlcs[iDLC]).size());
        memcpy((void*)pchName, (void*)name, slen);
        *(pchName + slen) = 0x0;

        return true;
}

extern "C" EUserHasLicenseForAppResult SteamAPI_ISteamUser_UserHasLicenseForApp(ISteamUser* self, uint64 steamID, AppId_t appID) {
    spdlog::info("SteamAPI_ISteamUser_UserHasLicenseForApp called (flat API) appID {}", appID);
    if (is_dlc_unlocked(appID)) {
        spdlog::info("SteamAPI_ISteamUser_UserHasLicenseForApp result: owned");
        return (EUserHasLicenseForAppResult)0;
    }
    spdlog::info("SteamAPI_ISteamUser_UserHasLicenseForApp result: not owned");
    return (EUserHasLicenseForAppResult)2;
}

extern "C" void* S_CALLTYPE SteamInternal_FindOrCreateUserInterface(HSteamUser hSteamUser, const char *pszVersion) {
    ensure_realdlsym();
    void* S_CALLTYPE (*real)(HSteamUser hSteamUser, const char *pszVersion);
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamInternal_FindOrCreateUserInterface");
    spdlog::info("SteamInternal_FindOrCreateUserInterface called pszVersion: {}", pszVersion);
    // Steamapps Interface call is hooked here (v008 and v009 share a vtable prefix, one wrapper is enough)
    if (strstr(pszVersion, STEAMAPPS_INTERFACE_VERSION_N008) == pszVersion ||
        strstr(pszVersion, STEAMAPPS_INTERFACE_VERSION_N009) == pszVersion) {
        ISteamApps* val = (ISteamApps*)real(hSteamUser, pszVersion);
        spdlog::info("SteamInternal_FindOrCreateUserInterface hooked ISteamApps {}", pszVersion);
        return Hookey_SteamApps(val);
    }

    // SteamUser v022/v023 (GetAuthTicketForWebApi shifted the vtable)
    if (strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_022) == pszVersion ||
        strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_023) == pszVersion) {
        ISteamUser* val = (ISteamUser*)real(hSteamUser, pszVersion);
        spdlog::info("SteamInternal_FindOrCreateUserInterface ISteamUser hook {}", pszVersion);
        return Hookey_SteamUser23(val);
    }
    // SteamUser v020/v021
    if (strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_020) == pszVersion ||
        strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_021) == pszVersion) {
        ISteamUser021* val = (ISteamUser021*)real(hSteamUser, pszVersion);
        spdlog::info("SteamInternal_FindOrCreateUserInterface ISteamUser(legacy) hook {}", pszVersion);
        return Hookey_SteamUser21(val);
    }
    auto val = real(hSteamUser, pszVersion);
    return val;
}

extern "C" void* S_CALLTYPE SteamInternal_CreateInterface(const char *pszVersion) {
    ensure_realdlsym();
     void* S_CALLTYPE (*real)(const char *pszVersion);
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamInternal_CreateInterface");
    spdlog::info("SteamInternal_CreateInterface called pszVersion: {}", pszVersion);

    // Steamapps Interface call is hooked here (v008 and v009 share a vtable prefix, one wrapper is enough)
    if (strstr(pszVersion, STEAMAPPS_INTERFACE_VERSION_N008) == pszVersion ||
        strstr(pszVersion, STEAMAPPS_INTERFACE_VERSION_N009) == pszVersion) {
        ISteamApps* val = (ISteamApps*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface hooked ISteamApps {}", pszVersion);
        return Hookey_SteamApps(val);
    }

    // SteamUser v022/v023 (GetAuthTicketForWebApi shifted the vtable)
    if (strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_022) == pszVersion ||
        strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_023) == pszVersion) {
        ISteamUser* val = (ISteamUser*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface ISteamUser hook {}", pszVersion);
        return Hookey_SteamUser23(val);
    }

    // SteamUser v020/v021
    if (strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_020) == pszVersion ||
        strstr(pszVersion, STEAMUSER_INTERFACE_VERSION_021) == pszVersion) {
        ISteamUser021* val = (ISteamUser021*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface ISteamUser(legacy) hook {}", pszVersion);
        return Hookey_SteamUser21(val);
    }

    // SteamClient v021/v022/v023
    if (strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_021) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_022) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_023) == pszVersion) {
        ISteamClient* val = (ISteamClient*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface ISteamClient hook {}", pszVersion);
        return Hookey_SteamClient23(val);
    }

    // SteamClient v017-v020 (legacy, vtable compatible)
    if (strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_017) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_018) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_019) == pszVersion ||
        strstr(pszVersion, STEAMCLIENT_INTERFACE_VERSION_020) == pszVersion) {
        ISteamClient020* val = (ISteamClient020*)real(pszVersion);
        spdlog::info("SteamInternal_CreateInterface ISteamClient(legacy) hook {}", pszVersion);
        return Hookey_SteamClient20(val);
    }

    auto val = real(pszVersion);
    return val;
}

// for older games
// NOTE: the v009/v023 headers define inline accessors with these names; we export our
// own symbols instead so old binaries that link SteamApps()/SteamUser() directly get hooked.
extern "C" ISteamApps *S_CALLTYPE SteamApps() {
    ensure_realdlsym();
    spdlog::info("SteamApps() called");

    //get isteamapps
    void* S_CALLTYPE (*real)();
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamApps");
    ISteamApps* val = (ISteamApps*)real();
    //return val;
    return Hookey_SteamApps(val);
}
// for older games
extern "C" ISteamUser *S_CALLTYPE SteamUser() {
    ensure_realdlsym();
    spdlog::info("SteamUser() called");

    //get isteamuser 
    void* S_CALLTYPE (*real)();
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamUser");
    ISteamUser* val = (ISteamUser*)real();
    //return val;
    // legacy accessor: games on old SDKs expect the SteamUser020/021 vtable
    return (ISteamUser*)Hookey_SteamUser21((ISteamUser021*)val);
}

// disabled for now due to PAYDAY 2 launch issues (Paradox launcher will show 'Not owned' without this)
//TODO: readd when codebase is desphagettifed and can support multiple SDK versions
//TODO: test whether this breaks/fixes other games
// this is a hooking method
// extern "C" ISteamClient *S_CALLTYPE SteamClient() {
//     ensure_realdlsym();
//     spdlog::info("SteamClient() called");

//     //get isteamuser
//     void* S_CALLTYPE (*real)();
//     *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamClient");
//     ISteamClient* val = (ISteamClient*)real();
//     return Hookey_SteamClient(val);
// }

// anti-debugger shenanigans
long ptrace(int request, int pid, void *addr, void *data) {
    return 0;
}


static bool hooked = false;

int printdliter(struct dl_phdr_info *info, size_t size, void *data) {
  printf("%s\n", info->dlpi_name);
  return 0;
}

extern "C" bool SteamAPI_Init()
{
    ensure_realdlsym();
    std::string creaminipath = "cream_api.ini";
    // for developers: enable these 2 lines if you want to log output to a file
    // auto logger = std::make_shared<spdlog::sinks::basic_file_sink_mt>("creamlinux_log.txt", true);
    // spdlog::default_logger().get()->sinks().push_back(logger);

    auto env = std::getenv("CREAM_CONFIG_PATH");
    //f env exists, use it
    if (env != NULL) {
        creaminipath = env;
    }
    
    spdlog::info("Reading config from {}", creaminipath);
    mINI::INIFile file(creaminipath);

    // Open ini file
    file.read(ini);
    // Find dlc's and add to vector
    for (pair<string,string> entry : ini["dlc"]) {
        auto dlctuple = std::make_tuple(stoi(entry.first), entry.second);
        dlcs.push_back(dlctuple);
        spdlog::info("Added dlc with id: {0}, name: {1}", entry.first, entry.second);
    }
    spdlog::info("SteamAPI_Init called in PID {0}", getpid());
    // finish api call
    // the spaghetti below this comment is calling the original Init function
    // can probably be simplified but i'm no c++ expert 
    bool (*real)();
    *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamAPI_Init");
    char* errstr = dlerror();
    if (errstr != NULL) {
        spdlog::error("SteamAPI_Init failed; A dynamic linking error occurred: {0}", errstr);
        spdlog::error("Listing open libraries.");
        dl_iterate_phdr(printdliter, NULL);
        return false;
    }
    spdlog::info("Calling real SteamAPI_Init");
    auto retval = real();
    spdlog::info("SteamAPI_Init returned {0}", retval);
    
    return retval;
}

struct SContextInitData { 
    void (*pFn)(void* ctx); 
    uintptr_t counter; 
    void *ptr;
};
// uncomment the below line and filelog->info lines to enable dlsym logging for quirky use cases (Proton, mono apps)
// auto filelog = spdlog::basic_logger_mt("dlsym_log", "creamlinux_dlsym_log.txt", true);

//TODO: add a flag to allow users to disable the dlsym hooking method
extern "C" void *dlsym(void *handle, const char *name)
{   
    // filelog->info("modified dlsym called with {0}", name);

    ensure_realdlsym();

    /* my target binary is even asking for dlsym() via dlsym()... */
    if (!strcmp(name,"dlsym")) 
        return (void*)dlsym;

    if (!strcmp(name, "SteamAPI_Init") && handle != RTLD_NEXT) {
        spdlog::info("returning custom impl for {0}", name);
        spdlog::info("custom: {0}, real: {1}", (void *)SteamAPI_Init, real_dlsym(RTLD_NEXT, "SteamAPI_Init"));
        // filelog->info("returning custom impl for {0}", name);
        // filelog->info("custom: {0}, real: {1}", (void *)SteamAPI_Init, real_dlsym(RTLD_NEXT, "SteamAPI_Init"));
        return (void *)SteamAPI_Init;
    }
    if (!strcmp(name, "SteamInternal_CreateInterface") && handle != RTLD_NEXT) {
        spdlog::info("returning custom impl for {0}", name);
        spdlog::info("custom: {0}, real: {1}", (void *)SteamInternal_CreateInterface, real_dlsym(RTLD_NEXT, "SteamInternal_CreateInterface"));
        //filelog->info("returning custom impl for {0}", name);
        //filelog->info("custom: {0}, real: {1}", (void *)SteamInternal_CreateInterface, real_dlsym(RTLD_NEXT, "SteamInternal_CreateInterface"));
        return (void *)SteamInternal_CreateInterface;
    }
    // enabling this makes some games (golf with your friends) start calling non-existing functions
    //TODO: add a setting to let users use this as a hooking method
    // if (!strcmp(name, "CreateInterface") && handle != RTLD_NEXT) {
    //     filelog->info("returning custom impl for {0}", name);
    //     filelog->info("custom: {0}, real: {1}", (void *)CreateInterface, real_dlsym(RTLD_NEXT, "CreateInterface"));
    //     return (void *)CreateInterface;
    // }

    return real_dlsym(handle,name);
}

//TODO: support this as a hooking method
// extern "C" void* SteamInternal_ContextInit(void *pContextInitData) {
//     spdlog::info("SteamInternal_ContextInit called");
//     void* S_CALLTYPE (*real)(void *pContextInitData);
//     *(void**)(&real) = real_dlsym(RTLD_NEXT, "SteamInternal_ContextInit");
//     return real(pContextInitData);
//     // struct SContextInitData *contextInitData = (struct SContextInitData *)pContextInitData;
//     // if (contextInitData->counter != global_counter) {
//     //     contextInitData->pFn(&contextInitData->ctx);
//     //     contextInitData->counter = global_counter;
//     // }

//     // return &contextInitData->ctx;
// }