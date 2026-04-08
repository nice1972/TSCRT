#include "TerminalWidget.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QtGlobal>

#include <vterm.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr int    kBlinkIntervalMs = 530;

QColor cellColor(const VTermColor &c, const QColor &fallback)
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&c) || VTERM_COLOR_IS_DEFAULT_BG(&c))
        return fallback;
    if (VTERM_COLOR_IS_RGB(&c))
        return QColor(c.rgb.red, c.rgb.green, c.rgb.blue);
    if (VTERM_COLOR_IS_INDEXED(&c)) {
        // libvterm 0.3 already maps indexed colors into the rgb field
        // when the state machine has a palette set; fall back to rgb anyway.
        return QColor(c.rgb.red, c.rgb.green, c.rgb.blue);
    }
    return fallback;
}

const VTermScreenCallbacks kScreenCallbacks = {
    /* damage      */ &TerminalWidget::s_damage,
    /* moverect    */ nullptr,
    /* movecursor  */ &TerminalWidget::s_moveCursor,
    /* settermprop */ &TerminalWidget::s_settermprop,
    /* bell        */ &TerminalWidget::s_bell,
    /* resize      */ nullptr,
    /* sb_pushline */ nullptr,
    /* sb_popline  */ nullptr,
    /* sb_clear    */ nullptr,
};

} // namespace

TerminalWidget::TerminalWidget(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_InputMethodEnabled);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Terminal display"));
    setAccessibleDescription(tr("Interactive terminal session"));

    horizontalScrollBar()->setVisible(false);

    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setPointSize(11);
    setTerminalFont(m_font);

    m_vt = vterm_new(m_rows, m_cols);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(m_vt, &TerminalWidget::s_outputCallback, this);

    m_screen = vterm_obtain_screen(m_vt);
    vterm_screen_set_callbacks(m_screen, &kScreenCallbacks, this);
    vterm_screen_reset(m_screen, 1);

    m_blinkTimer.setInterval(kBlinkIntervalMs);
    connect(&m_blinkTimer, &QTimer::timeout, this, [this] {
        m_blinkOn = !m_blinkOn;
        viewport()->update();
    });
    m_blinkTimer.start();

    viewport()->setCursor(Qt::IBeamCursor);
}

TerminalWidget::~TerminalWidget()
{
    if (m_vt) {
        vterm_free(m_vt);
        m_vt     = nullptr;
        m_screen = nullptr;
    }
}

void TerminalWidget::setTerminalFont(const QFont &font)
{
    m_font = font;
    m_font.setStyleHint(QFont::Monospace, QFont::PreferAntialias);
    m_font.setFixedPitch(true);
    recomputeMetrics();
    resizeGridFromPixels();
    viewport()->update();
}

void TerminalWidget::recomputeMetrics()
{
    QFontMetricsF fm(m_font);
    // Use horizontalAdvance("M") for the cell width — most reliable for fixed
    // pitch fonts. Round up to whole pixels.
    m_cellW    = std::max(1, qRound(fm.horizontalAdvance(QLatin1Char('M'))));
    m_cellH    = std::max(1, qRound(fm.height()));
    m_baseline = qRound(fm.ascent());
}

void TerminalWidget::resizeGridFromPixels()
{
    if (!m_vt)
        return;

    const QSize sz = viewport()->size();
    const int newCols = std::max(20, sz.width()  / m_cellW);
    const int newRows = std::max(5,  sz.height() / m_cellH);

    if (newCols == m_cols && newRows == m_rows)
        return;

    resizeGrid(newCols, newRows);
}

void TerminalWidget::resizeGrid(int cols, int rows)
{
    if (!m_vt || (cols == m_cols && rows == m_rows))
        return;
    m_cols = cols;
    m_rows = rows;
    vterm_set_size(m_vt, m_rows, m_cols);
    if (m_screen)
        vterm_screen_flush_damage(m_screen);
    emit gridResized(m_cols, m_rows);
    viewport()->update();
}

void TerminalWidget::feedBytes(const QByteArray &data)
{
    if (!m_vt || data.isEmpty())
        return;
    vterm_input_write(m_vt, data.constData(),
                      static_cast<size_t>(data.size()));
    if (m_screen)
        vterm_screen_flush_damage(m_screen);
    viewport()->update();
}

void TerminalWidget::clearScreen()
{
    if (m_screen)
        vterm_screen_reset(m_screen, 1);
    viewport()->update();
}

// ---- libvterm callbacks ---------------------------------------------------

int TerminalWidget::s_damage(VTermRect /*rect*/, void *user)
{
    auto *self = static_cast<TerminalWidget *>(user);
    self->scheduleRepaint();
    return 1;
}

int TerminalWidget::s_moveCursor(VTermPos pos, VTermPos /*oldpos*/,
                                 int visible, void *user)
{
    auto *self = static_cast<TerminalWidget *>(user);
    self->m_cursorRow     = pos.row;
    self->m_cursorCol     = pos.col;
    self->m_cursorVisible = (visible != 0);
    self->scheduleRepaint();
    return 1;
}

int TerminalWidget::s_settermprop(VTermProp /*prop*/, VTermValue * /*val*/,
                                  void * /*user*/)
{
    return 1;
}

int TerminalWidget::s_bell(void *user)
{
    Q_UNUSED(user);
    QApplication::beep();
    return 1;
}

void TerminalWidget::s_outputCallback(const char *s, size_t len, void *user)
{
    auto *self = static_cast<TerminalWidget *>(user);
    self->writeOut(s, len);
}

