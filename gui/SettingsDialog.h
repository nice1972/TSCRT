/*
 * SettingsDialog - top-level configuration window.
 *
 * Tabs:
 *   Common    : log_dir, terminal_type, encoding, scrollback
 *   Buttons   : 20 slots (label + action)
 *   Startup   : per-session startup commands
 *   Triggers  : per-session pattern -> action
 *   Periodic  : per-session interval task
 *
 * (Sessions live in their own SessionManagerDialog now.)
 *
 * Operates on a copy of profile_t. Caller calls profile() after exec()
 * returns Accepted, then writes via profile_save().
 */
#pragma once

#include "tscrt.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QTabWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const profile_t &initial, QWidget *parent = nullptr);

    profile_t profile() const { return m_p; }

private slots:
    void addStartup();
    void deleteStartup();

    void addTrigger();
    void deleteTrigger();

    void addPeriodic();
    void deletePeriodic();

    void onSessionFilterChanged();

    // SMTP
    void onSmtpTest();

private:
    QWidget *buildCommonTab();
    QWidget *buildButtonsTab();
    QWidget *buildStartupTab();
    QWidget *buildTriggersTab();
    QWidget *buildPeriodicTab();
    QWidget *buildSmtpTab();

    void commitCommon();
    void commitButtons();
    void commitSmtp();
    void accept() override;

    void reloadStartupTable();
    void reloadTriggerTable();
    void reloadPeriodicTable();

    QString currentFilterSession() const;

    profile_t        m_p{};

    // Common
    QLineEdit       *m_logDir       = nullptr;
    QComboBox       *m_termType     = nullptr;
    QComboBox       *m_encoding     = nullptr;
    QSpinBox        *m_scrollback   = nullptr;

    // Buttons (20 rows)
    QTableWidget    *m_btnTable     = nullptr;

    // Startup / Triggers / Periodic
    QComboBox       *m_startSess    = nullptr;
    QTableWidget    *m_startTable   = nullptr;
    QComboBox       *m_trigSess     = nullptr;
    QTableWidget    *m_trigTable    = nullptr;
    QComboBox       *m_perSess      = nullptr;
    QTableWidget    *m_perTable     = nullptr;

    // SMTP
    QLineEdit       *m_smtpHost     = nullptr;
    QSpinBox        *m_smtpPort     = nullptr;
    QComboBox       *m_smtpSecurity = nullptr;
    QLineEdit       *m_smtpUser     = nullptr;
    QLineEdit       *m_smtpPass     = nullptr;
    QLineEdit       *m_smtpFromAddr = nullptr;
    QLineEdit       *m_smtpFromName = nullptr;
    QSpinBox        *m_smtpTimeout  = nullptr;
};
