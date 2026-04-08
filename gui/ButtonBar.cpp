#include "ButtonBar.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
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

constexpr const char *kCssCmd =
    "QPushButton { background:#4a1f5a; color:#fff; padding:4px 10px;"
    " border:1px solid #6b2d80; border-radius:3px; }";

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
}

void ButtonBar::clearLayout()
{
    while (auto *item = m_layout->takeAt(0)) {
        if (auto *w = item->widget())
            w->deleteLater();
        delete item;
    }
}

QPushButton *ButtonBar::makeAction(const QString &label, const QString &action)
{
    auto *btn = new QPushButton(label, this);
    btn->setStyleSheet(QString::fromLatin1(kCssNormal));
    btn->setProperty("action", action);
    btn->setFocusPolicy(Qt::NoFocus);
    connect(btn, &QPushButton::clicked, this, &ButtonBar::onButtonClicked);
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

    // cmdwin (left)
    auto *cmd = makeSpecial(tr("cmd"), QString::fromLatin1(kCssCmd));
    connect(cmd, &QPushButton::clicked, this, &ButtonBar::cmdWindowRequested);
    m_layout->addWidget(cmd);

    // loop
    auto *loop = makeSpecial(tr("loop"), QString::fromLatin1(kCssLoop));
    connect(loop, &QPushButton::clicked, this, &ButtonBar::loopRequested);
    m_layout->addWidget(loop);

    // mark
    auto *mark = makeSpecial(tr("mark"), QString::fromLatin1(kCssMark));
    connect(mark, &QPushButton::clicked, this, &ButtonBar::markRequested);
    m_layout->addWidget(mark);

    // User-defined buttons
    for (int i = 0; i < MAX_BUTTONS; ++i) {
        const button_t &b = buttons[i];
        if (b.label[0] == '\0' && b.action[0] == '\0')
            continue;
        auto *btn = makeAction(QString::fromLocal8Bit(b.label),
                               QString::fromLocal8Bit(b.action));
        m_layout->addWidget(btn);
    }

    m_layout->addStretch();

    // help (right)
    auto *help = makeSpecial(tr("?"), QString::fromLatin1(kCssHelp));
    connect(help, &QPushButton::clicked, this, &ButtonBar::helpRequested);
    m_layout->addWidget(help);
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

void ButtonBar::onButtonDoubleClicked()
{
    // Reserved for future per-button repeat toggle. The current MVP
    // routes repeat through the "loop" special button instead.
}

void ButtonBar::onRepeatTick()
{
    if (!m_repeatAction.isEmpty())
        emit actionRequested(m_repeatAction);
}

} // namespace tscrt
