#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFont>
#include "interfaces/IBookPlugin.h"

class RibbonWidget;
class LeftPanel;
class QSplitter;
class EditorTabs;
class BookDocument;

class MainWindow : public QMainWindow, public BookContext
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    BookDocument *currentDocument() const override;
    void log(const QString &message) override;

public slots:
    void applySettings();

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void applyStyle();
    void applyScale(int percent);

    RibbonWidget *m_ribbon;
    LeftPanel *m_leftPanel;
    QSplitter *m_splitter;
    EditorTabs *m_editor;
    BookDocument *m_document;
    QFont m_baseFont;
};

#endif // MAINWINDOW_H
