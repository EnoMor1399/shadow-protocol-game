#include "SPTacticalPlanningComponent.h"
#include "Net/UnrealNetwork.h"
USPTacticalPlanningComponent::USPTacticalPlanningComponent(){ SetIsReplicatedByDefault(true); }
void USPTacticalPlanningComponent::ServerAddMarker_Implementation(const FSPTacticalMarker& Marker){ if(GetOwner()&&GetOwner()->HasAuthority()&&Markers.Num()<64) Markers.Add(Marker); }
void USPTacticalPlanningComponent::ServerClearOwnedMarkers_Implementation(int32 Id){ if(!GetOwner()||!GetOwner()->HasAuthority())return; Markers.RemoveAll([Id](const FSPTacticalMarker&M){return M.OwnerPlayerId==Id;}); }
void USPTacticalPlanningComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(USPTacticalPlanningComponent,Markers); }
