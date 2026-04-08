/*
 * ButtonBar - bottom bar of clickable action buttons.
 *
 * Reads button definitions from profile_t.buttons[]. Empty slots are
 * skipped. Single click emits actionRequested(action_string). Double
 * click prompts for an interval and starts repeating that action until
 * the next double-click stops it.
 *
 * Two special slots are also rendered: Mark (toggle highlight), Loop
 * (run an arbitrary command at an interval). They emit dedicated
 * signals so the host tab/window can update its state.
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

signals:
    void actionRequested(const QString &actionString);
    void markRequested();
    void loopRequested();
    void helpRequested();
    void cmdWindowRequested();

private slots:
    void onButtonClicked();
    void onButtonDoubleClicked();
    void onRepeatTick();

private:
    QPushButton *makeAction(const QString &label, const QString &action);
    QPushButton *makeSpecial(const QString &label, const QString &css);
    void clearLayout();

    QHBoxLayout       *m_layout      = nullptr;
    QTimer            *m_repeatTimer = nullptr;
    QString            m_repeatAction;
    int                m_repeatInterval = 0;
    QPushButton       *m_repeatBtn = nullptr;
};

} // namespace tscrt
