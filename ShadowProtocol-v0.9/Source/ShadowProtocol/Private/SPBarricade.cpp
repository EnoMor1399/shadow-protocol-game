#include "SPBarricade.h"
#include "Net/UnrealNetwork.h"

ASPBarricade::ASPBarricade()
{
    bReplicates = true;
    SetReplicateMovement(true);
}

void ASPBarricade::ServerDeploy_Implementation(ESPTeam Team)
{
    if(!HasAuthority() || bDeployed || Team==ESPTeam::None) return;
    OwningTeam = Team;
    Health = MaxHealth;
    bDeployed = true;
}

void ASPBarricade::ApplyTacticalDamage(float Amount)
{
    if(!HasAuthority() || !bDeployed || Amount<=0.f) return;
    Health = FMath::Max(0.f, Health-Amount);
    if(Health<=0.f)
    {
        bDeployed=false;
        BP_OnBarricadeBroken();
        SetLifeSpan(0.15f);
    }
}

void ASPBarricade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPBarricade,Health);
    DOREPLIFETIME(ASPBarricade,OwningTeam);
    DOREPLIFETIME(ASPBarricade,bDeployed);
}
