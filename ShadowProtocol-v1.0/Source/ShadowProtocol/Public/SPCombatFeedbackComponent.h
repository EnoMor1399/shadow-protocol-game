#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPCombatFeedbackComponent.generated.h"

UCLASS(ClassGroup=(ShadowProtocol), meta=(BlueprintSpawnableComponent))
class SHADOWPROTOCOL_API USPCombatFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPCombatFeedbackComponent();
    UFUNCTION(NetMulticast, Unreliable) void MulticastWeaponFired(bool bSuppressed);
    UFUNCTION(NetMulticast, Unreliable) void MulticastSuppression(float Intensity);
    UFUNCTION(NetMulticast, Unreliable) void MulticastBreachImpact(FVector_NetQuantize Location, float Intensity);
    UFUNCTION(BlueprintImplementableEvent, Category="Presentation") void BP_WeaponFired(bool bSuppressed);
    UFUNCTION(BlueprintImplementableEvent, Category="Presentation") void BP_Suppression(float Intensity);
    UFUNCTION(BlueprintImplementableEvent, Category="Presentation") void BP_BreachImpact(FVector Location, float Intensity);
};
