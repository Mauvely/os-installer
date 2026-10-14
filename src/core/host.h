#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  host — what the installer reads from the live system it runs on.
//
//  Thin on purpose: each function gathers input (a file, or a command through
//  the runner) and hands it to the pure parsers in disks, enrolment and luks.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/config.h"
#include "core/disks.h"

#include <QList>
#include <QString>

class Runner;

namespace host {

/** The kernel command line, or an empty string. */
QString kernelCmdline(const Config &config);

/** Whether this is the installer's live system: `rd.live.image` is on the
 *  kernel command line. main() refuses to start anywhere else. */
bool isLiveSystem(const Config &config);

/** Whether the kernel lists a TPM 2.0. bootc's `tpm2-luks` cannot work without
 *  one, and finding that out after `--wipe` has run would cost the disk. */
bool hasTpm2(const Config &config);

/** Every whole disk, each with its exclusion decided. Empty, with `error` set,
 *  when lsblk fails. */
QList<disks::Disk> listDisks(Runner &runner, const Config &config, QString *error = nullptr);

}  // namespace host
