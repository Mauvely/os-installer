# os-installer

The Mauvely OS installer, in Qt. It is a minimal fork of `Programs/app-base`:
it keeps `brand.h` and the theme, and drops the rest.

It runs full screen on a live system booted from the ISO. You enter the server
and an enrolment token, then choose a disk. It runs `bootc install to-disk`
with TPM-unlocked LUKS, adds a recovery key, and writes the two files the agent
reads at first boot. The installer never talks to the server and never enrols.

The design is in brief 19:
`Website/website-internal/docs/launch/plans/19-mauvely-os.md` in the Mauvely
superproject. This repo is its phase P6.

## Layout

| Path | Holds |
|---|---|
| `src/core/` | The logic, with no widgets. Each header opens with what it decides and why |
| `src/core/runner.h` | The process seam. Every command goes through `Runner::run()`; the tests use `tests/fakerunner.h` |
| `src/core/installer.cpp` | The sequence: re-check the disk, find the payload, bootc, the recovery key, the two files, unmount and close. Every command and its exit status is logged to `/var/log/mauvely-installer.log` |
| `src/core/disks.cpp` | What may be offered for wiping, from `lsblk --json`: whole disks only, never the live medium, never anything mounted |
| `src/core/bootc.h` | The install arguments, the target reference (`registry.invalid/…`, T4) and the failure classification |
| `src/core/luks.cpp` | Finding the new LUKS partition, and reading the recovery key out of `systemd-cryptenroll` |
| `src/core/target.cpp` | Writing `/etc/mauvely/{server,enrol}` into the deployment's `etc` |
| `src/core/enrolment.cpp`, `cmdline.cpp` | The server and the token format. The `mauvely.server=` and `rd.live.image` kernel arguments |
| `src/ui/` | `pages.cpp` has one page per step. `window.cpp` does all the wiring. `style.cpp` styles them on top of Theme |
| `src/main.cpp` | Refuses to start without `rd.live.image`, so the binary never lists a developer's disks |
| `tests/` | `tst_logic` (parsers and rules), `tst_installer` (the sequence, command by command), `tst_flow` (the real window, clicked through offscreen). `fixtures/` holds canned lsblk, podman, bootc and systemd-cryptenroll output |
| `Containerfile` | The live system. A builder stage builds and tests the app, then fedora-bootc 44 adds Qt, cage, seatd and the kiosk unit |
| `image/` | `iso.yaml` (the ISO's label and kernel arguments) and `mauvely-installer.service` (the kiosk on tty1) |
| `scripts/` | Image, ISO and VM test tooling. `env.sh` holds the paths, ports and the pinned image-builder |
| `scripts/test-*/` | **TEST ONLY** layers: root SSH with a throwaway key, and the local server on the installer's kernel command line |
| `proof/` | What the VM runs showed: screens, the live log, and `verify-installed.sh`'s reports |

### What came from app-base

At app-base `9e30321`:

- **`src/core/brand.h`**: byte-identical, and pinned. It is the ninth copy in the superproject's table.
- **`src/core/tokens.h`**: byte-identical, but not pinned.
- **`src/core/theme.{h,cpp}`**: a modified copy. Organisation branding is removed: `setBrandRamp`, `clearBrandRamp`, `isBranded` and their members are gone, and `brandStep()` reads `brand::violet()`. The installer runs before any organisation is known. Do not add it to a copies table.
- **`resources/fonts/`**: Baloo 2 (ExtraBold and Regular), Inter and JetBrains Mono. Phosphor, `tokens.cpp` and the widget kit were not copied.

## Building and testing

```sh
cmake -S . -B ~/.cache/mauvely-os/p6/build -G Ninja
cmake --build ~/.cache/mauvely-os/p6/build -j24
ctest --test-dir ~/.cache/mauvely-os/p6/build
```

- The tests need no root and no disk, and start no process.
- `INSTALLER_SHOTS=<dir>` makes `tst_flow` save each page as a PNG.
- The image build runs the same tests (`podman build`).

**The VM test** runs the real thing: an ISO in qemu with OVMF Secure Boot,
swtpm and a blank 64 GB raw disk. Everything big lives under
`~/.cache/mauvely-os/p6`.

| Step | Command |
|---|---|
| 1. The Fedora Cloud VM that builds ISOs | `scripts/builder-vm.sh prepare` once, then `start` |
| 2. The images | `scripts/build-images.sh` builds the installer, its test layer and the test payload. Set `PAYLOAD_BASE` and `PAYLOAD_REF` to choose the payload |
| 3. The ISO | `scripts/build-iso.sh [installer-ref] [out.iso]` runs image-builder `bootc-generic-iso` as root in the builder VM |
| 4. Install | `scripts/vm.sh install <iso>`, then drive it with `scripts/qmp-ui.py $P/vm/qmp.sock shot\|click\|type\|key`. `vm.sh ssh-live` reads the live log |
| 5. Boot the installed disk alone | `scripts/vm.sh boot` |
| 6. Check it | `RECOVERY_KEY=<off the screen> scripts/verify-installed.sh proof/<name>.txt` |
| 7. Tidy | `scripts/vm.sh stop`, `scripts/builder-vm.sh stop` |

Ports: HTTP 8110–8119 and SSH 2240–2249 belong to P6. The installed system
has no serial console, so wait for its SSH, not for its log.

## How the install works

Each of these is something a change would get wrong.

- **bootc runs on the live system itself.** Its source is
  `containers-storage:<payload>`, the one image in the live storage. In P1,
  running it in a container hung.
- **bootc 1.16.13 always fails at the end.** It leaves `/run/bootc/storage`
  mounted, so `Closing root LUKS device` fails after a complete install.
  - `bootc::classify()` recognises exactly that ending. The installer then
    runs `umount /run/bootc/storage` and `cryptsetup close root`.
  - Any other non-zero exit is a failure.
  - A clean exit 0 skips the workaround, so a fixed bootc needs no change here.
- **`tpm2-luks` binds the TPM to no PCRs.** That is what lets the live system
  open the volume to add the recovery key and write the two files.
  - The image's `mauvely-seal.service` re-binds the TPM to PCR 7 and drops
    `headless=true` at first boot.
  - The installer must never do either, and must never write
    `/etc/plasma-setup-done`.
- **The two files go in the deployment's `etc`.**
  - They are `ostree/deploy/default/deploy/<checksum>.0/etc/mauvely/`, where
    `server` is 0644 and `enrol` is 0600, both owned by root.
  - The live system runs with `selinux=0`. The label therefore comes from the
    target's own policy, through `setfiles -r <deployment>
    <deployment>/etc/selinux/targeted/contexts/files/file_contexts`. That
    gives `etc_t`, which `restorecon -n` on the installed machine agrees with.
- **The recovery key is a secret.**
  - `systemd-cryptenroll`'s output never reaches the log, even on failure.
  - The key is shown once and cleared from the window at Finish.
  - It is never written to disk.
- **Testing a recovery key needs the tokens off.** With a TPM token bound to
  no PCRs, `cryptsetup open --test-passphrase` unlocks through the token and
  accepts any key. Use `--disable-external-tokens --key-slot <recovery slot>`.

## Rules

- **`src/core/brand.h` is a pinned copy of app-base's.** Never edit it here.
  Change it in app-base and copy it.
- **Wiping a disk needs an explicit confirmation** naming the disk's model and
  size. There is no default disk.
- **The enrolment token is never logged.** It is written only to the target's
  `/etc/mauvely/enrol`, mode 0600.
- **The recovery key is never logged and never written to disk.**
- **Test only against VM disks or loop devices.** Never test against a disk on
  the machine you develop on.
  - Never run the app, `bootc install`, `cryptsetup` or `systemd-cryptenroll`
    against the host.
  - On the host, the app runs only inside the tests, with the runner faked.
- **The live system has no way in but its screen.** It has no sshd and no root
  password.
  - SSH exists only in the `scripts/test-*` layers.
  - Never ship an ISO built from them, and never push any of these images to a
    registry.
- **UI copy follows the design system's CONTENT FUNDAMENTALS.**
  - Use sentence case.
  - Headlines are short sentences with a full stop.
  - Buttons are verbs.
  - Say it once.
  - Use mono ` · ` for metadata.

## State, 2026-10-14

**Done and verified.** The app, the image, the ISO build, and two full VM runs
(`proof/`):

- **The spike payload** (`vm-*`).
- **P5's image** (`p5-*`), which also exercises the seal.

Both runs showed:

- Secure Boot was on.
- The installed disk booted alone with no unlock prompt.
- LUKS held a TPM2 slot and a recovery slot. The key on the screen opened the
  recovery slot, and a wrong key did not.
- `server` and `enrol` had the right mode, owner and `etc_t` label.
- Plasma Setup was reached.
- Neither the token nor the key was in the live log or journal.

With P5, the token named PCR 7 after the first boot, and the recovery slot
survived.

The production live image, with no test layer, was booted as far as the disk
page:

- The default server showed.
- Nothing listened on SSH.

**Never run:**

- real hardware, a USB stick or an NVMe disk; these are covered only by the
  lsblk fixtures;
- a VM with more than one candidate disk;
- Wi-Fi;
- a non-US keyboard;
- the no-TPM and failure pages on the live system; these are covered only
  offscreen.

**P7 ran on this installer.** The agent enrolled with the two files it wrote,
and they carried a real token. The proof is `OS/image/proof/p7/`.

Follow-ups are in `HANDOFF.md`.

## Handing off

`HANDOFF.md` is overwritten each session. It holds what is half-done, what was
tried and rejected, and what compiles but has never run.
