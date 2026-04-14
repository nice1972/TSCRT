/*
 * TabLayout - per-role tab restore list.
 *
 * When cross-instance tab links exist, TSCRT auto-spawns a second
 * process so the original two-window environment is restored. To know
 * *what* to open in each window, each role (A = primary, B = spawned)
 * persists an ordered list of session names to "<base>/layout_<R>.lst".
 * Plain text, one name per line, UTF-8.
 */
#pragma once

#include <QString>
#include <QStringList>

namespace tscrt { namespace TabLayout {

QString filePath(char role);                       // "A" / "B"
QStringList load(char role);
void save(char role, const QStringList &names);
void clear(char role);

} } // namespace tscrt::TabLayout
