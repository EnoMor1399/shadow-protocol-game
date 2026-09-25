#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/PlayerInput.h"
#include "SPControlSettings.generated.h"

/** Machine-local preferences; no session credentials or gameplay authority. */
UCLASS(Config=GameUserSettings)
class SHADOWPROTOCOL_API USPControlSettings : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config) float MouseSensitivity = 1.f;
    UPROPERTY(Config) float AimSensitivityMultiplier = 1.f;
    float GetSafeAimSensitivityMultiplier() const;
    UPROPERTY(Config) bool bInvertMouseY = false;
    UPROPERTY(Config) bool bToggleAim = false;
    UPROPERTY(Config) bool bHighContrastHUD = false;
    UPROPERTY(Config) bool bShowCrosshair = true;
    UPROPERTY(Config) bool bShowHUDHints = true;
    UPROPERTY(Config) bool bToggleScoreboard = false;
    UPROPERTY(Config) float CrosshairScale = 1.f;
    float GetSafeCrosshairScale() const;
    UPROPERTY(Config) TArray<FInputActionKeyMapping> ActionOverrides;
    // Empty uses project defaults; otherwise forward, backward, left, right.
    UPROPERTY(Config) TArray<FKey> MovementKeys;
    bool SetMovementKeys(const TArray<FKey>& Keys, FString& Error, bool bSaveSettings = true);
    // Validate and install action and movement overrides together.
    bool ApplyActionOverrides(FString& Error);
    bool RebindAction(FName Action, FKey Key, FString& Error);
    bool FindActionUsingKey(FKey Key, FName ExcludingAction, FName& OutAction) const;
    bool SwapActionBinding(FName Action, FName ConflictingAction, FKey NewKey, FString& Error, bool bSaveSettings = true);
    void ResetAllBindings(bool bSaveSettings = true);
    static FString GetActionKeyLabel(FName Action);
    static bool CanRebindAction(FName Action);
};
