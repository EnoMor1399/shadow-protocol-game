#include "SPSecurityDevice.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ASPSecurityDevice::ASPSecurityDevice()
{
    bReplicates = true;
    DeviceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceMesh"));
    SetRootComponent(DeviceMesh);
    Health = MaxHealth;
    Tags.Add(TEXT("SP_SecurityDevice"));
}

void ASPSecurityDevice::ApplyAuthoritativeDamage(float Amount, AController* InstigatorController)
{
    if(!HasAuthority() || bDestroyed || Amount<=0.f) return;
    Health = FMath::Max(0.f, Health-Amount);
    if(Health<=0.f){ bDestroyed=true; OnRep_Destroyed(); }
}
void ASPSecurityDevice::OnRep_Destroyed(){ if(bDestroyed) BP_OnSecurityDeviceDestroyed(DeviceType); }
void ASPSecurityDevice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out); DOREPLIFETIME(ASPSecurityDevice, bDestroyed); DOREPLIFETIME(ASPSecurityDevice, Health);
}
