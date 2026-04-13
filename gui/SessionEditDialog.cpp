#include "SessionEditDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
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
    resize(460, 0);   // decent default width; height adapts to content
}

void SessionEditDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    m_tabs = new QTabWidget(this);

    // ==== General tab ====
    auto *genPage = new QWidget;
    auto *genForm = new QFormLayout(genPage);
    genForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_name = new QLineEdit(genPage);
    m_name->setMaxLength(MAX_NAME_LEN - 1);
    m_name->setPlaceholderText(tr("e.g. production-server"));
    genForm->addRow(tr("Name"), m_name);

    m_type = new QComboBox(genPage);
    m_type->addItem(QStringLiteral("SSH"),    int(SESSION_TYPE_SSH));
    m_type->addItem(QStringLiteral("Serial"), int(SESSION_TYPE_SERIAL));
    genForm->addRow(tr("Type"), m_type);

    m_termType = new QComboBox(genPage);
    m_termType->setEditable(true);
    m_termType->addItem(QStringLiteral("xterm-256color"));
    m_termType->addItem(QStringLiteral("xterm"));
    m_termType->addItem(QStringLiteral("vt100"));
    m_termType->addItem(QStringLiteral("vt220"));
    m_termType->addItem(QStringLiteral("linux"));
    m_termType->addItem(QStringLiteral("ansi"));
    m_termType->setCurrentText(QString());
    m_termType->lineEdit()->setPlaceholderText(tr("(use Preferences default)"));
    genForm->addRow(tr("Terminal type"), m_termType);

    genForm->addItem(new QSpacerItem(0, 8));

    m_logEnable = new QCheckBox(tr("Save session log"), genPage);
    m_logEnable->setChecked(true);
    genForm->addRow(QString(), m_logEnable);

    auto *fsGroup = new QGroupBox(tr("Full-screen display"), genPage);
    auto *fsLayout = new QVBoxLayout(fsGroup);
    fsLayout->setContentsMargins(8, 4, 8, 4);
    m_fsCmdLine = new QCheckBox(tr("Show command line"), fsGroup);
    m_fsButtons = new QCheckBox(tr("Show button bar"), fsGroup);
    fsLayout->addWidget(m_fsCmdLine);
    fsLayout->addWidget(m_fsButtons);
    genForm->addRow(fsGroup);

    // ==== Connection tab ====
    auto *connPage = new QWidget;
    auto *connLayout = new QVBoxLayout(connPage);
    connLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(connPage);

    // ---- SSH page ----
    auto *sshPage = new QWidget;
    auto *sshForm = new QFormLayout(sshPage);
    sshForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_sshHost = new QLineEdit(sshPage);
    m_sshHost->setMaxLength(MAX_HOST_LEN - 1);
    m_sshHost->setPlaceholderText(QStringLiteral("hostname or IP"));
    sshForm->addRow(tr("Host"), m_sshHost);

    m_sshPort = new QSpinBox(sshPage);
    m_sshPort->setRange(1, 65535);
    m_sshPort->setValue(22);
    sshForm->addRow(tr("Port"), m_sshPort);

    m_sshUser = new QLineEdit(sshPage);
    m_sshUser->setMaxLength(MAX_USER_LEN - 1);
    sshForm->addRow(tr("Username"), m_sshUser);

    m_sshPass = new QLineEdit(sshPage);
    m_sshPass->setEchoMode(QLineEdit::Password);
    m_sshPass->setMaxLength(MAX_PASS_LEN - 1);
    sshForm->addRow(tr("Password"), m_sshPass);

    auto *keyRow = new QHBoxLayout;
    m_sshKey = new QLineEdit(sshPage);
    m_sshKey->setPlaceholderText(tr("Optional private key (.pem)"));
    auto *browse = new QPushButton(tr("..."), sshPage);
    browse->setMaximumWidth(36);
    connect(browse, &QPushButton::clicked, this, &SessionEditDialog::browseKeyfile);
    keyRow->addWidget(m_sshKey, 1);
    keyRow->addWidget(browse);
    keyRow->setSpacing(4);
    sshForm->addRow(tr("Key file"), keyRow);

    m_stack->addWidget(sshPage);

    // ---- Serial page ----
    auto *serPage = new QWidget;
    auto *serForm = new QFormLayout(serPage);
    serForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_serDevice = new QLineEdit(serPage);
    m_serDevice->setMaxLength(MAX_DEVICE_LEN - 1);
