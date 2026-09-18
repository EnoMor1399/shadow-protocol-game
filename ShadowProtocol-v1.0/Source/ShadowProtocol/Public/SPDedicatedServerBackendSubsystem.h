#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SPDedicatedServerBackendSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FSPDedicatedServerRegistration
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString NodeId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString ServerId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString Region;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString NetworkBuild;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString Status;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    int32 Capacity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    int32 ActiveAllocations = 0;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    int32 HeartbeatTtlMs = 0;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString CredentialExpiresAt;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    int32 CredentialTtlMs = 0;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    int32 CredentialGraceMs = 0;
};

USTRUCT(BlueprintType)
struct FSPDedicatedServerAdmission
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    bool bAdmitted = false;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString AllocationId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString MatchId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString ServerId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString Region;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString UserId;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString NetworkBuild;

    UPROPERTY(BlueprintReadOnly, Category="Shadow Protocol|Dedicated Server")
    FString BackendProtocol;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPDedicatedServerRegistered, FSPDedicatedServerRegistration, Registration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSPDedicatedServerHeartbeat, FString, ServerId, FString, Status, int32, ActiveAllocations);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPDedicatedServerAdmissionCompleted, FSPDedicatedServerAdmission, Admission);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSPDedicatedServerAdmissionFailed, FString, AllocationId, FString, MatchId, FString, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSPDedicatedServerAllocationReleased, FString, AllocationId, FString, MatchId, FString, Status, int32, ActiveAllocations);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPDedicatedServerDrainChanged, bool, bDraining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPDedicatedServerCredentialRotated, FString, CredentialExpiresAt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPDedicatedServerBackendFailed, FString, Context, FString, ErrorMessage);

/**
 * Dedicated-server-only bridge for the v1.0.1 regional registry and admission API.
 *
 * The registration bootstrap credential is loaded from SERVER_REGISTRATION_SECRET at runtime.
 * An optional single-use orchestrator attestation is loaded from SP_NODE_ATTESTATION and sent
 * only during registration. After registration, a per-node credential is kept only in server memory.
 * None of these trust materials are exposed to Blueprint, config files, logs, SaveGame data or the game client.
 * Shipped clients may contain this class as code, but ConfigureFromRuntime refuses
 * to activate it outside a dedicated-server process.
 */
UCLASS()
class SHADOWPROTOCOL_API USPDedicatedServerBackendSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Dedicated Server")
    FSPDedicatedServerRegistered OnRegistered;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Dedicated Server")
    FSPDedicatedServerHeartbeat OnHeartbeat;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Dedicated Server")
    FSPDedicatedServerAdmissionCompleted OnAdmissionCompleted;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Dedicated Server")
    FSPDedicatedServerAdmissionFailed OnAdmissionFailed;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Dedicated Server")
    FSPDedicatedServerAllocationReleased OnAllocationReleased;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Dedicated Server")
    FSPDedicatedServerDrainChanged OnDrainChanged;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Dedicated Server")
    FSPDedicatedServerCredentialRotated OnCredentialRotated;

    UPROPERTY(BlueprintAssignable, Category="Shadow Protocol|Dedicated Server")
    FSPDedicatedServerBackendFailed OnRequestFailed;

    /** Reads non-secret routing configuration from command-line arguments and the registration bootstrap secret from the process environment. */
    bool ConfigureFromRuntime();

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Dedicated Server")
    void RegisterNode();

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Dedicated Server")
    void SendHeartbeat();

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Dedicated Server")
    void RotateNodeCredential();

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Dedicated Server")
    void MarkDraining();

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Dedicated Server")
    void AdmitConnection(const FString& AllocationId, const FString& MatchId, const FString& ConnectToken);

    UFUNCTION(BlueprintCallable, Category="Shadow Protocol|Dedicated Server")
    void ReleaseAllocation(const FString& AllocationId, const FString& MatchId, bool bFailed = false);

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Dedicated Server")
    bool IsConfigured() const { return bConfigured; }

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Dedicated Server")
    bool IsRegistered() const { return bRegistered; }

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Dedicated Server")
    bool IsDraining() const { return bDraining; }

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Dedicated Server")
    FString GetServerId() const { return ServerId; }

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Dedicated Server")
    FString GetRegion() const { return Region; }

private:
    FString BackendBaseUrl = TEXT("http://127.0.0.1:8080");
    FString ServerId;
    FString Region;
    FString PublicHost;
    int32 PublicPort = 0;
    int32 Capacity = 10;
    FString RegistrationSecret;
    FString NodeAttestation;
    FString NodeCredential;

    bool bConfigured = false;
    bool bRegistered = false;
    bool bDraining = false;
    bool bRestoreDrainAfterRegistration = false;
    bool bRegistrationWasAttested = false;
    float HeartbeatIntervalSeconds = 10.0f;
    float CredentialRotationDelaySeconds = 0.0f;
    FTSTicker::FDelegateHandle HeartbeatTickerHandle;
    FTSTicker::FDelegateHandle CredentialRotationTickerHandle;

    FString BuildUrl(const FString& Path) const;
    bool HasRequiredConfiguration() const;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateInfrastructureJsonRequest(const FString& Path, const FString& Verb) const;
    bool TickHeartbeat(float DeltaSeconds);
    bool TickCredentialRotation(float DeltaSeconds);
    void StartHeartbeat(int32 HeartbeatTtlMs);
    void StopHeartbeat();
    void StartCredentialRotation(int32 CredentialTtlMs);
    void StopCredentialRotation();
    void RecoverNodeRegistration();
    void SendBestEffortShutdownDrain();

    void HandleRegistrationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleHeartbeatResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleCredentialRotationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleDrainResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleAdmissionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString AllocationId, FString MatchId);
    void HandleReleaseResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void BroadcastHttpFailure(const FString& Context, FHttpResponsePtr Response, bool bWasSuccessful);
};
