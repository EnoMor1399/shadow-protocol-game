import assert from 'node:assert/strict';
import { test } from 'node:test';
import { verifyEos, verifySteam } from '../src/providers.js';

const steamEnv = {
  NODE_ENV: 'test',
  STEAM_WEB_API_KEY: 'publisher-key-test',
  STEAM_APP_ID: '123456',
  STEAM_WEB_API_IDENTITY: 'shadow-protocol'
};

test('Steam adapter calls AuthenticateUserTicket and trusts only verified SteamID', async () => {
  let called = false;
  const fakeFetch: typeof fetch = async (input) => {
    called = true;
    const url = new URL(String(input));
    assert.equal(url.origin, 'https://partner.steam-api.com');
    assert.equal(url.pathname, '/ISteamUserAuth/AuthenticateUserTicket/v1/');
    assert.equal(url.searchParams.get('key'), steamEnv.STEAM_WEB_API_KEY);
    assert.equal(url.searchParams.get('appid'), steamEnv.STEAM_APP_ID);
    assert.equal(url.searchParams.get('identity'), steamEnv.STEAM_WEB_API_IDENTITY);
    assert.equal(url.searchParams.get('ticket'), 'ab'.repeat(64));
    return new Response(JSON.stringify({
      response: { params: { result: 'OK', steamid: '76561198000000000' } }
    }), { status: 200, headers: { 'content-type': 'application/json' } });
  };

  const result = await verifySteam({
    provider: 'steam',
    authType: 'webapi-ticket',
    authToken: 'ab'.repeat(64)
  }, steamEnv, fakeFetch);

  assert.equal(called, true);
  assert.deepEqual(result, { provider: 'steam', subject: '76561198000000000' });
});

test('Steam adapter fails closed before network on malformed ticket', async () => {
  let called = false;
  const fakeFetch: typeof fetch = async () => {
    called = true;
    throw new Error('must not call network');
  };

  await assert.rejects(
    verifySteam({ provider: 'steam', authType: 'webapi-ticket', authToken: 'not-hex' }, steamEnv, fakeFetch),
    /hexadecimal Web API ticket/
  );
  assert.equal(called, false);
});

test('EOS adapter delegates only to configured trusted SDK verifier and checks deployment metadata', async () => {
  const env = {
    NODE_ENV: 'production',
    EOS_VERIFIER_URL: 'https://eos-verifier.internal.example/verify-id-token',
    EOS_VERIFIER_SHARED_SECRET: '0123456789abcdef0123456789abcdef',
    EOS_EXPECTED_PRODUCT_ID: 'product-a',
    EOS_EXPECTED_SANDBOX_ID: 'sandbox-a',
    EOS_EXPECTED_DEPLOYMENT_ID: 'deployment-a'
  };

  const fakeFetch: typeof fetch = async (input, init) => {
    assert.equal(String(input), env.EOS_VERIFIER_URL);
    assert.equal(init?.method, 'POST');
    const headers = new Headers(init?.headers);
    assert.equal(headers.get('authorization'), `Bearer ${env.EOS_VERIFIER_SHARED_SECRET}`);
    const body = JSON.parse(String(init?.body));
    assert.equal(body.idToken, 'eos-id-token-value-123456789');
    assert.equal(body.authType, 'id-token');
    return new Response(JSON.stringify({
      subject: '0123456789abcdef0123456789abcdef',
      productId: 'product-a',
      sandboxId: 'sandbox-a',
      deploymentId: 'deployment-a'
    }), { status: 200, headers: { 'content-type': 'application/json' } });
  };

  const result = await verifyEos({
    provider: 'eos',
    authType: 'id-token',
    authToken: 'eos-id-token-value-123456789'
  }, env, fakeFetch);

  assert.deepEqual(result, { provider: 'eos', subject: '0123456789abcdef0123456789abcdef' });
});

test('EOS adapter rejects insecure production verifier URL', async () => {
  await assert.rejects(
    verifyEos({
      provider: 'eos',
      authType: 'id-token',
      authToken: 'eos-id-token-value-123456789'
    }, {
      NODE_ENV: 'production',
      EOS_VERIFIER_URL: 'http://127.0.0.1:9000/verify-id-token',
      EOS_VERIFIER_SHARED_SECRET: '0123456789abcdef0123456789abcdef'
    }),
    /must use HTTPS/
  );
});
