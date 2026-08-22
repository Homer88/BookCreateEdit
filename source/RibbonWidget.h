#ifndef RIBBONWIDGET_H
#define RIBBONWIDGET_H

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QIcon>
#include <QPropertyAnimation>

class QToolButton;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;

struct RibbonGroup
{
    QString title;
    QList<QToolButton*> buttons;
};

class RibbonTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit RibbonTabBar(QWidget *parent = nullptr);
    void setFileTabIndex(int index) { m_fileTabIndex = index; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_fileTabIndex;
};

class RibbonTab : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonTab(QWidget *parent = nullptr);
    void addGroup(const QString &title, const QList<QToolButton*> &buttons);
    void setFileTabStyle(bool isFileTab);

private:
    QWidget *createGroupWidget(const QString &title, const QList<QToolButton*> &buttons,
                               bool isLast);
    QHBoxLayout *m_layout;
    bool m_isFileTab;
};

class RibbonWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int ribbonHeight READ ribbonHeight WRITE setRibbonHeight)

public:
    explicit RibbonWidget(QWidget *parent = nullptr);

    RibbonTab *addTab(const QString &title);
    RibbonTab *addFileTab(const QString &title = "Файл");
    QToolButton *createButton(const QString &text, const QIcon &icon = QIcon(),
                              const QString &tooltip = QString());

    bool isCollapsed() const { return m_collapsed; }
    int ribbonHeight() const { return m_contentHeight; }
    void setRibbonHeight(int h);

signals:
    void tabChanged(int index);
    void collapsedChanged(bool collapsed);

public slots:
    void toggleCollapse();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUI();
    void onTabClicked(int index);
    void applyThemeStyle();
    void updateTabStyle(int index);

    RibbonTabBar *m_tabBar;
    QStackedWidget *m_stack;
    QList<RibbonTab*> m_tabs;
    QVBoxLayout *m_mainLayout;

    QWidget *m_contentWidget;
    int m_fullHeight;
    int m_contentHeight;
    bool m_collapsed;
    int m_fileTabIndex;
};

#endif // RIBBONWIDGET_H
