import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';

async function source(relativePath) {
  return readFile(new URL(relativePath, import.meta.url), 'utf8');
}

test('Unreal allocation travel carries only the one-time admission envelope', async () => {
  const sessionHeader = await source('../../Source/ShadowProtocol/Public/SPBackendSessionSubsystem.h');
  const sessionCpp = await source('../../Source/ShadowProtocol/Private/SPBackendSessionSubsystem.cpp');

  assert.match(sessionHeader, /BuildAllocationTravelUrl/);
  assert.match(sessionHeader, /ConnectToAllocation/);
  assert.match(sessionCpp, /spAllocationId=/);
  assert.match(sessionCpp, /spMatchId=/);
  assert.match(sessionCpp, /spConnectToken=/);
  assert.match(sessionCpp, /spServerId=/);
  assert.match(sessionCpp, /spNetworkBuild=/);
  assert.match(sessionCpp, /ClientTravel\(TravelUrl,\s*TRAVEL_Absolute\)/);

  assert.doesNotMatch(sessionCpp, /SESSION_BOOTSTRAP_SECRET\s*=/);
  assert.doesNotMatch(sessionCpp, /SERVER_REGISTRATION_SECRET\s*=/);
  assert.doesNotMatch(sessionCpp, /ORCHESTRATOR_ATTESTATION_SECRET\s*=/);
});

