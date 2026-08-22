#include "RibbonWidget.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QFrame>
#include <QStackedWidget>
#include <QFontMetrics>
#include <QApplication>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPen>
#include <QCursor>

// ===================== RibbonTabBar =====================
RibbonTabBar::RibbonTabBar(QWidget *parent)
    : QTabBar(parent)
    , m_fileTabIndex(-1)
{
}

void RibbonTabBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    for (int i = 0; i < count(); ++i) {
        QRect rect = tabRect(i);
        if (!rect.isValid()) continue;

        bool isSelected = (i == currentIndex());
        bool isFile = (i == m_fileTabIndex);
        bool isHovered = rect.contains(mapFromGlobal(QCursor::pos()));

        painter.save();

        if (isFile) {
            QColor fileBg = isDark ? QColor("#007acc") : QColor("#2a6ab9");
            painter.fillRect(rect, fileBg);
            painter.setPen(QColor("#ffffff"));
            QFont f = painter.font();
            f.setBold(true);
            painter.setFont(f);
        } else if (isSelected) {
            QColor selBg = isDark ? QColor("#1e1e1e") : QColor("#f8f8f8");
            painter.fillRect(rect, selBg);
            painter.setPen(isDark ? QColor("#ffffff") : QColor("#333333"));
            QFont f = painter.font();
            f.setBold(true);
            painter.setFont(f);

            QPen bottomPen(isDark ? QColor("#007acc") : QColor("#4a90d9"), 2);
            painter.setPen(bottomPen);
            painter.drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
        } else if (isHovered) {
            QColor hoverBg = isDark ? QColor("#3c3c3c") : QColor("#8ac0f0");
            painter.fillRect(rect, hoverBg);
            painter.setPen(isDark ? QColor("#d4d4d4") : QColor("#ffffff"));
        } else {
            QColor normalBg = isDark ? QColor("#2d2d2d") : QColor("#4a90d9");
            painter.fillRect(rect, normalBg);
            painter.setPen(isDark ? QColor("#969696") : QColor("#ffffff"));
        }

        painter.drawText(rect, Qt::AlignCenter, tabText(i));
        painter.restore();
    }
}

// ===================== RibbonTab =====================
RibbonTab::RibbonTab(QWidget *parent)
    : QWidget(parent)
    , m_isFileTab(false)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 2);
    m_layout->setSpacing(0);
    m_layout->addStretch();
}

void RibbonTab::setFileTabStyle(bool isFileTab)
{
    m_isFileTab = isFileTab;
}

void RibbonTab::addGroup(const QString &title, const QList<QToolButton*> &buttons)
{
    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    if (m_layout->count() > 1) {
        QFrame *sep = new QFrame(this);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Sunken);
        if (isDark) {
            sep->setStyleSheet("color: #3c3c3c; background: transparent; border: none;");
        } else {
            sep->setStyleSheet("color: #c0c0c0; background: transparent; border: none;");
        }
        sep->setFixedWidth(1);
        m_layout->insertWidget(m_layout->count() - 1, sep);
    }

    m_layout->insertWidget(m_layout->count() - 1,
                           createGroupWidget(title, buttons, false));
}

QWidget *RibbonTab::createGroupWidget(const QString &title, const QList<QToolButton*> &buttons,
                                      bool isLast)
{
    QFrame *group = new QFrame(this);
    group->setObjectName("ribbonGroup");
    group->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(6, 2, 6, 2);
    groupLayout->setSpacing(2);

    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(1);

    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    for (QToolButton *btn : buttons) {
        btn->setParent(group);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        int iconSz = 24;
        int btnW = 56;
        int btnH = 52;
        btn->setIconSize(QSize(iconSz, iconSz));
        btn->setFixedSize(btnW, btnH);
        btn->setAutoRaise(true);

        if (isDark) {
            btn->setStyleSheet(R"(
                QToolButton {
                    border: 1px solid transparent;
                    border-radius: 3px;
                    padding: 3px;
                    color: #d4d4d4;
                }
                QToolButton:hover {
                    border: 1px solid #505050;
                    background-color: #3c3c3c;
                }
                QToolButton:pressed {
                    background-color: #094771;
                    border: 1px solid #007acc;
                }
            )");
        } else {
            btn->setStyleSheet(R"(
                QToolButton {
                    border: 1px solid transparent;
                    border-radius: 3px;
                    padding: 3px;
                    color: #333;
                }
                QToolButton:hover {
                    border: 1px solid #b0d0f0;
                    background-color: #d8eaff;
                }
                QToolButton:pressed {
                    background-color: #b8d0f0;
                    border: 1px solid #6aafee;
                }
            )");
        }
        buttonsLayout->addWidget(btn);
    }

    groupLayout->addLayout(buttonsLayout);

    QLabel *label = new QLabel(title, group);
    label->setAlignment(Qt::AlignCenter);
    QFont labelFont = qApp->font();
    labelFont.setPointSize(qMax(6, labelFont.pointSize() - 2));
    label->setFont(labelFont);
    if (isDark) {
        label->setStyleSheet("color: #888; border: none; background: transparent;");
    } else {
        label->setStyleSheet("color: #606060; border: none; background: transparent;");
    }
    groupLayout->addWidget(label);

    return group;
}

