#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SPControlSettings.generated.h"

/** Machine-local preferences; no session credentials or gameplay authority. */
UCLASS(Config=GameUserSettings)
class SHADOWPROTOCOL_API USPControlSettings : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config) float MouseSensitivity = 1.f;
    UPROPERTY(Config) bool bInvertMouseY = false;
};
