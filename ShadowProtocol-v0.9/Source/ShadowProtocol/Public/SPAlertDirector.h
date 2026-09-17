#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPAlertDirector.generated.h"

UCLASS(BlueprintType)
class SHADOWPROTOCOL_API ASPAlertDirector : public AActor
{
    GENERATED_BODY()
public:
    ASPAlertDirector();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Security") float Exposure = 0.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Security") int32 AlertLevel = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Security") int32 ReinforcementWave = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Security") bool bLockdown = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Security") float PassiveDecayPerSecond = 0.055f;

    UFUNCTION(BlueprintCallable, Category="Security") void AddExposure(float Amount, FName SourceTag);
    UFUNCTION(BlueprintCallable, Category="Security") void ClearExposure(float Amount);
    UFUNCTION(BlueprintImplementableEvent, Category="Security") void BP_OnAlertLevelChanged(int32 NewLevel, FName SourceTag);
    UFUNCTION(BlueprintImplementableEvent, Category="Security") void BP_RequestReinforcementWave(int32 WaveIndex);

protected:
    void EvaluateAlert(FName SourceTag);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
