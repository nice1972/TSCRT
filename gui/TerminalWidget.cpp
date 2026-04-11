#include "TerminalWidget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMenu>
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

QColor cellColor(VTermColor c, const VTermScreen *screen, const QColor &fallback)
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&c) || VTERM_COLOR_IS_DEFAULT_BG(&c))
        return fallback;
    // Convert indexed palette colors to RGB so we always read .rgb fields.
    if (VTERM_COLOR_IS_INDEXED(&c) && screen)
        vterm_screen_convert_color_to_rgb(screen, &c);
    if (VTERM_COLOR_IS_RGB(&c))
        return QColor(c.rgb.red, c.rgb.green, c.rgb.blue);
    return fallback;
}

const VTermScreenCallbacks kScreenCallbacks = {
    /* damage      */ &TerminalWidget::s_damage,
    /* moverect    */ nullptr,
    /* movecursor  */ &TerminalWidget::s_moveCursor,
    /* settermprop */ &TerminalWidget::s_settermprop,
    /* bell        */ &TerminalWidget::s_bell,
    /* resize      */ nullptr,
    /* sb_pushline */ &TerminalWidget::s_sbPushline,
    /* sb_popline  */ &TerminalWidget::s_sbPopline,
    /* sb_clear    */ &TerminalWidget::s_sbClear,
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
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    verticalScrollBar()->setSingleStep(1);

    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setPointSize(11);
    setTerminalFont(m_font);

    m_vt = vterm_new(m_rows, m_cols);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(m_vt, &TerminalWidget::s_outputCallback, this);

    m_screen = vterm_obtain_screen(m_vt);
    vterm_screen_enable_altscreen(m_screen, 1);
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

int TerminalWidget::s_settermprop(VTermProp prop, VTermValue *val,
                                  void *user)
{
    auto *self = static_cast<TerminalWidget *>(user);
    switch (prop) {
    case VTERM_PROP_CURSORVISIBLE:
        self->m_cursorVisible = val->boolean;
        self->scheduleRepaint();
        break;
    case VTERM_PROP_MOUSE:
        self->m_mouseMode = val->number;
        break;
    default:
        break;
    }
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

int TerminalWidget::s_sbPushline(int cols, const VTermScreenCell *cells, void *user)
{
    auto *self = static_cast<TerminalWidget *>(user);
    const bool wasAtBottom = (self->m_scrollOffset == 0);
    std::vector<VTermScreenCell> line(cells, cells + cols);
    self->m_scrollback.push_back(std::move(line));
    if (int(self->m_scrollback.size()) > self->m_scrollMax)
        self->m_scrollback.pop_front();
    self->updateScrollBarRange();
    if (wasAtBottom) self->scrollToBottom();
    self->scheduleRepaint();
    return 1;
}

int TerminalWidget::s_sbPopline(int cols, VTermScreenCell *cells, void *user)
{
    auto *self = static_cast<TerminalWidget *>(user);
    if (self->m_scrollback.empty()) return 0;
    const auto &line = self->m_scrollback.back();
    const int n = std::min(cols, int(line.size()));
    for (int i = 0; i < n; ++i) cells[i] = line[i];
    // Pad missing columns with the last cell's blank attributes.
    for (int i = n; i < cols; ++i) {
        VTermScreenCell blank{};
        cells[i] = blank;
    }
    self->m_scrollback.pop_back();
    self->updateScrollBarRange();
    self->scheduleRepaint();
    return 1;
}

int TerminalWidget::s_sbClear(void *user)
{
    auto *self = static_cast<TerminalWidget *>(user);
    self->m_scrollback.clear();
    self->m_scrollOffset = 0;
    self->updateScrollBarRange();
    self->scheduleRepaint();
    return 1;
}

void TerminalWidget::updateScrollBarRange()
{
    auto *sb = verticalScrollBar();
    const int max = int(m_scrollback.size());
    sb->setRange(0, max);
    sb->setPageStep(m_rows);
    // Translate m_scrollOffset (lines above bottom) to scrollbar value.
    // Convention: value 0 = top of scrollback, value max = bottom (live).
    sb->blockSignals(true);
    sb->setValue(max - m_scrollOffset);
    sb->blockSignals(false);
}

void TerminalWidget::scrollToBottom()
{
    m_scrollOffset = 0;
    auto *sb = verticalScrollBar();
    sb->blockSignals(true);
    sb->setValue(sb->maximum());
    sb->blockSignals(false);
}

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    QAbstractScrollArea::wheelEvent(event);
}

void TerminalWidget::scrollContentsBy(int /*dx*/, int /*dy*/)
{
    const int max = int(m_scrollback.size());
    m_scrollOffset = max - verticalScrollBar()->value();
    if (m_scrollOffset < 0)        m_scrollOffset = 0;
    if (m_scrollOffset > max)      m_scrollOffset = max;
    viewport()->update();
}

void TerminalWidget::writeOut(const char *buf, size_t len)
{
    emit outputBytes(QByteArray(buf, static_cast<int>(len)));
}

void TerminalWidget::scheduleRepaint()
{
    viewport()->update();
}

// ---- Selection helpers ----------------------------------------------------

TerminalWidget::GridPos TerminalWidget::cellAt(QPoint pos) const
{
    GridPos g;
    g.col = pos.x() / m_cellW;
    g.row = pos.y() / m_cellH;
    if (g.col < 0) g.col = 0;
    if (g.row < 0) g.row = 0;
    if (g.col >= m_cols) g.col = m_cols - 1;
    if (g.row >= m_rows) g.row = m_rows - 1;
    return g;
}

bool TerminalWidget::cellInSelection(int row, int col) const
{
    if (!m_selAnchor.valid() || !m_selCursor.valid())
        return false;
    GridPos a = m_selAnchor;
    GridPos b = m_selCursor;
    if (b < a) std::swap(a, b);
    GridPos here{ row, col };
    return !(here < a) && (here < b || here == b);
}

bool TerminalWidget::cellInHighlight(int row, int col) const
{
    if (m_highlight.isEmpty() || !m_screen)
        return false;
    // Build the row text and find substring offsets that cover this column.
    QString line;
    line.reserve(m_cols);
    for (int c = 0; c < m_cols; ++c) {
        VTermPos p{ row, c };
        VTermScreenCell cell;
        if (!vterm_screen_get_cell(m_screen, p, &cell)) {
            line.append(QLatin1Char(' '));
            continue;
        }
        if (cell.chars[0])
            line.append(QChar::fromUcs4(cell.chars[0]));
        else
            line.append(QLatin1Char(' '));
    }
    int from = 0;
    while (true) {
        const int idx = line.indexOf(m_highlight, from);
        if (idx < 0) return false;
        if (col >= idx && col < idx + m_highlight.length()) return true;
        from = idx + 1;
    }
}

QString TerminalWidget::selectionText() const
{
    if (!m_selAnchor.valid() || !m_selCursor.valid() || !m_screen)
        return {};
    GridPos a = m_selAnchor;
    GridPos b = m_selCursor;
    if (b < a) std::swap(a, b);

    QString out;
    for (int row = a.row; row <= b.row; ++row) {
        const int cStart = (row == a.row) ? a.col : 0;
        const int cEnd   = (row == b.row) ? b.col : (m_cols - 1);
        QString line;
        for (int col = cStart; col <= cEnd; ++col) {
            VTermPos p{ row, col };
            VTermScreenCell cell;
            if (!vterm_screen_get_cell(m_screen, p, &cell))
                continue;
            if (cell.chars[0])
                line.append(QChar::fromUcs4(cell.chars[0]));
            else
                line.append(QLatin1Char(' '));
        }
        // Trim trailing spaces (terminal cells are space-padded).
        int end = line.length();
        while (end > 0 && line.at(end - 1) == QLatin1Char(' ')) --end;
        out.append(line.left(end));
        if (row < b.row)
            out.append(QLatin1Char('\n'));
    }
    return out;
}

void TerminalWidget::copySelection()
{
    const QString text = selectionText();
    if (!text.isEmpty())
        QGuiApplication::clipboard()->setText(text);
}

void TerminalWidget::pasteFromClipboard()
{
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    emit outputBytes(text.toUtf8());
}

void TerminalWidget::setHighlightPattern(const QString &pattern)
{
    m_highlight = pattern;
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

    const int sbCount = int(m_scrollback.size());

    for (int row = 0; row < m_rows; ++row) {
        // Map this visible row to either a scrollback line or a
        // current-screen row, taking m_scrollOffset into account.
        const int bottomLogical = sbCount + m_rows - 1 - m_scrollOffset;
        const int logical       = bottomLogical - (m_rows - 1 - row);
        if (logical < 0) continue;

        for (int col = 0; col < m_cols; ++col) {
            VTermScreenCell cell{};
            if (logical < sbCount) {
                const auto &line = m_scrollback[logical];
                if (col >= int(line.size())) continue;
                cell = line[col];
            } else {
                VTermPos pos { logical - sbCount, col };
                if (!vterm_screen_get_cell(m_screen, pos, &cell))
                    continue;
            }

            // Continuation cell of a wide character — already drawn.
            if (cell.width == 0)
                continue;

            const int cellSpan = std::max(1, int(cell.width));
            const int x = col * m_cellW;
            const int y = row * m_cellH;
            const int w = cellSpan * m_cellW;

            QColor bg = cellColor(cell.bg, m_screen, m_bg);
            QColor fg = cellColor(cell.fg, m_screen, m_fg);

            if (cell.attrs.reverse)
                std::swap(fg, bg);

            // Selection takes precedence over the cell background
            if (cellInSelection(row, col)) {
                bg = QColor(70, 110, 200);
                fg = QColor(0xff, 0xff, 0xff);
            } else if (cellInHighlight(row, col)) {
                // Mark highlight: amber background
                bg = QColor(0xc0, 0x70, 0x00);
                fg = QColor(0x00, 0x00, 0x00);
            }

            p.fillRect(x, y, w, m_cellH, bg);

            if (cell.chars[0] != 0 && cell.chars[0] != 0x20) {
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

            // Skip continuation columns of wide characters.
            if (cellSpan > 1)
                col += cellSpan - 1;
        }
    }

    // Cursor — only when looking at the live screen.
    if (m_scrollOffset == 0 && m_cursorVisible) {
        const int x = m_cursorCol * m_cellW;
        const int y = m_cursorRow * m_cellH;

        // Read the cell under the cursor for glyph redraw.
        VTermScreenCell cell{};
        VTermPos cpos{ m_cursorRow, m_cursorCol };
        if (m_screen)
            vterm_screen_get_cell(m_screen, cpos, &cell);

        // Pick cursor brightness: bright when focused+blink-on,
        // dimmer otherwise — always a solid filled block.
        QColor curBg;
        if (hasFocus() && m_blinkOn)
            curBg = QColor(0xFF, 0xFF, 0xFF);   // bright white
        else if (hasFocus())
            curBg = QColor(0x80, 0x80, 0x80);   // medium gray (blink-off)
        else
            curBg = QColor(0x70, 0x70, 0x70);   // unfocused
        static const QColor curFg(0x00, 0x00, 0x00);

        p.fillRect(x, y, m_cellW, m_cellH, curBg);

        if (cell.chars[0] != 0 && cell.chars[0] != 0x20) {
            QString glyph;
            for (int k = 0; k < VTERM_MAX_CHARS_PER_CELL && cell.chars[k]; ++k)
                glyph.append(QChar::fromUcs4(cell.chars[k]));
            p.setFont(cell.attrs.bold
                ? [&]{ QFont bf = m_font; bf.setBold(true); return bf; }()
                : m_font);
            p.setPen(curFg);
            p.drawText(x, y + m_baseline, glyph);
        }
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

bool TerminalWidget::focusNextPrevChild(bool /*next*/)
{
    // Prevent Tab / Shift+Tab from moving focus away — the terminal
    // needs those keys for shell tab-completion, etc.
    return false;
}

// ---- Keyboard input -------------------------------------------------------

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_vt) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    // Ctrl-Shift-C / Ctrl-Shift-V: clipboard. Plain Ctrl-C/V remain
    // available as terminal-side keystrokes (sigint, etc.).
    if ((event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
        == (Qt::ControlModifier | Qt::ShiftModifier)) {
        if (event->key() == Qt::Key_C) { copySelection();      event->accept(); return; }
        if (event->key() == Qt::Key_V) { pasteFromClipboard(); event->accept(); return; }
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

    // Ctrl + letter / digit / symbol: Qt gives event->text() as the
    // already-encoded control byte (e.g. "\x03" for Ctrl+C). Passing
    // that to vterm_keyboard_unichar together with VTERM_MOD_CTRL would
    // double-encode it and libvterm produces nothing. Instead send the
    // unmodified key code as the unichar and let libvterm encode it.
    if ((qtMod & Qt::ControlModifier) && !(qtMod & Qt::AltModifier)) {
        const int k = event->key();
        if (k >= Qt::Key_A && k <= Qt::Key_Z) {
            vterm_keyboard_unichar(m_vt, uint32_t('a' + (k - Qt::Key_A)), mod);
            event->accept();
            return;
        }
        if (k >= Qt::Key_0 && k <= Qt::Key_9) {
            vterm_keyboard_unichar(m_vt, uint32_t('0' + (k - Qt::Key_0)), mod);
            event->accept();
            return;
        }
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

// ---- Mouse: selection + terminal forwarding --------------------------------

static VTermModifier qtMouseMod(Qt::KeyboardModifiers m)
{
    VTermModifier mod = VTERM_MOD_NONE;
    if (m & Qt::ShiftModifier)   mod = VTermModifier(mod | VTERM_MOD_SHIFT);
    if (m & Qt::ControlModifier) mod = VTermModifier(mod | VTERM_MOD_CTRL);
    if (m & Qt::AltModifier)     mod = VTermModifier(mod | VTERM_MOD_ALT);
    return mod;
}

static int qtButtonToVterm(Qt::MouseButton btn)
{
    switch (btn) {
    case Qt::LeftButton:   return 1;
    case Qt::MiddleButton: return 2;
    case Qt::RightButton:  return 3;
    default:               return 0;
    }
}

void TerminalWidget::mousePressEvent(QMouseEvent *e)
{
    setFocus(Qt::MouseFocusReason);

    // When the remote app has mouse tracking on and Shift is NOT held,
    // forward to vterm so tmux/vim/etc. receive mouse events.
    if (m_mouseMode != VTERM_PROP_MOUSE_NONE
        && !(e->modifiers() & Qt::ShiftModifier) && m_vt) {
        const GridPos g = cellAt(e->pos());
        vterm_mouse_move(m_vt, g.row, g.col, qtMouseMod(e->modifiers()));
        vterm_mouse_button(m_vt, qtButtonToVterm(e->button()),
                           true, qtMouseMod(e->modifiers()));
        e->accept();
        return;
    }

    // Local selection (no mouse tracking, or Shift held).
    if (e->button() == Qt::LeftButton) {
        m_selecting = true;
        m_selAnchor = cellAt(e->pos());
        m_selCursor = m_selAnchor;
        viewport()->update();
    } else if (e->button() == Qt::MiddleButton) {
        pasteFromClipboard();
    }
    e->accept();
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_mouseMode >= VTERM_PROP_MOUSE_DRAG
        && !(e->modifiers() & Qt::ShiftModifier) && m_vt) {
        const GridPos g = cellAt(e->pos());
        vterm_mouse_move(m_vt, g.row, g.col, qtMouseMod(e->modifiers()));
        e->accept();
        return;
    }

    if (m_selecting) {
        m_selCursor = cellAt(e->pos());
        viewport()->update();
    }
    e->accept();
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_mouseMode != VTERM_PROP_MOUSE_NONE
        && !(e->modifiers() & Qt::ShiftModifier) && m_vt) {
        const GridPos g = cellAt(e->pos());
        vterm_mouse_move(m_vt, g.row, g.col, qtMouseMod(e->modifiers()));
        vterm_mouse_button(m_vt, qtButtonToVterm(e->button()),
                           false, qtMouseMod(e->modifiers()));
        e->accept();
        return;
    }

    if (e->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        if (m_selAnchor.valid() && m_selCursor.valid()
            && !(m_selAnchor == m_selCursor)) {
            copySelection();
        } else {
            m_selAnchor = {};
            m_selCursor = {};
            viewport()->update();
        }
    }
    e->accept();
}

void TerminalWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_screen) {
        QAbstractScrollArea::mouseDoubleClickEvent(e);
        return;
    }
    // Word selection: walk left/right while non-space
    const GridPos g = cellAt(e->pos());
    auto isWordChar = [&](int r, int c) -> bool {
        VTermPos p{ r, c };
        VTermScreenCell cell;
        if (!vterm_screen_get_cell(m_screen, p, &cell))
            return false;
        const uint32_t ch = cell.chars[0];
        return ch != 0 && ch != ' ' && ch != '\t';
    };
    int left = g.col;
    while (left > 0 && isWordChar(g.row, left - 1)) --left;
    int right = g.col;
    while (right < m_cols - 1 && isWordChar(g.row, right + 1)) ++right;

    m_selAnchor = { g.row, left };
    m_selCursor = { g.row, right };
    copySelection();
    viewport()->update();
    e->accept();
}

void TerminalWidget::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu menu(this);
    auto *copyAct = menu.addAction(tr("&Copy"));
    copyAct->setEnabled(m_selAnchor.valid() && m_selCursor.valid()
                        && !(m_selAnchor == m_selCursor));
    connect(copyAct, &QAction::triggered, this, &TerminalWidget::copySelection);

    auto *pasteAct = menu.addAction(tr("&Paste"));
    pasteAct->setEnabled(!QGuiApplication::clipboard()->text().isEmpty());
    connect(pasteAct, &QAction::triggered, this, &TerminalWidget::pasteFromClipboard);

    menu.addSeparator();
    auto *clearAct = menu.addAction(tr("Clear &screen"));
    connect(clearAct, &QAction::triggered, this, &TerminalWidget::clearScreen);

    menu.exec(e->globalPos());
    e->accept();
}
