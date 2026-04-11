/*
 * LogSettingsDialog - dedicated dialog for log-related preferences.
 *
 * Split out of SettingsDialog so the day-to-day "where do my logs live"
 * question is one click away under the Logs top-level menu instead of
 * buried inside Preferences.
 */
#pragma once

#include "tscrt.h"

#include <QDialog>

class QLineEdit;

class LogSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit LogSettingsDialog(const profile_t &initial,
                               QWidget *parent = nullptr);

    profile_t profile() const { return m_p; }

private:
    void accept() override;

    profile_t  m_p{};
    QLineEdit *m_logDir = nullptr;
};
