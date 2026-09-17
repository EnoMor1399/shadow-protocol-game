#include "SPInteractiveDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ASPInteractiveDoor::ASPInteractiveDoor()
{
    bReplicates = true;
    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    SetRootComponent(DoorMesh);
    DoorMesh->SetIsReplicated(true);
    Tags.Add(TEXT("SP_InteractiveDoor"));
}

void ASPInteractiveDoor::ServerCycleDoor_Implementation(AController* RequestingController)
{
    if(!RequestingController || DoorState==ESPDoorState::Breached) return;
    DoorState = DoorState==ESPDoorState::Closed ? ESPDoorState::Peek : DoorState==ESPDoorState::Peek ? ESPDoorState::Open : ESPDoorState::Closed;
    OnRep_DoorState();
}

void ASPInteractiveDoor::ServerBreachDoor_Implementation(AController* RequestingController)
{
    if(!RequestingController || DoorState==ESPDoorState::Breached) return;
    DoorState = ESPDoorState::Breached;
    OnRep_DoorState();
}

void ASPInteractiveDoor::OnRep_DoorState(){ BP_ApplyDoorState(DoorState); }
void ASPInteractiveDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out); DOREPLIFETIME(ASPInteractiveDoor, DoorState);
}
