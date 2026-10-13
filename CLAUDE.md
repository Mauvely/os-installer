# os-installer

The Mauvely OS installer, in Qt. It is a minimal fork of `Programs/app-base`:
it keeps `brand.h` and the theme, and drops the rest.

The design is in brief 19:
`Website/website-internal/docs/launch/plans/19-mauvely-os.md` in the Mauvely
superproject.

## Rules

- **`src/core/brand.h` is a pinned copy of app-base's.** Never edit it here.
  Change it in app-base and copy it.
- **Wiping a disk needs an explicit confirmation** naming the disk's model and
  size. There is no default disk.
- **The enrolment token is never logged.** It is written only to the target's
  `/etc/mauvely/enrol`, mode 0600.
- **Test only against VM disks or loop devices.** Never test against a disk on
  the machine you develop on.

## Handing off

`HANDOFF.md` is overwritten each session. It holds what is half-done, what was
tried and rejected, and what compiles but has never run.
