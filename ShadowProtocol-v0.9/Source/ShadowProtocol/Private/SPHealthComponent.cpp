#include "SPHealthComponent.h"
#include "Net/UnrealNetwork.h"

USPHealthComponent::USPHealthComponent()
{
    SetIsReplicatedByDefault(true);
    Health = MaxHealth;
}

void USPHealthComponent::ApplyLocalizedDamage(float Amount, ESPBodyZone Zone, bool bAllowDownedState)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f || (!IsAlive())) return;
    const float Old = Health;
    Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);

    if ((Zone == ESPBodyZone::LeftArm || Zone == ESPBodyZone::RightArm) && Health > 0.f)
        ArmStabilityMultiplier = FMath::Min(ArmStabilityMultiplier, 0.72f);
    if ((Zone == ESPBodyZone::LeftLeg || Zone == ESPBodyZone::RightLeg) && Health > 0.f)
        LegSpeedMultiplier = FMath::Min(LegSpeedMultiplier, 0.68f);

    if (Health <= 0.f && bAllowDownedState)
    {
        bDowned = true;
        Health = 1.f;
        OnDownedChanged.Broadcast(true);
    }
    OnHealthChanged.Broadcast(Health, Health - Old);
}

bool USPHealthComponent::Stabilize(float HealthRestored)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bDowned) return false;
    bDowned = false;
    Health = FMath::Clamp(HealthRestored, 1.f, MaxHealth);
    OnDownedChanged.Broadcast(false);
    OnHealthChanged.Broadcast(Health, Health);
    return true;
}

void USPHealthComponent::OnRep_Health(float OldHealth)
{
    OnHealthChanged.Broadcast(Health, Health - OldHealth);
}

void USPHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USPHealthComponent, Health);
    DOREPLIFETIME(USPHealthComponent, ArmStabilityMultiplier);
    DOREPLIFETIME(USPHealthComponent, LegSpeedMultiplier);
    DOREPLIFETIME(USPHealthComponent, bDowned);
}
