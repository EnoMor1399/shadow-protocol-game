#include "SPObjectiveSite.h"
#include "Net/UnrealNetwork.h"
ASPObjectiveSite::ASPObjectiveSite(){bReplicates=true;SetReplicateMovement(false);}
void ASPObjectiveSite::SetActiveSite(bool bActive){if(HasAuthority())bActiveThisRound=bActive;}
void ASPObjectiveSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ASPObjectiveSite,bActiveThisRound);
}