void TerminalWidget::writeOut(const char *buf, size_t len)
{
    emit outputBytes(QByteArray(buf, static_cast<int>(len)));
}

void TerminalWidget::scheduleRepaint()
{
    viewport()->update();
}

// ---- Painting -------------------------------------------------------------

void TerminalWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(viewport());
    p.fillRect(rect(), m_bg);
    if (!m_screen)
        return;

    p.setFont(m_font);

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            VTermPos     pos { row, col };
            VTermScreenCell cell;
            if (!vterm_screen_get_cell(m_screen, pos, &cell))
                continue;

            const int x = col * m_cellW;
            const int y = row * m_cellH;

            const QColor bg = cellColor(cell.bg, m_bg);
            const QColor fg = cellColor(cell.fg, m_fg);

            p.fillRect(x, y, m_cellW, m_cellH, bg);

            if (cell.chars[0] != 0 && cell.chars[0] != 0x20) {
                // libvterm exposes up to VTERM_MAX_CHARS_PER_CELL UCS4 chars.
                QString glyph;
                glyph.reserve(2);
                for (int k = 0; k < VTERM_MAX_CHARS_PER_CELL && cell.chars[k]; ++k)
                    glyph.append(QChar::fromUcs4(cell.chars[k]));

                if (cell.attrs.bold) {
                    QFont bf = m_font;
                    bf.setBold(true);
                    p.setFont(bf);
                } else {
                    p.setFont(m_font);
                }
                p.setPen(fg);
                p.drawText(x, y + m_baseline, glyph);
            }
        }
    }

    // Cursor (block)
    if (m_cursorVisible && hasFocus() && m_blinkOn) {
        const int x = m_cursorCol * m_cellW;
        const int y = m_cursorRow * m_cellH;
        p.fillRect(x, y, m_cellW, m_cellH, QColor(255, 255, 255, 110));
    } else if (m_cursorVisible) {
        // Hollow cursor when unfocused
        const int x = m_cursorCol * m_cellW;
        const int y = m_cursorRow * m_cellH;
        p.setPen(QColor(255, 255, 255, 160));
        p.drawRect(x, y, m_cellW - 1, m_cellH - 1);
    }
}

// ---- Geometry / focus -----------------------------------------------------

void TerminalWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    resizeGridFromPixels();
}

void TerminalWidget::focusInEvent(QFocusEvent *e)
{
    QAbstractScrollArea::focusInEvent(e);
    m_blinkOn = true;
    viewport()->update();
}

void TerminalWidget::focusOutEvent(QFocusEvent *e)
{
    QAbstractScrollArea::focusOutEvent(e);
    viewport()->update();
}

// ---- Keyboard input -------------------------------------------------------

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_vt) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    VTermModifier mod = VTERM_MOD_NONE;
    const auto qtMod  = event->modifiers();
    if (qtMod & Qt::ShiftModifier)   mod = (VTermModifier)(mod | VTERM_MOD_SHIFT);
    if (qtMod & Qt::AltModifier)     mod = (VTermModifier)(mod | VTERM_MOD_ALT);
    if (qtMod & Qt::ControlModifier) mod = (VTermModifier)(mod | VTERM_MOD_CTRL);

    // Map Qt special keys to VTermKey
    VTermKey vk = VTERM_KEY_NONE;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:     vk = VTERM_KEY_ENTER;    break;
    case Qt::Key_Tab:       vk = VTERM_KEY_TAB;      break;
    case Qt::Key_Backspace: vk = VTERM_KEY_BACKSPACE;break;
    case Qt::Key_Escape:    vk = VTERM_KEY_ESCAPE;   break;
    case Qt::Key_Up:        vk = VTERM_KEY_UP;       break;
    case Qt::Key_Down:      vk = VTERM_KEY_DOWN;     break;
    case Qt::Key_Left:      vk = VTERM_KEY_LEFT;     break;
    case Qt::Key_Right:     vk = VTERM_KEY_RIGHT;    break;
    case Qt::Key_Home:      vk = VTERM_KEY_HOME;     break;
    case Qt::Key_End:       vk = VTERM_KEY_END;      break;
    case Qt::Key_PageUp:    vk = VTERM_KEY_PAGEUP;   break;
    case Qt::Key_PageDown:  vk = VTERM_KEY_PAGEDOWN; break;
    case Qt::Key_Insert:    vk = VTERM_KEY_INS;      break;
    case Qt::Key_Delete:    vk = VTERM_KEY_DEL;      break;
    default:
        if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F12) {
            vk = (VTermKey)(VTERM_KEY_FUNCTION_0 + 1 + (event->key() - Qt::Key_F1));
        }
        break;
    }

    if (vk != VTERM_KEY_NONE) {
        vterm_keyboard_key(m_vt, vk, mod);
        event->accept();
        return;
    }

    // Plain text keys
    const QString text = event->text();
    if (!text.isEmpty()) {
        for (const QChar &c : text) {
            vterm_keyboard_unichar(m_vt, c.unicode(), mod);
        }
        event->accept();
        return;
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *event)
{
    const QString commit = event->commitString();
    if (!commit.isEmpty() && m_vt) {
        for (const QChar &c : commit)
            vterm_keyboard_unichar(m_vt, c.unicode(), VTERM_MOD_NONE);
    }
    event->accept();
}

// ---- Mouse (placeholders for now) -----------------------------------------

void TerminalWidget::mousePressEvent(QMouseEvent *e)
{
    setFocus(Qt::MouseFocusReason);
    e->accept();
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *e)      { e->accept(); }
void TerminalWidget::mouseReleaseEvent(QMouseEvent *e)   { e->accept(); }
