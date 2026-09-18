#include "SPReadyRoomWidget.h"
#include "SPObserverPlayerController.h"
#include "SPPlayerState.h"
#include "SPProtocolGameState.h"
#include "SPBackendSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/World.h"

TSharedRef<SWidget> USPReadyRoomWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        auto* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ReadyPanel"));
        Panel->SetPadding(FMargin(32.0f));
        Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.055f, 0.96f));
        Panel->SetHorizontalAlignment(HAlign_Center);
        Panel->SetVerticalAlignment(VAlign_Center);
        auto* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ReadyContent"));
        Panel->SetContent(Column);
        StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReadyStatus"));
        FSlateFontInfo StatusFont = StatusText->GetFont();
        StatusFont.Size = 24;
        StatusText->SetFont(StatusFont);
        StatusText->SetJustification(ETextJustify::Center);
        Column->AddChildToVerticalBox(StatusText);
        ReadyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ReadyButton"));
        ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReadyLabel"));
        FSlateFontInfo ButtonFont = ButtonText->GetFont();
        ButtonFont.Size = 22;
        ButtonText->SetFont(ButtonFont);
        ReadyButton->SetContent(ButtonText);
        ReadyButton->OnClicked.AddUniqueDynamic(this, &USPReadyRoomWidget::ToggleReady);
        Column->AddChildToVerticalBox(ReadyButton);
        WidgetTree->RootWidget = Panel;
    }
    return Super::RebuildWidget();
}

void USPReadyRoomWidget::RefreshReadyRoom()
{
    if (!StatusText || !ReadyButton || !ButtonText) return;
    const auto* Controller = GetOwningPlayer<ASPObserverPlayerController>();
    const auto* Player = Controller ? Controller->GetPlayerState<ASPPlayerState>() : nullptr;
    const auto* GS = GetWorld() ? GetWorld()->GetGameState<ASPProtocolGameState>() : nullptr;
    const bool bAdmitted = Player && Player->bSessionAuthenticated
        && Player->ConnectionState == ESPConnectionState::Connected && Player->Team != ESPTeam::None;
    const bool bEditable = bAdmitted && GS && !GS->bMatchComplete
        && GS->MatchPhase == ESPMatchPhase::Planning && GS->RoundState == ESPRoundState::Waiting;
    ReadyButton->SetIsEnabled(bEditable);
    ButtonText->SetText(FText::FromString(Player && Player->bReady ? TEXT("Cancel ready") : TEXT("Ready")));
    FString Status = TEXT("SHADOW PROTOCOL\nAuthenticating admission...");
    if (bAdmitted && GS)
    {
        const TCHAR* Team = Player->Team == ESPTeam::DirectorateNine ? TEXT("Directorate Nine") : TEXT("Helix");
        Status = FString::Printf(TEXT("SHADOW PROTOCOL\n%s | Spawn: %s\nReady: %d / %d\n%s"),
            Team, *Player->SelectedSpawnGroup.ToString(), GS->ReadyPlayerCount, GS->ExpectedPlayerCount,
            GS->bAllPlayersReady ? TEXT("All players ready. Preparing match...") : TEXT("Waiting for all players to confirm."));
    }
    const auto* Instance = GetGameInstance();
    const auto* Backend = Instance ? Instance->GetSubsystem<USPBackendSessionSubsystem>() : nullptr;
    if (Backend && Backend->HasExpiredSession())
        Status += TEXT("\nBackend session expired. Sign in again before matchmaking or reconnect.");
    const FText NewText = FText::FromString(Status);
    if (!StatusText->GetText().EqualTo(NewText)) StatusText->SetText(NewText);
}

void USPReadyRoomWidget::ToggleReady()
{
    auto* Controller = GetOwningPlayer<ASPObserverPlayerController>();
    const auto* Player = Controller ? Controller->GetPlayerState<ASPPlayerState>() : nullptr;
    if (Player && ReadyButton && ReadyButton->GetIsEnabled()) Controller->RequestReadyState(!Player->bReady);
}
