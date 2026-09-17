#include "SPTacticalEquipmentComponent.h"
#include "GameFramework/Actor.h"
#include "SPTacticalProjectile.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

USPTacticalEquipmentComponent::USPTacticalEquipmentComponent()
{
    SetIsReplicatedByDefault(true);
}

void USPTacticalEquipmentComponent::CycleEquipment()
{
    ServerCycleEquipment();
}

void USPTacticalEquipmentComponent::DeploySelected(const FVector& TargetLocation)
{
    ServerDeploySelected(TargetLocation);
}

void USPTacticalEquipmentComponent::ServerCycleEquipment_Implementation()
{
    SelectedEquipment = SelectedEquipment==ESPTacticalEquipment::FlashGrenade ? ESPTacticalEquipment::SmokeGrenade : ESPTacticalEquipment::FlashGrenade;
}

void USPTacticalEquipmentComponent::ServerDeploySelected_Implementation(FVector_NetQuantize TargetLocation)
{
    AActor* OwnerActor=GetOwner(); if(!OwnerActor) return;
    const FVector Origin=OwnerActor->GetActorLocation();
    FVector SafeTarget=TargetLocation;
    if(FVector::DistSquared(Origin,SafeTarget)>FMath::Square(MaxThrowDistance))
        SafeTarget=Origin+(SafeTarget-Origin).GetSafeNormal()*MaxThrowDistance;

    if(SelectedEquipment==ESPTacticalEquipment::FlashGrenade)
    {
        if(FlashGrenades<=0) return;
        --FlashGrenades;
    }
    else
    {
        if(SmokeGrenades<=0) return;
        --SmokeGrenades;
    }
    if(ProjectileClass)
    {
        const FVector Dir=(SafeTarget-Origin).GetSafeNormal();
        FActorSpawnParameters Params; Params.Owner=OwnerActor; Params.Instigator=Cast<APawn>(OwnerActor);
        if(ASPTacticalProjectile* Projectile=GetWorld()->SpawnActor<ASPTacticalProjectile>(ProjectileClass,Origin+Dir*60.f,Dir.Rotation(),Params))
            Projectile->Arm(SelectedEquipment,Dir*ThrowSpeed+FVector::UpVector*260.f);
    }
    MulticastEquipmentDeployed(SelectedEquipment,SafeTarget);
}

void USPTacticalEquipmentComponent::MulticastEquipmentDeployed_Implementation(ESPTacticalEquipment Equipment, FVector_NetQuantize TargetLocation)
{
    BP_OnEquipmentDeployed(Equipment,TargetLocation);
}

void USPTacticalEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USPTacticalEquipmentComponent,SelectedEquipment);
    DOREPLIFETIME(USPTacticalEquipmentComponent,FlashGrenades);
    DOREPLIFETIME(USPTacticalEquipmentComponent,SmokeGrenades);
}
