#include "ButtonBar.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>

#include <cstring>

namespace tscrt {

namespace {

constexpr const char *kCssNormal =
    "QPushButton { background:#2c2c2c; color:#e0e0e0; padding:4px 10px;"
    " border:1px solid #444; border-radius:3px; }"
    "QPushButton:hover { background:#3a3a3a; }"
    "QPushButton:pressed { background:#1a1a1a; }";

constexpr const char *kCssMark =
    "QPushButton { background:#7a4b00; color:#fff; padding:4px 10px;"
    " border:1px solid #a36500; border-radius:3px; font-weight:bold; }";

constexpr const char *kCssLoop =
    "QPushButton { background:#7a7a00; color:#000; padding:4px 10px;"
    " border:1px solid #a3a300; border-radius:3px; font-weight:bold; }";

constexpr const char *kCssHelp =
    "QPushButton { background:#1f4d7a; color:#fff; padding:4px 10px;"
    " border:1px solid #2f6ba8; border-radius:3px; }";

// Looping button: idle between ticks — subtle highlight.
constexpr const char *kCssLooping =
    "QPushButton { background:#2c2c2c; color:#ffcc00; padding:4px 10px;"
    " border:2px solid #a36500; border-radius:3px; font-weight:bold; }"
    "QPushButton:hover { background:#3a3a3a; }";

// Looping button: flash on tick — bright pop.
constexpr const char *kCssFlash =
    "QPushButton { background:#ff6600; color:#fff; padding:4px 10px;"
    " border:2px solid #ff9900; border-radius:3px; font-weight:bold; }";

constexpr int kFlashDurationMs = 300;

} // namespace

ButtonBar::ButtonBar(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ButtonBar"));
    setStyleSheet(QStringLiteral("QWidget#ButtonBar { background:#1a1a1a; }"));

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 3, 4, 3);
    m_layout->setSpacing(4);

    m_repeatTimer = new QTimer(this);
    connect(m_repeatTimer, &QTimer::timeout, this, &ButtonBar::onRepeatTick);

    m_flashTimer = new QTimer(this);
    m_flashTimer->setSingleShot(true);
    connect(m_flashTimer, &QTimer::timeout, this, &ButtonBar::onFlashOff);
}

void ButtonBar::clearLayout()
{
    while (auto *item = m_layout->takeAt(0)) {
        if (auto *w = item->widget())
            w->deleteLater();
        delete item;
    }
}

QPushButton *ButtonBar::makeAction(int slot, const QString &label, const QString &action)
{
    auto *btn = new QPushButton(label, this);
    btn->setStyleSheet(QString::fromLatin1(kCssNormal));
    btn->setProperty("action", action);
    btn->setProperty("slotIndex", slot);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    btn->installEventFilter(this);
    connect(btn, &QPushButton::clicked,
            this, &ButtonBar::onButtonClicked);
    connect(btn, &QPushButton::customContextMenuRequested,
            this, &ButtonBar::onButtonContextMenu);
    return btn;
}

QPushButton *ButtonBar::makeSpecial(const QString &label, const QString &css)
{
    auto *btn = new QPushButton(label, this);
    btn->setStyleSheet(css);
    btn->setFocusPolicy(Qt::NoFocus);
    return btn;
}

