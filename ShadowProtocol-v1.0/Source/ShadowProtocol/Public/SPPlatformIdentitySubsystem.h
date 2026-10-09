#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SPPlatformIdentitySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPPlatformIdentityBridgeStarted, FString, Provider);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPPlatformIdentityBridgeFailed, FString, Provider, FString, ErrorMessage);

/**
 * Client-side bridge from an authenticated Unreal OnlineSubsystem identity to the
 * trusted Shadow Protocol identity gateway.
 *
 * SECURITY:
 * - provider auth tokens remain memory-only and are sent only to the configured gateway;
 * - provider tokens are never sent directly to the gameplay backend;
 * - backend/verifier secrets are never present in the shipped client;
 * - the gateway must derive the trusted provider subject from token verification.
 */
UCLASS(Config=Game)
class SHADOWPROTOCOL_API USPPlatformIdentitySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Identity")
    FSPPlatformIdentityBridgeStarted OnBridgeStarted;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Identity")
    FSPPlatformIdentityBridgeFailed OnBridgeFailed;

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Identity")
    void SetIdentityGatewayBaseUrl(const FString& InBaseUrl);

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Identity")
    FString GetIdentityGatewayBaseUrl() const { return IdentityGatewayBaseUrl; }

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Identity")
    FString GetDetectedProvider() const;

    /**
     * Starts the provider -> trusted gateway -> backend assertion exchange.
     * The active OnlineSubsystem must be Steam/EOS and the backend compatibility
     * handshake must already be green. If the provider is not logged in and
     * bAttemptAutoLogin is enabled, OnlineSubsystem AutoLogin is attempted first.
     */
    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Identity")
    void BeginPlatformSession(const FString& Region, const FString& DeviceNonce);

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Identity")
    void CancelPlatformSession();

private:
    UPROPERTY(Config)
    FString IdentityGatewayBaseUrl;

    UPROPERTY(Config)
    bool bAttemptAutoLogin = true;

    FHttpRequestPtr GatewayRequest;
    IOnlineIdentityPtr PendingIdentity;
    FDelegateHandle LoginCompleteHandle;
    uint64 RequestGeneration = 0;
    FString PendingProvider;
    FString PendingRegion;
    FString PendingDeviceNonce;

    static FString NormalizeProviderName(FName SubsystemName);
    bool ValidateGatewayUrl(FString& OutError) const;
    void ContinueWithLoggedInIdentity(uint64 Generation);
    void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, uint64 Generation);
    void HandleSteamWebApiToken(int32 LocalUserNum, bool bWasSuccessful, const FExternalAuthToken& AuthToken, uint64 Generation);
    void SubmitProviderToken(const FString& AuthToken, const FString& AuthType, uint64 Generation);
    void HandleGatewayResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, uint64 Generation);
    void ClearLoginDelegate();
    void ClearPendingState();
    void Fail(const FString& ErrorMessage);
};
