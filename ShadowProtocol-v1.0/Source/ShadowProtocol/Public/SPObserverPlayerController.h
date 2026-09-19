#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SPObserverPlayerController.generated.h"

class USPReadyRoomWidget;
class USPControlsWidget;

UCLASS()
class SHADOWPROTOCOL_API ASPObserverPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Observer") bool bFreeObserverCamera = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Observer") int32 ObserverTargetIndex = 0;
    UFUNCTION(BlueprintCallable, Server, Reliable) void ServerCycleObserverTarget(int32 Direction);
    UFUNCTION(BlueprintCallable, Server, Reliable) void ServerSetFreeObserverCamera(bool bEnabled);
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Ready Room") TArray<FName> AvailableSpawnGroups;
    UFUNCTION(BlueprintPure, Category="Ready Room") bool IsReadyRoomRequestPending() const { return PendingReadyRoomRequestId != 0; }
    UFUNCTION(BlueprintPure, Category="Ready Room") FString GetReadyRoomFeedback() const { return ReadyRoomFeedback; }
    UPROPERTY(EditDefaultsOnly, Category="Ready Room") bool bShowNativeReadyRoom = true;
    UFUNCTION(BlueprintCallable, Category="Ready Room") void RequestReadyState(bool bReady);
    UFUNCTION(BlueprintCallable, Category="Ready Room") void RequestSpawnGroup(FName SpawnGroupId);
    UFUNCTION(BlueprintCallable, Category="Controls") void ToggleControls();
    UFUNCTION(BlueprintPure, Category="Controls") bool IsGameplayInputBlocked() const { return bControlsOpen || bReadyRoomInputActive; }
    UFUNCTION(BlueprintPure, Category="Controls") bool AreControlsOpen() const { return bControlsOpen; }
    float GetMouseSensitivity() const { return MouseSensitivity; }
    bool IsMouseYInverted() const { return bInvertMouseY; }
    void SetMouseSensitivity(float Value);
    void SetMouseYInverted(bool bEnabled) { bInvertMouseY = bEnabled; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UFUNCTION(Server, Reliable) void ServerSetReadyState(bool bReady, int32 RequestId);
    UFUNCTION(Server, Reliable) void ServerSelectSpawnGroup(FName SpawnGroupId, int32 RequestId);
    bool ConsumeReadyRoomRequest();
    bool BeginReadyRoomRequest();
    UFUNCTION(Client, Reliable) void ClientReadyRoomResult(int32 RequestId, bool bAccepted, const FString& Message);
    int32 ReadyRoomRequestSequence = 0;
    int32 PendingReadyRoomRequestId = 0;
    double ReadyRoomRequestDeadline = 0.0;
    float SpawnChoicesRefreshRemaining = 0.f;
    FString ReadyRoomFeedback;
    UPROPERTY(Transient) TObjectPtr<USPReadyRoomWidget> ReadyRoomWidget;
    double NextReadyRoomRequestSeconds = 0.0;
    double NextLocalReadyRoomRequestSeconds = 0.0;
    bool bReadyRoomInputActive = false;
    bool bControlsOpen = false;
    bool bInterfaceInputIgnored = false;
    float MouseSensitivity = 1.f;
    bool bInvertMouseY = false;
    float ReadyRoomRefreshRemaining = 0.f;
    UPROPERTY(Transient) TObjectPtr<USPControlsWidget> ControlsWidget;
    void ApplyInterfaceInputMode();
    void SaveControlSettings();
    virtual void SetupInputComponent() override;
    void CycleObserverNext();
    void ToggleFreeObserver();
};
