import assert from 'node:assert/strict';
import { after, before, test } from 'node:test';
import { spawn } from 'node:child_process';
import pg from 'pg';

const { Client } = pg;
const PORT = 18082;
const BASE_URL = `http://127.0.0.1:${PORT}`;
const DATABASE_URL = process.env.DATABASE_URL;
const BOOTSTRAP_SECRET = 'ci-postgres-bootstrap-secret';
const SERVER_REGISTRATION_SECRET = 'ci-postgres-registration-secret';
const PRIMARY_SERVER_ID = 'ACC-PRIMARY';
const PRIMARY_SERVER_HOST = '10.10.0.10';
const PRIMARY_SERVER_PORT = 7781;
const TEST_USER_ID = '44444444-4444-4444-8444-444444444444';
const TEST_EMAIL = 'postgres-integration@shadow-protocol.test';

let serverProcess;
let db;

async function waitForServer() {
  let lastError;
  for (let attempt = 0; attempt < 80; attempt += 1) {
    try {
      const response = await fetch(`${BASE_URL}/health`);
      if (response.ok) return;
    } catch (error) {
      lastError = error;
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw lastError ?? new Error('Shadow Protocol PostgreSQL integration backend did not become ready.');
}

async function bootstrapPost(path, body) {
  return fetch(`${BASE_URL}${path}`, { method: 'POST', headers: { 'content-type': 'application/json', 'x-match-server-secret': SERVER_REGISTRATION_SECRET }, body: JSON.stringify(body) });
}
async function nodePost(path, body, serverId, nodeCredential) {
  return fetch(`${BASE_URL}${path}`, { method: 'POST', headers: { 'content-type': 'application/json', 'x-sp-server-id': serverId, 'x-sp-node-credential': nodeCredential }, body: JSON.stringify(body) });
}

before(async () => {
  assert.ok(DATABASE_URL, 'DATABASE_URL must be provided to the PostgreSQL integration test.');
  db = new Client({ connectionString: DATABASE_URL });
  await db.connect();
  await db.query(
    `insert into users(id,email,status,trust_score)
     values($1,$2,'active',88)
     on conflict(id) do update set email=excluded.email,status='active',trust_score=88`,
    [TEST_USER_ID, TEST_EMAIL]
  );

  serverProcess = spawn(process.execPath, ['--import', 'tsx', 'src/server.ts'], {
    cwd: process.cwd(),
    env: {
      ...process.env,
      NODE_ENV: 'production',
      PORT: String(PORT),
      REDIS_URL: '',
      SESSION_SIGNING_SECRET: 'ci-postgres-session-signing-secret',
      SESSION_BOOTSTRAP_SECRET: BOOTSTRAP_SECRET,
      SERVER_REGISTRATION_SECRET,
      ACCEPTED_NETWORK_BUILDS: 'SP-1.0.1',
      SERVER_HEARTBEAT_TTL_MS: '30000',
      NODE_CREDENTIAL_TTL_MS: '600000',
      NODE_CREDENTIAL_GRACE_MS: '30000',
      GAME_SERVER_PUBLIC_HOST: '',
      GAME_SERVER_PUBLIC_PORT: ''
    },
    stdio: ['ignore', 'pipe', 'pipe']
  });

  let stderr = '';
  serverProcess.stderr.on('data', (chunk) => { stderr += chunk.toString(); });
  serverProcess.on('exit', (code) => {
    if (code && code !== 0) process.stderr.write(stderr);
  });

  await waitForServer();
});

after(async () => {
  if (serverProcess && !serverProcess.killed) serverProcess.kill('SIGTERM');
  if (db) await db.end();
});

test('schedules healthy regional nodes and preserves admission, ready, reconnect and release authority', async () => {
  const publicRegistration = await fetch(`${BASE_URL}/v1/servers/register`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ serverId: PRIMARY_SERVER_ID, region: 'acc', networkBuild: 'SP-1.0.1', publicHost: PRIMARY_SERVER_HOST, publicPort: PRIMARY_SERVER_PORT, capacity: 1 })
  });
  assert.equal(publicRegistration.status, 401);

  const primaryRegistration = await bootstrapPost('/v1/servers/register', {
    serverId: PRIMARY_SERVER_ID,
    region: 'acc',
    networkBuild: 'SP-1.0.1',
    publicHost: PRIMARY_SERVER_HOST,
    publicPort: PRIMARY_SERVER_PORT,
    capacity: 1
  });
  assert.equal(primaryRegistration.status, 200);
  const primaryNode = await primaryRegistration.json();
  assert.ok(primaryCredential.length >= 32);
  assert.equal(primaryNode.credentialTtlMs, 600000);
  assert.equal(primaryNode.credentialGraceMs, 30000);
  assert.ok(Date.parse(primaryNode.credentialExpiresAt) > Date.now());
  let primaryCredential = primaryCredential;

  const drainRegistration = await bootstrapPost('/v1/servers/register', {
    serverId: 'ACC-DRAIN',
    region: 'acc',
    networkBuild: 'SP-1.0.1',
    publicHost: '10.10.0.11',
    publicPort: 7782,
    capacity: 10
  });
  assert.equal(drainRegistration.status, 200);
  const drainNode = await drainRegistration.json();
  assert.ok(drainNode.nodeCredential.length >= 32);
  const drainResponse = await nodePost('/v1/servers/drain', { serverId: 'ACC-DRAIN' }, 'ACC-DRAIN', drainNode.nodeCredential);
  assert.equal(drainResponse.status, 200);

  const staleRegistration = await bootstrapPost('/v1/servers/register', {
    serverId: 'ACC-STALE',
    region: 'acc',
    networkBuild: 'SP-1.0.1',
    publicHost: '10.10.0.12',
    publicPort: 7783,
    capacity: 10
  });
  assert.equal(staleRegistration.status, 200);
  const staleNode = await staleRegistration.json();
  assert.ok(staleNode.nodeCredential.length >= 32);
  await db.query(`update game_server_nodes set last_heartbeat_at=now()-interval '2 minutes' where server_id='ACC-STALE'`);

  const sessionResponse = await fetch(`${BASE_URL}/v1/auth/game-session`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      'x-session-bootstrap-secret': BOOTSTRAP_SECRET
    },
    body: JSON.stringify({
      userId: TEST_USER_ID,
      region: 'acc',
      build: 'SP-1.0.1',
      deviceNonce: 'postgres-integration-device-nonce'
    })
  });
  assert.equal(sessionResponse.status, 201);
  const session = await sessionResponse.json();
  const playerHeaders = {
    'content-type': 'application/json',
    authorization: `Bearer ${session.sessionToken}`
  };

  const allocationResponse = await fetch(`${BASE_URL}/v1/matches/allocate`, {
    method: 'POST',
    headers: playerHeaders,
    body: JSON.stringify({ region: 'acc', mode: 'PROTOCOL', map: 'EMBASSY', ranked: true })
  });
  assert.equal(allocationResponse.status, 201);
  const allocation = await allocationResponse.json();
  assert.equal(allocation.serverId, PRIMARY_SERVER_ID);
  assert.equal(allocation.connectHost, PRIMARY_SERVER_HOST);
  assert.equal(allocation.connectPort, PRIMARY_SERVER_PORT);
  assert.equal(allocation.allocator, 'registry');

  const persistedAllocation = await db.query(
    `select user_id,node_id,server_id,connect_host,connect_port,connect_token_hash,connect_token_consumed_at,status
     from server_allocations where id=$1`,
    [allocation.allocationId]
  );
  assert.equal(persistedAllocation.rowCount, 1);
  assert.equal(persistedAllocation.rows[0].user_id, TEST_USER_ID);
  assert.ok(persistedAllocation.rows[0].node_id);
  assert.equal(persistedAllocation.rows[0].server_id, PRIMARY_SERVER_ID);
  assert.equal(persistedAllocation.rows[0].connect_host, PRIMARY_SERVER_HOST);
  assert.equal(Number(persistedAllocation.rows[0].connect_port), PRIMARY_SERVER_PORT);
  assert.equal(persistedAllocation.rows[0].status, 'reserved');
  assert.equal(persistedAllocation.rows[0].connect_token_consumed_at, null);
  assert.notEqual(persistedAllocation.rows[0].connect_token_hash, allocation.connectToken);

  const primaryLoad = await db.query(`select active_allocations,credential_hash from game_server_nodes where server_id=$1`, [PRIMARY_SERVER_ID]);
  assert.equal(Number(primaryLoad.rows[0].active_allocations), 1);
  assert.ok(primaryLoad.rows[0].credential_hash);
  assert.notEqual(primaryLoad.rows[0].credential_hash, primaryCredential);

  const saturatedResponse = await fetch(`${BASE_URL}/v1/matches/allocate`, {
    method: 'POST',
    headers: playerHeaders,
    body: JSON.stringify({ region: 'acc', mode: 'PROTOCOL', map: 'EMBASSY', ranked: true })
  });
  assert.equal(saturatedResponse.status, 503);
  const saturated = await saturatedResponse.json();
  assert.equal(saturated.error, 'no-healthy-game-server');

  const credentialBeforeRotation = primaryCredential;
  const rotateResponse = await nodePost('/v1/servers/rotate-credential', { serverId: PRIMARY_SERVER_ID }, PRIMARY_SERVER_ID, primaryCredential);
  assert.equal(rotateResponse.status, 200);
  const rotated = await rotateResponse.json();
  assert.ok(rotated.nodeCredential.length >= 32);
  assert.notEqual(rotated.nodeCredential, credentialBeforeRotation);
  assert.equal(rotated.credentialTtlMs, 600000);
  assert.ok(Date.parse(rotated.credentialExpiresAt) > Date.now());
  assert.ok(Date.parse(rotated.previousCredentialValidUntil) > Date.now());

  const oldCredentialGraceHeartbeat = await nodePost('/v1/servers/heartbeat', { serverId: PRIMARY_SERVER_ID, status: 'ready' }, PRIMARY_SERVER_ID, credentialBeforeRotation);
  assert.equal(oldCredentialGraceHeartbeat.status, 200);

  primaryCredential = rotated.nodeCredential;
  const rotatedCredentialHeartbeat = await nodePost('/v1/servers/heartbeat', { serverId: PRIMARY_SERVER_ID, status: 'ready' }, PRIMARY_SERVER_ID, primaryCredential);
  assert.equal(rotatedCredentialHeartbeat.status, 200);

  const credentialState = await db.query(`select credential_hash,credential_expires_at,previous_credential_hash,previous_credential_valid_until
    from game_server_nodes where server_id=$1`, [PRIMARY_SERVER_ID]);
  assert.notEqual(credentialState.rows[0].credential_hash, primaryCredential);
  assert.notEqual(credentialState.rows[0].previous_credential_hash, credentialBeforeRotation);
  assert.ok(credentialState.rows[0].previous_credential_hash);
  assert.ok(new Date(credentialState.rows[0].credential_expires_at).getTime() > Date.now());

  await db.query(`update game_server_nodes set previous_credential_valid_until=now()-interval '1 second' where server_id=$1`, [PRIMARY_SERVER_ID]);
  const expiredOldCredential = await nodePost('/v1/servers/heartbeat', { serverId: PRIMARY_SERVER_ID, status: 'ready' }, PRIMARY_SERVER_ID, credentialBeforeRotation);
  assert.equal(expiredOldCredential.status, 401);

  const staleRotation = await nodePost('/v1/servers/rotate-credential', { serverId: PRIMARY_SERVER_ID }, PRIMARY_SERVER_ID, credentialBeforeRotation);
  assert.equal(staleRotation.status, 401);

  const admissionBody = {
    allocationId: allocation.allocationId,
    matchId: allocation.matchId,
    connectToken: allocation.connectToken
  };

  const publicAdmission = await fetch(`${BASE_URL}/v1/matches/admit`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(admissionBody)
  });
  assert.equal(publicAdmission.status, 401);

  const bootstrapAdmission = await fetch(`${BASE_URL}/v1/matches/admit`, { method: 'POST', headers: { 'content-type': 'application/json', 'x-match-server-secret': SERVER_REGISTRATION_SECRET }, body: JSON.stringify(admissionBody) });
  assert.equal(bootstrapAdmission.status, 401);
  const crossNodeAdmission = await nodePost('/v1/matches/admit', admissionBody, 'ACC-DRAIN', drainNode.nodeCredential);
  assert.equal(crossNodeAdmission.status, 403);
  const deniedAdmission = await nodePost('/v1/matches/admit', { ...admissionBody, connectToken: 'wrong-token-value-that-is-long-enough' }, PRIMARY_SERVER_ID, primaryCredential);
  assert.equal(deniedAdmission.status, 403);
  const admissionResponse = await nodePost('/v1/matches/admit', admissionBody, PRIMARY_SERVER_ID, primaryCredential);
  assert.equal(admissionResponse.status, 200);
  const admission = await admissionResponse.json();
  assert.equal(admission.admitted, true);
  assert.equal(admission.userId, TEST_USER_ID);
  assert.equal(admission.matchId, allocation.matchId);
  assert.equal(admission.serverId, PRIMARY_SERVER_ID);
  assert.equal(admission.networkBuild, 'SP-1.0.1');
  assert.equal(admission.authority, 'dedicated-server');

  const replayAdmission = await nodePost('/v1/matches/admit', admissionBody, PRIMARY_SERVER_ID, primaryCredential);
  assert.equal(replayAdmission.status, 403);

  const consumedAllocation = await db.query(
    `select status,connect_token_consumed_at from server_allocations where id=$1`,
    [allocation.allocationId]
  );
  assert.equal(consumedAllocation.rows[0].status, 'live');
  assert.ok(consumedAllocation.rows[0].connect_token_consumed_at);

  const slotResponse = await nodePost('/v1/matches/player-slots', {
    matchId: allocation.matchId,
    roundNumber: 1,
    slotIndex: 0,
    userId: TEST_USER_ID,
    team: 'SPECTRE',
    tacticalSide: 'attack',
    spawnGroup: 'ALPHA',
    connectionState: 'connected',
    ready: false
  }, PRIMARY_SERVER_ID, primaryCredential);
  assert.equal(slotResponse.status, 202);
  const crossNodeTelemetry = await nodePost('/v1/anti-cheat/events', { matchId: allocation.matchId, userId: TEST_USER_ID, signal: 'integration-cross-node', severity: 1, evidence: {} }, 'ACC-DRAIN', drainNode.nodeCredential);
  assert.equal(crossNodeTelemetry.status, 403);
  const ownedTelemetry = await nodePost('/v1/anti-cheat/events', { matchId: allocation.matchId, userId: TEST_USER_ID, signal: 'integration-owned-node', severity: 1, evidence: {} }, PRIMARY_SERVER_ID, primaryCredential);
  assert.equal(ownedTelemetry.status, 202);

  const readyResponse = await fetch(`${BASE_URL}/v1/matches/ready-state`, {
    method: 'POST',
    headers: playerHeaders,
    body: JSON.stringify({
      matchId: allocation.matchId,
      roundNumber: 1,
      slotIndex: 0,
      ready: true,
      spawnGroup: 'BRAVO'
    })
  });
  assert.equal(readyResponse.status, 200);
  const ready = await readyResponse.json();
  assert.equal(ready.ready, true);
  assert.equal(ready.spawn_group, 'BRAVO');

  const ticketResponse = await fetch(`${BASE_URL}/v1/matches/reconnect-ticket`, {
    method: 'POST',
    headers: playerHeaders,
    body: JSON.stringify({ matchId: allocation.matchId, roundNumber: 1, slotIndex: 0 })
  });
  assert.equal(ticketResponse.status, 201);
  const ticket = await ticketResponse.json();
  assert.ok(ticket.reconnectToken.length >= 32);
  assert.equal(ticket.graceSeconds, 90);

  const reconnectingSlot = await db.query(
    `select connection_state,ready,reconnect_token_hash,reconnect_deadline
     from match_player_slots where match_id=$1 and round_number=1 and slot_index=0`,
    [allocation.matchId]
  );
  assert.equal(reconnectingSlot.rows[0].connection_state, 'reconnecting');
  assert.equal(reconnectingSlot.rows[0].ready, false);
  assert.ok(reconnectingSlot.rows[0].reconnect_token_hash);
  assert.notEqual(reconnectingSlot.rows[0].reconnect_token_hash, ticket.reconnectToken);
  assert.ok(new Date(reconnectingSlot.rows[0].reconnect_deadline).getTime() > Date.now());

  const deniedResponse = await fetch(`${BASE_URL}/v1/matches/reconnect`, {
    method: 'POST',
    headers: playerHeaders,
    body: JSON.stringify({
      matchId: allocation.matchId,
      roundNumber: 1,
      slotIndex: 0,
      reconnectToken: 'x'.repeat(48)
    })
  });
  assert.equal(deniedResponse.status, 403);

  const reconnectResponse = await fetch(`${BASE_URL}/v1/matches/reconnect`, {
    method: 'POST',
    headers: playerHeaders,
    body: JSON.stringify({
      matchId: allocation.matchId,
      roundNumber: 1,
      slotIndex: 0,
      reconnectToken: ticket.reconnectToken
    })
  });
  assert.equal(reconnectResponse.status, 200);
  const reconnect = await reconnectResponse.json();
  assert.equal(reconnect.reconnected, true);
  assert.equal(Number(reconnect.slot.slot_index), 0);
  assert.equal(reconnect.slot.user_id, TEST_USER_ID);
  assert.equal(reconnect.networkBuild, 'SP-1.0.1');

  const restoredSlot = await db.query(
    `select connection_state,reconnect_token_hash,reconnect_deadline
     from match_player_slots where match_id=$1 and round_number=1 and slot_index=0`,
    [allocation.matchId]
  );
  assert.equal(restoredSlot.rows[0].connection_state, 'connected');
  assert.equal(restoredSlot.rows[0].reconnect_token_hash, null);
  assert.equal(restoredSlot.rows[0].reconnect_deadline, null);

  const crossNodeRelease = await nodePost('/v1/servers/release-allocation', { allocationId: allocation.allocationId, matchId: allocation.matchId, outcome: 'closed' }, 'ACC-DRAIN', drainNode.nodeCredential);
  assert.equal(crossNodeRelease.status, 403);
  const releaseResponse = await nodePost('/v1/servers/release-allocation', {
    allocationId: allocation.allocationId,
    matchId: allocation.matchId,
    outcome: 'closed'
  }, PRIMARY_SERVER_ID, primaryCredential);
  assert.equal(releaseResponse.status, 200);
  const release = await releaseResponse.json();
  assert.equal(release.released, true);
  assert.equal(release.serverId, PRIMARY_SERVER_ID);
  assert.equal(release.status, 'closed');
  assert.equal(release.activeAllocations, 0);

  const releasedNode = await db.query(`select active_allocations from game_server_nodes where server_id=$1`, [PRIMARY_SERVER_ID]);
  assert.equal(Number(releasedNode.rows[0].active_allocations), 0);
});
