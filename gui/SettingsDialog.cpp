#include "SettingsDialog.h"

#include "Credentials.h"
#include "SmtpClient.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
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

QString fromCStr(const char *s)
{
    return QString::fromLocal8Bit(s ? s : "");
}

} // namespace

SettingsDialog::SettingsDialog(const profile_t &initial, QWidget *parent)
    : QDialog(parent), m_p(initial)
{
    setWindowTitle(tr("Preferences"));
    setModal(true);
    resize(720, 520);

    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildCommonTab(),   tr("Common"));
    tabs->addTab(buildButtonsTab(),  tr("Buttons"));
    tabs->addTab(buildStartupTab(),  tr("Startup"));
    tabs->addTab(buildTriggersTab(), tr("Triggers"));
    tabs->addTab(buildPeriodicTab(), tr("Periodic"));
    tabs->addTab(buildSmtpTab(),     tr("SMTP"));
    root->addWidget(tabs);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                    this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);
}

// ---- Common --------------------------------------------------------------

QWidget *SettingsDialog::buildCommonTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);

    m_logDir = new QLineEdit(fromCStr(m_p.common.log_dir), page);

    m_termType = new QComboBox(page);
    m_termType->addItems({
        QStringLiteral("xterm-256color"),
        QStringLiteral("xterm"),
        QStringLiteral("vt220"),
        QStringLiteral("vt100"),
        QStringLiteral("ansi"),
        QStringLiteral("linux"),
        QStringLiteral("screen-256color"),
        QStringLiteral("screen"),
        QStringLiteral("tmux-256color"),
        QStringLiteral("tmux"),
    });
    {
        const QString cur = fromCStr(m_p.common.terminal_type);
        int idx = m_termType->findText(cur);
        if (idx >= 0) m_termType->setCurrentIndex(idx);
        else { m_termType->addItem(cur); m_termType->setCurrentIndex(m_termType->count() - 1); }
    }

    m_encoding = new QComboBox(page);
    m_encoding->addItems({
        QStringLiteral("UTF-8"),
        QStringLiteral("EUC-KR"),
        QStringLiteral("ISO-8859-1"),
        QStringLiteral("ISO-8859-15"),
        QStringLiteral("Windows-1252"),
        QStringLiteral("Shift_JIS"),
        QStringLiteral("EUC-JP"),
        QStringLiteral("GB2312"),
        QStringLiteral("Big5"),
    });
    {
        const QString cur = fromCStr(m_p.common.encoding);
        int idx = m_encoding->findText(cur, Qt::MatchFixedString);
        if (idx >= 0) m_encoding->setCurrentIndex(idx);
        else { m_encoding->addItem(cur); m_encoding->setCurrentIndex(m_encoding->count() - 1); }
    }

    m_scrollback = new QSpinBox(page);
    m_scrollback->setRange(0, 10'000'000);
    m_scrollback->setValue(m_p.common.scrollback);

    auto *logRow = new QHBoxLayout;
    logRow->addWidget(m_logDir, 1);
    auto *logBrowse = new QPushButton(tr("Browse..."), page);
    logRow->addWidget(logBrowse);
    connect(logBrowse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this,
            tr("Session log directory"), m_logDir->text());
        if (!d.isEmpty())
            m_logDir->setText(QDir::toNativeSeparators(d));
    });
    form->addRow(tr("Session log directory:"), logRow);
    form->addRow(tr("Terminal type:"), m_termType);
    form->addRow(tr("Encoding:"),      m_encoding);
    form->addRow(tr("Scrollback:"),    m_scrollback);
    return page;
}

void SettingsDialog::commitCommon()
{
    setStr(m_p.common.log_dir,       sizeof(m_p.common.log_dir),       m_logDir->text());
    setStr(m_p.common.terminal_type, sizeof(m_p.common.terminal_type), m_termType->currentText());
    setStr(m_p.common.encoding,      sizeof(m_p.common.encoding),      m_encoding->currentText());
    m_p.common.scrollback = m_scrollback->value();
}

// ---- Buttons -------------------------------------------------------------

