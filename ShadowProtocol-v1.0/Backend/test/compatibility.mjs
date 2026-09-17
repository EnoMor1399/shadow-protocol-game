import assert from 'node:assert/strict';
import { after, before, test } from 'node:test';
import { spawn } from 'node:child_process';

const PORT = 18081;
const BASE_URL = `http://127.0.0.1:${PORT}`;
const BOOTSTRAP_SECRET = 'ci-bootstrap-secret';
const TEST_USER_ID = '11111111-1111-4111-8111-111111111111';
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

async function createGameSession(build) {
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

test('issues and allocates an authenticated SP-1.0.1 session', async () => {
  const sessionResponse = await createGameSession('SP-1.0.1');
  assert.equal(sessionResponse.status, 201);
  const session = await sessionResponse.json();
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
