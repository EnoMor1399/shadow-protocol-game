#include "SPBreachableDoor.h"
#include "Net/UnrealNetwork.h"
ASPBreachableDoor::ASPBreachableDoor(){ bReplicates=true; }
void ASPBreachableDoor::ServerApplyBreach_Implementation(float Force){ if(!HasAuthority()||bBreached||Force<BreachResistance)return; bBreached=true; BP_OnBreached(); }
void ASPBreachableDoor::OnRep_Breached(){ if(bBreached) BP_OnBreached(); }
void ASPBreachableDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ASPBreachableDoor,bBreached); }