QWidget *SettingsDialog::buildButtonsTab()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);

    auto *help = new QLabel(
        tr("Bottom-bar buttons. Escape sequences: \\r \\n \\t \\b \\e \\p \\\\"),
        page);
    help->setWordWrap(true);
    root->addWidget(help);

    m_btnTable = new QTableWidget(MAX_BUTTONS, 2, page);
    m_btnTable->setHorizontalHeaderLabels({ tr("Label"), tr("Action") });
    m_btnTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_btnTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_btnTable->verticalHeader()->setVisible(false);
    for (int i = 0; i < MAX_BUTTONS; ++i) {
        m_btnTable->setItem(i, 0, new QTableWidgetItem(fromCStr(m_p.buttons[i].label)));
        m_btnTable->setItem(i, 1, new QTableWidgetItem(fromCStr(m_p.buttons[i].action)));
    }
    root->addWidget(m_btnTable);
    return page;
}

void SettingsDialog::commitButtons()
{
    for (int i = 0; i < MAX_BUTTONS; ++i) {
        const auto *l = m_btnTable->item(i, 0);
        const auto *a = m_btnTable->item(i, 1);
        setStr(m_p.buttons[i].label,  sizeof(m_p.buttons[i].label),
               l ? l->text() : QString());
        setStr(m_p.buttons[i].action, sizeof(m_p.buttons[i].action),
               a ? a->text() : QString());
    }
}

// ---- Helpers for per-session tables --------------------------------------

QString SettingsDialog::currentFilterSession() const
{
    if (auto *cb = qobject_cast<QComboBox *>(sender()))
        return cb->currentText();
    return {};
}

// ---- Startup -------------------------------------------------------------

QWidget *SettingsDialog::buildStartupTab()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);

    auto *hdr = new QHBoxLayout;
    hdr->addWidget(new QLabel(tr("Session:"), page));
    m_startSess = new QComboBox(page);
    for (int i = 0; i < m_p.session_count; ++i)
        m_startSess->addItem(fromCStr(m_p.sessions[i].name));
    hdr->addWidget(m_startSess, 1);
    auto *addBtn = new QPushButton(tr("&Add"),    page);
    auto *delBtn = new QPushButton(tr("&Delete"), page);
    hdr->addWidget(addBtn);
    hdr->addWidget(delBtn);
    root->addLayout(hdr);

    m_startTable = new QTableWidget(0, 2, page);
    m_startTable->setHorizontalHeaderLabels({ tr("Name"), tr("Action") });
    m_startTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_startTable->verticalHeader()->setVisible(false);
    root->addWidget(m_startTable);

    connect(m_startSess, &QComboBox::currentTextChanged,
            this, &SettingsDialog::reloadStartupTable);
    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addStartup);
    connect(delBtn, &QPushButton::clicked, this, &SettingsDialog::deleteStartup);
    reloadStartupTable();
    return page;
}

void SettingsDialog::reloadStartupTable()
{
    if (!m_startTable) return;
    const QString sess = m_startSess->currentText();
    m_startTable->setRowCount(0);
    for (int i = 0; i < m_p.startup_count; ++i) {
        if (fromCStr(m_p.startups[i].session) != sess) continue;
        const int row = m_startTable->rowCount();
        m_startTable->insertRow(row);
        m_startTable->setItem(row, 0, new QTableWidgetItem(fromCStr(m_p.startups[i].name)));
        m_startTable->setItem(row, 1, new QTableWidgetItem(fromCStr(m_p.startups[i].action)));
        m_startTable->item(row, 0)->setData(Qt::UserRole, i);
    }
}

