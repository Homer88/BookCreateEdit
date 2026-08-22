#include "MainWindow.h"
#include "RibbonWidget.h"
#include "LeftPanel.h"
#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "PublishDialog.h"
#include "RestoreDialog.h"
#include "EditorTabs.h"
#include "BookDocument.h"
#include "ImportDialog.h"
#include "PluginManager.h"
#include "ThemeManager.h"

#include <QSplitter>
#include <QTextEdit>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QApplication>
#include <QMessageBox>
#include <QToolButton>
#include <QAction>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QDebug>
#include <QInputDialog>
#include <QFileInfo>

static QIcon icon(const QString &name)
{
    return QIcon(":/icons/" + name + ".svg");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_baseFont = QFont("Segoe UI", 10);
    QApplication::setFont(m_baseFont);

    m_document = new BookDocument(this);

    setupUI();
    setupMenuBar();
    setupStatusBar();
    applySettings();
    setWindowTitle("BookCreateEdit");
    resize(1200, 800);

    PluginManager::instance().setContext(this);
    PluginManager::instance().loadAllPlugins();

    m_editor->setDocument(m_document);
    m_leftPanel->setDocument(m_document);

    connect(m_leftPanel, &LeftPanel::pageSelected, m_editor, &EditorTabs::setCurrentPage);

    ThemeManager::instance().setTheme(ThemeManager::Light);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    m_ribbon = new RibbonWidget(this);

    RibbonTab *tabFile = m_ribbon->addFileTab("Файл");
    QToolButton *btnNew = m_ribbon->createButton("Новый", icon("new"), "Создать новый проект");
    QToolButton *btnOpen = m_ribbon->createButton("Открыть", icon("open"), "Открыть проект");
    QToolButton *btnSave = m_ribbon->createButton("Сохранить", icon("save"), "Сохранить проект");
    QToolButton *btnSaveAs = m_ribbon->createButton("Сохранить как", icon("saveas"), "Сохранить проект как");
    QToolButton *btnPublish = m_ribbon->createButton("Опубликовать", icon("publish"), "Опубликовать книгу");
    QToolButton *btnRestore = m_ribbon->createButton("Восстановить", icon("restore"), "Восстановить из автосохранения");
    QToolButton *btnClose = m_ribbon->createButton("Закрыть", icon("close"), "Закрыть проект");
    QToolButton *btnImport = m_ribbon->createButton("Импорт", icon("import"), "Импорт файла в проект");
    QToolButton *btnSettings = m_ribbon->createButton("Настройки", icon("settings"), "Настройки программы");
    tabFile->addGroup("Проект", {btnNew, btnOpen, btnSave, btnSaveAs});
    tabFile->addGroup("Публикация", {btnPublish, btnRestore, btnImport});
    tabFile->addGroup("Управление", {btnClose, btnSettings});

    connect(btnNew, &QToolButton::clicked, this, [this]() {
        if (m_document->isModified()) {
            auto btn = QMessageBox::question(this, "Новый проект",
                "Сохранить изменения перед созданием нового?",
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            if (btn == QMessageBox::Cancel) return;
            if (btn == QMessageBox::Yes && !m_document->filePath().isEmpty())
                m_document->save();
        }
        m_document->newDocument();
        m_leftPanel->refreshContents();
        setWindowTitle("BookCreateEdit — Новый проект");
        statusBar()->showMessage("Создан новый проект");
    });
    connect(btnOpen, &QToolButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Открыть проект", QString(),
            "Проекты BookCreateEdit (*.bce);;Все файлы (*)");
        if (!filePath.isEmpty()) {
            if (m_document->load(filePath)) {
                m_leftPanel->refreshContents();
                setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                statusBar()->showMessage("Проект открыт: " + filePath);
            } else {
                QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл:\n" + filePath);
            }
        }
    });
    connect(btnSave, &QToolButton::clicked, this, [this]() {
        if (m_document->filePath().isEmpty()) {
            QString filePath = QFileDialog::getSaveFileName(this, "Сохранить проект", QString(),
                "Проекты BookCreateEdit (*.bce)");
            if (!filePath.isEmpty()) {
                if (m_document->saveAs(filePath)) {
                    setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                    statusBar()->showMessage("Проект сохранён");
                }
            }
        } else {
            if (m_document->save())
                statusBar()->showMessage("Проект сохранён");
        }
    });
    connect(btnSaveAs, &QToolButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this, "Сохранить как", QString(),
            "Проекты BookCreateEdit (*.bce)");
        if (!filePath.isEmpty()) {
            if (m_document->saveAs(filePath)) {
                setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                statusBar()->showMessage("Проект сохранён как: " + filePath);
            }
        }
    });
    connect(btnPublish, &QToolButton::clicked, this, [this]() {
        PublishDialog dlg(m_document, this);
        dlg.exec();
    });
    connect(btnRestore, &QToolButton::clicked, this, [this]() {
        RestoreDialog dlg(this);
        dlg.exec();
    });
    connect(btnImport, &QToolButton::clicked, this, [this]() {
        ImportDialog dlg(m_document, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_leftPanel->refreshContents();
        }
    });
    connect(btnClose, &QToolButton::clicked, this, [this]() {
        QMessageBox::StandardButton btn = QMessageBox::question(this,
            "Закрыть проект", "Сохранить изменения перед закрытием?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (btn == QMessageBox::Yes) {
            if (!m_document->filePath().isEmpty())
                m_document->save();
            m_document->newDocument();
            m_leftPanel->refreshContents();
            setWindowTitle("BookCreateEdit");
            statusBar()->showMessage("Проект закрыт");
        } else if (btn == QMessageBox::No) {
            m_document->newDocument();
            m_leftPanel->refreshContents();
            setWindowTitle("BookCreateEdit");
            statusBar()->showMessage("Проект закрыт без сохранения");
        }
    });
    connect(btnSettings, &QToolButton::clicked, this, [this]() {
        SettingsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            applySettings();
        }
    });

    RibbonTab *tabEdit = m_ribbon->addTab("Редактирование");
    QToolButton *btnUndo = m_ribbon->createButton("Отменить", icon("undo"), "Отменить действие (Ctrl+Z)");
    QToolButton *btnRedo = m_ribbon->createButton("Повторить", icon("redo"), "Повторить действие (Ctrl+Y)");
    QToolButton *btnCut = m_ribbon->createButton("Вырезать", icon("cut"), "Вырезать (Ctrl+X)");
    QToolButton *btnCopy = m_ribbon->createButton("Копировать", icon("copy"), "Копировать (Ctrl+C)");
    QToolButton *btnPaste = m_ribbon->createButton("Вставить", icon("paste"), "Вставить (Ctrl+V)");
    QToolButton *btnFind = m_ribbon->createButton("Найти", icon("find"), "Найти текст (Ctrl+F)");
    tabEdit->addGroup("Буфер обмена", {btnCut, btnCopy, btnPaste});
    tabEdit->addGroup("История", {btnUndo, btnRedo});
    tabEdit->addGroup("Поиск", {btnFind});

    connect(btnCut, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor())
            ed->cut();
    });
    connect(btnCopy, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor())
            ed->copy();
    });
    connect(btnPaste, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor())
            ed->paste();
    });
    connect(btnUndo, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor())
            ed->undo();
    });
    connect(btnRedo, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor())
            ed->redo();
    });

    RibbonTab *tabInsert = m_ribbon->addTab("Вставка");
    QToolButton *btnAddPage = m_ribbon->createButton("Страница", icon("page"), "Добавить страницу");
    QToolButton *btnAddImage = m_ribbon->createButton("Изображение", icon("image"), "Добавить изображение");
    QToolButton *btnAddTable = m_ribbon->createButton("Таблица", icon("table"), "Добавить таблицу");
    QToolButton *btnAddLink = m_ribbon->createButton("Ссылка", icon("link"), "Добавить ссылку");
    tabInsert->addGroup("Страницы", {btnAddPage});
    tabInsert->addGroup("Элементы", {btnAddImage, btnAddTable, btnAddLink});

    connect(btnAddPage, &QToolButton::clicked, this, [this]() {
        m_editor->addPage();
        m_leftPanel->refreshContents();
    });
    connect(btnAddImage, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            QString filePath = QFileDialog::getOpenFileName(this, "Вставить изображение", QString(),
                "Изображения (*.png *.jpg *.jpeg *.gif *.bmp *.svg);;Все файлы (*)");
            if (!filePath.isEmpty()) {
                QString html = QString("<img src=\"%1\" alt=\"image\" />").arg(filePath);
                ed->textCursor().insertHtml(html);
            }
        }
    });
    connect(btnAddTable, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            QString html = "<table border=\"1\" cellpadding=\"4\" cellspacing=\"0\">"
                           "<tr><td>Ячейка 1</td><td>Ячейка 2</td></tr>"
                           "<tr><td>Ячейка 3</td><td>Ячейка 4</td></tr>"
                           "</table><br>";
            ed->textCursor().insertHtml(html);
        }
    });
    connect(btnAddLink, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            bool ok;
            QString url = QInputDialog::getText(this, "Вставить ссылку",
                "URL:", QLineEdit::Normal, "https://", &ok);
            if (ok && !url.isEmpty()) {
                QString html = QString("<a href=\"%1\">%1</a>").arg(url);
                ed->textCursor().insertHtml(html);
            }
        }
    });

    RibbonTab *tabFormat = m_ribbon->addTab("Формат");
    QToolButton *btnBold = m_ribbon->createButton("Жирный", icon("bold"), "Жирный текст (Ctrl+B)");
    QToolButton *btnItalic = m_ribbon->createButton("Курсив", icon("italic"), "Курсивный текст (Ctrl+I)");
    QToolButton *btnUnderline = m_ribbon->createButton("Подчёркнутый", icon("underline"), "Подчёркнутый текст (Ctrl+U)");
    QToolButton *btnAlignLeft = m_ribbon->createButton("По левому", icon("alignleft"), "Выравнивание по левому краю");
    QToolButton *btnAlignCenter = m_ribbon->createButton("По центру", icon("aligncenter"), "Выравнивание по центру");
    QToolButton *btnAlignRight = m_ribbon->createButton("По правому", icon("alignright"), "Выравнивание по правому краю");
    tabFormat->addGroup("Шрифт", {btnBold, btnItalic, btnUnderline});
    tabFormat->addGroup("Выравнивание", {btnAlignLeft, btnAlignCenter, btnAlignRight});

    connect(btnBold, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            QTextCharFormat fmt;
            fmt.setFontWeight(ed->fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
            ed->mergeCurrentCharFormat(fmt);
        }
    });
    connect(btnItalic, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            QTextCharFormat fmt;
            fmt.setFontItalic(!ed->fontItalic());
            ed->mergeCurrentCharFormat(fmt);
        }
    });
    connect(btnUnderline, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            QTextCharFormat fmt;
            fmt.setFontUnderline(!ed->fontUnderline());
            ed->mergeCurrentCharFormat(fmt);
        }
    });
    connect(btnAlignLeft, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor())
            ed->setAlignment(Qt::AlignLeft);
    });
    connect(btnAlignCenter, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor())
            ed->setAlignment(Qt::AlignHCenter);
    });
    connect(btnAlignRight, &QToolButton::clicked, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor())
            ed->setAlignment(Qt::AlignRight);
    });

    RibbonTab *tabView = m_ribbon->addTab("Вид");
    QToolButton *btnContents = m_ribbon->createButton("Оглавление", icon("contents"), "Показать/скрыть оглавление");
    QToolButton *btnFiles = m_ribbon->createButton("Файлы", icon("files"), "Показать/скрыть список файлов");
    QToolButton *btnPreview = m_ribbon->createButton("Предпросмотр", icon("preview"), "Предпросмотр книги");
    tabView->addGroup("Панели", {btnContents, btnFiles});
    tabView->addGroup("Просмотр", {btnPreview});

    connect(btnContents, &QToolButton::clicked, this, [this]() {
        m_leftPanel->setVisible(!m_leftPanel->isVisible());
    });

    m_leftPanel = new LeftPanel(this);
    m_leftPanel->setMinimumWidth(220);
    m_leftPanel->setMaximumWidth(400);

    m_editor = new EditorTabs(this);
    m_editor->setMinimumWidth(400);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_leftPanel);
    m_splitter->addWidget(m_editor);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({280, 920});

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_ribbon);
    mainLayout->addWidget(m_splitter, 1);

    setCentralWidget(centralWidget);
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&Файл");
    QAction *actNew = fileMenu->addAction(icon("new"), "Новый проект");
    QAction *actOpen = fileMenu->addAction(icon("open"), "Открыть проект");
    QMenu *recentMenu = fileMenu->addMenu("Открыть недавний");
    recentMenu->addAction("Проект 1");
    recentMenu->addAction("Проект 2");
    fileMenu->addSeparator();
    QAction *actImportProject = fileMenu->addAction(icon("import"), "Импорт проекта...");
    QAction *actImportFile = fileMenu->addAction(icon("import"), "Импорт файла...");
    fileMenu->addSeparator();
    QAction *actSave = fileMenu->addAction(icon("save"), "Сохранить");
    QAction *actSaveAs = fileMenu->addAction(icon("saveas"), "Сохранить как...");
    fileMenu->addSeparator();
    QAction *actCloseProject = fileMenu->addAction(icon("close"), "Закрыть проект");
    fileMenu->addSeparator();
    QAction *actPublish = fileMenu->addAction(icon("publish"), "Опубликовать...");
    QAction *actRestore = fileMenu->addAction(icon("restore"), "Восстановить...");
    fileMenu->addSeparator();
    QAction *actExit = fileMenu->addAction("Выход");

    actNew->setShortcut(QKeySequence::New);
    actOpen->setShortcut(QKeySequence::Open);
    actSave->setShortcut(QKeySequence::Save);
    actSaveAs->setShortcut(QKeySequence::SaveAs);
    actExit->setShortcut(QKeySequence::Quit);

    connect(actNew, &QAction::triggered, this, [this]() {
        if (m_document->isModified()) {
            auto btn = QMessageBox::question(this, "Новый проект",
                "Сохранить изменения перед созданием нового?",
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            if (btn == QMessageBox::Cancel) return;
            if (btn == QMessageBox::Yes && !m_document->filePath().isEmpty())
                m_document->save();
        }
        m_document->newDocument();
        m_leftPanel->refreshContents();
        setWindowTitle("BookCreateEdit — Новый проект");
        statusBar()->showMessage("Создан новый проект");
    });
    connect(actOpen, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Открыть проект", QString(),
            "Проекты BookCreateEdit (*.bce);;Все файлы (*)");
        if (!filePath.isEmpty()) {
            if (m_document->load(filePath)) {
                m_leftPanel->refreshContents();
                setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                statusBar()->showMessage("Проект открыт: " + filePath);
            } else {
                QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл:\n" + filePath);
            }
        }
    });
    connect(actSave, &QAction::triggered, this, [this]() {
        if (m_document->filePath().isEmpty()) {
            QString filePath = QFileDialog::getSaveFileName(this, "Сохранить проект", QString(),
                "Проекты BookCreateEdit (*.bce)");
            if (!filePath.isEmpty()) {
                if (m_document->saveAs(filePath)) {
                    setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                    statusBar()->showMessage("Проект сохранён");
                }
            }
        } else {
            if (m_document->save())
                statusBar()->showMessage("Проект сохранён");
        }
    });
    connect(actSaveAs, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this, "Сохранить как", QString(),
            "Проекты BookCreateEdit (*.bce)");
        if (!filePath.isEmpty()) {
            if (m_document->saveAs(filePath)) {
                setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                statusBar()->showMessage("Проект сохранён как: " + filePath);
            }
        }
    });
    connect(actImportFile, &QAction::triggered, this, [this]() {
        ImportDialog dlg(m_document, this);
        if (dlg.exec() == QDialog::Accepted)
            m_leftPanel->refreshContents();
    });
    connect(actImportProject, &QAction::triggered, this, [this]() {
        QFileDialog::getOpenFileName(this, "Импорт проекта", QString(),
            "Файлы проектов (*.bceproj);;Все файлы (*)");
    });
    connect(actPublish, &QAction::triggered, this, [this]() {
        PublishDialog dlg(m_document, this);
        dlg.exec();
    });
    connect(actRestore, &QAction::triggered, this, [this]() {
        RestoreDialog dlg(this);
        dlg.exec();
    });
    connect(actCloseProject, &QAction::triggered, this, [this]() {
        QMessageBox::StandardButton btn = QMessageBox::question(this,
            "Закрыть проект", "Сохранить изменения перед закрытием?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (btn == QMessageBox::Yes) {
            if (!m_document->filePath().isEmpty()) m_document->save();
            m_document->newDocument();
            m_leftPanel->refreshContents();
            setWindowTitle("BookCreateEdit");
        } else if (btn == QMessageBox::No) {
            m_document->newDocument();
            m_leftPanel->refreshContents();
            setWindowTitle("BookCreateEdit");
        }
    });
    connect(actExit, &QAction::triggered, qApp, &QApplication::quit);

    QMenu *editMenu = menuBar()->addMenu("&Редактирование");
    QAction *actUndo = editMenu->addAction(icon("undo"), "Отменить");
    QAction *actRedo = editMenu->addAction(icon("redo"), "Повторить");
    editMenu->addSeparator();
    QAction *actCut = editMenu->addAction(icon("cut"), "Вырезать");
    QAction *actCopy = editMenu->addAction(icon("copy"), "Копировать");
    QAction *actPaste = editMenu->addAction(icon("paste"), "Вставить");
    editMenu->addSeparator();
    QAction *actFind = editMenu->addAction(icon("find"), "Найти и заменить");
    QAction *actSelectAll = editMenu->addAction("Выделить всё");

    actUndo->setShortcut(QKeySequence::Undo);
    actRedo->setShortcut(QKeySequence::Redo);
    actCut->setShortcut(QKeySequence::Cut);
    actCopy->setShortcut(QKeySequence::Copy);
    actPaste->setShortcut(QKeySequence::Paste);
    actFind->setShortcut(QKeySequence::Find);
    actSelectAll->setShortcut(QKeySequence::SelectAll);

    connect(actUndo, &QAction::triggered, this, [this]() { if (auto *ed = m_editor->wysiwygEditor()) ed->undo(); });
    connect(actRedo, &QAction::triggered, this, [this]() { if (auto *ed = m_editor->wysiwygEditor()) ed->redo(); });
    connect(actCut, &QAction::triggered, this, [this]() { if (auto *ed = m_editor->wysiwygEditor()) ed->cut(); });
    connect(actCopy, &QAction::triggered, this, [this]() { if (auto *ed = m_editor->wysiwygEditor()) ed->copy(); });
    connect(actPaste, &QAction::triggered, this, [this]() { if (auto *ed = m_editor->wysiwygEditor()) ed->paste(); });
    connect(actSelectAll, &QAction::triggered, this, [this]() { if (auto *ed = m_editor->wysiwygEditor()) ed->selectAll(); });

    QMenu *insertMenu = menuBar()->addMenu("&Вставка");
    QAction *actAddPage = insertMenu->addAction(icon("page"), "Страница");
    QAction *actAddImage = insertMenu->addAction(icon("image"), "Изображение...");
    QAction *actAddTable = insertMenu->addAction(icon("table"), "Таблица");
    QAction *actAddLink = insertMenu->addAction(icon("link"), "Ссылка...");

    connect(actAddPage, &QAction::triggered, this, [this]() { m_editor->addPage(); m_leftPanel->refreshContents(); });
    connect(actAddImage, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Вставить изображение", QString(),
            "Изображения (*.png *.jpg *.jpeg *.gif *.bmp *.svg);;Все файлы (*)");
        if (!filePath.isEmpty()) {
            if (auto *ed = m_editor->wysiwygEditor()) {
                ed->textCursor().insertHtml(QString("<img src=\"%1\" />").arg(filePath));
            }
        }
    });

    QMenu *formatMenu = menuBar()->addMenu("Фо&рмат");
    QAction *actBold = formatMenu->addAction(icon("bold"), "Жирный");
    QAction *actItalic = formatMenu->addAction(icon("italic"), "Курсив");
    QAction *actUnderline = formatMenu->addAction(icon("underline"), "Подчёркнутый");

    actBold->setShortcut(QKeySequence::Bold);
    actItalic->setShortcut(QKeySequence::Italic);
    actUnderline->setShortcut(QKeySequence::Underline);

    connect(actBold, &QAction::triggered, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            QTextCharFormat fmt;
            fmt.setFontWeight(ed->fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
            ed->mergeCurrentCharFormat(fmt);
        }
    });
    connect(actItalic, &QAction::triggered, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            QTextCharFormat fmt;
            fmt.setFontItalic(!ed->fontItalic());
            ed->mergeCurrentCharFormat(fmt);
        }
    });
    connect(actUnderline, &QAction::triggered, this, [this]() {
        if (auto *ed = m_editor->wysiwygEditor()) {
            QTextCharFormat fmt;
            fmt.setFontUnderline(!ed->fontUnderline());
            ed->mergeCurrentCharFormat(fmt);
        }
    });

    QMenu *settingsMenu = menuBar()->addMenu("&Настройки");
    QAction *actProjectSettings = settingsMenu->addAction("Настройки проекта...");
    QAction *actProgramSettings = settingsMenu->addAction(icon("settings"), "Настройки программы...");
    settingsMenu->addSeparator();
    QAction *actImageEditor = settingsMenu->addAction("Настройки редактора изображений...");

    connect(actProjectSettings, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "Настройки проекта", "Настройки проекта");
    });
    connect(actProgramSettings, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            applySettings();
        }
    });
    connect(actImageEditor, &QAction::triggered, this, [this]() {
        ImageEditorSettingsDialog dlg(this);
        dlg.exec();
    });

    QMenu *helpMenu = menuBar()->addMenu("&Справка");
    helpMenu->addAction("Информация", this, [this]() {
        QMessageBox::information(this, "Информация",
            "BookCreateEdit v1.0.0\n\n"
            "Программа для создания электронных книг.\n"
            "Поддерживаемые форматы: EPUB, MOBI, PDF, FB2, DOC, RTF, HTML, Markdown\n\n"
            "C++ / Qt 5.14");
    });
    helpMenu->addAction("О программе", this, [this]() {
        QMessageBox::about(this, "О программе",
            "<h2>BookCreateEdit</h2>"
            "<p>Версия 1.0.0</p>"
            "<p>Программа для создания и редактирования электронных книг.</p>"
            "<p>Поддержка форматов: EPUB, MOBI, PDF, FB2, FB3, DOC, RTF, HTML, Markdown, CHM, HLP, EXE</p>");
    });
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Готово");
}

