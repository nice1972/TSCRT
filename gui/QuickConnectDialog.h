/*
 * QuickConnectDialog - one-shot SSH or Serial connection without
 * saving to the profile.
 *
 * The user picks a session type, fills the relevant fields, and the
 * caller pulls back a complete session_entry_t.
 */
#pragma once

#include "tscrt.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QStackedWidget;

class QuickConnectDialog : public QDialog {
    Q_OBJECT
public:
    explicit QuickConnectDialog(QWidget *parent = nullptr);

    /// Build a session_entry_t from the current form values. The name
    /// field is auto-generated based on the connection details.
    session_entry_t entry() const;

private slots:
    void typeChanged(int index);

private:
    QComboBox      *m_type      = nullptr;
    QStackedWidget *m_stack     = nullptr;

    // SSH page
    QLineEdit      *m_host      = nullptr;
    QSpinBox       *m_port      = nullptr;
    QLineEdit      *m_user      = nullptr;
    QLineEdit      *m_pass      = nullptr;

    // Serial page
    QLineEdit      *m_serDevice = nullptr;
    QComboBox      *m_serBaud   = nullptr;
    QComboBox      *m_serData   = nullptr;
    QComboBox      *m_serStop   = nullptr;
    QComboBox      *m_serParity = nullptr;
    QComboBox      *m_serFlow   = nullptr;
};
