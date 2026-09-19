#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
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
    SetIsFocusable(true);
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        auto* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ReadyPanel"));
        Panel->SetPadding(FMargin(32.0f));
        Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.055f, 0.96f));
        Panel->SetHorizontalAlignment(HAlign_Center);
        Panel->SetVerticalAlignment(VAlign_Fill);
        auto* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ReadyContent"));
        auto* Width = WidgetTree->ConstructWidget<USizeBox>();
        Width->SetMaxDesiredWidth(820.f);
        Panel->SetContent(Width);
        auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
        Width->SetContent(Scroll);
        Scroll->AddChild(Column);
        auto* Title = WidgetTree->ConstructWidget<UTextBlock>();
        Title->SetText(FText::FromString(TEXT("SHADOW PROTOCOL  /  DEPLOYMENT")));
        auto TitleFont = Title->GetFont(); TitleFont.Size = 30; Title->SetFont(TitleFont);
        Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.85f, 0.8f)));
        Title->SetAutoWrapText(true);
        Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 20));
        StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReadyStatus"));
        FSlateFontInfo StatusFont = StatusText->GetFont();
        StatusFont.Size = 20;
        StatusText->SetFont(StatusFont);
        StatusText->SetJustification(ETextJustify::Left);
        StatusText->SetAutoWrapText(true);
        Column->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0, 12));
        ReadyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ReadyButton"));
        ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReadyLabel"));
        FSlateFontInfo ButtonFont = ButtonText->GetFont();
        ButtonFont.Size = 22;
        ButtonText->SetFont(ButtonFont);
        ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
        ReadyButton->SetBackgroundColor(FLinearColor(0.35f, 0.85f, 0.8f));
        ReadyButton->SetContent(ButtonText);
        ReadyButton->OnClicked.AddUniqueDynamic(this, &USPReadyRoomWidget::ToggleReady);
        Column->AddChildToVerticalBox(ReadyButton)->SetPadding(FMargin(0, 16));
        auto* ControlsButton = WidgetTree->ConstructWidget<UButton>();
        auto* ControlsLabel = WidgetTree->ConstructWidget<UTextBlock>();
        ControlsLabel->SetText(FText::FromString(TEXT("Controls & mouse settings  [Esc]")));
        ControlsLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
        ControlsButton->SetContent(ControlsLabel);
        ControlsButton->OnClicked.AddUniqueDynamic(this, &USPReadyRoomWidget::OpenControls);
        Column->AddChildToVerticalBox(ControlsButton)->SetPadding(FMargin(0, 8));
        RosterText = WidgetTree->ConstructWidget<UTextBlock>();
        RosterText->SetAutoWrapText(true);
        auto RosterFont = RosterText->GetFont(); RosterFont.Size = 18; RosterText->SetFont(RosterFont);
        Column->AddChildToVerticalBox(RosterText)->SetPadding(FMargin(0, 24));
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
    FString Status = TEXT("Connecting securely...\nYour Ready button unlocks after server admission.");
    if (bAdmitted && GS)
    {
        const TCHAR* Team = Player->Team == ESPTeam::DirectorateNine ? TEXT("Directorate Nine") : TEXT("Helix");
        Status = FString::Printf(TEXT("%s  /  Spawn %s\nTeam readiness   %d / %d\n%s"),
            Team, *Player->SelectedSpawnGroup.ToString(), GS->ReadyPlayerCount, GS->ExpectedPlayerCount,
            GS->bAllPlayersReady ? TEXT("All players ready. Preparing match...") : TEXT("Waiting for all players to confirm."));
    }
    if (RosterText && GS)
    {
        FString Roster = TEXT("TEAM ROSTER\n");
        for (const auto& Slot : GS->PlayerSlots)
        {
            const TCHAR* State = Slot.ConnectionState == ESPConnectionState::Reconnecting ? TEXT("Reconnecting")
                : Slot.ConnectionState == ESPConnectionState::Disconnected ? TEXT("Disconnected")
                : Slot.bReady ? TEXT("Ready") : TEXT("Not ready");
            const TCHAR* Team = Slot.Team == ESPTeam::DirectorateNine ? TEXT("D9") : Slot.Team == ESPTeam::Helix ? TEXT("HELIX") : TEXT("--");
            FString Callsign = Slot.Callsign.Left(32).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" "));
            Roster += FString::Printf(TEXT("\n%02d   %s   %s   /   %s"), Slot.SlotIndex + 1, Team, *Callsign, State);
        }
        const FText NewRoster = FText::FromString(Roster);
        if (!RosterText->GetText().EqualTo(NewRoster)) RosterText->SetText(NewRoster);
    }
    const auto* Instance = GetGameInstance();
    const auto* Backend = Instance ? Instance->GetSubsystem<USPBackendSessionSubsystem>() : nullptr;
    if (Backend && Backend->HasExpiredSession())
        Status += TEXT("\nBackend session expired. Sign in again before matchmaking or reconnect.");
    if (Backend && !Backend->GetLastConnectionError().IsEmpty())
        Status += TEXT("\n") + Backend->GetLastConnectionError();
    const FText NewText = FText::FromString(Status);
    if (!StatusText->GetText().EqualTo(NewText)) StatusText->SetText(NewText);
}

void USPReadyRoomWidget::ToggleReady()
{
    auto* Controller = GetOwningPlayer<ASPObserverPlayerController>();
    const auto* Player = Controller ? Controller->GetPlayerState<ASPPlayerState>() : nullptr;
    if (Player && ReadyButton && ReadyButton->GetIsEnabled()) Controller->RequestReadyState(!Player->bReady);
}

void USPReadyRoomWidget::OpenControls()
{
    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>()) PC->ToggleControls();
}
FReply USPReadyRoomWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape && !Event.IsRepeat())
    {
        OpenControls();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