#ifdef _WIN32
    m_serDevice->setPlaceholderText(QStringLiteral("COM3"));
#else
    m_serDevice->setPlaceholderText(QStringLiteral("/dev/cu.usbserial-xxx"));
#endif
    serForm->addRow(tr("Device"), m_serDevice);

    m_serBaud = new QComboBox(serPage);
    for (int b : { 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 })
        m_serBaud->addItem(QString::number(b), b);
    m_serBaud->setCurrentText(QStringLiteral("115200"));
    m_serBaud->setEditable(true);
    serForm->addRow(tr("Baud rate"), m_serBaud);

    auto *portRow = new QHBoxLayout;
    m_serData = new QComboBox(serPage);
    for (int d : { 5, 6, 7, 8 }) m_serData->addItem(QString::number(d), d);
    m_serData->setCurrentText(QStringLiteral("8"));
    auto *dataLabel = new QLabel(tr("Data"), serPage);
    portRow->addWidget(dataLabel);
    portRow->addWidget(m_serData);

    m_serStop = new QComboBox(serPage);
    m_serStop->addItem(QStringLiteral("1"), 1);
    m_serStop->addItem(QStringLiteral("2"), 2);
    auto *stopLabel = new QLabel(tr("Stop"), serPage);
    portRow->addSpacing(12);
    portRow->addWidget(stopLabel);
    portRow->addWidget(m_serStop);

    m_serParity = new QComboBox(serPage);
    m_serParity->addItem(tr("None"), int(PARITY_NONE));
    m_serParity->addItem(tr("Odd"),  int(PARITY_ODD));
    m_serParity->addItem(tr("Even"), int(PARITY_EVEN));
    auto *parLabel = new QLabel(tr("Parity"), serPage);
    portRow->addSpacing(12);
    portRow->addWidget(parLabel);
    portRow->addWidget(m_serParity);

    portRow->addStretch();
    serForm->addRow(tr("Line settings"), portRow);

    m_serFlow = new QComboBox(serPage);
    m_serFlow->addItem(tr("None"),     int(FLOW_NONE));
    m_serFlow->addItem(tr("Hardware"), int(FLOW_HARDWARE));
    m_serFlow->addItem(tr("Software"), int(FLOW_SOFTWARE));
    serForm->addRow(tr("Flow control"), m_serFlow);

    m_stack->addWidget(serPage);
    connLayout->addWidget(m_stack);

    m_tabs->addTab(connPage, tr("Connection"));
    m_tabs->addTab(genPage, tr("General"));

    // ==== Advanced tab ====
    auto *advPage = new QWidget;
    auto *advLayout = new QVBoxLayout(advPage);

    // Reconnect group
    auto *rcGroup = new QGroupBox(tr("Auto-reconnect"), advPage);
    auto *rcForm  = new QFormLayout(rcGroup);
    rcForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_autoReconnect = new QCheckBox(tr("Reconnect automatically on disconnect"), rcGroup);
    rcForm->addRow(m_autoReconnect);

    m_reconnectMax = new QSpinBox(rcGroup);
    m_reconnectMax->setRange(0, 1000);
    m_reconnectMax->setValue(10);
    m_reconnectMax->setSpecialValueText(tr("unlimited"));
    rcForm->addRow(tr("Max attempts"), m_reconnectMax);

    m_reconnectBaseMs = new QSpinBox(rcGroup);
    m_reconnectBaseMs->setRange(100, 60000);
    m_reconnectBaseMs->setSingleStep(100);
    m_reconnectBaseMs->setValue(500);
    m_reconnectBaseMs->setSuffix(QStringLiteral(" ms"));
    rcForm->addRow(tr("Base delay"), m_reconnectBaseMs);
    advLayout->addWidget(rcGroup);

    // Keepalive group (SSH only)
    auto *kaGroup = new QGroupBox(tr("SSH keepalive"), advPage);
    auto *kaForm  = new QFormLayout(kaGroup);
    kaForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_sshKeepaliveSec = new QSpinBox(kaGroup);
    m_sshKeepaliveSec->setRange(0, 3600);
    m_sshKeepaliveSec->setValue(30);
    m_sshKeepaliveSec->setSpecialValueText(tr("off"));
    m_sshKeepaliveSec->setSuffix(QStringLiteral(" s"));
    m_keepaliveLabel = new QLabel(tr("Interval"), kaGroup);
    kaForm->addRow(m_keepaliveLabel, m_sshKeepaliveSec);

    m_sshTcpKeepalive = new QCheckBox(tr("TCP keepalive (SO_KEEPALIVE)"), kaGroup);
    m_sshTcpKeepalive->setChecked(true);
    kaForm->addRow(m_sshTcpKeepalive);

    advLayout->addWidget(kaGroup);
    advLayout->addStretch();

    m_tabs->addTab(advPage, tr("Advanced"));

    root->addWidget(m_tabs);

    // ---- OK/Cancel ----
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
    const int t = m_type->currentData().toInt();
    m_stack->setCurrentIndex(t);
    const bool ssh = (t == int(SESSION_TYPE_SSH));
    if (m_keepaliveLabel)  m_keepaliveLabel->setVisible(ssh);
    if (m_sshKeepaliveSec) m_sshKeepaliveSec->setVisible(ssh);
    if (m_sshTcpKeepalive) m_sshTcpKeepalive->setVisible(ssh);

    // Show/hide the keepalive group box parent if all children hidden
    if (auto *kaGroup = qobject_cast<QGroupBox *>(m_sshTcpKeepalive->parentWidget()))
        kaGroup->setVisible(ssh);
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
    m_termType->setCurrentText(QString::fromLocal8Bit(s.terminal_type));
    m_logEnable->setChecked(s.log_enabled != 0);
    m_fsCmdLine->setChecked(s.show_cmdline != 0);
    m_fsButtons->setChecked(s.show_buttons != 0);

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

    m_autoReconnect->setChecked(s.auto_reconnect != 0);
    m_reconnectMax->setValue(s.reconnect_max);
    m_reconnectBaseMs->setValue(s.reconnect_base_ms > 0 ? s.reconnect_base_ms : 500);
    m_sshKeepaliveSec->setValue(s.ssh_keepalive_sec);
    m_sshTcpKeepalive->setChecked(s.ssh_tcp_keepalive != 0);
    typeChanged(0);
}

session_entry_t SessionEditDialog::session() const
{
    session_entry_t s;
    memset(&s, 0, sizeof(s));
    setStr(s.name, sizeof(s.name), m_name->text());
    s.type         = session_type_t(m_type->currentData().toInt());
    setStr(s.terminal_type, sizeof(s.terminal_type), m_termType->currentText().trimmed());
    s.log_enabled  = m_logEnable->isChecked() ? 1 : 0;
    s.show_cmdline = m_fsCmdLine->isChecked() ? 1 : 0;
    s.show_buttons = m_fsButtons->isChecked() ? 1 : 0;

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

    s.auto_reconnect    = m_autoReconnect->isChecked() ? 1 : 0;
    s.reconnect_max     = m_reconnectMax->value();
    s.reconnect_base_ms = m_reconnectBaseMs->value();
    if (s.type == SESSION_TYPE_SSH) {
        s.ssh_keepalive_sec = m_sshKeepaliveSec->value();
        s.ssh_tcp_keepalive = m_sshTcpKeepalive->isChecked() ? 1 : 0;
    }
    return s;
}
