#include "SPAlertDirector.h"
#include "Net/UnrealNetwork.h"

ASPAlertDirector::ASPAlertDirector()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
}

void ASPAlertDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || AlertLevel >= 3) return;
    Exposure = FMath::Max(0.f, Exposure - PassiveDecayPerSecond * DeltaSeconds);
    EvaluateAlert(TEXT("Decay"));
}

void ASPAlertDirector::AddExposure(float Amount, FName SourceTag)
{
    if (!HasAuthority() || Amount <= 0.f) return;
    Exposure = FMath::Clamp(Exposure + Amount, 0.f, 1.f);
    EvaluateAlert(SourceTag);
}

void ASPAlertDirector::ClearExposure(float Amount)
{
    if (!HasAuthority() || Amount <= 0.f) return;
    Exposure = FMath::Clamp(Exposure - Amount, 0.f, 1.f);
    EvaluateAlert(TEXT("Countermeasure"));
}

void ASPAlertDirector::EvaluateAlert(FName SourceTag)
{
    const int32 NewLevel = Exposure >= .90f ? 3 : Exposure >= .62f ? 2 : Exposure >= .30f ? 1 : 0;
    if (NewLevel == AlertLevel) return;
    AlertLevel = NewLevel;
    bLockdown = AlertLevel >= 3;
    BP_OnAlertLevelChanged(AlertLevel, SourceTag);
    if (AlertLevel >= 2 && ReinforcementWave < AlertLevel - 1)
    {
        ++ReinforcementWave;
        BP_RequestReinforcementWave(ReinforcementWave);
    }
}

void ASPAlertDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPAlertDirector, Exposure);
    DOREPLIFETIME(ASPAlertDirector, AlertLevel);
    DOREPLIFETIME(ASPAlertDirector, ReinforcementWave);
    DOREPLIFETIME(ASPAlertDirector, bLockdown);
}
