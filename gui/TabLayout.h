/*
 * TabLayout - per-role tab restore list.
 *
 * When cross-instance tab links exist, TSCRT auto-spawns a second
 * process so the original two-window environment is restored. To know
 * *what* to open in each window, each role (A = primary, B = spawned)
 * persists an ordered list of (session_name, tab_title) pairs to
 * "<base>/layout_<R>.lst". Plain text, one entry per line, UTF-8.
 *
 * Line format:
 *   "<session>\t<title>"   if the user renamed the tab
 *   "<session>"            if the tab still uses the session-derived name
 *
 * The legacy "session-only" form is still accepted on load.
 */
#pragma once

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace tscrt { namespace TabLayout {

/// (session_name, tab_title). title is empty when the tab kept its default.
using Entry = QPair<QString, QString>;

QString filePath(char role);                       // "A" / "B"

/// Load entries with full (session, title) information. Lines without a
/// tab character produce entries whose title field is empty.
QVector<Entry> loadEntries(char role);

/// Backward-compatible: just the session names.
QStringList load(char role);

void saveEntries(char role, const QVector<Entry> &entries);
void save(char role, const QStringList &names);
void clear(char role);

} } // namespace tscrt::TabLayout
