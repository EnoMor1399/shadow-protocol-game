#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPTypes.h"
#include "SPHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPDownedChanged, bool, bDowned);

UCLASS(ClassGroup=(ShadowProtocol), meta=(BlueprintSpawnableComponent))
class SHADOWPROTOCOL_API USPHealthComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPHealthComponent();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float MaxHealth = 100.f;
    UPROPERTY(ReplicatedUsing=OnRep_Health, BlueprintReadOnly) float Health = 100.f;
    UPROPERTY(Replicated, BlueprintReadOnly) float ArmStabilityMultiplier = 1.f;
    UPROPERTY(Replicated, BlueprintReadOnly) float LegSpeedMultiplier = 1.f;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bDowned = false;
    UPROPERTY(BlueprintAssignable) FSPHealthChanged OnHealthChanged;
    UPROPERTY(BlueprintAssignable) FSPDownedChanged OnDownedChanged;

    UFUNCTION(BlueprintCallable) void ApplyLocalizedDamage(float Amount, ESPBodyZone Zone, bool bAllowDownedState);
    UFUNCTION(BlueprintCallable) bool Stabilize(float HealthRestored = 25.f);
    UFUNCTION(BlueprintPure) bool IsAlive() const { return Health > 0.f || bDowned; }

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION() void OnRep_Health(float OldHealth);
};
