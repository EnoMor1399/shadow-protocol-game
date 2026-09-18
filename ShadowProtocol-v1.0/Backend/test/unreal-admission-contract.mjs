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
