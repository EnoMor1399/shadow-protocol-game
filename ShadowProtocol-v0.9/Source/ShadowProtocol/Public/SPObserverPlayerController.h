#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SPObserverPlayerController.generated.h"

UCLASS()
class SHADOWPROTOCOL_API ASPObserverPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Observer") bool bFreeObserverCamera = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Observer") int32 ObserverTargetIndex = 0;
    UFUNCTION(BlueprintCallable, Server, Reliable) void ServerCycleObserverTarget(int32 Direction);
    UFUNCTION(BlueprintCallable, Server, Reliable) void ServerSetFreeObserverCamera(bool bEnabled);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void SetupInputComponent() override;
    void CycleObserverNext();
    void ToggleFreeObserver();
};
