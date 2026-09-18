#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SPObserverPlayerController.generated.h"

class USPReadyRoomWidget;

UCLASS()
class SHADOWPROTOCOL_API ASPObserverPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Observer") bool bFreeObserverCamera = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Observer") int32 ObserverTargetIndex = 0;
    UFUNCTION(BlueprintCallable, Server, Reliable) void ServerCycleObserverTarget(int32 Direction);
    UFUNCTION(BlueprintCallable, Server, Reliable) void ServerSetFreeObserverCamera(bool bEnabled);
    UPROPERTY(EditDefaultsOnly, Category="Ready Room") bool bShowNativeReadyRoom = true;
    UFUNCTION(BlueprintCallable, Category="Ready Room") void RequestReadyState(bool bReady);
    UFUNCTION(BlueprintCallable, Category="Ready Room") void RequestSpawnGroup(FName SpawnGroupId);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UFUNCTION(Server, Reliable) void ServerSetReadyState(bool bReady);
    UFUNCTION(Server, Reliable) void ServerSelectSpawnGroup(FName SpawnGroupId);
    bool ConsumeReadyRoomRequest();
    UPROPERTY(Transient) TObjectPtr<USPReadyRoomWidget> ReadyRoomWidget;
    double NextReadyRoomRequestSeconds = 0.0;
    double NextLocalReadyRoomRequestSeconds = 0.0;
    bool bReadyRoomInputActive = false;
    virtual void SetupInputComponent() override;
    void CycleObserverNext();
    void ToggleFreeObserver();
};
