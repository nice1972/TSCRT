/*
 * HelpDialog - shows the user-facing usage guide loaded from a
 * language-specific HTML resource (:/help/usage_<lang>.html).
 *
 * The displayed language follows the user's UI language preference
 * (QSettings "ui/language"), falling back to English.
 */
#pragma once

#include <QDialog>

class QTextBrowser;

class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent = nullptr);

private:
    QTextBrowser *m_view = nullptr;
};
