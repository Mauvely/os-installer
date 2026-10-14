#!/usr/bin/env bash
# verify-installed.sh <out.txt>
#
# Run against the installed machine, booted alone (vm.sh boot), over the test
# payload's SSH. Prints, and saves to <out.txt>, each thing P6 promises:
#
#   Secure Boot is on
#   the root unlocked with no prompt, from the TPM
#   LUKS holds a TPM2 slot and a recovery slot, and nothing else
#   /etc/mauvely/server and /etc/mauvely/enrol: owner, mode, SELinux label,
#   and that the label is the one the policy expects (restorecon has nothing to do)
#
# The token's value is never printed: its length and format are.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:?verify-installed.sh <out.txt>}"

"$here/vm.sh" ssh-target bash -s <<'REMOTE' | tee "$out"
set -uo pipefail
section() { printf '\n== %s\n' "$1"; }

section "Secure Boot"
mokutil --sb-state 2>/dev/null || true
od -An -t u1 -j4 -N1 /sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c \
  | awk '{print "SecureBoot efivar:", ($1 == 1 ? "1 (enabled)" : $1)}'
journalctl -k -b -g 'secureboot' --no-pager -o cat | head -3

section "Unlock"
dev="$(lsblk -nrpo NAME,FSTYPE | awk '$2 == "crypto_LUKS" {print $1; exit}')"
echo "LUKS partition: $dev"
journalctl -b --no-pager -o cat -t systemd-cryptsetup | head -10
echo "password prompts this boot: $(journalctl -b --no-pager -o cat | grep -ci 'please enter passphrase')"
findmnt -no SOURCE,FSTYPE /sysroot 2>/dev/null || findmnt -no SOURCE,FSTYPE /

section "LUKS slots and tokens"
cryptsetup luksDump "$dev" | sed -n '/^Keyslots:/,/^Digests:/p'
echo "keyslots: $(cryptsetup luksDump "$dev" | sed -n '/^Keyslots:/,/^Tokens:/p' | grep -cE '^\s+[0-9]+: luks2')"
echo "systemd-tpm2 tokens: $(cryptsetup luksDump "$dev" | grep -c 'systemd-tpm2')"
echo "systemd-recovery tokens: $(cryptsetup luksDump "$dev" | grep -c 'systemd-recovery')"

section "Enrolment files"
getenforce
ls -lZd /etc/mauvely
ls -lZ /etc/mauvely
stat -c '%n  mode %a  owner %U:%G  label %C' /etc/mauvely/server /etc/mauvely/enrol
echo "server: $(cat /etc/mauvely/server)"
tok="$(cat /etc/mauvely/enrol)"
echo "enrol: ${#tok} characters, prefix ${tok:0:4}, $( [[ $tok =~ ^mvd_[A-Za-z0-9_-]{43}$ ]] && echo well-formed || echo MALFORMED)"
echo "policy expects: $(matchpathcon /etc/mauvely/server /etc/mauvely/enrol | tr '\n' ' ')"
echo "restorecon -nv (empty means every label is already right):"
restorecon -nRv /etc/mauvely
echo "(end of restorecon)"

section "First boot"
systemctl is-active display-manager.service plasmalogin.service sddm.service 2>/dev/null | paste -sd' '
test -e /etc/plasma-setup-done && echo "/etc/plasma-setup-done exists" || echo "/etc/plasma-setup-done absent"
systemctl list-unit-files --no-pager | grep -iE 'mauvely|seal' || echo "no mauvely or seal units in this payload"
rpm-ostree status 2>/dev/null | head -8 || bootc status | head -8
REMOTE
