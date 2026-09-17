import assert from 'node:assert/strict';
import { after, before, test } from 'node:test';
import { spawn } from 'node:child_process';

const PORT = 18081;
const BASE_URL = `http://127.0.0.1:${PORT}`;
const BOOTSTRAP_SECRET = 'ci-bootstrap-secret';
const MATCH_SERVER_SECRET = 'ci-match-server-secret';
const TEST_USER_ID = '11111111-1111-4111-8111-111111111111';
const SPOOFED_USER_ID = '22222222-2222-4222-8222-222222222222';
const TEST_MATCH_ID = '33333333-3333-4333-8333-333333333333';
let serverProcess;

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
  throw lastError ?? new Error('Shadow Protocol backend did not become ready.');
}

async function createGameSession(build = 'SP-1.0.1') {
  return fetch(`${BASE_URL}/v1/auth/game-session`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      'x-session-bootstrap-secret': BOOTSTRAP_SECRET
    },
    body: JSON.stringify({
      userId: TEST_USER_ID,
      region: 'acc',
      build,
      deviceNonce: `integration-${build}-nonce`
    })
  });
}

async function getValidSession() {
  const response = await createGameSession();
  assert.equal(response.status, 201);
  return response.json();
}

before(async () => {
  serverProcess = spawn(process.execPath, ['--import', 'tsx', 'src/server.ts'], {
    cwd: process.cwd(),
    env: {
      ...process.env,
      PORT: String(PORT),
      DATABASE_URL: '',
      REDIS_URL: '',
      SESSION_SIGNING_SECRET: 'ci-session-signing-secret',
      SESSION_BOOTSTRAP_SECRET: BOOTSTRAP_SECRET,
      MATCH_SERVER_SECRET,
      ACCEPTED_NETWORK_BUILDS: 'SP-1.0.1'
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

after(() => {
  if (serverProcess && !serverProcess.killed) serverProcess.kill('SIGTERM');
});

test('publishes the v1.0.1 compatibility contract', async () => {
  const response = await fetch(`${BASE_URL}/v1/compatibility`);
  assert.equal(response.status, 200);
  const payload = await response.json();
  assert.equal(payload.gameRelease, '1.0.1');
  assert.equal(payload.networkBuild, 'SP-1.0.1');
  assert.equal(payload.backendProtocol, '0.8.0');
  assert.deepEqual(payload.acceptedNetworkBuilds, ['SP-1.0.1']);
  assert.equal(payload.enforcement, 'strict');
});

test('rejects an incompatible client before issuing a game session', async () => {
  const response = await createGameSession('SP-0.9.0');
  assert.equal(response.status, 426);
  const payload = await response.json();
  assert.equal(payload.error, 'client-build-incompatible');
  assert.equal(payload.receivedBuild, 'SP-0.9.0');
  assert.deepEqual(payload.acceptedNetworkBuilds, ['SP-1.0.1']);
});

test('refreshes an authenticated compatible game session', async () => {
  const session = await getValidSession();
  const originalExpiry = Date.parse(session.expiresAt);

  await new Promise((resolve) => setTimeout(resolve, 5));
  const response = await fetch(`${BASE_URL}/v1/auth/refresh`, {
    method: 'POST',
    headers: { authorization: `Bearer ${session.sessionToken}` }
  });

  assert.equal(response.status, 200);
  const refreshed = await response.json();
  assert.equal(refreshed.sessionId, session.sessionId);
  assert.equal(refreshed.region, 'acc');
  assert.ok(refreshed.sessionToken);
  assert.ok(Date.parse(refreshed.expiresAt) >= originalExpiry);
  assert.equal(refreshed.compatibility.networkBuild, 'SP-1.0.1');
  assert.equal(refreshed.compatibility.backendProtocol, '0.8.0');
});

test('issues and allocates an authenticated SP-1.0.1 session', async () => {
  const session = await getValidSession();
  assert.ok(session.sessionId);
  assert.ok(session.sessionToken);
  assert.equal(session.compatibility.networkBuild, 'SP-1.0.1');
  assert.equal(session.compatibility.backendProtocol, '0.8.0');

  const allocationResponse = await fetch(`${BASE_URL}/v1/matches/allocate`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      authorization: `Bearer ${session.sessionToken}`
    },
    body: JSON.stringify({ region: 'acc', mode: 'PROTOCOL', map: 'EMBASSY', ranked: true })
  });

  assert.equal(allocationResponse.status, 201);
  const allocation = await allocationResponse.json();
  assert.ok(allocation.allocationId);
  assert.ok(allocation.matchId);
  assert.ok(allocation.serverId);
  assert.ok(allocation.connectToken);
  assert.equal(allocation.networkBuild, 'SP-1.0.1');
  assert.equal(allocation.backendProtocol, '0.8.0');
  assert.equal(allocation.tickRate, 60);
});

test('requires an authenticated compatible session for matchmaking', async () => {
  const unauthenticated = await fetch(`${BASE_URL}/v1/matchmaking/queue`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ region: 'acc', latencyMs: 42, partySize: 1 })
  });
  assert.equal(unauthenticated.status, 401);

  const session = await getValidSession();
  const queued = await fetch(`${BASE_URL}/v1/matchmaking/queue`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      authorization: `Bearer ${session.sessionToken}`
    },
    body: JSON.stringify({
      region: 'acc',
      latencyMs: 42,
      partySize: 1,
      userId: SPOOFED_USER_ID,
      skillRating: 9999,
      trustScore: 100
    })
  });

  assert.equal(queued.status, 200);
  const payload = await queued.json();
  assert.ok(payload.ticket.startsWith('mm_'));
  assert.equal(payload.userId, TEST_USER_ID);
  assert.equal(payload.region, 'acc');
});

test('rejects public clients from authoritative match telemetry', async () => {
  const eventBody = {
    matchId: TEST_MATCH_ID,
    eventType: 'objective_state',
    gameTimeMs: 1200,
    payload: { state: 'secured' }
  };

  const publicResponse = await fetch(`${BASE_URL}/v1/matches/events`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(eventBody)
  });
  assert.equal(publicResponse.status, 401);
  assert.equal((await publicResponse.json()).error, 'match-server-auth-required');

  const serverResponse = await fetch(`${BASE_URL}/v1/matches/events`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      'x-match-server-secret': MATCH_SERVER_SECRET
    },
    body: JSON.stringify(eventBody)
  });
  assert.equal(serverResponse.status, 202);
  assert.equal((await serverResponse.json()).authority, 'dedicated-server');
});

test('fails reconnect and ready-state ownership checks closed without PostgreSQL', async () => {
  const session = await getValidSession();
  const headers = {
    'content-type': 'application/json',
    authorization: `Bearer ${session.sessionToken}`
  };

  const reconnectTicket = await fetch(`${BASE_URL}/v1/matches/reconnect-ticket`, {
    method: 'POST',
    headers,
    body: JSON.stringify({ matchId: TEST_MATCH_ID, roundNumber: 1, slotIndex: 0 })
  });
  assert.equal(reconnectTicket.status, 503);
  assert.equal((await reconnectTicket.json()).error, 'database-not-configured');

  const readyState = await fetch(`${BASE_URL}/v1/matches/ready-state`, {
    method: 'POST',
    headers,
    body: JSON.stringify({ matchId: TEST_MATCH_ID, roundNumber: 1, slotIndex: 0, ready: true, spawnGroup: 'ALPHA' })
  });
  assert.equal(readyState.status, 503);
  assert.equal((await readyState.json()).error, 'database-not-configured');
});
