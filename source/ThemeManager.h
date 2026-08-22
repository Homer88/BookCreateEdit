#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>

class QApplication;

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme { Light, Dark, System };

    static ThemeManager &instance();

    void setTheme(Theme theme);
    Theme currentTheme() const;

    void applyTheme();
    QString themeStyleSheet() const;

signals:
    void themeChanged(Theme theme);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    Theme m_theme;
    Theme m_resolvedTheme;
};

#endif // THEMEMANAGER_H
