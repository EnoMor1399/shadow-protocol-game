#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-7777}"

if ! [[ "${PORT}" =~ ^[0-9]+$ ]] || (( PORT < 1 || PORT > 65535 )); then
  echo "Invalid game port: ${PORT}" >&2
  exit 1
fi

if command -v ufw >/dev/null 2>&1; then
  echo "Configuring UFW. SSH remains allowed before enabling the firewall."
  sudo ufw allow OpenSSH
  sudo ufw allow "${PORT}/udp" comment 'Shadow Protocol game traffic'
  sudo ufw status verbose
else
  echo "UFW is not installed. Allow UDP ${PORT} in your provider firewall/security group."
fi
