#!/usr/bin/env bash
# vm.sh install <iso> | boot | stop | ssh-live [cmd] | ssh-target [cmd]
#
# The VM the installer is tested in, under $P/vm: Secure Boot OVMF with the
# Microsoft and Red Hat keys enrolled, its own swtpm, and a blank 64 GB raw
# disk with the serial TARGET. Never a disk of this machine.
#
#   install <iso>  fresh varstore, TPM and disk; boots the ISO, the disk second
#   boot           the installed disk alone, same varstore and TPM
#   stop           quit qemu and swtpm
#
# Both run with -no-reboot, so the installer's Restart ends the VM. The screen
# is 1280x800 on a std VGA, driven and captured over QMP ($P/vm/qmp.sock) with
# qmp-ui.py; the serial console goes to $P/vm/serial-<mode>.log.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/env.sh"
D="$P/vm"; mkdir -p "$D"
cmd="${1:?install <iso>|boot|stop|ssh-live|ssh-target}"; shift || true

ssh_to() {
  local port="$1"; shift
  ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
    -o ConnectTimeout=5 -i "$SSHDIR/id_ed25519" -p "$port" root@127.0.0.1 "$@"
}

stop() {
  [ -S "$D/qmp.sock" ] && python3 "$here/qmp.py" "$D/qmp.sock" quit >/dev/null 2>&1 || true
  for _ in $(seq 20); do [ -f "$D/qemu.pid" ] && kill -0 "$(cat "$D/qemu.pid")" 2>/dev/null || break; sleep 0.5; done
  [ -f "$D/swtpm.pid" ] && kill "$(cat "$D/swtpm.pid")" 2>/dev/null || true
  rm -f "$D/qemu.pid" "$D/swtpm.pid"
}

run() {  # run <mode> <ssh-port> [extra qemu args...]
  local mode="$1" port="$2"; shift 2
  swtpm socket --tpm2 --tpmstate "dir=$D/tpm" --ctrl "type=unixio,path=$D/swtpm.sock" \
    --pid "file=$D/swtpm.pid" --log "file=$D/swtpm.log" --daemon
  qemu-system-x86_64 -name "p6-$mode" -machine q35,smm=on,accel=kvm -cpu host -smp 4 -m 8G \
    -global driver=cfi.pflash01,property=secure,value=on \
    -drive "if=pflash,format=raw,unit=0,file=/usr/share/edk2/x64/OVMF_CODE.secboot.4m.fd,readonly=on" \
    -drive "if=pflash,format=raw,unit=1,file=$D/vars.fd" \
    -chardev "socket,id=chrtpm,path=$D/swtpm.sock" -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-crb,tpmdev=tpm0 \
    -device VGA,xres=1280,yres=800 -display none \
    -device qemu-xhci -device usb-tablet \
    -serial "file:$D/serial-$mode.log" -qmp "unix:$D/qmp.sock,server,nowait" \
    "$@" \
    -drive "if=none,id=tgt,format=raw,file=$D/target.raw" -device virtio-blk-pci,drive=tgt,serial=TARGET,bootindex=2 \
    -netdev "user,id=n0,hostfwd=tcp:127.0.0.1:$port-:22" -device virtio-net-pci,netdev=n0 -device virtio-rng-pci \
    -no-reboot -pidfile "$D/qemu.pid" -daemonize
}

case "$cmd" in
install)
  iso="${1:?install <iso>}"
  stop
  rm -rf "$D/tpm" "$D/vars.fd" "$D/target.raw"; mkdir -p "$D/tpm"
  "$VENV/bin/virt-fw-vars" -i /usr/share/edk2/x64/OVMF_VARS.4m.fd -o "$D/vars.fd" \
    --enroll-redhat --secure-boot >/dev/null 2>&1
  truncate -s 64G "$D/target.raw"
  run install "$LIVE_SSH_PORT" \
    -drive "if=none,id=cd,format=raw,media=cdrom,readonly=on,file=$iso" -device ide-cd,drive=cd,bootindex=1
  ;;
boot) stop; run boot "$TARGET_SSH_PORT" ;;
stop) stop ;;
ssh-live) ssh_to "$LIVE_SSH_PORT" "$@" ;;
ssh-target) ssh_to "$TARGET_SSH_PORT" "$@" ;;
*) echo "unknown command: $cmd" >&2; exit 2 ;;
esac
