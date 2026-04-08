/*
 * QuickConnectDialog - one-shot SSH connection without saving to profile.
 *
 * Use case: connect to an ad-hoc host once without cluttering the
 * Sessions list. Returns a fully populated ssh_config_t through cfg().
 */
#pragma once

#include "tscrt.h"

#include <QDialog>

class QLineEdit;
class QSpinBox;

class QuickConnectDialog : public QDialog {
    Q_OBJECT
public:
    explicit QuickConnectDialog(QWidget *parent = nullptr);

    ssh_config_t cfg() const;
    QString      sessionName() const;

private:
    QLineEdit *m_host = nullptr;
    QSpinBox  *m_port = nullptr;
    QLineEdit *m_user = nullptr;
    QLineEdit *m_pass = nullptr;
};
