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
  assert.match(gameModeCpp, /Backend->AdmitConnection\(Pending->AllocationId,\s*Pending->MatchId,\s*ConnectToken\)/);
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
  assert.match(header, /UFUNCTION\(Server, Reliable\) void ServerSetReadyState\(bool bReady\)/);
  assert.match(header, /UFUNCTION\(Server, Reliable\) void ServerSelectSpawnGroup\(FName SpawnGroupId\)/);
  assert.match(controller, /Mode->SetPlayerReady\(GetPlayerState<ASPPlayerState>\(\), bReady\)/);
  assert.match(controller, /Mode->SelectSpawnGroup\(GetPlayerState<ASPPlayerState>\(\), SpawnGroupId\)/);
  assert.match(controller, /Now < NextReadyRoomRequestSeconds/);
  assert.equal((controller.match(/if \(!ConsumeReadyRoomRequest\(\)\) return;/g) || []).length, 2);
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
