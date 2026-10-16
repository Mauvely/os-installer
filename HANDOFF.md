# Handoff

## 2026-10-16: progress for the copy

Timur tested on Proxmox and took the install for hung. bootc prints
`layers already present: 0; layers needed: 266 (7.5 GB)` and nothing more until
the copy ends, so the page showed that line and a sweeping bar for minutes.

**What the page shows now.** While "Copy Mauvely OS to vda" runs, the bar fills
and the line under it reads `3.1 of 7.5 GB`, in the same mono faint text as
before. The numbers are bytes written to the target disk against the total in
bootc's line. The bar stops at 99% and the text at `7.5 of 7.5 GB` until the
step ends. Screenshot: `proof/copy-progress.png`, from `tst_flow`.

**What changed.**

- `src/core/copyprogress.{h,cpp}` is new and pure: the size parse
  (`B`, `kB`/`MB`/`GB`/`TB`, `KiB`…`TiB`, `byte(s)`), the stat parse, the
  `/dev/vda` to `vda` mapping and `Meter`, which holds the baseline, the total,
  the clamp and the text.
- `Config::sysBlock` and `host::sectorsWritten()` read
  `/sys/block/<disk>/stat`. The tests point `sysBlock` at a temp directory.
  That is the seam, not `Runner`: this is a file, like `sysClassTpm`, and
  `cat` once a second would only start a process to read one line.
- `ui/window.cpp` owns a 1 s `QTimer`. It takes the baseline when the copy step
  starts, samples every second, and stops at the next step, success or failure.
  `Installer` did not change.
- `BusyBar` is now `ProgressBar`, with `setFraction()`: negative sweeps, as
  before. `InstallPage::setCopy()` takes the fraction and the line.

**Fallbacks, none of them an error.**

- No total yet: bootc's last line and a sweeping bar, as before.
- Total but no readable counter: the same, so bootc's `layers needed` line stays.
- Counter but no total: `3.1 GB written` under a sweeping bar, once more than
  100 MB has been written. The partitioning and the LUKS header write less than
  that and would otherwise flash a number before the copy starts.

**One change beyond the copy step.** The steps after it now clear the line and
sweep with nothing under the bar. Before, the line kept bootc's last output
through "Add a recovery key" and "Save the server and token", which on the
leaked-mount ending is bootc's own `error: … Closing root LUKS device failed`.

**Verified.** All three suites pass, 124 tests: `tst_logic` 101 (31 new: the
size parse, the stat parse, the sysfs name, the file read, and `Meter`'s delta,
clamp, monotonic and fallback behaviour), `tst_installer` 13, `tst_flow` 10 (3
new: progress following a faked counter, the two fallbacks). `tst_flow` ran six
times in a row without a failure. The page was rendered offscreen at 1280×800
and read for clipping. Nothing touched a disk.

**Never run: an install with it.** Open questions for the next VM run:

- **Bytes written against the total.** The total is bootc's layer size. The
  kernel counts what reaches the disk: xfs metadata, ostree's checkout and its
  sharing move it, and the page cache delays it, so the bar may lag and then
  jump when the copy flushes. If it stalls at 70% for good, the clamp hides
  nothing and the total needs a factor. `cat /sys/block/vda/stat` before and
  after, from a test layer's SSH, gives the number.
- **That the whole disk's counter includes LUKS's writes.** The kernel adds a
  partition's I/O to its disk, and dm-crypt submits to the partition, but that
  is from the kernel's accounting and not seen on a disk.
- **bootc's wording.** The parse is written against the one line in
  `proof/live-install-p5-payload.log` and the units glib's `g_format_size`
  prints, which is what that format looks like. bootc's source was not read. A
  different line leaves the page on the `written` fallback.

## 2026-10-14: plan 19 phase P6

The installer is built and verified end to end in a VM. `CLAUDE.md` describes the shape of the repo, how the install works and
how to run the VM test. This file covers what that does not.

**Since T4 (2026-10-14) the target reference is `ghcr.io/mauvely/os:stable`.**
Every ISO built before that, the proof runs' included, writes the
`registry.invalid/…` placeholder.

## The runs

Every run booted the ISO under OVMF Secure Boot with swtpm, with a blank 64 GB
disk. Runs 1 and 2 drove the UI over QMP with a fake well-formed token and
`mauvely.server=http://10.0.2.2:8798`. Each then booted the installed disk
alone.

| Run | Installer ISO | Payload | Proof |
|---|---|---|---|
| 1 | `localhost/mauvely-installer:p6-test` | `localhost/mauvely-os:p6-test`: P1's `:spike` plus the test sshd layer | `proof/vm-*`, `live-boot-spike-payload.txt`, `installed-spike-payload.txt` |
| 2 | the same | `localhost/mauvely-os:p6-p5-test`: P5's `:p5` plus the same layer | `proof/p5-*`, `live-boot-p5-payload.txt`, `live-install-p5-payload.log`, `installed-p5-payload-boot{1,2}.txt` |
| 3 | `localhost/mauvely-installer:p6`, the production image with no test layer | as run 2, but not installed | `proof/live-*.png`, `live-boot-production.txt` |
| 4 | `localhost/mauvely-installer:t4`: HEAD `7218b1b`, production, no test layer | `ghcr.io/mauvely/os:stable`, `sha256:46945ca1…`, as published | `proof/stable-*.png` |