void SettingsDialog::addStartup()
{
    if (m_startSess->count() == 0) return;
    if (m_p.startup_count >= MAX_STARTUP) {
        QMessageBox::warning(this, tr("Limit"),
            tr("Maximum startup entries reached."));
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Startup"),
        tr("Name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) return;
    const QString action = QInputDialog::getText(this, tr("Startup"),
        tr("Action:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || action.isEmpty()) return;

    startup_entry_t &su = m_p.startups[m_p.startup_count++];
    setStr(su.session, sizeof(su.session), m_startSess->currentText());
    setStr(su.name,    sizeof(su.name),    name);
    setStr(su.action,  sizeof(su.action),  action);
    reloadStartupTable();
}

void SettingsDialog::deleteStartup()
{
    int row = m_startTable->currentRow();
    if (row < 0) return;
    int idx = m_startTable->item(row, 0)->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_p.startup_count) return;
    if (idx < m_p.startup_count - 1)
        memmove(&m_p.startups[idx], &m_p.startups[idx + 1],
                (m_p.startup_count - idx - 1) * sizeof(startup_entry_t));
    m_p.startup_count--;
    reloadStartupTable();
}

// ---- Triggers ------------------------------------------------------------

QWidget *SettingsDialog::buildTriggersTab()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    auto *hdr  = new QHBoxLayout;
    hdr->addWidget(new QLabel(tr("Session:"), page));
    m_trigSess = new QComboBox(page);
    for (int i = 0; i < m_p.session_count; ++i)
        m_trigSess->addItem(fromCStr(m_p.sessions[i].name));
    hdr->addWidget(m_trigSess, 1);
    auto *addBtn = new QPushButton(tr("&Add"),    page);
    auto *delBtn = new QPushButton(tr("&Delete"), page);
    hdr->addWidget(addBtn);
    hdr->addWidget(delBtn);
    root->addLayout(hdr);

    m_trigTable = new QTableWidget(0, 2, page);
    m_trigTable->setHorizontalHeaderLabels({ tr("Pattern"), tr("Action") });
    m_trigTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_trigTable->verticalHeader()->setVisible(false);
    root->addWidget(m_trigTable);

    connect(m_trigSess, &QComboBox::currentTextChanged,
            this, &SettingsDialog::reloadTriggerTable);
    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addTrigger);
    connect(delBtn, &QPushButton::clicked, this, &SettingsDialog::deleteTrigger);
    reloadTriggerTable();
    return page;
}

void SettingsDialog::reloadTriggerTable()
{
    if (!m_trigTable) return;
    const QString sess = m_trigSess->currentText();
    m_trigTable->setRowCount(0);
    for (int i = 0; i < m_p.trigger_count; ++i) {
        if (fromCStr(m_p.triggers[i].session) != sess) continue;
        const int row = m_trigTable->rowCount();
        m_trigTable->insertRow(row);
        m_trigTable->setItem(row, 0, new QTableWidgetItem(fromCStr(m_p.triggers[i].pattern)));
        m_trigTable->setItem(row, 1, new QTableWidgetItem(fromCStr(m_p.triggers[i].action)));
        m_trigTable->item(row, 0)->setData(Qt::UserRole, i);
    }
}

void SettingsDialog::addTrigger()
{
    if (m_trigSess->count() == 0) return;
    if (m_p.trigger_count >= MAX_TRIGGERS) {
        QMessageBox::warning(this, tr("Limit"), tr("Maximum trigger entries reached."));
        return;
    }
    bool ok = false;
    const QString pat = QInputDialog::getText(this, tr("Trigger"),
        tr("Pattern (substring):"), QLineEdit::Normal, QString(), &ok);
    if (!ok || pat.isEmpty()) return;
    const QString action = QInputDialog::getText(this, tr("Trigger"),
        tr("Action:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || action.isEmpty()) return;

    trigger_entry_t &t = m_p.triggers[m_p.trigger_count++];
    setStr(t.session, sizeof(t.session), m_trigSess->currentText());
    setStr(t.pattern, sizeof(t.pattern), pat);
    setStr(t.action,  sizeof(t.action),  action);
    reloadTriggerTable();
}

void SettingsDialog::deleteTrigger()
{
    int row = m_trigTable->currentRow();
    if (row < 0) return;
    int idx = m_trigTable->item(row, 0)->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_p.trigger_count) return;
    if (idx < m_p.trigger_count - 1)
        memmove(&m_p.triggers[idx], &m_p.triggers[idx + 1],
                (m_p.trigger_count - idx - 1) * sizeof(trigger_entry_t));
    m_p.trigger_count--;
    reloadTriggerTable();
}

// ---- Periodic ------------------------------------------------------------

QWidget *SettingsDialog::buildPeriodicTab()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    auto *hdr  = new QHBoxLayout;
    hdr->addWidget(new QLabel(tr("Session:"), page));
    m_perSess = new QComboBox(page);
    for (int i = 0; i < m_p.session_count; ++i)
        m_perSess->addItem(fromCStr(m_p.sessions[i].name));
    hdr->addWidget(m_perSess, 1);
    auto *addBtn = new QPushButton(tr("&Add"),    page);
    auto *delBtn = new QPushButton(tr("&Delete"), page);
    hdr->addWidget(addBtn);
    hdr->addWidget(delBtn);
    root->addLayout(hdr);

    m_perTable = new QTableWidget(0, 3, page);
    m_perTable->setHorizontalHeaderLabels(
        { tr("Name"), tr("Interval (s)"), tr("Action") });
    m_perTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_perTable->verticalHeader()->setVisible(false);
    root->addWidget(m_perTable);

    connect(m_perSess, &QComboBox::currentTextChanged,
            this, &SettingsDialog::reloadPeriodicTable);
    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addPeriodic);
    connect(delBtn, &QPushButton::clicked, this, &SettingsDialog::deletePeriodic);
    reloadPeriodicTable();
    return page;
}

