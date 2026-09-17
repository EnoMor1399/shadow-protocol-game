#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPTypes.h"
#include "SPSecurityDevice.generated.h"

class UStaticMeshComponent;

UCLASS()
class SHADOWPROTOCOL_API ASPSecurityDevice : public AActor
{
    GENERATED_BODY()
public:
    ASPSecurityDevice();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> DeviceMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ESPSecurityDeviceType DeviceType = ESPSecurityDeviceType::Camera;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxHealth = 35.f;
    UPROPERTY(ReplicatedUsing=OnRep_Destroyed, BlueprintReadOnly) bool bDestroyed = false;
    UPROPERTY(Replicated, BlueprintReadOnly) float Health = 35.f;
    UFUNCTION(BlueprintCallable) void ApplyAuthoritativeDamage(float Amount, AController* InstigatorController);
    UFUNCTION() void OnRep_Destroyed();
    UFUNCTION(BlueprintImplementableEvent, Category="Security") void BP_OnSecurityDeviceDestroyed(ESPSecurityDeviceType Type);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
