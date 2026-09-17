#include "SPEnvironmentZone.h"
#include "Components/BoxComponent.h"
ASPEnvironmentZone::ASPEnvironmentZone(){PrimaryActorTick.bCanEverTick=false;Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));RootComponent=Bounds;Bounds->SetCollisionProfileName(TEXT("Trigger"));}
