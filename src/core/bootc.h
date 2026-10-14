#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  bootc — the install command, and what its output means.
//
//  The install runs `bootc install to-disk` from the live system itself, with
//  the OS image the ISO carries in containers-storage as its source. Running it
//  in a container (`podman run … <payload> bootc install`), the documented
//  route, hung for over ten minutes in P1 and was not pursued.
//
//  Run outside a container, bootc 1.16.13 leaves `/run/bootc/storage` mounted
//  from the new root, so its last step, `Closing root LUKS device`, fails with
//  the install already complete. `classify()` recognises exactly that ending
//  and nothing else; the caller then unmounts and closes by hand.
// ─────────────────────────────────────────────────────────────────────────────

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace bootc {

/**
 * The update reference written into the installed system, which is where
 * `bootc upgrade` looks next. **Undecided: T4 in brief 19** (the image
 * registry). `--skip-fetch-check` lets it name an image that exists nowhere
 * yet; the agent moves the machine to its collection's ring at the first
 * check-in. Change it here and nowhere else.
 */
inline QString targetImgref() { return QStringLiteral("registry.invalid/mauvely/os:stable"); }

/** `install to-disk …` for `bootc`, wiping `disk`. */
QStringList installArgs(const QString &payloadRef, const QString &disk);

enum class Outcome {
    Succeeded,    // exit 0
    LeakedMount,  // the known `Closing root LUKS device` failure: the install is complete
    Failed,       // anything else
};

/** Decides from bootc's exit code and its merged output. */
Outcome classify(int exitCode, const QString &output);

/** The command the leaked-mount workaround runs, in order. */
QList<QStringList> leakedMountWorkaround();

/**
 * The payload's reference, from `podman images --format json`. The installer
 * image's own storage holds nothing else, so exactly one named image is
 * expected; none, or more than one, is an error rather than a guess.
 */
QString payloadFromPodmanImages(const QByteArray &json, QString *error = nullptr);

}  // namespace bootc
