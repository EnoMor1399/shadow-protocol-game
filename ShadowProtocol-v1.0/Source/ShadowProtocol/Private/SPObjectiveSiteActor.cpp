#include "SPObjectiveSiteActor.h"
#include "Net/UnrealNetwork.h"

ASPObjectiveSiteActor::ASPObjectiveSiteActor()
{
    bReplicates = true;
    SetReplicateMovement(false);
    DisplayName = FText::FromString(TEXT("Archive Core"));
}

void ASPObjectiveSiteActor::SetActiveSiteAuthority(bool bActive)
{
    if (!HasAuthority())
    {
        return;
    }

    bActiveSite = bActive;
    BP_OnObjectiveSiteStateChanged(bActiveSite);
}

void ASPObjectiveSiteActor::OnRep_Active()
{
    BP_OnObjectiveSiteStateChanged(bActiveSite);
}

void ASPObjectiveSiteActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPObjectiveSiteActor, SiteId);
    DOREPLIFETIME(ASPObjectiveSiteActor, DisplayName);
    DOREPLIFETIME(ASPObjectiveSiteActor, EmbassyZone);
    DOREPLIFETIME(ASPObjectiveSiteActor, bActiveSite);
}