void SettingsDialog::reloadPeriodicTable()
{
    if (!m_perTable) return;
    const QString sess = m_perSess->currentText();
    m_perTable->setRowCount(0);
    for (int i = 0; i < m_p.periodic_count; ++i) {
        if (fromCStr(m_p.periodics[i].session) != sess) continue;
        const int row = m_perTable->rowCount();
        m_perTable->insertRow(row);
        m_perTable->setItem(row, 0, new QTableWidgetItem(fromCStr(m_p.periodics[i].name)));
        m_perTable->setItem(row, 1, new QTableWidgetItem(QString::number(m_p.periodics[i].interval)));
        m_perTable->setItem(row, 2, new QTableWidgetItem(fromCStr(m_p.periodics[i].action)));
        m_perTable->item(row, 0)->setData(Qt::UserRole, i);
    }
}

void SettingsDialog::addPeriodic()
{
    if (m_perSess->count() == 0) return;
    if (m_p.periodic_count >= MAX_PERIODIC) {
        QMessageBox::warning(this, tr("Limit"), tr("Maximum periodic entries reached."));
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Periodic"),
        tr("Name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) return;
    const int sec = QInputDialog::getInt(this, tr("Periodic"),
        tr("Interval (seconds):"), 60, 1, 86400, 1, &ok);
    if (!ok) return;
    const QString action = QInputDialog::getText(this, tr("Periodic"),
        tr("Action:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || action.isEmpty()) return;

    periodic_entry_t &pe = m_p.periodics[m_p.periodic_count++];
    setStr(pe.session, sizeof(pe.session), m_perSess->currentText());
    setStr(pe.name,    sizeof(pe.name),    name);
    setStr(pe.action,  sizeof(pe.action),  action);
    pe.interval = sec;
    reloadPeriodicTable();
}

void SettingsDialog::deletePeriodic()
{
    int row = m_perTable->currentRow();
    if (row < 0) return;
    int idx = m_perTable->item(row, 0)->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_p.periodic_count) return;
    if (idx < m_p.periodic_count - 1)
        memmove(&m_p.periodics[idx], &m_p.periodics[idx + 1],
                (m_p.periodic_count - idx - 1) * sizeof(periodic_entry_t));
    m_p.periodic_count--;
    reloadPeriodicTable();
}

// ---- Stub for unused slot in MOC -----------------------------------------

void SettingsDialog::onSessionFilterChanged() {}

// ---- SMTP ----------------------------------------------------------------

QWidget *SettingsDialog::buildSmtpTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);

    m_smtpHost = new QLineEdit(fromCStr(m_p.smtp.host), page);
    m_smtpPort = new QSpinBox(page);
    m_smtpPort->setRange(1, 65535);
    m_smtpPort->setValue(m_p.smtp.port > 0 ? m_p.smtp.port : 587);

    m_smtpSecurity = new QComboBox(page);
    m_smtpSecurity->addItems({ tr("None"), tr("STARTTLS"), tr("SMTPS (implicit TLS)") });
    m_smtpSecurity->setCurrentIndex(m_p.smtp.security);

    m_smtpUser = new QLineEdit(fromCStr(m_p.smtp.username), page);

    m_smtpPass = new QLineEdit(page);
    m_smtpPass->setEchoMode(QLineEdit::Password);
    if (m_p.smtp.password[0]) {
        const QString plain = tscrt::decryptSecret(
            QString::fromLocal8Bit(m_p.smtp.password));
        m_smtpPass->setText(plain);
    }

    m_smtpFromAddr = new QLineEdit(fromCStr(m_p.smtp.from_addr), page);
    m_smtpFromName = new QLineEdit(fromCStr(m_p.smtp.from_name), page);

    m_smtpTimeout = new QSpinBox(page);
    m_smtpTimeout->setRange(5, 300);
    m_smtpTimeout->setValue(m_p.smtp.timeout_sec > 0 ? m_p.smtp.timeout_sec : 30);
    m_smtpTimeout->setSuffix(tr(" s"));

    form->addRow(tr("SMTP host:"),     m_smtpHost);
    form->addRow(tr("Port:"),          m_smtpPort);
    form->addRow(tr("Security:"),      m_smtpSecurity);
    form->addRow(tr("Username:"),      m_smtpUser);
    form->addRow(tr("Password:"),      m_smtpPass);
    form->addRow(tr("From address:"),  m_smtpFromAddr);
    form->addRow(tr("From name:"),     m_smtpFromName);
    form->addRow(tr("Timeout:"),       m_smtpTimeout);

    auto *testBtn = new QPushButton(tr("Send test email..."), page);
    connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::onSmtpTest);
    form->addRow(QString(), testBtn);

    return page;
}

