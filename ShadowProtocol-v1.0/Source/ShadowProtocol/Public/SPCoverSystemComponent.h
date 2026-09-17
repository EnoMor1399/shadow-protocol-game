#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPCoverSystemComponent.generated.h"

UCLASS(ClassGroup=(ShadowProtocol), meta=(BlueprintSpawnableComponent))
class SHADOWPROTOCOL_API USPCoverSystemComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPCoverSystemComponent();

    UPROPERTY(ReplicatedUsing=OnRep_CoverState, BlueprintReadOnly, Category="Cover")
    bool bInCover = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Cover")
    FVector_NetQuantizeNormal CoverNormal = FVector::ZeroVector;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Cover", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float PeekAlpha = 0.f;

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Cover")
    void ServerSetCoverState(bool bRequestedInCover, FVector_NetQuantizeNormal RequestedNormal);

    UFUNCTION(Server, Unreliable, BlueprintCallable, Category="Cover")
    void ServerSetPeekAlpha(float RequestedPeekAlpha);

    UFUNCTION(BlueprintPure, Category="Cover")
    bool IsExposedFromCover() const { return bInCover && FMath::Abs(PeekAlpha) > 0.15f; }

    UFUNCTION(BlueprintImplementableEvent, Category="Cover")
    void BP_OnCoverStateChanged(bool bNowInCover, FVector Normal, float CurrentPeekAlpha);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_CoverState();
};
