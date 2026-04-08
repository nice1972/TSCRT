/*
 * SessionEditDialog - modal editor for a single session entry.
 *
 * Form switches between SSH and Serial fields based on the type combo.
 * Returns the edited session via session() once accepted.
 */
#pragma once

#include "tscrt.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QStackedWidget;

class SessionEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionEditDialog(QWidget *parent = nullptr);

    void                  setSession(const session_entry_t &s);
    session_entry_t       session() const;

private slots:
    void browseKeyfile();
    void typeChanged(int index);

private:
    void buildUi();

    QLineEdit      *m_name      = nullptr;
    QComboBox      *m_type      = nullptr;
    QStackedWidget *m_stack     = nullptr;

    // SSH fields
    QLineEdit      *m_sshHost   = nullptr;
    QSpinBox       *m_sshPort   = nullptr;
    QLineEdit      *m_sshUser   = nullptr;
    QLineEdit      *m_sshPass   = nullptr;
    QLineEdit      *m_sshKey    = nullptr;

    // Serial fields
    QLineEdit      *m_serDevice = nullptr;
    QComboBox      *m_serBaud   = nullptr;
    QComboBox      *m_serData   = nullptr;
    QComboBox      *m_serStop   = nullptr;
    QComboBox      *m_serParity = nullptr;
    QComboBox      *m_serFlow   = nullptr;
};
