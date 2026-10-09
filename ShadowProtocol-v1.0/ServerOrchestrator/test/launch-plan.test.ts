import assert from 'node:assert/strict';
import { test } from 'node:test';
import { buildLaunchPlan } from '../src/launch-plan.js';

function baseEnv(): NodeJS.ProcessEnv {
  return {
    NODE_ENV: 'production',
    ORCHESTRATOR_ATTESTATION_SECRET: 'orchestrator-secret-that-is-definitely-long-enough',
    SERVER_REGISTRATION_SECRET: 'registration-secret-that-is-definitely-long-enough',
    GAME_SERVER_EXECUTABLE: '/opt/shadow/ShadowProtocolServer',
    GAME_SERVER_MAP: '/Game/Maps/Embassy/Embassy_P',
    SP_BACKEND_URL: 'https://shadow-protocol-game.onrender.com',
    SP_SERVER_ID: 'ACC-PROTOCOL-01',
    SP_REGION: 'acc',
    SP_PUBLIC_HOST: 'game-01.example.net',
    SP_PUBLIC_PORT: '7777',
    SP_CAPACITY: '10',
    SP_NETWORK_BUILD: 'SP-1.0.1',
    SP_SERVER_EXTRA_ARGS_JSON: '["-unattended","-NoCrashDialog"]',
    SERVER_ATTESTATION_TTL_MS: '60000'
  };
}

test('builds a shell-free launch plan without leaking the signing secret', () => {
  const env = baseEnv();
  const plan = buildLaunchPlan(env, 1_800_000_000_000);

  assert.equal(plan.executable, '/opt/shadow/ShadowProtocolServer');
  assert.equal(plan.identity.serverId, 'ACC-PROTOCOL-01');
  assert.ok(plan.args.includes('-server'));
  assert.ok(plan.args.includes('-SPBackendUrl=https://shadow-protocol-game.onrender.com'));
  assert.ok(plan.args.includes('-SPServerId=ACC-PROTOCOL-01'));
  assert.ok(plan.args.includes('-SPRegion=acc'));
  assert.ok(plan.args.includes('-SPPublicHost=game-01.example.net'));
  assert.ok(plan.args.includes('-SPPublicPort=7777'));
  assert.ok(plan.args.includes('-SPCapacity=10'));
  assert.ok(plan.args.includes('-unattended'));

  assert.equal(plan.childEnv.ORCHESTRATOR_ATTESTATION_SECRET, undefined);
  assert.equal(plan.childEnv.SP_SERVER_EXTRA_ARGS_JSON, undefined);
  assert.equal(plan.childEnv.SERVER_REGISTRATION_SECRET, env.SERVER_REGISTRATION_SECRET);
  assert.ok((plan.childEnv.SP_NODE_ATTESTATION ?? '').length > 64);

  const commandMaterial = [plan.executable, ...plan.args].join(' ');
  assert.ok(!commandMaterial.includes(env.ORCHESTRATOR_ATTESTATION_SECRET!));
  assert.ok(!commandMaterial.includes(env.SERVER_REGISTRATION_SECRET!));
  assert.ok(!commandMaterial.includes(plan.childEnv.SP_NODE_ATTESTATION!));
});

test('rejects insecure production backend URLs and unsafe extra arguments', () => {
  assert.throws(
    () => buildLaunchPlan({ ...baseEnv(), SP_BACKEND_URL: 'http://example.net' }),
    /must use HTTPS/
  );
  assert.throws(
    () => buildLaunchPlan({ ...baseEnv(), SP_SERVER_EXTRA_ARGS_JSON: '["ok","bad\\narg"]' }),
    /invalid argument/
  );
});

test('allows local HTTP only outside production', () => {
  const plan = buildLaunchPlan({
    ...baseEnv(),
    NODE_ENV: 'development',
    SP_BACKEND_URL: 'http://127.0.0.1:8080'
  });
  assert.ok(plan.args.includes('-SPBackendUrl=http://127.0.0.1:8080'));
});
