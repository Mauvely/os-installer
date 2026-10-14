#!/usr/bin/env bash
# qemu-img is not installed on the host (and there is no root to install it), so
# run it from a throwaway Fedora container. Paths under $HOME work unchanged.
set -euo pipefail
img=localhost/mauvely-tools:qemu-img
if ! podman image exists "$img"; then
  printf 'FROM quay.io/fedora/fedora:44\nRUN dnf -y install qemu-img && dnf clean all\n' |
    podman build -q -t "$img" -f - . >/dev/null
fi
exec podman run --rm -v "$HOME:$HOME" "$img" qemu-img "$@"
