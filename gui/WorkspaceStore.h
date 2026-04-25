/*
 * WorkspaceStore - on-disk store for named session workspaces.
 *
 * A "workspace" captures the entire multi-instance / multi-window tab
 * layout that was visible when the user invoked Save: every connected
 * TSCRT process's role and tab list, plus the full tab_links table. On
 * launch the first MainWindow consults the "_last" marker to decide
 * which workspace (if any) to auto-restore.
 *
 * Files live under "<home>/<TSCRT_DIR_NAME>/workspaces/":
 *   <name>.json    one JSON document per workspace
 *   _last          single-line text file holding the last-saved name
 *
 * The cluster snapshot is built by the calling process from its own
 * state plus LinkBroker's cached peerTabs/peerRole — no extra IPC is
 * needed since peers already publish their tab lists continuously.
 */
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace tscrt { namespace WorkspaceStore {

/// Directory holding all workspace files. Created on first access.
QString dir();

/// Lists workspace names (without ".json"), sorted by saved_at descending.
QStringList list();

bool exists(const QString &name);

/// Loads <name>.json. Returns an empty object on miss / parse error.
QJsonObject load(const QString &name);

/// Persists <name>.json. The caller-supplied object is wrapped: we add
/// "name" and "saved_at" if missing. Returns false on disk error.
bool save(const QString &name, QJsonObject workspace);

/// Deletes <name>.json. If <name> was the "_last", clears the marker too.
bool remove(const QString &name);

/// Last-saved workspace tracking. lastName() returns "" if absent.
QString lastName();
void    setLastName(const QString &name);
void    clearLastName();

} } // namespace tscrt::WorkspaceStore
