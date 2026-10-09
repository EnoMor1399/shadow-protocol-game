import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';

async function text(path: string) {
  return readFile(new URL(path, import.meta.url), 'utf8');
}

test('UE5.6 packaging script preserves dedicated-server BuildCookRun contract', async () => {
  const packageScript = await text('../../Build/Package-DedicatedServer.ps1');
  const verifyScript = await text('../../Build/Verify-DedicatedServerPackage.ps1');
  const bundleScript = await text('../../Build/Create-ServerHostBundle.ps1');

  assert.match(packageScript, /ShadowProtocolServer/);
  assert.match(packageScript, /BuildCookRun/);
  assert.match(packageScript, /'-server'/);
  assert.match(packageScript, /'-noclient'/);
  assert.match(packageScript, /-serverplatform=/);
  assert.match(packageScript, /-serverconfig=/);
  assert.match(packageScript, /Embassy\/Embassy_P/);
  assert.match(verifyScript, /AssetRegistry\.bin/);
  assert.match(verifyScript, /Potential secret material found in package/);
  assert.match(bundleScript, /ServerOrchestrator/);
  assert.match(bundleScript, /bundle-manifest\.json/);
});

test('Linux host service runs orchestrator as non-root and retains graceful restart controls', async () => {
  const service = await text('../../Deploy/Linux/shadow-protocol-server.service');
  const installer = await text('../../Deploy/Linux/install-host.sh');
  const firewall = await text('../../Deploy/Linux/configure-firewall.sh');

  assert.match(service, /User=shadowprotocol/);
  assert.match(service, /EnvironmentFile=\/etc\/shadow-protocol\/server\.env/);
  assert.match(service, /ExecStart=\/usr\/bin\/node .*launcher\.js/);
  assert.match(service, /Restart=on-failure/);
  assert.match(service, /KillSignal=SIGTERM/);
  assert.match(service, /NoNewPrivileges=true/);
  assert.match(installer, /Node\.js 22\+/);
  assert.match(installer, /useradd --system/);
  assert.match(installer, /chmod 0750/);
  assert.match(firewall, /udp/);
});
