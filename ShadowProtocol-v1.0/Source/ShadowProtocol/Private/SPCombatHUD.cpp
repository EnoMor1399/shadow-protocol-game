#include "SPCombatHUD.h"
#include "SPControlSettings.h"
#include "SPObserverPlayerController.h"
#include "SPCharacter.h"
#include "SPHealthComponent.h"
#include "SPWeaponBase.h"
#include "SPProtocolGameState.h"
#include "SPTacticalEquipmentComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void ASPCombatHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* PC = Cast<ASPObserverPlayerController>(PlayerOwner);
    const auto* GS = GetWorld() ? GetWorld()->GetGameState<ASPProtocolGameState>() : nullptr;
    if (!GEngine || !Canvas || !PC || !PC->IsLocalController() || PC->IsGameplayInputBlocked() || !GS) return;
    const float Scale = FMath::Max(0.1f, FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f));
    const float W = Canvas->ClipX / Scale, H = Canvas->ClipY / Scale;
    const auto* Settings = GetDefault<USPControlSettings>();
    const FLinearColor Panel = Settings->bHighContrastHUD ? FLinearColor(0, 0, 0, 1) : FLinearColor(0.012f, 0.022f, 0.035f, 0.9f);
    const FLinearColor Accent = Settings->bHighContrastHUD ? FLinearColor(1.f, 0.9f, 0.2f) : FLinearColor(0.35f, 0.85f, 0.8f);
    const auto Text = [&](const FString& Value, float X, float Y, FLinearColor Color, float Size = 1.f)
    { DrawText(Value, Color, X * Scale, Y * Scale, GEngine->GetMediumFont(), Size * Scale, false); };
    const auto Rect = [&](float X, float Y, float Width, float Height, FLinearColor Color)
    { DrawRect(Color, X * Scale, Y * Scale, Width * Scale, Height * Scale); };
    const TCHAR* Phase = GS->bMatchComplete ? TEXT("MATCH COMPLETE")
        : GS->RoundState == ESPRoundState::Preparation ? TEXT("PREPARE")
        : GS->RoundState == ESPRoundState::Overtime ? TEXT("OVERTIME")
        : GS->RoundState == ESPRoundState::PostRound ? TEXT("ROUND COMPLETE") : TEXT("PROTOCOL");
    const int32 Seconds = FMath::Max(0, FMath::CeilToInt(GS->RoundTimeRemaining));
    Rect(W / 2 - 240, 20, 480, 80, Panel);
    Text(FString::Printf(TEXT("D9  %d   |   ROUND %d   |   %d  HELIX"), GS->DirectorateRoundWins, GS->RoundNumber, GS->HelixRoundWins), W / 2 - 215, 30, FLinearColor::White);
    Text(FString::Printf(TEXT("%s   %02d:%02d"), Phase, Seconds / 60, Seconds % 60), W / 2 - 215, 64, Accent);
    if (Settings->bShowHUDHints) Text(TEXT("Esc  Controls & HUD settings"), 24, 112, FLinearColor::White, 0.85f);
    const TCHAR* Objective = GS->bMatchComplete ? TEXT("Match finished")
        : GS->RoundState == ESPRoundState::Waiting ? TEXT("Awaiting team readiness")
        : GS->RoundState == ESPRoundState::PostRound ? TEXT("Round finished / Await next round")
        : GS->RoundState == ESPRoundState::Preparation ? TEXT("Prepare for deployment")
        : GS->bObjectiveSecured ? TEXT("Extraction active")
        : GS->bTrueObjectiveRevealed ? TEXT("Objective identified") : TEXT("Locate intelligence");
    Rect(W / 2 - 240, 108, 480, 34, Panel);
    Text(Objective, W / 2 - 215, 114, Accent, 0.9f);
    const auto* Pawn = Cast<ASPCharacter>(PC->GetPawn());
    if (!Pawn || !Pawn->Health)
    {
        Text(TEXT("Waiting for player deployment..."), 24, H - 64, Accent);
        return;
    }
    Rect(24, H - 124, 300, 100, Panel);
    const bool bDown = Pawn->Health->bDowned;
    const bool bAlive = Pawn->Health->IsAlive();
    Text(bDown ? TEXT("DOWNED / Awaiting assistance") : !bAlive ? TEXT("ELIMINATED")
        : FString::Printf(TEXT("HEALTH  %d"), FMath::CeilToInt(Pawn->Health->Health)), 38, H - 112,
        bDown || !bAlive ? FLinearColor(1.f, 0.35f, 0.3f) : FLinearColor::White);
    Rect(38, H - 78, 272, 6, FLinearColor(0.12f, 0.16f, 0.2f));
    Rect(38, H - 78, 272 * FMath::Clamp(Pawn->Health->Health / FMath::Max(1.f, Pawn->Health->MaxHealth), 0.f, 1.f), 6, Accent);
    Text(FString::Printf(TEXT("STAMINA  %d   /   %s"), FMath::RoundToInt(Pawn->Stamina), Pawn->bSprinting ? TEXT("SPRINT") : Pawn->bSilentMovement ? TEXT("SILENT") : Pawn->bAiming ? TEXT("AIM") : TEXT("WALK")), 38, H - 61, FLinearColor::White, 0.8f);
    Rect(W - 344, H - 124, 320, 100, Panel);
    Text(Pawn->EquippedWeapon ? FString::Printf(TEXT("AMMO  %02d / %02d"), Pawn->EquippedWeapon->AmmoInMagazine, Pawn->EquippedWeapon->MagazineSize)
        : TEXT("NO WEAPON EQUIPPED"), W - 328, H - 112, FLinearColor::White);
    if (Pawn->TacticalEquipment)
    {
        const bool bSmoke = Pawn->TacticalEquipment->SelectedEquipment == ESPTacticalEquipment::SmokeGrenade;
        Text(FString::Printf(TEXT("EQUIPMENT  %s  %d"), bSmoke ? TEXT("SMOKE") : TEXT("FLASH"),
            bSmoke ? Pawn->TacticalEquipment->SmokeGrenades : Pawn->TacticalEquipment->FlashGrenades), W - 328, H - 70, Accent, 0.8f);
    }
    if (Settings->bShowCrosshair && bAlive && !bDown && Pawn->EquippedWeapon && !Pawn->bSprinting && !GS->bMatchComplete)
    {
        const float ReticleScale = Scale * Settings->GetSafeCrosshairScale();
        const float X = Canvas->ClipX / 2, Y = Canvas->ClipY / 2, Gap = (Pawn->bAiming ? 3.f : 6.f) * ReticleScale;
        const auto ReticleLine = [&](float X1, float Y1, float X2, float Y2)
        {
            DrawLine(X1, Y1, X2, Y2, FLinearColor::Black, ReticleScale + 2.f);
            DrawLine(X1, Y1, X2, Y2, Accent, ReticleScale);
        };
        ReticleLine(X - Gap - 6 * ReticleScale, Y, X - Gap, Y);
        ReticleLine(X + Gap, Y, X + Gap + 6 * ReticleScale, Y);
        ReticleLine(X, Y - Gap - 6 * ReticleScale, X, Y - Gap);
        ReticleLine(X, Y + Gap, X, Y + Gap + 6 * ReticleScale);
    }
}
