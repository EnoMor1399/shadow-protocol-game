#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SPCombatHUD.generated.h"

/** Read-only presentation of replicated match/pawn state. */
UCLASS()
class SHADOWPROTOCOL_API ASPCombatHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
