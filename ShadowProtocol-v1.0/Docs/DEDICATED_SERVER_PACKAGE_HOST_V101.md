# UE5.6 Dedicated Server Package and Game Host

This runbook closes the remaining packaging/hosting gap for Shadow Protocol v1.0.1.

## Target architecture

- Unreal Engine 5.6 source build on the build machine.
- Linux x86-64 dedicated-server package for the first game host.
- Render remains the gameplay control-plane/backend and trusted identity gateway.
- PostgreSQL remains behind the gameplay backend.
- The Unreal game server runs on a normal VM/game host with a public UDP port.
- `ServerOrchestrator` runs on the same host and launches the Unreal process with a short-lived registration attestation.

## Initial host sizing

For the first real 10-player / 60-tick validation host, start with:

- 4 modern x86-64 vCPU
- 8 GB RAM
- 50 GB or more SSD/NVMe
- Ubuntu 24.04 LTS x86-64
- static or reserved public IPv4
- UDP 7777 open for game traffic
- TCP 22 restricted to administrator source addresses for SSH

This is a validation starting point, not final capacity sizing. CPU frame time, memory, network rate and 10-client soak results must determine the production size.

Johannesburg and Cape Town are useful first Africa-region candidates. Measure actual latency from intended player networks before choosing the permanent region.

## 1. Prepare UE5.6 source build

The project declares `EngineAssociation: 5.6` and already includes `ShadowProtocolServer.Target.cs` with `TargetType.Server`.

On the Windows build machine, install the UE5.6 source engine and the Linux cross-compilation toolchain required by that engine build.

Example engine location:

```text
C:\UnrealEngine-5.6
```

## 2. Build, cook, stage and archive the Linux dedicated server

From the repository root:

```powershell
cd .\ShadowProtocol-v1.0
.\Build\Package-DedicatedServer.ps1 `
  -UnrealRoot 'C:\UnrealEngine-5.6' `
  -ServerPlatform Linux `
  -Configuration Shipping
```

The script performs:

1. Editor/UHT compile.
2. `ShadowProtocolServer` Linux compile.
3. UE `BuildCookRun`.
4. Embassy map cook.
5. server-only staging with `-server -noclient`.
6. pak/archive generation.
7. package validation for executable, pak, `AssetRegistry.bin` and obvious leaked secret files.

Default package output:

```text
ShadowProtocol-v1.0\Artifacts\DedicatedServer\Linux-Shipping
```

Do not continue to hosting if this command fails. A source-level CI pass is not a substitute for successful UHT/UE compilation.

## 3. Create the host upload bundle

```powershell
.\Build\Create-ServerHostBundle.ps1 `
  -PackageRoot '.\Artifacts\DedicatedServer\Linux-Shipping' `
  -ServerPlatform Linux
```

This produces a host bundle containing:

- the cooked Unreal server;
- compiled `ServerOrchestrator/dist`;
- Linux installer;
- hardened systemd unit;
- firewall helper;
- environment template with the discovered packaged server executable path;
- bundle manifest.

Default output:

```text
ShadowProtocol-v1.0\Artifacts\HostBundle\Linux
ShadowProtocol-v1.0\Artifacts\HostBundle\Linux.zip
```

## 4. Provision the Linux game host

Create an Ubuntu 24.04 x86-64 VM in the selected region. Assign or reserve a public IPv4 address.

Provider/network firewall:

- allow UDP 7777 from player networks or from the Internet during testing;
- allow TCP 22 only from trusted administrator IPs where practical;
- outbound HTTPS must be allowed so the node can register and heartbeat against the Render backend.

Do not expose the Render/PostgreSQL database directly to the game-server VM.

## 5. Upload and install the bundle

Example from the build workstation:

```bash
scp Linux.zip root@GAME_SERVER_IP:/root/
```

On the VM:

```bash
sudo apt update
sudo apt install -y unzip
cd /root
unzip Linux.zip -d shadow-protocol-bundle
cd shadow-protocol-bundle
sudo bash deploy/linux/install-host.sh
```

Node.js 22 or newer must already be installed. The installer deliberately does not curl and execute a third-party Node installer.

## 6. Configure host secrets and node identity

Edit:

```bash
sudo nano /etc/shadow-protocol/server.env
```

Required values include:

```text
ORCHESTRATOR_ATTESTATION_SECRET=<same verifier secret configured on the gameplay backend>
SERVER_REGISTRATION_SECRET=<same registration bootstrap secret configured on the gameplay backend>
SP_BACKEND_URL=https://shadow-protocol-game.onrender.com
SP_SERVER_ID=ACC-PROTOCOL-01
SP_REGION=acc
SP_PUBLIC_HOST=<public IPv4 or DNS name>
SP_PUBLIC_PORT=7777
SP_CAPACITY=10
SP_NETWORK_BUILD=SP-1.0.1
```

Never paste these two secrets into Unreal config files, command-line flags, GitHub, or the packaged server directory.

If the original backend secret values are no longer available, rotate the backend and host values together before starting the node. A mismatched secret must fail registration.

## 7. Configure host firewall

```bash
sudo bash deploy/linux/configure-firewall.sh 7777
```

Also configure the provider firewall/security group. Host firewall rules alone do not open a blocked cloud firewall.

## 8. Start the dedicated server

```bash
sudo systemctl start shadow-protocol-server
sudo systemctl status shadow-protocol-server --no-pager
sudo journalctl -u shadow-protocol-server -f
```

The service runs as the non-root `shadowprotocol` user. The orchestrator generates a new one-time attestation and launches the Unreal server. After successful backend registration, the Unreal subsystem receives a node credential and enters its heartbeat lifecycle.

## 9. Verify control-plane registration

After startup verify:

- the process remains active;
- backend registration succeeds;
- heartbeat remains healthy;
- the node appears `ready` in `game_server_nodes`;
- `active_allocations` starts at zero;
- `last_heartbeat_at` continues moving forward;
- region/build/public host/public port match the host configuration.

Then allocate a test player session through the backend and confirm the allocation returns this server host/port.

## 10. Runtime gates before production sign-off

Complete all of the following with packaged builds:

- UE5.6/UHT and Linux Shipping server compile;
- packaged client to dedicated server travel;
- real UDP 7777 reachability from outside the VM;
- successful and denied backend admission before pawn spawn;
- heartbeat-loss fail-closed behavior and recovery;
- node credential rotation;
- 10-player 5v5 assembly;
- round/match completion and atomic 10-allocation release;
- reconnect reservation and reconnect admission;
- graceful SIGTERM drain;
- forced crash/restart recovery;
- sustained 10-client soak while recording server frame time, CPU, memory, packet loss and latency.

Do not mark the runtime layer production-ready until these external UE/game-host tests pass.
