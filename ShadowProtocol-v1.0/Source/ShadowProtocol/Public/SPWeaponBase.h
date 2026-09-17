#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPWeaponBase.generated.h"

class ASPCharacter;

UCLASS()
class SHADOWPROTOCOL_API ASPWeaponBase : public AActor
{
    GENERATED_BODY()
public:
    ASPWeaponBase();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Damage = 32.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Range = 12000.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Recoil = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Handling = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Penetration = 0.35f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 MaxPenetrations = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float PenetrationDamageMultiplier = 0.55f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float SuppressionRadius = 120.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float AccuracySpreadDegrees = 0.65f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float LagCompensationRadius = 55.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 MagazineSize = 30;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 AmmoInMagazine = 30;

    void ServerTryFire(ASPCharacter* Shooter, float ClientServerTimeSeconds);
    void ServerReload();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
