#include "SPCoverSystemComponent.h"
#include "Net/UnrealNetwork.h"

USPCoverSystemComponent::USPCoverSystemComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void USPCoverSystemComponent::ServerSetCoverState_Implementation(bool bRequestedInCover, FVector_NetQuantizeNormal RequestedNormal)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    FVector Normal = FVector(RequestedNormal);
    if (bRequestedInCover)
    {
        if (Normal.IsNearlyZero())
        {
            return;
        }
        Normal.Normalize();
    }
    else
    {
        Normal = FVector::ZeroVector;
        PeekAlpha = 0.f;
    }

    bInCover = bRequestedInCover;
    CoverNormal = Normal;
    BP_OnCoverStateChanged(bInCover, CoverNormal, PeekAlpha);
}

void USPCoverSystemComponent::ServerSetPeekAlpha_Implementation(float RequestedPeekAlpha)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    PeekAlpha = bInCover ? FMath::Clamp(RequestedPeekAlpha, -1.f, 1.f) : 0.f;
    BP_OnCoverStateChanged(bInCover, CoverNormal, PeekAlpha);
}

void USPCoverSystemComponent::OnRep_CoverState()
{
    BP_OnCoverStateChanged(bInCover, CoverNormal, PeekAlpha);
}

void USPCoverSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USPCoverSystemComponent, bInCover);
    DOREPLIFETIME(USPCoverSystemComponent, CoverNormal);
    DOREPLIFETIME(USPCoverSystemComponent, PeekAlpha);
}
