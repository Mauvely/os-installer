#!/usr/bin/env bash
# build-iso.sh [installer-ref] [out.iso]
#
# Builds the installer ISO with image-builder's `bootc-generic-iso`, as root
# inside the builder VM (builder-vm.sh start first): the installer image
# becomes the live system, and $PAYLOAD_REF rides in its containers-storage as
# the OS that `bootc install` writes. Both images are copied from this
# machine's podman into the VM's root storage when the VM does not already hold
# the same image id. The ISO lands in $P/iso/.
#
# Defaults to the test installer image, which is what the VM test boots. For
# an ISO without the test layer: build-iso.sh "$INSTALLER_REF" <out.iso>.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/env.sh"
vm="$here/builder-vm.sh"
inst="${1:-$INSTALLER_TEST_REF}"
out="${2:-$P/iso/mauvely-installer-p6-test.iso}"
mkdir -p "$(dirname "$out")"

for ref in "$inst" "$PAYLOAD_REF"; do
  id="$(podman image inspect --format '{{.Id}}' "$ref")"
  there="$("$vm" ssh "sudo podman image inspect --format '{{.Id}}' $ref 2>/dev/null || true")"
  if [ "$id" != "$there" ]; then
    echo "copying $ref to the builder VM"
    podman save "$ref" | "$vm" ssh "sudo podman load -q"
  fi
done

"$vm" ssh "sudo bash -s" <<REMOTE
set -euo pipefail
# /var, not /tmp: /tmp in the Cloud image is a 4 GB tmpfs, and the ISO is not.
rm -rf /var/p6/out && mkdir -p /var/p6/out /var/p6/cache
podman run --rm --privileged --security-opt label=type:unconfined_t \
  -v /var/lib/containers/storage:/var/lib/containers/storage \
  -v /var/p6/out:/output -v /var/p6/cache:/root/.cache \
  $IMAGE_BUILDER build \
  --bootc-ref $inst \
  --bootc-installer-payload-ref $PAYLOAD_REF \
  --bootc-default-fs xfs bootc-generic-iso
find /var/p6/out -name '*.iso' -printf '%s %p\n'
REMOTE

iso="$("$vm" ssh "sudo find /var/p6/out -name '*.iso' | head -1")"
[ -n "$iso" ] || { echo "image-builder made no ISO" >&2; exit 1; }
"$vm" ssh "sudo cat '$iso'" > "$out.part" && mv "$out.part" "$out"
ls -l "$out"
