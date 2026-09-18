#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SPBackendSessionSubsystem.generated.h"

class FJsonObject;
class APlayerController;

USTRUCT(BlueprintType)
struct FSPBackendCompatibility
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString GameRelease;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString NetworkBuild;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString ContentRevision;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString BackendProtocol;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    TArray<FString> AcceptedNetworkBuilds;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString Enforcement;
};

USTRUCT(BlueprintType)
struct FSPMatchAllocation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString AllocationId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString MatchId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString ServerId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString Region;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    int32 TickRate = 0;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString ConnectToken;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString ConnectHost;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    int32 ConnectPort = 0;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString ExpiresAt;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString NetworkBuild;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString BackendProtocol;
};

USTRUCT(BlueprintType)
struct FSPReconnectResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    bool bReconnected = false;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString UserId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString Team;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString SpawnGroup;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString NetworkBuild;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Network")
    FString BackendProtocol;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPCompatibilityChecked, bool, bCompatible, FSPBackendCompatibility, Compatibility);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPAllocationCompleted, FSPMatchAllocation, Allocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPSessionExpired, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPSessionRefreshed, FString, SessionId, FString, ExpiresAt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSPReconnectTicketIssued, FString, ReconnectToken, FString, ReconnectDeadline, int32, GraceSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPReconnectCompleted, FSPReconnectResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSPUpgradeRequired, FString, RequiredBuild, FString, BackendProtocol, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPBackendRequestFailed, FString, Context, FString, ErrorMessage);

/**
 * Thin client-side bridge for the Shadow Protocol backend compatibility,
 * authenticated session rotation, allocation and reconnect flow.
 *
 * SECURITY: this subsystem never stores or transmits SESSION_BOOTSTRAP_SECRET.
 * An authenticated game-session token must be supplied by a trusted platform /
 * identity integration and is kept in memory only for the current game instance.
 */
UCLASS()
class SHADOWPROTOCOL_API USPBackendSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Network")
    FSPSessionExpired OnSessionExpired;

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Network")
    bool HasExpiredSession() const { return bSessionExpired; }

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Network")
    float GetSessionSecondsRemaining() const;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Network")
    FSPCompatibilityChecked OnCompatibilityChecked;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Network")
    FSPAllocationCompleted OnAllocationCompleted;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Network")
    FSPSessionRefreshed OnSessionRefreshed;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Network")
    FSPReconnectTicketIssued OnReconnectTicketIssued;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Network")
    FSPReconnectCompleted OnReconnectCompleted;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Network")
    FSPUpgradeRequired OnUpgradeRequired;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Network")
    FSPBackendRequestFailed OnRequestFailed;

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    void SetBackendBaseUrl(const FString& InBaseUrl);

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Network")
    FString GetBackendBaseUrl() const { return BackendBaseUrl; }

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    void CheckCompatibility();

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    void InstallAuthenticatedSession(const FString& InSessionId, const FString& InSessionToken, const FString& InRegion, const FString& InExpiresAt);

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    void RefreshAuthenticatedSession();

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    void ClearAuthenticatedSession();

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Network")
    bool HasAuthenticatedSession() const { return GetSessionSecondsRemaining() > 0.0f; }

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Network")
    bool HasVerifiedCompatibility() const { return bCompatibilityVerified; }

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Network")
    FString GetSessionExpiresAt() const { return SessionExpiresAt; }

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    void AllocateProtocolServer(const FString& Region, bool bRanked = true);

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Network")
    FString BuildAllocationTravelUrl(const FSPMatchAllocation& Allocation) const;

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    bool ConnectToAllocation(APlayerController* PlayerController, const FSPMatchAllocation& Allocation) const;

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    void RequestReconnectTicket(const FString& MatchId, int32 RoundNumber, int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Network")
    void ReconnectToReservedSlot(const FString& MatchId, int32 RoundNumber, int32 SlotIndex, const FString& ReconnectToken);

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Network")
    FSPBackendCompatibility GetLastCompatibility() const { return LastCompatibility; }

private:
    FTSTicker::FDelegateHandle ExpiryTicker;
    FDateTime SessionExpiryUtc;
    double SessionExpiryMonotonic = 0.0;
    bool bSessionExpired = false;
    bool bRefreshPending = false;
    TArray<FHttpRequestPtr> ActiveAuthenticatedRequests;
    FHttpRequestPtr CompatibilityRequest;
    bool TickSessionExpiry(float DeltaSeconds);
    bool SetSessionExpiry(const FString& ExpiresAt);
    void ExpireSession();
    bool ConsumeAuthenticatedResponse(FHttpRequestPtr Request);
    void CancelAuthenticatedRequests();
    FString BackendBaseUrl = TEXT("http://127.0.0.1:8080");
    FString SessionId;
    FString SessionToken;
    FString SessionRegion;
    FString SessionExpiresAt;
    bool bCompatibilityVerified = false;
    bool bPendingRankedAllocation = true;
    FSPBackendCompatibility LastCompatibility;

    FString BuildUrl(const FString& Path) const;
    bool CanUseAuthenticatedMatchEndpoint(const FString& Context);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateAuthenticatedJsonRequest(const FString& Path, const FString& Verb);
    void HandleCompatibilityResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleSessionRefreshResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleAllocationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleReconnectTicketResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleReconnectResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void BroadcastHttpFailure(const FString& Context, FHttpResponsePtr Response, bool bWasSuccessful);
    bool ParseCompatibility(const TSharedPtr<FJsonObject>& JsonObject, FSPBackendCompatibility& OutCompatibility) const;
    void HandleUpgradeResponse(const TSharedPtr<FJsonObject>& JsonObject, const FString& FallbackReason);
};
