#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  luks — finding the new LUKS volume, and reading the recovery key.
//
//  bootc's `tpm2-luks` wipes every keyslot but its TPM one and binds that to no
//  PCRs (P1, S1 traps 2 and 3). The live system can therefore still unlock it,
//  and the installer uses that window to add a recovery key. Binding the TPM
//  slot to PCR 7 and dropping `headless=true` are the image's first-boot jobs,
//  not the installer's.
// ─────────────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace luks {

/** `lsblk` arguments for `partition()`, after the disk's path. */
QStringList lsblkArgs(const QString &disk);

/** The one `crypto_LUKS` partition on the disk, from `lsblk --json`; empty, with
 *  `error` set, when there is none or more than one. */
QString partition(const QByteArray &lsblkJson, QString *error = nullptr);

/** `systemd-cryptenroll` arguments that add a recovery key, unlocking with the
 *  TPM. The key is printed on its output. */
QStringList addRecoveryKeyArgs(const QString &device);

/**
 * The recovery key in `systemd-cryptenroll`'s output: eight groups of eight
 * modhex characters (`cbdefghijklnrtuv`) joined by dashes. Empty unless exactly
 * one distinct key appears.
 */
QString recoveryKey(const QString &output);

}  // namespace luks