test('dedicated-server admission callbacks are allocation correlated', async () => {
  const serverHeader = await source('../../Source/ShadowProtocol/Public/SPDedicatedServerBackendSubsystem.h');
  const serverCpp = await source('../../Source/ShadowProtocol/Private/SPDedicatedServerBackendSubsystem.cpp');

  assert.match(serverHeader, /FSPDedicatedServerAdmissionFailed/);
  assert.match(serverHeader, /OnAdmissionFailed/);
  assert.match(serverCpp, /HandleAdmissionResponse[\s\S]*AllocationId[\s\S]*MatchId/);
  assert.match(serverCpp, /Admission\.AllocationId\s*!=\s*AllocationId/);
  assert.match(serverCpp, /Admission\.MatchId\s*!=\s*MatchId/);
  assert.match(serverCpp, /OnAdmissionFailed\.Broadcast\(AllocationId,\s*MatchId/);
});

test('Protocol GameMode gates spawn and competitive slots until backend admission', async () => {
  const gameModeHeader = await source('../../Source/ShadowProtocol/Public/SPProtocolGameMode.h');
  const gameModeCpp = await source('../../Source/ShadowProtocol/Private/SPProtocolGameMode.cpp');
  const playerStateHeader = await source('../../Source/ShadowProtocol/Public/SPPlayerState.h');
  const playerStateCpp = await source('../../Source/ShadowProtocol/Private/SPPlayerState.cpp');

  assert.match(gameModeHeader, /#include "SPDedicatedServerBackendSubsystem\.h"/);
  assert.match(gameModeHeader, /PendingAdmissionTimeoutSeconds/);
  assert.match(gameModeCpp, /ASPProtocolGameMode::PreLogin/);
  assert.match(gameModeCpp, /ASPProtocolGameMode::InitNewPlayer/);
  assert.match(gameModeCpp, /ASPProtocolGameMode::HandleStartingNewPlayer_Implementation/);
  assert.match(gameModeCpp, /FindPendingAdmissionForController\(NewPlayer/);
  assert.match(gameModeCpp, /Backend->AdmitConnection\(Pending->AllocationId,\s*Pending->MatchId,\s*ConnectToken,\s*Pending->RequestId,\s*Pending->ReconnectGrantId,\s*GetGameState<ASPProtocolGameState>\(\)->RoundNumber\)/);
  assert.match(gameModeCpp, /Pending->ConnectToken\.Reset\(\)/);
  assert.match(gameModeCpp, /HandleBackendAdmissionCompleted/);
  assert.match(gameModeCpp, /HandleBackendAdmissionFailed/);
  assert.match(gameModeCpp, /Super::HandleStartingNewPlayer_Implementation\(PlayerController\)/);
  assert.match(gameModeCpp, /if \(bRequireAuthenticatedSessions && !PS->bSessionAuthenticated\) continue;/);
  assert.match(gameModeCpp, /GameSession->KickPlayer\(PlayerController,\s*ReasonText\)/);

  assert.match(playerStateHeader, /AuthenticatedUserId/);
  assert.match(playerStateCpp, /DOREPLIFETIME\(ASPPlayerState, AuthenticatedUserId\)/);
});

test('dedicated build wires the NULL provider and GameMode-owned online session', async () => {
  const project = JSON.parse(await source('../../ShadowProtocol.uproject'));
  assert.ok(project.Plugins.some(p => p.Name === 'OnlineSubsystemNull' && p.Enabled));
  assert.match(await source('../../Source/ShadowProtocolServer.Target.cs'), /Type = TargetType.Server/);
  const mode = await source('../../Source/ShadowProtocol/Private/SPProtocolGameMode.cpp');
  assert.match(mode, /GameSessionClass=ASPOnlineGameSession::StaticClass\(\)/);
  assert.match(mode, /DefaultPawnClass=ASPCharacter::StaticClass\(\)/);
  const preparation = mode.split('void ASPProtocolGameMode::BeginPreparation()')[1].split('void ASPProtocolGameMode::BeginDeployment()')[0];
  assert.ok(preparation.indexOf('StartProtocolSession()') < preparation.indexOf('ResetRoundState()'));
  assert.match(mode, /OnlineSession->EndProtocolSession\(\)/);
});

test('OSS source contract keeps admission private and bounds asynchronous operations', async () => {
  const session = await source('../../Source/ShadowProtocol/Private/SPOnlineGameSession.cpp');
  assert.match(session, /Online::GetSessionInterface\(GetWorld\(\)\)/);
  assert.match(session, /Settings.bShouldAdvertise = false/);
  assert.match(session, /Settings.bAllowInvites = false/);
  for (const operation of ['Create', 'Start', 'End']) {
    assert.match(session, new RegExp(`Sessions->${operation}Session\\(`));
    assert.match(session, new RegExp(`ClearOn${operation}SessionCompleteDelegate_Handle`));
  }
  assert.match(session, /FPlatformTime::Seconds\(\) >= OperationDeadline/);
  assert.match(session, /Backend->MarkDraining\(\)/);
  assert.match(session, /bOwnsSession && Sessions->GetNamedSession/);
  assert.doesNotMatch(session, /Settings\.Set\([^;]*(?:ConnectToken|NodeCredential|SessionToken)/);
});

test('late admissions cannot bypass timeout, provider loss or dedicated identity authority', async () => {
  const mode = await source('../../Source/ShadowProtocol/Private/SPProtocolGameMode.cpp');
  const promotion = mode.split('void ASPProtocolGameMode::PromoteAdmittedPlayer(')[1].split('void ASPProtocolGameMode::HandleBackendAdmissionFailed')[0];
  assert.match(promotion, /Pending->DeadlineRealSeconds <= FPlatformTime::Seconds\(\)/);
  assert.match(promotion, /!OnlineSession->IsAcceptingAdmissions\(\)/);
  assert.match(promotion, /Backend->IsDraining\(\)/);
  assert.ok(promotion.indexOf('DeadlineRealSeconds') < promotion.indexOf('PS->bSessionAuthenticated = true'));
  assert.match(mode, /if\(IsDedicatedAdmissionRequired\(\) \|\| !Player \|\| SessionId.Len\(\)<8\) return false/);
  assert.match(mode, /if \(GameSession && GameSession->KickPlayer\(PlayerController, ReasonText\)\) return/);
});

test('ready-room RPCs derive identity from their owning controller', async () => {
  const header = await source('../../Source/ShadowProtocol/Public/SPObserverPlayerController.h');
  const controller = await source('../../Source/ShadowProtocol/Private/SPObserverPlayerController.cpp');
  assert.match(header, /UFUNCTION\(Server, Reliable\) void ServerSetReadyState\(bool bReady, int32 RequestId\)/);
  assert.match(header, /UFUNCTION\(Server, Reliable\) void ServerSelectSpawnGroup\(FName SpawnGroupId, int32 RequestId\)/);
  assert.match(controller, /Mode->SetPlayerReady\(GetPlayerState<ASPPlayerState>\(\), bReady\)/);
  assert.match(controller, /Mode->SelectSpawnGroup\(GetPlayerState<ASPPlayerState>\(\), SpawnGroupId\)/);
  assert.match(controller, /Now < NextReadyRoomRequestSeconds/);
  assert.equal((controller.match(/if \(!ConsumeReadyRoomRequest\(\)\)/g) || []).length, 2);
  assert.doesNotMatch(header, /Server(?:SetReadyState|SelectSpawnGroup)\([^)]*(?:PlayerState|UserId|SessionId)/);
});

test('ready-room mutations are planning-only and require admitted connected players', async () => {
  const mode = await source('../../Source/ShadowProtocol/Private/SPProtocolGameMode.cpp');
  const gate = mode.split('bool ASPProtocolGameMode::CanEditReadyRoom(')[1].split('bool ASPProtocolGameMode::SetPlayerReady(')[0];
  for (const guard of ['HasAuthority()', 'PlayerArray.Contains(Player)', 'bMatchComplete',
    'ESPMatchPhase::Planning', 'ESPRoundState::Waiting', 'ESPConnectionState::Connected',
    'ESPTeam::None', 'bSessionAuthenticated', 'Backend->IsDraining()', 'IsAcceptingAdmissions()'])
    assert.ok(gate.includes(guard), `missing guard: ${guard}`);
  const ready = mode.split('bool ASPProtocolGameMode::SetPlayerReady(')[1].split('bool ASPProtocolGameMode::SelectSpawnGroup(')[0];
  assert.ok(ready.indexOf('CanEditReadyRoom(Player)') < ready.indexOf('Player->bReady=bReady'));
  const spawn = mode.split('bool ASPProtocolGameMode::SelectSpawnGroup(')[1].split('bool ASPProtocolGameMode::CanStartCompetitiveMatch(')[0];
  assert.match(spawn, /CanEditReadyRoom\(Player\)/);
  assert.match(spawn, /Group.Team==Player->Team/);
  assert.match(spawn, /Player->bReady=false/);
  const preparation = mode.split('void ASPProtocolGameMode::BeginPreparation()')[1].split('void ASPProtocolGameMode::BeginDeployment()')[0];
  assert.ok(preparation.indexOf('!CanStartCompetitiveMatch()') < preparation.indexOf('StartProtocolSession()'));
});

test('native ready-room UI consumes replicated state without granting local readiness', async () => {
  const widget = await source('../../Source/ShadowProtocol/Private/SPReadyRoomWidget.cpp');
  const controller = await source('../../Source/ShadowProtocol/Private/SPObserverPlayerController.cpp');
  assert.match(widget, /Player->bSessionAuthenticated/);
  assert.match(widget, /ReadyButton->SetIsEnabled\(bEditable\)/);
  assert.match(widget, /Controller->RequestReadyState\(!Player->bReady\)/);
  assert.doesNotMatch(widget, /Player->bReady\s*=/);
  assert.match(controller, /IsLocalController\(\) && GetNetMode\(\) != NM_DedicatedServer/);
  assert.match(controller, /FInputModeUIOnly/);
  assert.match(controller, /FInputModeGameOnly/);
  assert.match(controller, /ReadyRoomWidget->RemoveFromParent\(\)/);
});

test('client session lifetime is checked using UTC and monotonic time', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPBackendSessionSubsystem.cpp');
  const header = await source('../../Source/ShadowProtocol/Public/SPBackendSessionSubsystem.h');
  assert.match(header, /HasAuthenticatedSession\(\) const \{ return GetSessionSecondsRemaining\(\) > 0.0f;/);
  assert.match(cpp, /FDateTime::ParseIso8601/);
  assert.match(cpp, /if \(Remaining <= 0.0\) return false/);
  assert.match(cpp, /FMath::Min\(\(SessionExpiryUtc - FDateTime::UtcNow\(\)\).GetTotalSeconds\(\),\s*SessionExpiryMonotonic - FPlatformTime::Seconds\(\)\)/);
  assert.match(cpp, /!SetSessionExpiry\(InExpiresAt\)/);
  assert.match(cpp, /!SetSessionExpiry\(RefreshedExpiresAt\)/);
  assert.match(cpp, /RemoveTicker\(ExpiryTicker\)/);
  assert.match(cpp, /OnSessionExpired.Broadcast/);
});

test('authenticated callbacks cannot restore a cleared or replaced session', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPBackendSessionSubsystem.cpp');
  for (const method of ['SessionRefresh', 'Allocation', 'ReconnectTicket', 'Reconnect']) {
    const handler = cpp.split(`void USPBackendSessionSubsystem::Handle${method}Response(`)[1];
    assert.ok(handler.trim().split('\n')[2].includes('ConsumeAuthenticatedResponse(Request)'), method);
  }
  assert.match(cpp, /ActiveAuthenticatedRequests.Remove\(Request\) == 0/);
  assert.match(cpp, /Request->GetHeader\(TEXT\("Authorization"\)\) == TEXT\("Bearer "\) \+ SessionToken/);
  assert.match(cpp, /ClearAuthenticatedSession\(\)\s*\{\s*CancelAuthenticatedRequests\(\)/);
  assert.match(cpp, /OnProcessRequestComplete\(\).Unbind\(\);\s*Request->CancelRequest\(\)/);
  assert.match(cpp, /if \(bRefreshPending\) return/);
  assert.match(cpp, /Request->SetTimeout\(15.0f\)/);
  assert.match(cpp, /if \(Request != CompatibilityRequest\) return/);
});

test('expiry UI and ticker handles use the intended Unreal contracts', async () => {
  const widget = await source('../../Source/ShadowProtocol/Private/SPReadyRoomWidget.cpp');
  assert.match(widget, /Backend->HasExpiredSession\(\)/);
  for (const name of ['SPBackendSessionSubsystem', 'SPDedicatedServerBackendSubsystem']) {
    const header = await source(`../../Source/ShadowProtocol/Public/${name}.h`);
    assert.match(header, /#include "Containers\/Ticker.h"/);
    assert.doesNotMatch(header, /(?<!::)\bFDelegateHandle\s+\w*Ticker\w*;/);
    assert.match(header, /FTSTicker::FDelegateHandle/);
  }
});

test('allocation travel uses the compiled host validator and checks expiry again at travel', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPBackendSessionSubsystem.cpp');
  const travel = cpp.split('FString USPBackendSessionSubsystem::BuildAllocationTravelUrl(')[1].split('void USPBackendSessionSubsystem::RequestReconnectTicket(')[0];
  assert.match(travel, /SPTravelValidation::IsValidHost/);
  assert.match(travel, /AllocationExpiry <= FDateTime::UtcNow\(\)/);
  assert.match(travel, /IsNetworkBuildCompatible\(Allocation.NetworkBuild, true\)/);
  assert.match(travel, /Allocation.ConnectToken.Len\(\) > 256/);
  assert.match(travel, /PlayerController->GetGameInstance\(\) != GetGameInstance\(\)/);
  assert.match(travel, /CanUseAuthenticatedMatchEndpoint\(TEXT\("travel"\)\)/);
  assert.match(cpp, /ConnectPort == static_cast<double>\(static_cast<int32>\(ConnectPort\)\)/);
});

test('engine failure reporting is scoped and does not forward raw token-bearing errors', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPBackendSessionSubsystem.cpp');
  for (const type of ['Network', 'Travel']) {
    assert.match(cpp, new RegExp(`On${type}Failure\\(\\).AddUObject`));
    assert.match(cpp, new RegExp(`On${type}Failure\\(\\).Remove\\(${type}FailureHandle\\)`));
  }
  const handlers = cpp.split('void USPBackendSessionSubsystem::HandleNetworkFailure(')[1].split('bool USPBackendSessionSubsystem::SetSessionExpiry')[0];
  assert.equal((handlers.match(/World->GetGameInstance\(\) != GetGameInstance\(\)/g) || []).length, 2);
  assert.doesNotMatch(handlers, /const FString&\s+\w+/);
  assert.match(handlers, /ReportConnectionFailure\(TEXT\("network"\)/);
  assert.match(handlers, /ReportConnectionFailure\(TEXT\("travel"\)/);
});

test('reconnect travel and promotion require a fresh grant and matching local reservation', async () => {
  const client = await source('../../Source/ShadowProtocol/Private/SPBackendSessionSubsystem.cpp');
  const mode = await source('../../Source/ShadowProtocol/Private/SPProtocolGameMode.cpp');
  const server = await source('../../Source/ShadowProtocol/Private/SPDedicatedServerBackendSubsystem.cpp');
  assert.match(client, /RequestReconnectAllocation/);
  assert.match(client, /\/v1\/matches\/reconnect-allocation/);
  assert.match(client, /spReconnectGrantId=/);
  assert.match(server, /Admission.RequestId = RequestId/);
  assert.match(server, /Admission.ReconnectGrantId != ReconnectGrantId/);
  assert.match(mode, /Pending->RequestId != Admission.RequestId/);
  assert.match(mode, /Pending->RequestId != RequestId/);
  const promote = mode.split('void ASPProtocolGameMode::PromoteAdmittedPlayer')[1].split('void ASPProtocolGameMode::HandleBackendAdmissionFailed')[0];
  for (const guard of ['ReservedMatch', 'Admission.RoundNumber', 'Admission.SlotIndex', 'ESPMatchPhase::Planning', 'ESPConnectionState::Reconnecting', 'Deadline']) assert.ok(promote.includes(guard));
  assert.match(promote, /PS->Team = Reserved->Team/);
  assert.match(promote, /PS->SelectedSpawnGroup = Reserved->SpawnGroup/);
  assert.match(promote, /PS->bReady = false/);
});

test('controls modal releases held input and preserves paired controller ignore state', async () => {
  const controller = await source('../../Source/ShadowProtocol/Private/SPObserverPlayerController.cpp');
  const pawn = await source('../../Source/ShadowProtocol/Private/SPCharacter.cpp');
  const widget = await source('../../Source/ShadowProtocol/Private/SPControlsWidget.cpp');
  const mode = controller.split('void ASPObserverPlayerController::ApplyInterfaceInputMode()')[1];
  assert.ok(mode.indexOf('ReleaseHeldControls()') < mode.indexOf('SetInputMode(Mode)'));
  assert.match(mode, /FlushPressedKeys/);
  assert.match(mode, /bInterfaceInputIgnored != bModal/);
  assert.doesNotMatch(mode, /ResetIgnore/);
  assert.match(mode, /FInputModeGameOnly/);
  assert.match(controller, /SaveControlSettings/);
  assert.match(controller, /FMath::IsFinite\(Value\)/);
  assert.match(pawn, /HeldLean.Reset\(\)/);
  assert.match(pawn, /PC->IsGameplayInputBlocked\(\)/);
  assert.match(widget, /NativeOnPreviewKeyDown/);
  assert.match(widget, /EKeys::Escape && !Event.IsRepeat/);
  assert.match(widget, /GetActionMappings/);
});

test('combat HUD reads live replicated state and hides behind modal UI', async () => {
  const hud = await source('../../Source/ShadowProtocol/Private/SPCombatHUD.cpp');
  const mode = await source('../../Source/ShadowProtocol/Private/SPProtocolGameMode.cpp');
  assert.match(mode, /HUDClass=ASPCombatHUD::StaticClass/);
  assert.match(hud, /PC->IsGameplayInputBlocked\(\)/);
  for (const state of ['RoundTimeRemaining', 'AmmoInMagazine', 'Health', 'Stamina', 'SelectedEquipment']) assert.ok(hud.includes(state));
  assert.doesNotMatch(hud, /Server[A-Za-z]+\(/);
  assert.match(hud, /NO WEAPON EQUIPPED/);
});


test('spawn selector is owner-scoped, server filtered and request correlated', async () => {
  const mode = await source('../../Source/ShadowProtocol/Private/SPProtocolGameMode.cpp');
  const controller = await source('../../Source/ShadowProtocol/Private/SPObserverPlayerController.cpp');
  const widget = await source('../../Source/ShadowProtocol/Private/SPReadyRoomWidget.cpp');
  const choices = mode.split('TArray<FName> ASPProtocolGameMode::GetReadyRoomSpawnGroups')[1].split('bool ASPProtocolGameMode::SetPlayerReady')[0];
  assert.match(choices, /CanEditReadyRoom\(Player\)/);
  assert.match(choices, /Group.Team == Player->Team/);
  assert.match(choices, /AddUnique/);
  assert.match(controller, /AvailableSpawnGroups,COND_OwnerOnly/);
  assert.match(controller, /RequestId != PendingReadyRoomRequestId/);
  assert.match(controller, /FPlatformTime::Seconds\(\) >= ReadyRoomRequestDeadline/);
  assert.match(widget, /bSynchronizingSpawnChoice \|\| SelectionType == ESelectInfo::Direct/);
  assert.match(widget, /!Controller->IsReadyRoomRequestPending\(\)/);
  assert.match(widget, /PC->RequestSpawnGroup\(Group\)/);
  assert.doesNotMatch(widget, /Player->SelectedSpawnGroup\s*=/);
});

test('action rebinding validates candidates and confirms atomic swaps before changing live mappings', async () => {
  const settingsHeader = await source('../../Source/ShadowProtocol/Public/SPControlSettings.h');
  const settings = await source('../../Source/ShadowProtocol/Private/SPControlSettings.cpp');
  const widgetHeader = await source('../../Source/ShadowProtocol/Public/SPControlsWidget.h');
  const widget = await source('../../Source/ShadowProtocol/Private/SPControlsWidget.cpp');
  const apply = settings.split('bool USPControlSettings::ApplyActionOverrides')[1].split('bool USPControlSettings::RebindAction')[0];
  for (const guard of ['CanRebindAction', 'Seen.Contains', 'EKeys::Escape', 'EKeys::Tab', 'ConsoleKeys', 'GetAxisMappings', 'Existing.Key == Override.Key']) assert.ok(apply.includes(guard));
  assert.ok(apply.indexOf('Candidate.RemoveAll') < apply.indexOf('Candidate.Add(Override)'));
  assert.ok(apply.lastIndexOf('return false') < apply.indexOf('Install(Candidate)'));
  assert.doesNotMatch(settings, /SaveKeyMappings|Input->SaveConfig/);
  assert.match(settings, /ActionOverrides = Previous; return false/);

  assert.match(settingsHeader, /FindActionUsingKey/);
  assert.match(settingsHeader, /SwapActionBinding/);
  const swap = settings.split('bool USPControlSettings::SwapActionBinding')[1].split('void USPControlSettings::ResetActionBindings')[0];
  assert.match(swap, /FindActionUsingKey\(NewKey, Action, CurrentOwner\)/);
  assert.match(swap, /ActionOverrides = PreviousOverrides/);
  assert.ok(swap.indexOf('ActionOverrides.Add(FInputActionKeyMapping(Action, NewKey))') < swap.indexOf('ApplyActionOverrides(Error)'));
  assert.ok(swap.indexOf('ActionOverrides.Add(FInputActionKeyMapping(ConflictingAction, PreviousActionKey))') < swap.indexOf('ApplyActionOverrides(Error)'));

  assert.match(widgetHeader, /ConfirmSwapButton/);
  assert.match(widgetHeader, /PendingConflictAction/);
  assert.match(widget, /FindActionUsingKey\(Chord.Key, \*Action, ConflictAction\)/);
  assert.match(widget, /ConfirmSwapButton->SetIsEnabled\(true\)/);
  assert.match(widget, /ConfirmPendingSwap/);
  assert.match(widget, /SwapActionBinding\(/);
  assert.match(widget, /ClearPendingSwap\(\)/);
  assert.match(widget, /GetIsSelectingKey/);
  assert.match(widget, /if \(bSynchronizingBinding\) return/);
});


test('shared 5v5 admission preserves backend roster slot and team authority', async () => {
  const server = await source('../../Source/ShadowProtocol/Private/SPDedicatedServerBackendSubsystem.cpp');
  const header = await source('../../Source/ShadowProtocol/Public/SPDedicatedServerBackendSubsystem.h');
  const mode = await source('../../Source/ShadowProtocol/Private/SPProtocolGameMode.cpp');
  const playerHeader = await source('../../Source/ShadowProtocol/Public/SPPlayerState.h');
  assert.match(header, /FString Team;/);
  assert.match(header, /FString TacticalSide;/);
  assert.match(server, /TryGetStringField\(TEXT\("team"\), Admission.Team\)/);
  assert.match(server, /TryGetStringField\(TEXT\("tacticalSide"\), Admission.TacticalSide\)/);
  assert.match(playerHeader, /CompetitiveSlotIndex = INDEX_NONE/);
  assert.match(mode, /PS->CompetitiveSlotIndex = Admission.SlotIndex/);
  assert.match(mode, /Admission.Team.Equals\(TEXT\("DirectorateNine"\)/);
  assert.match(mode, /Admission.Team.Equals\(TEXT\("Helix"\)/);
  const promote = mode.split('void ASPProtocolGameMode::PromoteAdmittedPlayer')[1].split('void ASPProtocolGameMode::HandleBackendAdmissionFailed')[0];
  assert.doesNotMatch(promote, /AssignCompetitiveTeam\(PS\)/);
});