void ButtonBar::setButtons(const button_t buttons[MAX_BUTTONS])
{
    clearLayout();

    // User-defined buttons.
    for (int i = 0; i < MAX_BUTTONS; ++i) {
        const button_t &b = buttons[i];
        if (b.label[0] == '\0' && b.action[0] == '\0')
            continue;
        auto *btn = makeAction(i,
                               QString::fromLocal8Bit(b.label),
                               QString::fromLocal8Bit(b.action));
        m_layout->addWidget(btn);
    }

    m_layout->addStretch();

    // loop / mark — subtle accent, right-aligned before help.
    m_loopBtn = new QPushButton(tr("loop"), this);
    m_loopBtn->setStyleSheet(QString::fromLatin1(kCssLoop));
    m_loopBtn->setFocusPolicy(Qt::NoFocus);
    m_loopBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_loopBtn->installEventFilter(this);
    connect(m_loopBtn, &QPushButton::clicked,
            this, &ButtonBar::loopClicked);
    connect(m_loopBtn, &QPushButton::customContextMenuRequested,
            this, [this](const QPoint &) { emit loopRightClicked(); });
    m_layout->addWidget(m_loopBtn);

    m_markBtn = new QPushButton(tr("mark"), this);
    m_markBtn->setStyleSheet(QString::fromLatin1(kCssMark));
    m_markBtn->setFocusPolicy(Qt::NoFocus);
    m_markBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_markBtn->installEventFilter(this);
    connect(m_markBtn, &QPushButton::clicked,
            this, &ButtonBar::markClicked);
    connect(m_markBtn, &QPushButton::customContextMenuRequested,
            this, [this](const QPoint &) { emit markRightClicked(); });
    m_layout->addWidget(m_markBtn);

    auto *help = makeSpecial(tr("?"), QString::fromLatin1(kCssHelp));
    connect(help, &QPushButton::clicked, this, &ButtonBar::helpRequested);
    m_layout->addWidget(help);

    setLoopRunning(m_loopRunning);
}

void ButtonBar::setLoopRunning(bool running)
{
    m_loopRunning = running;
    if (!m_loopBtn) return;
    if (running) {
        m_loopBtn->setText(tr("loop ●"));
        m_loopBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:#a30000; color:#fff;"
            " padding:4px 10px; border:1px solid #c00; border-radius:3px;"
            " font-weight:bold; }"));
    } else {
        m_loopBtn->setText(tr("loop"));
        m_loopBtn->setStyleSheet(QString::fromLatin1(kCssLoop));
    }
}

void ButtonBar::setMarkActive(bool active)
{
    if (!m_markBtn) return;
    if (active) {
        m_markBtn->setText(tr("mark ●"));
        m_markBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:#a30000; color:#fff;"
            " padding:4px 10px; border:1px solid #c00; border-radius:3px;"
            " font-weight:bold; }"));
    } else {
        m_markBtn->setText(tr("mark"));
        m_markBtn->setStyleSheet(QString::fromLatin1(kCssMark));
    }
}

void ButtonBar::onButtonContextMenu(const QPoint &pos)
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) return;
    QMenu menu(this);
    QAction *edit = menu.addAction(tr("Edit button..."));
    if (menu.exec(btn->mapToGlobal(pos)) == edit)
        emit buttonEditRequested(btn->property("slotIndex").toInt());
}

void ButtonBar::onButtonClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;
    const QString action = btn->property("action").toString();
    if (action.isEmpty())
        return;
    emit actionRequested(action);
}

void ButtonBar::onRepeatTick()
{
    if (!m_repeatAction.isEmpty())
        emit actionRequested(m_repeatAction);
}

void ButtonBar::setLoopingAction(const QString &action)
{
    // Revert the previous looping button to normal style.
    if (m_loopingBtn) {
        m_flashTimer->stop();
        m_loopingBtn->setStyleSheet(QString::fromLatin1(kCssNormal));
        m_loopingBtn = nullptr;
    }

    if (action.isEmpty())
        return;

    // Find the button whose action matches.
    for (int i = 0; i < m_layout->count(); ++i) {
        auto *btn = qobject_cast<QPushButton *>(m_layout->itemAt(i)->widget());
        if (btn && btn->property("action").toString() == action) {
            m_loopingBtn = btn;
            m_loopingBtn->setStyleSheet(QString::fromLatin1(kCssLooping));
            break;
        }
    }
}

void ButtonBar::flashLoopingButton()
{
    if (!m_loopingBtn) return;
    m_loopingBtn->setStyleSheet(QString::fromLatin1(kCssFlash));
    m_flashTimer->start(kFlashDurationMs);
}

void ButtonBar::onFlashOff()
{
    if (m_loopingBtn)
        m_loopingBtn->setStyleSheet(QString::fromLatin1(kCssLooping));
}

bool ButtonBar::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *btn = qobject_cast<QPushButton *>(obj);
        if (btn) {
            if (btn == m_loopBtn) {
                emit loopDoubleClicked();
                return true;
            }
            if (btn == m_markBtn) {
                emit markDoubleClicked();
                return true;
            }
            const QString action = btn->property("action").toString();
            if (!action.isEmpty()) {
                emit buttonLoopRequested(action);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace tscrt