// ===================== RibbonWidget =====================
RibbonWidget::RibbonWidget(QWidget *parent)
    : QWidget(parent)
    , m_collapsed(false)
    , m_fullHeight(0)
    , m_contentHeight(0)
    , m_fileTabIndex(-1)
{
    setupUI();
    applyThemeStyle();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &RibbonWidget::applyThemeStyle);
}

void RibbonWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_tabBar = new RibbonTabBar(this);
    m_tabBar->setExpanding(false);
    m_tabBar->setObjectName("ribbonTabBar");

    m_contentWidget = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_stack = new QStackedWidget(this);
    contentLayout->addWidget(m_stack);

    m_mainLayout->addWidget(m_tabBar);
    m_mainLayout->addWidget(m_contentWidget, 1);

    connect(m_tabBar, &QTabBar::currentChanged, this, &RibbonWidget::onTabClicked);
}

void RibbonWidget::applyThemeStyle()
{
    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    if (isDark) {
        setStyleSheet(R"(
            RibbonWidget {
                background-color: #2d2d2d;
                border-bottom: 1px solid #3c3c3c;
            }
        )");
        m_tabBar->setStyleSheet("RibbonTabBar { background: #2d2d2d; }");
    } else {
        setStyleSheet(R"(
            RibbonWidget {
                background-color: #f0f4f8;
                border-bottom: 2px solid #6699cc;
            }
        )");
        m_tabBar->setStyleSheet(R"(
            RibbonTabBar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4a90d9, stop:1 #2a6ab9); }
        )");
    }

    m_tabBar->setFileTabIndex(m_fileTabIndex);
    m_tabBar->update();
    updateTabStyle(m_tabBar->currentIndex());
}

void RibbonWidget::updateTabStyle(int)
{
}

RibbonTab *RibbonWidget::addTab(const QString &title)
{
    RibbonTab *tab = new RibbonTab(this);
    m_tabs.append(tab);
    m_stack->addWidget(tab);
    m_tabBar->addTab(title);

    if (m_fullHeight == 0) {
        m_fullHeight = 80;
        m_contentHeight = m_fullHeight;
    }
    return tab;
}

RibbonTab *RibbonWidget::addFileTab(const QString &title)
{
    RibbonTab *tab = new RibbonTab(this);
    tab->setFileTabStyle(true);
    m_tabs.append(tab);
    m_stack->addWidget(tab);

    m_fileTabIndex = m_tabBar->addTab(title);

    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    if (isDark) {
        m_tabBar->setTabData(m_fileTabIndex, "#007acc");
    } else {
        m_tabBar->setTabData(m_fileTabIndex, "#2a6ab9");
    }

    return tab;
}

QToolButton *RibbonWidget::createButton(const QString &text, const QIcon &icon,
                                         const QString &tooltip)
{
    QToolButton *btn = new QToolButton(this);
    btn->setText(text);
    if (!icon.isNull()) {
        btn->setIcon(icon);
    }
    if (!tooltip.isEmpty()) {
        btn->setToolTip(tooltip);
    }
    return btn;
}

void RibbonWidget::toggleCollapse()
{
    m_collapsed = !m_collapsed;

    if (m_collapsed) {
        m_contentWidget->hide();
    } else {
        m_contentWidget->show();
    }
    emit collapsedChanged(m_collapsed);
}

void RibbonWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F1 && event->modifiers() == Qt::ControlModifier) {
        toggleCollapse();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void RibbonWidget::onTabClicked(int index)
{
    if (index >= 0 && index < m_tabs.size()) {
        m_stack->setCurrentIndex(index);

        bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (i == m_fileTabIndex) {
                continue;
            }
        }

        emit tabChanged(index);
    }
}

void RibbonWidget::setRibbonHeight(int h)
{
    m_contentHeight = h;
    updateGeometry();
}
