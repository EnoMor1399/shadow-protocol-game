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
