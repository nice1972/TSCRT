#include "QuickConnectDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cstring>

QuickConnectDialog::QuickConnectDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Quick Connect"));
    setModal(true);

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_host = new QLineEdit(this);
    m_host->setPlaceholderText(QStringLiteral("hostname or IP"));
    m_host->setMaxLength(MAX_HOST_LEN - 1);
    form->addRow(tr("&Host:"), m_host);

    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    form->addRow(tr("&Port:"), m_port);

    m_user = new QLineEdit(this);
    m_user->setMaxLength(MAX_USER_LEN - 1);
    form->addRow(tr("&Username:"), m_user);

    m_pass = new QLineEdit(this);
    m_pass->setEchoMode(QLineEdit::Password);
    m_pass->setMaxLength(MAX_PASS_LEN - 1);
    form->addRow(tr("Pass&word:"), m_pass);

    root->addLayout(form);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                    this);
    bb->button(QDialogButtonBox::Ok)->setText(tr("&Connect"));
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);

    m_host->setFocus();
}

ssh_config_t QuickConnectDialog::cfg() const
{
    ssh_config_t c{};
    auto put = [](char *dst, size_t cap, const QString &s) {
        const QByteArray b = s.toLocal8Bit();
        const int n = qMin<int>(int(cap) - 1, b.size());
        memcpy(dst, b.constData(), n);
        dst[n] = '\0';
    };
    put(c.host,     sizeof(c.host),     m_host->text());
    put(c.username, sizeof(c.username), m_user->text());
    put(c.password, sizeof(c.password), m_pass->text());
    c.port = m_port->value();
    return c;
}

QString QuickConnectDialog::sessionName() const
{
    return QStringLiteral("%1@%2").arg(m_user->text(), m_host->text());
}
