#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  copyprogress — how far the copy of the OS to the disk has got.
//
//  bootc prints `layers already present: 0; layers needed: 266 (7.5 GB)` when
//  the copy starts and nothing more until it ends: its own bar is drawn only on
//  a terminal. So the installer measures instead. The total comes from that
//  line, and the progress from the kernel's count of sectors written to the
//  target disk, `/sys/block/<disk>/stat`. LUKS sits on a partition of the disk,
//  and the kernel counts a partition's writes on its whole disk as well, so the
//  disk's own counter covers the encrypted writes too.
//
//  Everything here is pure; core/host.cpp reads the file. The count is of bytes
//  that reached the disk, which is not the image's size: filesystem metadata
//  and ostree's sharing move it either way, and the page cache delays it. The
//  bar therefore stops short of full until the step really ends.
// ─────────────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QString>

namespace copyprogress {

/** The bytes to fetch, from a line of bootc's output: 7.5 GB, 512 MiB, 980 kB,
 *  1 byte. 0 when `line` is not that line, the unit is not one bootc prints,
 *  or there is nothing to fetch. */
qint64 totalFromLine(const QString &line);

/** Sectors written, field 7 of `/sys/block/<disk>/stat`, always counted in 512
 *  bytes whatever the disk's own sector size. -1 when `stat` is not that. */
qint64 sectorsWritten(const QByteArray &stat);

/** The directory under `/sys/block` for a device node: `vda` for `/dev/vda`,
 *  `cciss!c0d0` for `/dev/cciss/c0d0`. Empty when `devicePath` is not under
 *  `/dev/`. */
QString sysfsName(const QString &devicePath);

/** What has been written since the copy began, against what bootc said it
 *  needs. */
class Meter {
public:
    /** The first reading is the baseline; each later one moves `written()`
     *  forward and never back. -1, an unreadable counter, is ignored. */
    void sample(qint64 sectors);
    /** From `totalFromLine()`; 0 means not known. */
    void setTotal(qint64 bytes) { m_total = bytes > 0 ? bytes : 0; }

    /** Bytes since the baseline. */
    qint64 written() const { return m_written; }
    qint64 total() const { return m_total; }

    /** 0 up to 0.99 once both the total and a counter are known, never more
     *  while the step runs. -1 when either is missing: the bar then sweeps. */
    double fraction() const;
    /** `3.1 of 7.5 GB` against a total, `3.1 GB written` without one, and
     *  empty when there is nothing to say yet: no counter, or without a total
     *  too little to tell the copy from the partitioning before it. */
    QString text() const;

private:
    bool m_haveBase = false;
    qint64 m_base = 0;
    qint64 m_written = 0;
    qint64 m_total = 0;
};

}  // namespace copyprogress
