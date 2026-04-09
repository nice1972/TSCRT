/*
 * SessionHistory - shared helpers that map a session entry to its
 * persistent command-history file.
 *
 * History is stored under <profile.base_dir>\history\<sanitized>.history.
 * The same sanitization rule is reused everywhere so save / load /
 * delete agree on the path.
 */
#pragma once

#include "tscrt.h"

#include <QString>

namespace tscrt {

QString historyDirFor(const profile_t &profile);
QString historyPathFor(const profile_t &profile, const QString &sessionName);

/// Walk the history directory and delete any *.history files that no
/// longer correspond to a session in the profile. Called after profile
/// changes (delete, rename, manager save).
void pruneOrphanHistoryFiles(const profile_t &profile);

} // namespace tscrt
