#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPTypes.h"
#include "SPTacticalProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class SHADOWPROTOCOL_API ASPTacticalProjectile : public AActor
{
    GENERATED_BODY()
public:
    ASPTacticalProjectile();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UProjectileMovementComponent> Movement;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPTacticalEquipment EquipmentType = ESPTacticalEquipment::FlashGrenade;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float FuseSeconds = 1.35f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float EffectRadius = 550.f;
    UFUNCTION(BlueprintCallable) void Arm(ESPTacticalEquipment InType, const FVector& InitialVelocity);
    UFUNCTION(NetMulticast, Reliable) void MulticastDetonate(ESPTacticalEquipment Type, FVector_NetQuantize Location, float Radius);
    UFUNCTION(BlueprintImplementableEvent) void BP_OnDetonated(ESPTacticalEquipment Type, FVector Location, float Radius);
protected:
    virtual void BeginPlay() override;
    FTimerHandle FuseHandle;
    void Detonate();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
