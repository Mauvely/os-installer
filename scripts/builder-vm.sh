#!/usr/bin/env bash
# builder-vm.sh prepare|start|stop|ssh [command...]
#
# The VM that builds the ISO. image-builder needs root (loop devices, mounts),
# and this machine has no sudo, so it runs as root inside a throwaway Fedora
# Cloud VM, as P1's spike did. The VM is a copy-on-write overlay on the
# downloaded Fedora Cloud image; the download itself is never written to.
#
#   prepare  ssh key, cloud-init seed, the overlay disk (idempotent)
#   start    serve the seed on $SEED_PORT and boot the VM, headless
#   stop     ACPI power-off, then the seed server
#   ssh      a shell, or one command, as the `fedora` user (passwordless sudo)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/env.sh"
B="$P/builder"
cmd="${1:?prepare|start|stop|ssh}"; shift || true

case "$cmd" in
prepare)
  mkdir -p "$B" "$SSHDIR" "$SEEDDIR"
  [ -f "$SSHDIR/id_ed25519" ] || ssh-keygen -q -t ed25519 -N '' -C mauvely-os-p6 -f "$SSHDIR/id_ed25519"
  cat > "$SEEDDIR/meta-data" <<META
instance-id: mauvely-os-p6-builder-1
local-hostname: p6-builder
META
  cat > "$SEEDDIR/user-data" <<USERDATA
#cloud-config
users:
  - name: fedora
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    ssh_authorized_keys:
      - $(cat "$SSHDIR/id_ed25519.pub")
ssh_pwauth: false
growpart:
  mode: auto
  devices: ['/']
resize_rootfs: true
USERDATA
  [ -f "$B/builder.qcow2" ] ||
    "$here/qemu-img.sh" create -q -f qcow2 -b "$CLOUD_QCOW2" -F qcow2 "$B/builder.qcow2" 100G
  echo "prepared in $B"
  ;;
start)
  if ! ss -ltn | grep -q ":$SEED_PORT "; then
    (cd "$SEEDDIR" && nohup python3 -m http.server "$SEED_PORT" --bind 127.0.0.1 \
      >"$B/seed-http.log" 2>&1 & echo $! >"$B/seed-http.pid")
  fi
  exec qemu-system-x86_64 -name p6-builder -machine q35,accel=kvm -cpu host -smp 8 -m 8G \
    -display none -serial "file:$B/serial.log" -qmp "unix:$B/qmp.sock,server,nowait" \
    -drive "if=none,id=sys,format=qcow2,file=$B/builder.qcow2" -device virtio-blk-pci,drive=sys,bootindex=1 \
    -netdev "user,id=n0,hostfwd=tcp:127.0.0.1:$BUILDER_SSH_PORT-:22" -device virtio-net-pci,netdev=n0 \
    -smbios "type=1,serial=ds=nocloud;s=http://10.0.2.2:$SEED_PORT/" \
    -device virtio-rng-pci -pidfile "$B/qemu.pid" -daemonize
  ;;
stop)
  [ -S "$B/qmp.sock" ] && python3 "$here/qmp.py" "$B/qmp.sock" system_powerdown >/dev/null 2>&1 || true
  for _ in $(seq 90); do [ -f "$B/qemu.pid" ] && kill -0 "$(cat "$B/qemu.pid")" 2>/dev/null || break; sleep 1; done
  [ -f "$B/qemu.pid" ] && kill -0 "$(cat "$B/qemu.pid")" 2>/dev/null && kill "$(cat "$B/qemu.pid")" || true
  [ -f "$B/seed-http.pid" ] && kill "$(cat "$B/seed-http.pid")" 2>/dev/null || true
  rm -f "$B/seed-http.pid"
  ;;
ssh)
  exec ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
    -o ConnectTimeout=5 -o ServerAliveInterval=30 -i "$SSHDIR/id_ed25519" -p "$BUILDER_SSH_PORT" \
    fedora@127.0.0.1 "$@"
  ;;
*) echo "unknown command: $cmd" >&2; exit 2 ;;
esac
