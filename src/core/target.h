#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  target — the installed root, mounted, and the two files the agent reads.
//
//  On an ostree system the live `/etc` is the deployment's own `etc`:
//  `<root>/ostree/deploy/<os>/deploy/<checksum>.<serial>/etc`. A fresh install
//  has exactly one deployment, and that is where `/etc/mauvely/server` and
//  `/etc/mauvely/enrol` go. The agent (OS/agent) trims both and strips a
//  trailing slash from the server, then deletes `enrol` once it has enrolled.
//
//  The files must carry the label the policy expects in /etc, or the agent's
//  service cannot read and delete them. The live system runs with SELinux off,
//  so nothing labels them by default; `setfilesArgs()` labels them from the
//  installed system's own policy.
// ─────────────────────────────────────────────────────────────────────────────

#include <QString>
#include <QStringList>

namespace target {

/** The one deployment directory under a mounted root; empty, with `error` set,
 *  when there is none or more than one. */
QString findDeployment(const QString &root, QString *error = nullptr);

/**
 * Writes `etc/mauvely/server` (0644) and `etc/mauvely/enrol` (0600) under the
 * deployment, each one line with a trailing newline, and syncs them. The
 * directory is 0755. Owners are whoever runs this: root, on the live system.
 */
bool writeEnrolment(const QString &deployment, const QString &server, const QString &token,
                    QString *error = nullptr);

/** SELINUXTYPE from the deployment's `etc/selinux/config`; `targeted` when the
 *  file does not say. */
QString selinuxType(const QString &deployment);

/** `setfiles` arguments that label `etc/mauvely` from the deployment's policy. */
QStringList setfilesArgs(const QString &deployment);

}  // namespace target
