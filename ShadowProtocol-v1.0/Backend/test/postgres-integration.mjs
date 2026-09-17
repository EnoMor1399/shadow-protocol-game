import assert from 'node:assert/strict';
import { after, before, test } from 'node:test';
import { spawn } from 'node:child_process';
import pg from 'pg';

const { Client } = pg;
const PORT = 18082;
const BASE_URL = `http://127.0.0.1:${PORT}`;
const DATABASE_URL = process.env.DATABASE_URL;
const BOOTSTRAP_SECRET = 'ci-postgres-bootstrap-secret';
const MATCH_SERVER_SECRET = 'ci-postgres-match-secret';
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

async function serverPost(path, body) {
  return fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      'x-match-server-secret': MATCH_SERVER_SECRET
    },
    body: JSON.stringify(body)
  });
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
      MATCH_SERVER_SECRET,
      ACCEPTED_NETWORK_BUILDS: 'SP-1.0.1',
      SERVER_HEARTBEAT_TTL_MS: '30000',
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

  const primaryRegistration = await serverPost('/v1/servers/register', {
    serverId: PRIMARY_SERVER_ID,
    region: 'acc',
    networkBuild: 'SP-1.0.1',
    publicHost: PRIMARY_SERVER_HOST,
    publicPort: PRIMARY_SERVER_PORT,
    capacity: 1
  });
  assert.equal(primaryRegistration.status, 200);

  const drainRegistration = await serverPost('/v1/servers/register', {
    serverId: 'ACC-DRAIN',
    region: 'acc',
    networkBuild: 'SP-1.0.1',
    publicHost: '10.10.0.11',
    publicPort: 7782,
    capacity: 10
  });
  assert.equal(drainRegistration.status, 200);
  const drainResponse = await serverPost('/v1/servers/drain', { serverId: 'ACC-DRAIN' });
  assert.equal(drainResponse.status, 200);

  const staleRegistration = await serverPost('/v1/servers/register', {
    serverId: 'ACC-STALE',
    region: 'acc',
    networkBuild: 'SP-1.0.1',
    publicHost: '10.10.0.12',
    publicPort: 7783,
    capacity: 10
  });
  assert.equal(staleRegistration.status, 200);
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

  const primaryLoad = await db.query(`select active_allocations from game_server_nodes where server_id=$1`, [PRIMARY_SERVER_ID]);
  assert.equal(Number(primaryLoad.rows[0].active_allocations), 1);

  const saturatedResponse = await fetch(`${BASE_URL}/v1/matches/allocate`, {
    method: 'POST',
    headers: playerHeaders,
    body: JSON.stringify({ region: 'acc', mode: 'PROTOCOL', map: 'EMBASSY', ranked: true })
  });
  assert.equal(saturatedResponse.status, 503);
  const saturated = await saturatedResponse.json();
  assert.equal(saturated.error, 'no-healthy-game-server');

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

  const deniedAdmission = await serverPost('/v1/matches/admit', { ...admissionBody, connectToken: 'wrong-token-value-that-is-long-enough' });
  assert.equal(deniedAdmission.status, 403);

  const admissionResponse = await serverPost('/v1/matches/admit', admissionBody);
  assert.equal(admissionResponse.status, 200);
  const admission = await admissionResponse.json();
  assert.equal(admission.admitted, true);
  assert.equal(admission.userId, TEST_USER_ID);
  assert.equal(admission.matchId, allocation.matchId);
  assert.equal(admission.serverId, PRIMARY_SERVER_ID);
  assert.equal(admission.networkBuild, 'SP-1.0.1');
  assert.equal(admission.authority, 'dedicated-server');

  const replayAdmission = await serverPost('/v1/matches/admit', admissionBody);
  assert.equal(replayAdmission.status, 403);

  const consumedAllocation = await db.query(
    `select status,connect_token_consumed_at from server_allocations where id=$1`,
    [allocation.allocationId]
  );
  assert.equal(consumedAllocation.rows[0].status, 'live');
  assert.ok(consumedAllocation.rows[0].connect_token_consumed_at);

  const slotResponse = await serverPost('/v1/matches/player-slots', {
    matchId: allocation.matchId,
    roundNumber: 1,
    slotIndex: 0,
    userId: TEST_USER_ID,
    team: 'SPECTRE',
    tacticalSide: 'attack',
    spawnGroup: 'ALPHA',
    connectionState: 'connected',
    ready: false
  });
  assert.equal(slotResponse.status, 202);

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

  const releaseResponse = await serverPost('/v1/servers/release-allocation', {
    allocationId: allocation.allocationId,
    matchId: allocation.matchId,
    outcome: 'closed'
  });
  assert.equal(releaseResponse.status, 200);
  const release = await releaseResponse.json();
  assert.equal(release.released, true);
  assert.equal(release.serverId, PRIMARY_SERVER_ID);
  assert.equal(release.status, 'closed');
  assert.equal(release.activeAllocations, 0);

  const releasedNode = await db.query(`select active_allocations from game_server_nodes where server_id=$1`, [PRIMARY_SERVER_ID]);
  assert.equal(Number(releasedNode.rows[0].active_allocations), 0);
});