void MainWindow::applyScale(int percent)
{
    qreal k = percent / 100.0;
    QFont scaledFont = m_baseFont;
    scaledFont.setPointSize(qMax(6, qRound(m_baseFont.pointSize() * k)));
    QApplication::setFont(scaledFont);

    int sidebarW = qRound(280 * k);
    m_leftPanel->setMinimumWidth(qRound(180 * k));
    m_leftPanel->setMaximumWidth(qRound(600 * k));
    m_splitter->setSizes({sidebarW, width() - sidebarW});
}

void MainWindow::applySettings()
{
    SettingsManager &s = SettingsManager::instance();

    int scale = s.autoScale() ? 100 : s.scaleFactor();
    applyScale(scale);

    int sidebarW = s.sidebarWidth();
    m_leftPanel->setMinimumWidth(180);
    m_leftPanel->setMaximumWidth(600);
    m_splitter->setSizes({sidebarW, width() - sidebarW});

    ThemeManager::instance().setTheme(ThemeManager::Light);
}

void MainWindow::applyStyle()
{
}

BookDocument *MainWindow::currentDocument() const
{
    return m_document;
}

void MainWindow::log(const QString &message)
{
    qDebug() << "[BookCreateEdit]" << message;
    if (statusBar())
        statusBar()->showMessage(message, 5000);
}