**Run 4 is the first full install of what a customer would get** (2026-10-14):
the production installer with the published, signed `stable` image as its
payload. Server `https://devices.mauvely.dev` and a fake well-formed token,
so nothing was enrolled. It copied 266 layers (7.5 GB) to `vda` in about
four minutes, showed the recovery key, and the installed disk then booted
alone twice to Plasma Setup with no unlock prompt, the second time after the
seal's first boot. Neither system has SSH, so the LUKS slots, the files and
the agent were not inspected; runs 2 and P7 cover those. The ISO is
`~/.cache/mauvely-os/p6/iso/mauvely-os-stable.iso`, built by
`build-iso.sh localhost/mauvely-installer:t4` with
`PAYLOAD_REF=ghcr.io/mauvely/os:stable`. It is the one to hand anybody who
wants to try the OS in a VM: Secure Boot OVMF and a TPM are required.

**Run 3 was a boot check of the production live system.** The kiosk started
under Secure Boot. Server showed `https://devices.mauvely.com`, because there
was no `mauvely.server=`. A token was entered, and the disk page listed `vda`.
SSH was refused at key exchange, because nothing listens on port 22. The run
stopped before Erase.

**Run 2 used `:p5` with my own test layer, not `:p5-ssh`.** `:p5-ssh`
enables sshd but bakes in no key: its layer is `systemctl enable sshd`
alone. The installer adds none either, so there would be no way in to check
the installed machine. The test layer bakes in P6's throwaway key, which
`verify-installed.sh` logs in with. The OS under test is `:p5` unchanged,
seal included.

What run 2 showed:

- **The first boot.** The seal enrolled a TPM2 token on PCR 7 as slot 2 and
  wiped slot 1. Recovery slot 0 was kept, and the key on the screen still
  opened it.
- **The second boot.** There was no prompt, and the token was still on
  PCR 7. The seal reported "already names PCRs", and `headless=true` was gone
  from the kernel arguments.

The second boot's screen was byte-identical to the first's, so it is not
committed separately.

**The install took 2 min 40 s in the VM.** Most of that was the 7.5 GB payload
being deployed. bootc hit the known failure at "Closing root LUKS device" in
both runs, and the workaround finished the install both times.

The ISOs and images are still on this machine:

- the ISOs, about 3.5 GB each, in `~/.cache/mauvely-os/p6/iso/`;
- the images, in local podman storage.

**Run 2's installed machine is kept** in `~/.cache/mauvely-os/p6/vm-p5-installed/`:
its disk, varstore and swtpm state. It boots with `vm.sh boot` once that
directory is moved back to `vm/`. Its recovery key is the one in
`proof/p5-6-recovery.png`.

## Deviations from the brief

- **`theme.{h,cpp}` is a modified copy**, with branding removed. Only
  `brand.h` and `tokens.h` are byte-identical.
- **Welcome checks for a TPM 2.0** (`/sys/class/tpm/tpm*/tpm_version_major`).
  Without one, Start stays disabled and a note says why. The page cannot do
  its job without a TPM, so failing at bootc would be worse.
- **`main()` refuses to start without `rd.live.image`.** This guards a
  developer's own disks.
- **The production image has no `passwd -d root` and no sshd**, unlike the
  spike. The kiosk is a system unit and needs no login. SSH lives only in the
  test layers.
- **`--filesystem xfs` is kept.** The image's `50-mauvely.toml` now says xfs
  too, so the flag is redundant but harmless.

## Tried and rejected

- **The spike's empty root password on the live system.** It is described
  above.
- **`cryptsetup open --test-passphrase` alone to check the recovery key.** It
  accepted a wrong key through the PCR-less TPM token.
  `verify-installed.sh` uses `--disable-external-tokens --key-slot`.
- **Waiting for the installed boot on the serial log.** The installed kernel
  arguments have no `console=ttyS0`, so `vm.sh boot` waits for SSH instead.
- **`restorecon` or `chcon` from the live system.** Both use the running
  policy, and the live system runs with `selinux=0`. `setfiles -r` against the
  target's own `file_contexts` gives the label that policy expects.

## Never run

- **Real hardware.** No firmware with vendor keys, no discrete TPM, no USB
  live stick, and no NVMe or SATA disk. Those cases are covered by the lsblk
  fixtures only.
- **An install from the production ISO.** Run 3 stopped at the disk page.
  The app is the same build in all three runs. Only the test layer differs.
- **More than one candidate disk in the VM.**
- **The failure page, Start again and the no-TPM Welcome on a live system.**
  They are covered offscreen in `tst_flow` only.
- **`mauvelyd` enrolling with the written files.** No website-devices was
  running, and the brief leaves that to P7.

## Follow-ups

- **An online token check.** There is no endpoint that checks a token without
  spending it, so the installer checks the format only. An expired token
  surfaces at the first check-in.
- **Wi-Fi.** Only wired DHCP through NetworkManager works today. The install
  itself needs no network, but the enrolment at first boot does.
- **Recovery-key escrow (M3).** Today the key exists only on the screen and on
  the user's paper.
- **Keyboard layout.** cage starts with the default (US) layout, which matters
  for typing a token on QWERTZ or AZERTY.
- **Exporting the log.** `/var/log/mauvely-installer.log` lives in the live
  system's RAM, so a failed install's log is lost on restart. The failure
  page shows the last 12 lines and the path.
- **The UEFI boot entry is labelled "Fedora".** bootupd creates it, so this is
  an OS/image concern.
- **When bootc stops leaking the mount**, the workaround stops running on its
  own. Delete it once a fixed bootc is the payload's.
- **Whether `/etc/mauvely` wants its own SELinux type** depends on mauvelyd's
  policy. Today it is plain `etc_t`.
- **Cosmetic issues.**
  - A virtio disk's metadata reads "VIRTIO".
  - The failure page's last tail line repeats the message above it.
  - The image build logs a non-fatal `dracut-install: ERROR: installing '/root'`.
- **For the lead.** `brand.h` here is the ninth pinned copy in the
  superproject's table.
