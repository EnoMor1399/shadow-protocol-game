#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPTypes.h"
#include "SPTacticalEquipmentComponent.generated.h"

class ASPTacticalProjectile;

UCLASS(ClassGroup=(ShadowProtocol), meta=(BlueprintSpawnableComponent))
class SHADOWPROTOCOL_API USPTacticalEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPTacticalEquipmentComponent();

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Tactical Equipment") ESPTacticalEquipment SelectedEquipment = ESPTacticalEquipment::FlashGrenade;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Tactical Equipment") int32 FlashGrenades = 2;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Tactical Equipment") int32 SmokeGrenades = 2;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tactical Equipment") float MaxThrowDistance = 900.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tactical Equipment") float ThrowSpeed = 1150.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tactical Equipment") TSubclassOf<ASPTacticalProjectile> ProjectileClass;

    UFUNCTION(BlueprintCallable) void CycleEquipment();
    UFUNCTION(BlueprintCallable) void DeploySelected(const FVector& TargetLocation);
    UFUNCTION(Server, Reliable) void ServerCycleEquipment();
    UFUNCTION(Server, Reliable) void ServerDeploySelected(FVector_NetQuantize TargetLocation);
    UFUNCTION(NetMulticast, Reliable) void MulticastEquipmentDeployed(ESPTacticalEquipment Equipment, FVector_NetQuantize TargetLocation);
    UFUNCTION(BlueprintImplementableEvent, Category="Tactical Equipment") void BP_OnEquipmentDeployed(ESPTacticalEquipment Equipment, FVector TargetLocation);

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
