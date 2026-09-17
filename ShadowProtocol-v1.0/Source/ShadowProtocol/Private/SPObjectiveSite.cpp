#include "SPObjectiveSite.h"
#include "Net/UnrealNetwork.h"

ASPObjectiveSite::ASPObjectiveSite()
{
    bReplicates = true;
    SetReplicateMovement(false);
    DisplayName = FText::FromString(TEXT("Archive Core"));
}

void ASPObjectiveSite::SetActiveSite(bool bActive)
{
    if (!HasAuthority())
    {
        return;
    }

    bActiveThisRound = bActive;
    BP_OnObjectiveSiteStateChanged(bActiveThisRound);
}

void ASPObjectiveSite::OnRep_ActiveSite()
{
    BP_OnObjectiveSiteStateChanged(bActiveThisRound);
}

void ASPObjectiveSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPObjectiveSite, SiteId);
    DOREPLIFETIME(ASPObjectiveSite, DisplayName);
    DOREPLIFETIME(ASPObjectiveSite, EmbassyZone);
    DOREPLIFETIME(ASPObjectiveSite, bActiveThisRound);
}
