#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPLagCompensationComponent.generated.h"

USTRUCT()
struct FSPLagFrame
{
    GENERATED_BODY()
    UPROPERTY() float ServerTime = 0.f;
    UPROPERTY() FVector Location = FVector::ZeroVector;
};

UCLASS(ClassGroup=(ShadowProtocol), meta=(BlueprintSpawnableComponent))
class SHADOWPROTOCOL_API USPLagCompensationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPLagCompensationComponent();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float HistorySeconds = 0.25f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float CaptureInterval = 0.05f;
    FVector SampleLocationAt(float ServerTime) const;
protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
private:
    UPROPERTY() TArray<FSPLagFrame> Frames;
    float CaptureAccumulator = 0.f;
};
