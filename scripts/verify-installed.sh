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
#   with RECOVERY_KEY set (read off the installer's screen), that it opens the
#   recovery keyslot with every token disabled, and that a wrong key does not
#   if P5's seal unit is in the payload, what it did: PCR 7, the karg
#
# The token's value is never printed: its length and format are. Nor is the
# recovery key.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:?verify-installed.sh <out.txt>}"

{ printf 'RECOVERY_KEY=%q\n' "${RECOVERY_KEY:-}"; cat <<'REMOTE'; } | "$here/vm.sh" ssh-target bash -s | tee "$out"
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
journalctl -b --no-pager -o cat -u 'systemd-cryptsetup@*' | head -4
echo "password prompts this boot: $(journalctl -b --no-pager -o cat | grep -ci 'please enter passphrase')"
findmnt -no SOURCE,FSTYPE /sysroot 2>/dev/null || findmnt -no SOURCE,FSTYPE /

section "LUKS slots and tokens"
cryptsetup luksDump "$dev" | sed -n '/^Keyslots:/,/^Digests:/p'
echo "keyslots: $(cryptsetup luksDump "$dev" | sed -n '/^Keyslots:/,/^Tokens:/p' | grep -cE '^\s+[0-9]+: luks2')"
echo "systemd-tpm2 tokens: $(cryptsetup luksDump "$dev" | grep -c 'systemd-tpm2')"
echo "systemd-recovery tokens: $(cryptsetup luksDump "$dev" | grep -c 'systemd-recovery')"
echo "TPM2 token PCRs: [$(cryptsetup luksDump "$dev" | sed -n 's/^\s*tpm2-hash-pcrs:\s*//p' | xargs)]"
if [ -n "$RECOVERY_KEY" ]; then
  slot="$(cryptsetup luksDump "$dev" | awk '/systemd-recovery/ {f=1} f && /Keyslot:/ {print $2; exit}')"
  # Tokens off and one slot named: the TPM cannot be what opens it.
  printf %s "$RECOVERY_KEY" | cryptsetup open --test-passphrase --disable-external-tokens \
    --key-slot "$slot" --key-file=- "$dev" 2>/dev/null
  echo "recovery key from the screen opens keyslot $slot: $([ $? = 0 ] && echo yes || echo NO)"
  printf %s "cbdefghi-cbdefghi-cbdefghi-cbdefghi-cbdefghi-cbdefghi-cbdefghi-cbdefghi" |
    cryptsetup open --test-passphrase --disable-external-tokens --key-slot "$slot" --key-file=- "$dev" 2>/dev/null
  echo "a wrong key opens keyslot $slot: $([ $? = 0 ] && echo YES || echo no)"
fi

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
echo "display-manager, plasmalogin, sddm: $(systemctl is-active display-manager.service plasmalogin.service sddm.service 2>/dev/null | paste -sd" ")"
test -e /etc/plasma-setup-done && echo "/etc/plasma-setup-done exists" || echo "/etc/plasma-setup-done absent"
grep -E '^(NAME|VERSION)=' /etc/os-release
if systemctl cat mauvely-seal.service >/dev/null 2>&1; then
  echo "mauvely-seal.service: $(systemctl show -p ActiveState,Result --value mauvely-seal.service | paste -sd' ')"
  journalctl -b --no-pager -o cat -u mauvely-seal.service | tail -6
  echo "headless=true in this boot's kargs: $(grep -qw headless=true /proc/cmdline && echo yes || echo no)"
  echo "headless=true in the next boot's kargs: $(rpm-ostree kargs 2>/dev/null | grep -qw headless=true && echo yes || echo no)"
else
  echo "no mauvely-seal.service in this payload"
fi
rpm-ostree status 2>/dev/null | head -8 || bootc status | head -8
REMOTE
