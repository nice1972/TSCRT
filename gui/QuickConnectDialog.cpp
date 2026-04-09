#include "QuickConnectDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstring>

namespace {

void putStr(char *dst, size_t cap, const QString &s)
{
    const QByteArray b = s.toLocal8Bit();
    const int n = qMin<int>(static_cast<int>(cap) - 1, b.size());
    memcpy(dst, b.constData(), n);
    dst[n] = '\0';
}

} // namespace

QuickConnectDialog::QuickConnectDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Quick Connect"));
    setModal(true);

    auto *root = new QVBoxLayout(this);

    auto *typeRow = new QFormLayout;
    m_type = new QComboBox(this);
    m_type->addItem(QStringLiteral("SSH"),    int(SESSION_TYPE_SSH));
    m_type->addItem(QStringLiteral("Serial"), int(SESSION_TYPE_SERIAL));
    typeRow->addRow(tr("&Type:"), m_type);
    root->addLayout(typeRow);

    m_stack = new QStackedWidget(this);

    // ---- SSH page ----
    auto *sshPage = new QWidget(this);
    auto *sshForm = new QFormLayout(sshPage);

    m_host = new QLineEdit(sshPage);
    m_host->setPlaceholderText(QStringLiteral("hostname or IP"));
    m_host->setMaxLength(MAX_HOST_LEN - 1);
    sshForm->addRow(tr("&Host:"), m_host);

    m_port = new QSpinBox(sshPage);
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    sshForm->addRow(tr("&Port:"), m_port);

    m_user = new QLineEdit(sshPage);
    m_user->setMaxLength(MAX_USER_LEN - 1);
    sshForm->addRow(tr("&Username:"), m_user);

    m_pass = new QLineEdit(sshPage);
    m_pass->setEchoMode(QLineEdit::Password);
    m_pass->setMaxLength(MAX_PASS_LEN - 1);
    m_pass->setPlaceholderText(tr("password or key passphrase"));
    sshForm->addRow(tr("Pass&word:"), m_pass);

    auto *keyRow    = new QWidget(sshPage);
    auto *keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->setSpacing(4);
    m_keyfile = new QLineEdit(keyRow);
    m_keyfile->setMaxLength(MAX_PATH_LEN - 1);
    m_keyfile->setPlaceholderText(tr("private key file (optional)"));
    auto *browseBtn = new QToolButton(keyRow);
    browseBtn->setText(QStringLiteral("..."));
    browseBtn->setToolTip(tr("Browse for private key file"));
    keyLayout->addWidget(m_keyfile, 1);
    keyLayout->addWidget(browseBtn, 0);
    connect(browseBtn, &QToolButton::clicked, this, [this] {
        const QString start = m_keyfile->text().isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
              + QStringLiteral("/.ssh")
            : m_keyfile->text();
        const QString f = QFileDialog::getOpenFileName(
            this, tr("Select private key"), start,
            tr("All files (*)"));
        if (!f.isEmpty())
            m_keyfile->setText(QDir::toNativeSeparators(f));
    });
    sshForm->addRow(tr("&Key file:"), keyRow);

    m_stack->addWidget(sshPage);

    // ---- Serial page ----
    auto *serPage = new QWidget(this);
    auto *serForm = new QFormLayout(serPage);

    m_serDevice = new QLineEdit(serPage);
    m_serDevice->setPlaceholderText(QStringLiteral("COM3"));
    m_serDevice->setMaxLength(MAX_DEVICE_LEN - 1);
    serForm->addRow(tr("&Device:"), m_serDevice);

    m_serBaud = new QComboBox(serPage);
    for (int b : { 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 })
        m_serBaud->addItem(QString::number(b), b);
    m_serBaud->setCurrentText(QStringLiteral("115200"));
    m_serBaud->setEditable(true);
    serForm->addRow(tr("&Baud rate:"), m_serBaud);

    m_serData = new QComboBox(serPage);
    for (int d : { 5, 6, 7, 8 }) m_serData->addItem(QString::number(d), d);
    m_serData->setCurrentText(QStringLiteral("8"));
    serForm->addRow(tr("Data &bits:"), m_serData);

    m_serStop = new QComboBox(serPage);
    m_serStop->addItem(QStringLiteral("1"), 1);
    m_serStop->addItem(QStringLiteral("2"), 2);
    serForm->addRow(tr("&Stop bits:"), m_serStop);

    m_serParity = new QComboBox(serPage);
    m_serParity->addItem(tr("None"), int(PARITY_NONE));
    m_serParity->addItem(tr("Odd"),  int(PARITY_ODD));
    m_serParity->addItem(tr("Even"), int(PARITY_EVEN));
    serForm->addRow(tr("Pa&rity:"), m_serParity);

    m_serFlow = new QComboBox(serPage);
    m_serFlow->addItem(tr("None"),     int(FLOW_NONE));
    m_serFlow->addItem(tr("Hardware"), int(FLOW_HARDWARE));
    m_serFlow->addItem(tr("Software"), int(FLOW_SOFTWARE));
    serForm->addRow(tr("&Flow control:"), m_serFlow);

    m_stack->addWidget(serPage);

    root->addWidget(m_stack);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                    this);
    bb->button(QDialogButtonBox::Ok)->setText(tr("&Connect"));
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);

    connect(m_type, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &QuickConnectDialog::typeChanged);
    typeChanged(0);

    m_host->setFocus();
}

void QuickConnectDialog::typeChanged(int /*index*/)
{
    m_stack->setCurrentIndex(m_type->currentData().toInt());
}

session_entry_t QuickConnectDialog::entry() const
{
    session_entry_t s{};
    s.type = session_type_t(m_type->currentData().toInt());

    if (s.type == SESSION_TYPE_SSH) {
        const QString name =
            QStringLiteral("%1@%2").arg(m_user->text(), m_host->text());
        putStr(s.name,         sizeof(s.name),         name);
        putStr(s.ssh.host,     sizeof(s.ssh.host),     m_host->text());
        putStr(s.ssh.username, sizeof(s.ssh.username), m_user->text());
        putStr(s.ssh.password, sizeof(s.ssh.password), m_pass->text());
        putStr(s.ssh.keyfile,  sizeof(s.ssh.keyfile),  m_keyfile->text());
        s.ssh.port = m_port->value();
    } else {
        const QString name = QStringLiteral("%1 @ %2 baud")
                                 .arg(m_serDevice->text(),
                                      m_serBaud->currentText());
        putStr(s.name,          sizeof(s.name),          name);
        putStr(s.serial.device, sizeof(s.serial.device), m_serDevice->text());
        s.serial.baudrate    = m_serBaud->currentText().toInt();
        s.serial.databits    = m_serData->currentData().toInt();
        s.serial.stopbits    = m_serStop->currentData().toInt();
        s.serial.parity      = parity_t(m_serParity->currentData().toInt());
        s.serial.flowcontrol = flowcontrol_t(m_serFlow->currentData().toInt());
    }
    return s;
}
