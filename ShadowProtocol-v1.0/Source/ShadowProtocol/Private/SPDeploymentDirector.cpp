#include "SPDeploymentDirector.h"
#include "Net/UnrealNetwork.h"

ASPDeploymentDirector::ASPDeploymentDirector()
{
    bReplicates = true;
    SetReplicateMovement(false);
}

void ASPDeploymentDirector::ServerBeginDeployment_Implementation(FName RequestedSpawnGroup)
{
    if (!HasAuthority())
    {
        return;
    }

    static const TSet<FName> AllowedGroups = { TEXT("ALPHA"), TEXT("BRAVO"), TEXT("CHARLIE") };
    SpawnGroup = AllowedGroups.Contains(RequestedSpawnGroup) ? RequestedSpawnGroup : FName(TEXT("ALPHA"));
    DeploymentPhase = ESPDeploymentPhase::Authentication;
    BP_OnDeploymentPhaseChanged(DeploymentPhase);
}

void ASPDeploymentDirector::AdvanceDeploymentAuthority()
{
    if (!HasAuthority())
    {
        return;
    }

    switch (DeploymentPhase)
    {
        case ESPDeploymentPhase::Idle: DeploymentPhase = ESPDeploymentPhase::Authentication; break;
        case ESPDeploymentPhase::Authentication: DeploymentPhase = ESPDeploymentPhase::LoadoutCheck; break;
        case ESPDeploymentPhase::LoadoutCheck: DeploymentPhase = ESPDeploymentPhase::Insertion; break;
        case ESPDeploymentPhase::Insertion: DeploymentPhase = ESPDeploymentPhase::Live; break;
        case ESPDeploymentPhase::Live: return;
        default: DeploymentPhase = ESPDeploymentPhase::Idle; break;
    }

    BP_OnDeploymentPhaseChanged(DeploymentPhase);
}

void ASPDeploymentDirector::OnRep_DeploymentPhase()
{
    BP_OnDeploymentPhaseChanged(DeploymentPhase);
}

void ASPDeploymentDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPDeploymentDirector, DeploymentPhase);
    DOREPLIFETIME(ASPDeploymentDirector, SpawnGroup);
    DOREPLIFETIME(ASPDeploymentDirector, OperationId);
}
