#include "ThemeManager.h"
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

ThemeManager &ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_theme(System)
    , m_resolvedTheme(Light)
{
}

void ThemeManager::setTheme(Theme theme)
{
    m_theme = theme;
    if (theme == System) {
        QPalette p;
        m_resolvedTheme = (p.window().color().lightness() < 128) ? Dark : Light;
    } else {
        m_resolvedTheme = theme;
    }
    applyTheme();
    emit themeChanged(m_theme);
}

ThemeManager::ThemeManager::Theme ThemeManager::currentTheme() const
{
    return m_theme;
}

void ThemeManager::applyTheme()
{
    if (m_resolvedTheme == Dark) {
        qApp->setStyleSheet(themeStyleSheet());
    } else {
        qApp->setStyleSheet("");
        QPalette lightPalette = QPalette();
        lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::Base, Qt::white);
        lightPalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
        lightPalette.setColor(QPalette::ToolTipBase, Qt::white);
        lightPalette.setColor(QPalette::ToolTipText, Qt::black);
        lightPalette.setColor(QPalette::Text, Qt::black);
        lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::ButtonText, Qt::black);
        lightPalette.setColor(QPalette::BrightText, Qt::red);
        lightPalette.setColor(QPalette::Link, QColor(0, 100, 200));
        lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        lightPalette.setColor(QPalette::HighlightedText, Qt::white);
        lightPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(160, 160, 160));
        lightPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(160, 160, 160));
        qApp->setPalette(lightPalette);
    }
}

QString ThemeManager::themeStyleSheet() const
{
    return QStringLiteral(R"(
        * {
            font-family: "Segoe UI";
        }
        QWidget {
            background-color: #2d2d2d;
            color: #d4d4d4;
        }
        QMenuBar {
            background-color: #2d2d2d;
            color: #d4d4d4;
            border-bottom: 1px solid #3c3c3c;
        }
        QMenuBar::item {
            padding: 4px 8px;
            background: transparent;
        }
        QMenuBar::item:selected {
            background-color: #3c3c3c;
            border-radius: 3px;
        }
        QMenu {
            background-color: #2d2d2d;
            color: #d4d4d4;
            border: 1px solid #3c3c3c;
        }
        QMenu::item {
            padding: 6px 24px;
        }
        QMenu::item:selected {
            background-color: #094771;
        }
        QMenu::separator {
            height: 1px;
            background: #3c3c3c;
            margin: 4px 8px;
        }
        QStatusBar {
            background-color: #007acc;
            color: #ffffff;
            border-top: 1px solid #005a9e;
        }
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 3px;
            padding: 4px 8px;
            color: #d4d4d4;
        }
        QToolButton:hover {
            background-color: #3c3c3c;
            border: 1px solid #505050;
        }
        QToolButton:pressed {
            background-color: #094771;
            border: 1px solid #007acc;
        }
        QSplitter::handle {
            background-color: #3c3c3c;
            width: 2px;
        }
        QSplitter::handle:hover {
            background-color: #007acc;
        }
        QTabWidget::pane {
            border: 1px solid #3c3c3c;
            background: #1e1e1e;
        }
        QTabBar::tab {
            background: #2d2d2d;
            border: 1px solid #3c3c3c;
            border-bottom: none;
            padding: 6px 16px;
            color: #969696;
        }
        QTabBar::tab:selected {
            background: #1e1e1e;
            color: #ffffff;
            border-bottom: 2px solid #007acc;
        }
        QTabBar::tab:hover:!selected {
            background: #3c3c3c;
            color: #d4d4d4;
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 16px;
            color: #d4d4d4;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #3c3c3c;
            border: 1px solid #505050;
            border-radius: 3px;
            padding: 4px 8px;
            color: #d4d4d4;
            selection-background-color: #264f78;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid #007acc;
        }
        QPushButton {
            background-color: #0e639c;
            color: #ffffff;
            border: none;
            border-radius: 3px;
            padding: 6px 16px;
            min-width: 70px;
        }
        QPushButton:hover {
            background-color: #1177bb;
        }
        QPushButton:pressed {
            background-color: #094771;
        }
        QPushButton:disabled {
            background-color: #3c3c3c;
            color: #6c6c6c;
        }
        QHeaderView::section {
            background-color: #2d2d2d;
            color: #d4d4d4;
            border: 1px solid #3c3c3c;
            padding: 4px 8px;
            font-weight: bold;
        }
        QScrollBar:vertical {
            background: #1e1e1e;
            width: 12px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #424242;
            min-height: 30px;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical:hover {
            background: #555555;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: #1e1e1e;
            height: 12px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #424242;
            min-width: 30px;
            border-radius: 6px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #555555;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
        QTreeWidget, QListWidget {
            background-color: #1e1e1e;
            color: #d4d4d4;
            border: none;
        }
        QTreeWidget::item, QListWidget::item {
            padding: 3px;
        }
        QTreeWidget::item:selected, QListWidget::item:selected {
            background-color: #264f78;
        }
        QCheckBox { color: #d4d4d4; }
        QLabel { color: #d4d4d4; }
        QSlider::groove:horizontal {
            height: 4px;
            background: #3c3c3c;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #007acc;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #1c97ea;
        }
    )");
}
