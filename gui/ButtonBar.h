/*
 * ButtonBar - bottom bar of clickable action buttons.
 *
 * Layout: [user buttons...] [stretch] [loop] [mark] [?]
 *
 * - User-defined buttons come from profile_t.buttons[]; empty slots
 *   are skipped. Click emits actionRequested(action_string).
 *   Right-click emits buttonEditRequested(slot_index) so the host can
 *   pop up an editor for that slot.
 * - Loop / mark are special buttons that emit their own signals.
 */
#pragma once

#include "tscrt.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QHBoxLayout;
class QPushButton;
class QTimer;

namespace tscrt {

class ButtonBar : public QWidget {
    Q_OBJECT
public:
    explicit ButtonBar(QWidget *parent = nullptr);

    void setButtons(const button_t buttons[MAX_BUTTONS]);

    /// Update the visual state of the "loop" button so the user can
    /// tell at a glance whether a loop is currently ticking. Has no
    /// effect on signal routing — that lives in SessionTab.
    void setLoopRunning(bool running);

signals:
    void actionRequested(const QString &actionString);
    void buttonEditRequested(int slotIndex);
    void markClicked();           // left click
    void markRightClicked();      // right click
    void loopClicked();           // left click
    void loopRightClicked();      // right click
    void helpRequested();

private slots:
    void onButtonClicked();
    void onButtonContextMenu(const QPoint &pos);
    void onRepeatTick();

private:
    QPushButton *makeAction(int slot, const QString &label, const QString &action);
    QPushButton *makeSpecial(const QString &label, const QString &css);
    void clearLayout();

    QHBoxLayout       *m_layout      = nullptr;
    QTimer            *m_repeatTimer = nullptr;
    QString            m_repeatAction;
    int                m_repeatInterval = 0;
    QPushButton       *m_repeatBtn = nullptr;
    QPushButton       *m_loopBtn = nullptr;
    bool               m_loopRunning = false;
};

} // namespace tscrt
