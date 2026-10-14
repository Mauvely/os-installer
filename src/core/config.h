#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  Config — every path the installer reads or writes on the live system.
//
//  One struct, so the tests can point each of them at a temporary directory and
//  nothing in the logic has a hard-coded `/run` or `/sys` to trip over. The
//  defaults are the live system's.
// ─────────────────────────────────────────────────────────────────────────────

#include <QString>

struct Config {
    /** The kernel arguments. `mauvely.server=<url>` overrides the default server. */
    QString procCmdline = QStringLiteral("/proc/cmdline");
    /** Where the kernel lists TPMs. bootc's `tpm2-luks` needs a TPM 2.0. */
    QString sysClassTpm = QStringLiteral("/sys/class/tpm");
    /** The full log. On the live system, so in RAM; it never reaches the target. */
    QString logPath = QStringLiteral("/var/log/mauvely-installer.log");
    /** Where the installed root is mounted to write the enrolment files. */
    QString mountPoint = QStringLiteral("/run/mauvely-installer/target");
    /** The dm name the installed root is opened under. bootc uses `root`. */
    QString mapperName = QStringLiteral("mauvely-target");
    /** dmsquash-live mounts the install medium here. */
    QString liveMountPoint = QStringLiteral("/run/initramfs/live");
    /** The ISO's volume label, from image/iso.yaml. Also marks the live medium
     *  when it is not mounted, as with `rd.live.ram`. */
    QString liveLabel = QStringLiteral("Mauvely-OS-Installer");
};
