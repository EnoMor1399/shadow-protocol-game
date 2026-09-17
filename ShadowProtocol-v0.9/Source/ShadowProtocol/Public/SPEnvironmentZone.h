#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPEnvironmentZone.generated.h"
class UBoxComponent;
UENUM(BlueprintType) enum class ESPSurfaceProfile : uint8 { Concrete, Marble, Metal, Carpet, Gravel };
UCLASS()
class SHADOWPROTOCOL_API ASPEnvironmentZone : public AActor
{
    GENERATED_BODY()
public:
    ASPEnvironmentZone();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") FName ZoneName="Embassy Transit";
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") ESPSurfaceProfile Surface=ESPSurfaceProfile::Concrete;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") FName LightingProfile="Cool";
};
