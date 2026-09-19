import assert from 'node:assert/strict';
import { after, before, test } from 'node:test';
import { spawn } from 'node:child_process';

const PORT = 18082;
const BASE_URL = `http://127.0.0.1:${PORT}`;
const BOOTSTRAP_SECRET = 'ci-production-bootstrap-secret';
const TEST_USER_ID = '44444444-4444-4444-8444-444444444444';
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
  throw lastError ?? new Error('Production-mode backend did not become ready.');
}

before(async () => {
  const env = { ...process.env };
  delete env.GAME_SERVER_PUBLIC_HOST;
  delete env.GAME_SERVER_PUBLIC_PORT;

  serverProcess = spawn(process.execPath, ['--import', 'tsx', 'src/server.ts'], {
    cwd: process.cwd(),
    env: {
      ...env,
      NODE_ENV: 'production',
      PORT: String(PORT),
      DATABASE_URL: '',
      REDIS_URL: '',
      SESSION_SIGNING_SECRET: 'ci-production-session-secret',
      SESSION_BOOTSTRAP_SECRET: BOOTSTRAP_SECRET,
      MATCH_SERVER_SECRET: 'ci-production-match-secret',
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

test('production allocation fails closed without a configured public server target', async () => {
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
      deviceNonce: 'production-target-test-nonce'
    })
  });

  assert.equal(sessionResponse.status, 201);
  const session = await sessionResponse.json();

  const allocationResponse = await fetch(`${BASE_URL}/v1/matches/allocate`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      authorization: `Bearer ${session.sessionToken}`
    },
    body: JSON.stringify({ region: 'acc', mode: 'PROTOCOL', map: 'EMBASSY', ranked: true })
  });

  assert.equal(allocationResponse.status, 503);
  assert.equal((await allocationResponse.json()).error, 'game-server-connect-target-not-configured');
});
