#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run this installer as root." >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUNDLE_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

if ! command -v node >/dev/null 2>&1; then
  echo "Node.js 22+ is required before installation." >&2
  exit 1
fi

NODE_MAJOR="$(node -p "Number(process.versions.node.split('.')[0])")"
if [[ "${NODE_MAJOR}" -lt 22 ]]; then
  echo "Node.js 22+ is required. Found: $(node --version)" >&2
  exit 1
fi

if [[ ! -d "${BUNDLE_ROOT}/server" || ! -d "${BUNDLE_ROOT}/orchestrator/dist" ]]; then
  echo "Bundle is incomplete: expected server/ and orchestrator/dist/." >&2
  exit 1
fi

if ! id shadowprotocol >/dev/null 2>&1; then
  useradd --system --home /var/lib/shadow-protocol --create-home --shell /usr/sbin/nologin shadowprotocol
fi

install -d -o shadowprotocol -g shadowprotocol -m 0750 /opt/shadow-protocol
install -d -o shadowprotocol -g shadowprotocol -m 0750 /opt/shadow-protocol/server
install -d -o shadowprotocol -g shadowprotocol -m 0750 /opt/shadow-protocol/orchestrator
install -d -o shadowprotocol -g shadowprotocol -m 0750 /var/log/shadow-protocol
install -d -o shadowprotocol -g shadowprotocol -m 0750 /var/lib/shadow-protocol
install -d -o root -g shadowprotocol -m 0750 /etc/shadow-protocol

cp -a "${BUNDLE_ROOT}/server/." /opt/shadow-protocol/server/
cp -a "${BUNDLE_ROOT}/orchestrator/." /opt/shadow-protocol/orchestrator/
chown -R shadowprotocol:shadowprotocol /opt/shadow-protocol

find /opt/shadow-protocol/server -type f -name 'ShadowProtocolServer*' -exec chmod 0750 {} + || true

if [[ ! -f /etc/shadow-protocol/server.env ]]; then
  install -o root -g shadowprotocol -m 0640 "${SCRIPT_DIR}/server.env.example" /etc/shadow-protocol/server.env
  echo "Created /etc/shadow-protocol/server.env. Add the real secrets and public host before starting."
else
  echo "Keeping existing /etc/shadow-protocol/server.env."
fi

install -o root -g root -m 0644 "${SCRIPT_DIR}/shadow-protocol-server.service" /etc/systemd/system/shadow-protocol-server.service
systemctl daemon-reload
systemctl enable shadow-protocol-server.service

echo
echo "Installed Shadow Protocol dedicated-server bundle."
echo "Next:"
echo "  1. Edit /etc/shadow-protocol/server.env"
echo "  2. Confirm GAME_SERVER_EXECUTABLE points to the packaged Linux server binary"
echo "  3. Allow UDP 7777 in the cloud firewall and host firewall"
echo "  4. systemctl start shadow-protocol-server"
echo "  5. journalctl -u shadow-protocol-server -f"
