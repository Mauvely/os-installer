#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  disks — which whole disks the installer may offer, from `lsblk --json`.
//
//  Everything here is pure: it takes lsblk's output and the live medium's
//  identity and answers. The command itself runs in core/host.cpp, so the
//  rules that decide what can be wiped are tested without a disk.
//
//  The rules, in the order they are checked:
//    - only `type: disk` is a candidate; partitions, loops, ROMs and dm devices
//      are never offered on their own;
//    - zram is a `disk` to lsblk, and is not one;
//    - the live medium is excluded, found by its device or by its label;
//    - a disk with anything mounted, on itself or on any descendant (swap
//      counts), is excluded;
//    - read-only and empty disks are excluded.
// ─────────────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QList>
#include <QString>

namespace disks {

enum class Exclusion {
    None,
    LiveMedium,
    Mounted,
    ReadOnly,
    Empty,
    Virtual,  // zram
};

struct Disk {
    QString name;       // vda
    QString path;       // /dev/vda
    QString model;      // trimmed; often empty on virtual disks
    QString serial;
    QString transport;  // lsblk's TRAN: nvme, sata, usb … empty for virtio
    qint64 sizeBytes = 0;
    bool removable = false;
    Exclusion exclusion = Exclusion::None;

    bool eligible() const { return exclusion == Exclusion::None; }
    /** The model, or the kernel name when there is none. */
    QString title() const;
    /** `64 GB · NVME · /dev/nvme0n1`, the mono metadata line. */
    QString detail() const;
};

/** The arguments `parse()` expects lsblk to have been run with. */
QStringList lsblkArgs();

/**
 * Every whole disk in `json`, each with its exclusion decided.
 *
 * `liveSource` is the device dmsquash-live mounted the medium from (`findmnt -n
 * -o SOURCE /run/initramfs/live`), empty if nothing is mounted there; a match on
 * the disk or any of its partitions excludes the disk. `liveLabel` is the ISO's
 * volume label, matched the same way. Returns an empty list and sets `error`
 * when the JSON is not lsblk's.
 */
QList<Disk> parse(const QByteArray &json, const QString &liveSource,
                  const QString &liveLabel, QString *error = nullptr);

/** Only the disks that may be offered. */
QList<Disk> eligible(const QList<Disk> &all);

/** Decimal units, as drives are sold: 64 GB, 512 GB, 2 TB, 1.5 TB. */
QString humanSize(qint64 bytes);

}  // namespace disks
