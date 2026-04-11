/*
 * FindBar - slim search bar that appears under the TerminalWidget.
 * Shared between the Find and Mark features; toggling the Mark button
 * promotes the current pattern into a permanent highlight that survives
 * the bar being closed.
 */
#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;

namespace tscrt {

class FindBar : public QWidget {
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);

    QString pattern() const;
    bool    caseSensitive() const;
    bool    regex() const;
    bool    markActive() const;

    void setPattern(const QString &p);
    void setMatchInfo(int current, int total);
    void setMarkActive(bool on);

    void focusInput();

signals:
    void findNextRequested();
    void findPrevRequested();
    void patternOrOptionsChanged();
    void markToggled(bool on);
    void closeRequested();

protected:
    void keyPressEvent(QKeyEvent *e) override;

private:
    QLineEdit   *m_edit   = nullptr;
    QLabel      *m_count  = nullptr;
    QToolButton *m_btnPrev  = nullptr;
    QToolButton *m_btnNext  = nullptr;
    QToolButton *m_btnCase  = nullptr;
    QToolButton *m_btnRegex = nullptr;
    QToolButton *m_btnMark  = nullptr;
    QToolButton *m_btnClose = nullptr;
};

} // namespace tscrt
