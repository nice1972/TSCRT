#include "FindBar.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace tscrt {

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);

    auto *lbl = new QLabel(tr("Find:"), this);
    layout->addWidget(lbl);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("text or regex…"));
    m_edit->setClearButtonEnabled(true);
    layout->addWidget(m_edit, 1);

    m_count = new QLabel(QStringLiteral("0/0"), this);
    m_count->setMinimumWidth(50);
    m_count->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_count);

    auto makeBtn = [this](const QString &text, const QString &tip,
                          bool checkable) {
        auto *b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        b->setCheckable(checkable);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };

    m_btnPrev  = makeBtn(QStringLiteral("↑"), tr("Previous match (Shift+Enter)"), false);
    m_btnNext  = makeBtn(QStringLiteral("↓"), tr("Next match (Enter)"), false);
    m_btnCase  = makeBtn(QStringLiteral("Aa"), tr("Match case"), true);
    m_btnRegex = makeBtn(QStringLiteral(".*"), tr("Regular expression"), true);
    m_btnMark  = makeBtn(tr("Mark"), tr("Keep this pattern highlighted"), true);
    m_btnClose = makeBtn(QStringLiteral("✕"), tr("Close (Esc)"), false);

    layout->addWidget(m_btnPrev);
    layout->addWidget(m_btnNext);
    layout->addWidget(m_btnCase);
    layout->addWidget(m_btnRegex);
    layout->addWidget(m_btnMark);
    layout->addWidget(m_btnClose);

    connect(m_edit, &QLineEdit::textChanged,
            this, &FindBar::patternOrOptionsChanged);
    connect(m_edit, &QLineEdit::returnPressed,
            this, &FindBar::findNextRequested);
    connect(m_btnNext, &QToolButton::clicked,
            this, &FindBar::findNextRequested);
    connect(m_btnPrev, &QToolButton::clicked,
            this, &FindBar::findPrevRequested);
    connect(m_btnCase, &QToolButton::toggled,
            this, &FindBar::patternOrOptionsChanged);
    connect(m_btnRegex, &QToolButton::toggled,
            this, &FindBar::patternOrOptionsChanged);
    connect(m_btnMark, &QToolButton::toggled,
            this, &FindBar::markToggled);
    connect(m_btnClose, &QToolButton::clicked,
            this, &FindBar::closeRequested);
}

QString FindBar::pattern() const        { return m_edit->text(); }
bool FindBar::caseSensitive() const     { return m_btnCase->isChecked(); }
bool FindBar::regex() const             { return m_btnRegex->isChecked(); }
bool FindBar::markActive() const        { return m_btnMark->isChecked(); }

void FindBar::setPattern(const QString &p)
{
    if (m_edit->text() != p)
        m_edit->setText(p);
}

void FindBar::setMatchInfo(int current, int total)
{
    if (total <= 0) {
        m_count->setText(QStringLiteral("0/0"));
        m_count->setStyleSheet(pattern().isEmpty()
            ? QString()
            : QStringLiteral("color:#c44;"));
    } else {
        m_count->setText(QStringLiteral("%1/%2").arg(current + 1).arg(total));
        m_count->setStyleSheet(QString());
    }
}

void FindBar::setMarkActive(bool on)
{
    QSignalBlocker b(m_btnMark);
    m_btnMark->setChecked(on);
}

void FindBar::focusInput()
{
    m_edit->setFocus(Qt::ShortcutFocusReason);
    m_edit->selectAll();
}

void FindBar::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        emit closeRequested();
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (e->modifiers() & Qt::ShiftModifier)
            emit findPrevRequested();
        else
            emit findNextRequested();
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

} // namespace tscrt