void SettingsDialog::commitSmtp()
{
    if (!m_smtpHost) return;
    setStr(m_p.smtp.host,      sizeof(m_p.smtp.host),      m_smtpHost->text());
    m_p.smtp.port              = m_smtpPort->value();
    m_p.smtp.security          = m_smtpSecurity->currentIndex();
    setStr(m_p.smtp.username,  sizeof(m_p.smtp.username),  m_smtpUser->text());
    const QString plainPass = m_smtpPass->text();
    if (plainPass.isEmpty()) {
        m_p.smtp.password[0] = '\0';
    } else {
        const QString enc = tscrt::encryptSecret(plainPass);
        setStr(m_p.smtp.password, sizeof(m_p.smtp.password), enc);
    }
    setStr(m_p.smtp.from_addr, sizeof(m_p.smtp.from_addr), m_smtpFromAddr->text());
    setStr(m_p.smtp.from_name, sizeof(m_p.smtp.from_name), m_smtpFromName->text());
    m_p.smtp.timeout_sec       = m_smtpTimeout->value();
}

void SettingsDialog::onSmtpTest()
{
    commitSmtp();
    if (m_p.smtp.host[0] == '\0' || m_p.smtp.from_addr[0] == '\0') {
        QMessageBox::warning(this, tr("SMTP test"),
            tr("Fill in host and From address first."));
        return;
    }
    bool ok = false;
    const QString to = QInputDialog::getText(this, tr("SMTP test"),
        tr("Send test email to:"), QLineEdit::Normal,
        fromCStr(m_p.smtp.from_addr), &ok);
    if (!ok || to.isEmpty()) return;

    tscrt::SmtpClient client(m_p.smtp, this);
    QEventLoop loop;
    QString err;
    bool sent = false;
    connect(&client, &tscrt::SmtpClient::sent, &loop, [&] {
        sent = true; loop.quit();
    });
    connect(&client, &tscrt::SmtpClient::failed, &loop,
            [&](const QString &reason) {
        err = reason; loop.quit();
    });

    client.send(QStringList{ to },
                QStringLiteral("TSCRT SMTP test"),
                QStringLiteral("This is a test message from TSCRT.\n"));
    loop.exec();

    if (sent)
        QMessageBox::information(this, tr("SMTP test"),
            tr("Test email sent to %1").arg(to));
    else
        QMessageBox::critical(this, tr("SMTP test"),
            tr("Send failed: %1").arg(err));
}

// ---- Accept --------------------------------------------------------------

void SettingsDialog::accept()
{
    commitCommon();
    commitButtons();
    commitSmtp();
    QDialog::accept();
}
