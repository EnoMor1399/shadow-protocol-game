#include "SPLagCompensationComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

USPLagCompensationComponent::USPLagCompensationComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    SetIsReplicatedByDefault(false);
}
void USPLagCompensationComponent::BeginPlay()
{
    Super::BeginPlay();
    if(GetOwner() && GetOwner()->HasAuthority())
        Frames.Add({GetWorld()->GetTimeSeconds(),GetOwner()->GetActorLocation()});
}
void USPLagCompensationComponent::TickComponent(float DT,ELevelTick TickType,FActorComponentTickFunction* Fn)
{
    Super::TickComponent(DT,TickType,Fn);
    if(!GetOwner() || !GetOwner()->HasAuthority()) return;
    CaptureAccumulator+=DT;if(CaptureAccumulator<CaptureInterval)return;CaptureAccumulator=0.f;
    const float Now=GetWorld()->GetTimeSeconds();Frames.Add({Now,GetOwner()->GetActorLocation()});
    while(Frames.Num()>2 && Now-Frames[0].ServerTime>HistorySeconds)Frames.RemoveAt(0,1,EAllowShrinking::No);
}
FVector USPLagCompensationComponent::SampleLocationAt(float ServerTime) const
{
    if(Frames.Num()==0)return GetOwner()?GetOwner()->GetActorLocation():FVector::ZeroVector;
    if(ServerTime<=Frames[0].ServerTime)return Frames[0].Location;
    if(ServerTime>=Frames.Last().ServerTime)return Frames.Last().Location;
    for(int32 i=1;i<Frames.Num();++i)
    {
        if(Frames[i].ServerTime>=ServerTime)
        {
            const FSPLagFrame& A=Frames[i-1];const FSPLagFrame& B=Frames[i];
            const float Alpha=FMath::Clamp((ServerTime-A.ServerTime)/FMath::Max(KINDA_SMALL_NUMBER,B.ServerTime-A.ServerTime),0.f,1.f);
            return FMath::Lerp(A.Location,B.Location,Alpha);
        }
    }
    return Frames.Last().Location;
}
