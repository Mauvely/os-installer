# Shared by the scripts in this directory. Source it, do not run it.
#
# Everything big lives under ~/.cache/mauvely-os/p6, never in the repo or /tmp.
# The ports are P6's own (HTTP 8110-8119, SSH 2240-2249) so these VMs never
# collide with the image's (P5) or with the P1 spike's "machine".
export W="${MAUVELY_OS_CACHE:-$HOME/.cache/mauvely-os}"
export P="$W/p6"                     # every disk, vars file, TPM state and ISO
export SSHDIR="$P/ssh"
export SEEDDIR="$P/seed"
export SEED_PORT=8110                # cloud-init NoCloud for the builder VM
export BUILDER_SSH_PORT=2240         # the Fedora Cloud VM that builds the ISO
export LIVE_SSH_PORT=2241            # the live system, test installer image only
export TARGET_SSH_PORT=2242          # the installed machine, test payload only
export CLOUD_QCOW2="$W/dl/Fedora-Cloud-Base-Generic-44-1.7.x86_64.qcow2"
export VENV="$W/venv"                # virt-firmware, for the Secure Boot varstore

# The installer image, and the OS image the ISO carries as its payload.
export INSTALLER_REF="${INSTALLER_REF:-localhost/mauvely-installer:p6}"
export INSTALLER_TEST_REF="${INSTALLER_TEST_REF:-localhost/mauvely-installer:p6-test}"
export PAYLOAD_REF="${PAYLOAD_REF:-localhost/mauvely-os:p6-test}"

# image-builder, pinned. bootc-generic-iso is the newer, less used path, and the
# compat container is slated for retirement, so a moving tag is a risk.
# v86.0.0 (osbuild 194), the release current on 2026-10-14. Move it deliberately.
export IMAGE_BUILDER="${IMAGE_BUILDER:-ghcr.io/osbuild/image-builder-cli:v86.0.0@sha256:0979bbacc334d618bb000254528af65263f9b85cef72edc5be47ab5c56ca9e9b}"
