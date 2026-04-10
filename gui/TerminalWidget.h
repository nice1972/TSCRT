/*
 * TerminalWidget - VT100/xterm terminal widget backed by libvterm.
 *
 * Owns a VTerm instance and renders its screen with QPainter. Forwards
 * Qt key/mouse/resize events to the libvterm input layer and emits
 * outputBytes() whenever libvterm produces bytes that should be sent to
 * the remote end.
 *
 * Designed for commercial-quality use:
 *   - All user-visible strings go through tr()
 *   - No naked new/delete (RAII)
 *   - HiDPI safe (devicePixelRatioF used for metrics)
 *   - Keyboard focus + accessibility hints exposed
 */
#pragma once

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QVector>
#include <vector>

#include <vterm.h>

class TerminalWidget : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    /// Configure the monospace font and recompute cell metrics.
    void setTerminalFont(const QFont &font);

    /// Reset internal state and resize the libvterm grid to (cols, rows).
    void resizeGrid(int cols, int rows);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

public slots:
    /// Feed bytes coming from the remote session into the VT parser.
    void feedBytes(const QByteArray &data);

    /// Clear the on-screen contents (does not touch the connection).
    void clearScreen();

    /// Copy the current selection to the clipboard, or do nothing if
    /// nothing is selected.
    void copySelection();

    /// Paste clipboard text as if typed by the user.
    void pasteFromClipboard();

    /// Highlight pattern (substring). Empty string clears the mark.
    void setHighlightPattern(const QString &pattern);

signals:
    /// Bytes that the terminal wants to send back to the host (key input,
    /// mouse reports, status responses).
    void outputBytes(const QByteArray &data);

    /// Emitted whenever the displayed grid size changes (so the session can
    /// request a matching PTY/window size on the remote end).
    void gridResized(int cols, int rows);

protected:
    void paintEvent(QPaintEvent *event)        override;
    void resizeEvent(QResizeEvent *event)      override;
    void wheelEvent(QWheelEvent *event)        override;
    void scrollContentsBy(int dx, int dy)      override;
    void keyPressEvent(QKeyEvent *event)       override;
    void inputMethodEvent(QInputMethodEvent *) override;
    void mousePressEvent(QMouseEvent *e)       override;
    void mouseMoveEvent(QMouseEvent *e)        override;
    void mouseReleaseEvent(QMouseEvent *e)     override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void focusInEvent(QFocusEvent *e)          override;
    void focusOutEvent(QFocusEvent *e)         override;
    bool focusNextPrevChild(bool next)         override;

public:
    // libvterm callback trampolines (public so the static callback table
    // in the .cpp can take their addresses; not part of the public API).
    static int  s_damage(VTermRect rect, void *user);
    static int  s_moveCursor(VTermPos pos, VTermPos oldpos,
                             int visible, void *user);
    static int  s_settermprop(VTermProp prop, VTermValue *val, void *user);
    static int  s_bell(void *user);
    static void s_outputCallback(const char *s, size_t len, void *user);
    static int  s_sbPushline(int cols, const VTermScreenCell *cells, void *user);
    static int  s_sbPopline (int cols,       VTermScreenCell *cells, void *user);
    static int  s_sbClear   (void *user);

private:
    struct GridPos {
        int row = -1;
        int col = -1;
        bool valid() const { return row >= 0 && col >= 0; }
        bool operator==(const GridPos &o) const { return row == o.row && col == o.col; }
        bool operator<(const GridPos &o) const {
            return row < o.row || (row == o.row && col < o.col);
        }
    };

    void recomputeMetrics();
    void resizeGridFromPixels();
    void writeOut(const char *buf, size_t len);
    void scheduleRepaint();

    GridPos cellAt(QPoint pos) const;
    QString selectionText() const;
    bool    cellInSelection(int row, int col) const;
    bool    cellInHighlight(int row, int col) const;

    // State
    VTerm        *m_vt        = nullptr;
    VTermScreen  *m_screen    = nullptr;
    int           m_cols      = 80;
    int           m_rows      = 24;

    QFont         m_font;
    int           m_cellW     = 8;
    int           m_cellH     = 16;
    int           m_baseline  = 12;

    bool          m_cursorVisible = true;
    int           m_cursorRow     = 0;
    int           m_cursorCol     = 0;
    bool          m_blinkOn       = true;
    QTimer        m_blinkTimer;

    QColor        m_bg{ 0x10, 0x10, 0x10 };
    QColor        m_fg{ 0xE0, 0xE0, 0xE0 };

    // Selection
    bool          m_selecting = false;
    GridPos       m_selAnchor;
    GridPos       m_selCursor;

    // Highlight (mark) pattern
    QString       m_highlight;

    // Scrollback. Each entry is one screen-wide line of cells; new lines
    // (the most recent rolled-off content) are appended to the back.
    // m_scrollOffset > 0 means the user has scrolled up by that many
    // lines; 0 means follow the live screen.
    std::vector<std::vector<VTermScreenCell>> m_scrollback;
    int           m_scrollMax    = 10000;
    int           m_scrollOffset = 0;

    void updateScrollBarRange();
    void scrollToBottom();
};
