#include "SPTacticalProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ASPTacticalProjectile::ASPTacticalProjectile()
{
    bReplicates=true;
    SetReplicateMovement(true);
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(7.f);
    Collision->SetCollisionProfileName(TEXT("PhysicsActor"));
    RootComponent=Collision;
    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->bShouldBounce=true;
    Movement->Bounciness=.34f;
    Movement->Friction=.6f;
    Movement->ProjectileGravityScale=1.f;
    Movement->InitialSpeed=1150.f;
    Movement->MaxSpeed=1450.f;
}

void ASPTacticalProjectile::BeginPlay()
{
    Super::BeginPlay();
    if(HasAuthority()) GetWorldTimerManager().SetTimer(FuseHandle,this,&ASPTacticalProjectile::Detonate,FuseSeconds,false);
}

void ASPTacticalProjectile::Arm(ESPTacticalEquipment InType,const FVector& InitialVelocity)
{
    if(!HasAuthority()) return;
    EquipmentType=InType;
    Movement->Velocity=InitialVelocity;
}

void ASPTacticalProjectile::Detonate()
{
    if(!HasAuthority()) return;
    MulticastDetonate(EquipmentType,GetActorLocation(),EffectRadius);
    Destroy();
}

void ASPTacticalProjectile::MulticastDetonate_Implementation(ESPTacticalEquipment Type,FVector_NetQuantize Location,float Radius)
{
    BP_OnDetonated(Type,Location,Radius);
}

void ASPTacticalProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPTacticalProjectile,EquipmentType);
}
