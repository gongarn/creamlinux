#include "steam_hooks.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "config.h"
#include "interface_versions.h"
#include "spdlog/spdlog.h"

class Hookey_SteamApps_Class : public ISteamApps {
public:
    bool BIsSubscribed() { return true; }
    bool BIsLowViolence() { return false; }
    bool BIsCybercafe() { return false; }
    bool BIsVACBanned() { return false; }
    int GetDLCCount() {
        spdlog::info("ISteamApps->GetDLCCount called PID {0}", getpid());
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
