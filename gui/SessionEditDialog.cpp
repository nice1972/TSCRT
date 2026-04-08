#include "SessionEditDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cstring>

namespace {

void setStr(char *dst, size_t cap, const QString &src)
{
    const QByteArray b = src.toLocal8Bit();
    const int n = qMin<int>(static_cast<int>(cap) - 1, b.size());
    memcpy(dst, b.constData(), n);
    dst[n] = '\0';
}

} // namespace

SessionEditDialog::SessionEditDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Session"));
    setModal(true);
    buildUi();
}

void SessionEditDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *header = new QFormLayout;
    m_name = new QLineEdit(this);
    m_name->setMaxLength(MAX_NAME_LEN - 1);
    header->addRow(tr("&Name:"), m_name);

    m_type = new QComboBox(this);
    m_type->addItem(tr("SSH"),    int(SESSION_TYPE_SSH));
    m_type->addItem(tr("Serial"), int(SESSION_TYPE_SERIAL));
    header->addRow(tr("&Type:"), m_type);
    root->addLayout(header);

    m_stack = new QStackedWidget(this);

    // ---- SSH page ----
    auto *sshPage   = new QWidget(this);
    auto *sshForm   = new QFormLayout(sshPage);
    m_sshHost = new QLineEdit(sshPage);
    m_sshHost->setMaxLength(MAX_HOST_LEN - 1);
    sshForm->addRow(tr("&Host:"), m_sshHost);

    m_sshPort = new QSpinBox(sshPage);
    m_sshPort->setRange(1, 65535);
    m_sshPort->setValue(22);
    sshForm->addRow(tr("&Port:"), m_sshPort);

    m_sshUser = new QLineEdit(sshPage);
    m_sshUser->setMaxLength(MAX_USER_LEN - 1);
    sshForm->addRow(tr("&Username:"), m_sshUser);

    m_sshPass = new QLineEdit(sshPage);
    m_sshPass->setEchoMode(QLineEdit::Password);
    m_sshPass->setMaxLength(MAX_PASS_LEN - 1);
    sshForm->addRow(tr("Pass&word:"), m_sshPass);

    auto *keyRow = new QHBoxLayout;
    m_sshKey = new QLineEdit(sshPage);
    m_sshKey->setPlaceholderText(tr("Optional private key (OpenSSH .pem)"));
    auto *browse = new QPushButton(tr("Browse..."), sshPage);
    connect(browse, &QPushButton::clicked, this, &SessionEditDialog::browseKeyfile);
    keyRow->addWidget(m_sshKey, 1);
    keyRow->addWidget(browse);
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
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);

    connect(m_type, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SessionEditDialog::typeChanged);
    typeChanged(0);
}

void SessionEditDialog::typeChanged(int /*index*/)
{
    m_stack->setCurrentIndex(m_type->currentData().toInt());
}

void SessionEditDialog::browseKeyfile()
{
    const QString f = QFileDialog::getOpenFileName(this,
        tr("Select private key"), m_sshKey->text(),
        tr("Key files (*.pem *.key);;All files (*)"));
    if (!f.isEmpty())
        m_sshKey->setText(QDir::toNativeSeparators(f));
}

void SessionEditDialog::setSession(const session_entry_t &s)
{
    m_name->setText(QString::fromLocal8Bit(s.name));
    m_type->setCurrentIndex(m_type->findData(int(s.type)));

    if (s.type == SESSION_TYPE_SSH) {
        m_sshHost->setText(QString::fromLocal8Bit(s.ssh.host));
        m_sshPort->setValue(s.ssh.port > 0 ? s.ssh.port : 22);
        m_sshUser->setText(QString::fromLocal8Bit(s.ssh.username));
        m_sshPass->setText(QString::fromLocal8Bit(s.ssh.password));
        m_sshKey->setText(QString::fromLocal8Bit(s.ssh.keyfile));
    } else {
        m_serDevice->setText(QString::fromLocal8Bit(s.serial.device));
        m_serBaud->setCurrentText(QString::number(s.serial.baudrate));
        m_serData->setCurrentText(QString::number(s.serial.databits));
        m_serStop->setCurrentText(QString::number(s.serial.stopbits));
        m_serParity->setCurrentIndex(m_serParity->findData(int(s.serial.parity)));
        m_serFlow->setCurrentIndex(m_serFlow->findData(int(s.serial.flowcontrol)));
    }
}

session_entry_t SessionEditDialog::session() const
{
    session_entry_t s;
    memset(&s, 0, sizeof(s));
    setStr(s.name, sizeof(s.name), m_name->text());
    s.type = session_type_t(m_type->currentData().toInt());

    if (s.type == SESSION_TYPE_SSH) {
        setStr(s.ssh.host,     sizeof(s.ssh.host),     m_sshHost->text());
        s.ssh.port = m_sshPort->value();
        setStr(s.ssh.username, sizeof(s.ssh.username), m_sshUser->text());
        setStr(s.ssh.password, sizeof(s.ssh.password), m_sshPass->text());
        setStr(s.ssh.keyfile,  sizeof(s.ssh.keyfile),  m_sshKey->text());
    } else {
        setStr(s.serial.device, sizeof(s.serial.device), m_serDevice->text());
        s.serial.baudrate = m_serBaud->currentText().toInt();
        s.serial.databits = m_serData->currentData().toInt();
        s.serial.stopbits = m_serStop->currentData().toInt();
        s.serial.parity   = parity_t(m_serParity->currentData().toInt());
        s.serial.flowcontrol = flowcontrol_t(m_serFlow->currentData().toInt());
    }
    return s;
}
