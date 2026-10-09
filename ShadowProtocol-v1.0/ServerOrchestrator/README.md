# Shadow Protocol Server Orchestrator

This package is the host-side launch boundary for a Shadow Protocol Unreal dedicated server.

It exists because the gameplay backend requires a short-lived, single-use node attestation before a production server can register. The HMAC signing key belongs to the orchestrator/host and must never be placed in the Unreal project, packaged game, launch command line, logs, or client configuration.

## Trust flow

1. A game-server host receives a launch assignment with a server id, region, public route, capacity and network build.
2. The orchestrator signs a short-lived attestation using `ORCHESTRATOR_ATTESTATION_SECRET`.
3. The launcher starts the Unreal dedicated server with non-secret routing values on the command line.
4. `SP_NODE_ATTESTATION` and `SERVER_REGISTRATION_SECRET` are passed only in the child environment.
5. The Unreal server registers with the gameplay backend.
6. The backend consumes the attestation JTI once, issues a per-node credential, and the Unreal server clears the one-time attestation from memory after successful registration.
7. Heartbeat, admission, drain, release and credential rotation use the per-node credential rather than the orchestrator signing secret.

## Required host environment

Copy `.env.example` into your host secret/configuration system. Do not commit a populated file.

Required secrets:

- `ORCHESTRATOR_ATTESTATION_SECRET` — must exactly match the gameplay backend verifier secret.
- `SERVER_REGISTRATION_SECRET` — must exactly match the gameplay backend registration bootstrap secret.

Required routing configuration:

- `GAME_SERVER_EXECUTABLE`
- `SP_BACKEND_URL`
- `SP_SERVER_ID`
- `SP_REGION`
- `SP_PUBLIC_HOST`
- `SP_PUBLIC_PORT`

Defaults:

- `SP_CAPACITY=10`
- `SP_NETWORK_BUILD=SP-1.0.1`
- `GAME_SERVER_MAP=/Game/Maps/Embassy/Embassy_P`
- `SERVER_ATTESTATION_TTL_MS=60000`

Optional extra Unreal flags can be supplied as a JSON string array in `SP_SERVER_EXTRA_ARGS_JSON`. The launcher never invokes a shell.

## Build and run

```bash
npm install --no-audit --no-fund
npm run typecheck
npm test
npm run build
npm start
```

The launcher intentionally does not provide a cloud provider implementation. The same package can run on a VM, bare-metal game host, or container host once a packaged Unreal dedicated-server executable exists. Render continues to host the control-plane API and identity gateway; the latency-sensitive UDP Unreal server belongs on a game-server-capable host.

## Current validation boundary

CI validates the attestation format, HMAC signature, input bounds and secret isolation. UE5.6/UHT, a packaged dedicated-server executable, real UDP reachability, orchestrator host lifecycle and 10-client runtime soak testing remain external runtime gates.
