#!/usr/bin/env bash
# build-images.sh   Builds, with rootless podman on this machine:
#
#   $INSTALLER_REF       the installer image, from the Containerfile at the root
#   $INSTALLER_TEST_REF  that, plus the local server karg and root SSH (TEST ONLY)
#   $PAYLOAD_REF         the OS image the ISO carries, plus sshd (TEST ONLY)
#
# The payload's base is localhost/mauvely-os:spike: P1's build of OS/image's
# Containerfile, unchanged. Nothing here pushes anywhere.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/env.sh"
repo="$(cd "$here/.." && pwd)"

[ -f "$SSHDIR/id_ed25519.pub" ] || { echo "run builder-vm.sh prepare first (it makes the key)" >&2; exit 1; }

podman build -f "$repo/Containerfile" -t "$INSTALLER_REF" "$repo"

ctx="$P/test-installer-ctx"; mkdir -p "$ctx"
cp "$SSHDIR/id_ed25519.pub" "$ctx/authorized_keys"
podman build -f "$here/test-installer/Containerfile" --build-arg "BASE=$INSTALLER_REF" \
  -t "$INSTALLER_TEST_REF" "$ctx"

if ! podman image exists "$PAYLOAD_REF"; then
  ctx="$P/test-payload-ctx"; mkdir -p "$ctx"
  cp "$SSHDIR/id_ed25519.pub" "$ctx/authorized_keys"
  podman build -f "$here/test-payload/Containerfile" --build-arg "BASE=${PAYLOAD_BASE:-localhost/mauvely-os:spike}" \
    -t "$PAYLOAD_REF" "$ctx"
fi
