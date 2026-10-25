#!/bin/bash
# Download a build artifact from the latest-dev prerelease.
# Usage: ./scripts/dl-release.sh <filename>   e.g. qimgv-x64_v1.0.5.7z
set -u
F="${1:?usage: dl-release.sh <filename>}"
DEST="${2:-release-check2/$F}"
mkdir -p "$(dirname "$DEST")"
cd "$(dirname "$DEST")" || exit 9
curl.exe -L --ssl-no-revoke -C - --retry 60 --retry-delay 3 --retry-all-errors --max-time 3000 \
  -o "$(basename "$F")" \
  "https://github.com/BRaysMK/qimgv/releases/download/latest-dev/$F"
echo "curl_exit=$?"
ls -la "$(basename "$F")" | awk '{print $5, $9}'
