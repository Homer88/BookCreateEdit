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
#include "ProjectManager.h"
#include "NewProjectDialog.h"
#include "interfaces/IBookFormat.h"

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
#include <QDir>

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
    m_project = new ProjectManager(this);

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

    // Вставка страниц из контекстного меню дерева оглавления:
    // сначала сбрасываем правки активного редактора (индексы ещё не сдвинуты),
    // затем меняем документ и перестраиваем вкладки редактора
    connect(m_leftPanel, &LeftPanel::pageInsertRequested, this,
            [this](int pageIndex, int mode) {
        if (!m_document)
            return;
        m_editor->flushCurrentPage();

        int at = -1;
        QString parentId;
        switch (mode) {
        case 0:
            // До выбранной страницы (перед всем её поддеревом)
            at = pageIndex;
            break;
        case 1:
            // После выбранной — после последнего потомка её поддерева
            at = m_document->lastDescendantIndex(pageIndex) + 1;
            break;
        case 2:
            // Дочерняя страница — в конец поддерева родителя
            parentId = m_document->pageId(pageIndex);
            at = m_document->lastDescendantIndex(pageIndex) + 1;
            break;
        default:
            at = -1;                            // в конец книги
            break;
        }

        const int newIdx = m_document->insertPage(at, parentId);
        m_editor->reloadFromDocument(newIdx);
        m_leftPanel->refreshContents(newIdx);
    });

    // Удаление страницы из дерева: правки сохраняем до удаления,
    // затем пересобираем редактор и выделяем соседнюю страницу
    connect(m_leftPanel, &LeftPanel::pageDeleteRequested, this,
            [this](int pageIndex) {
        if (!m_document || m_document->pageCount() <= 1)
            return;
        m_editor->flushCurrentPage();
        m_document->removePage(pageIndex);
        const int sel = qMin(pageIndex, m_document->pageCount() - 1);
        m_editor->reloadFromDocument(sel);
        m_leftPanel->refreshContents(sel);
    });

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
        createNewProject();
    });
    connect(btnOpen, &QToolButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Открыть проект", QString(),
            "Проекты BookCreateEdit (*.bceproj);;" + buildOpenFilter());
        if (filePath.isEmpty()) return;

        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == "bceproj") {
            // Полноценный проект
            if (!openProject(filePath))
                QMessageBox::warning(this, "Ошибка",
                    "Не удалось открыть проект:\n" + filePath);
            return;
        }

        if (ext == "bce") {
            // Одиночный документ
            m_editor->flushCurrentPage();
            if (m_document->load(filePath)) {
                reloadEditor(0);
                setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                updateFormatIndicator();
            } else {
                QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл:\n" + filePath);
            }
            return;
        }

        // Любой другой формат — импортируем через плагин
        IBookFormat *fmt = detectFormatByExtension(filePath);
        if (fmt && fmt->canRead()) {
            m_editor->flushCurrentPage();
            m_document->newDocument();
            if (fmt->read(filePath, m_document)) {
                QString bcePath = QFileInfo(filePath).absolutePath() + "/"
                    + QFileInfo(filePath).completeBaseName() + ".bce";
                m_document->saveAs(bcePath);
                reloadEditor(0);
                setWindowTitle("BookCreateEdit — " + QFileInfo(bcePath).fileName());
                updateFormatIndicator();
                statusBar()->showMessage(
                    QString("Импортировано из %1: %2 → %3")
                        .arg(fmt->formatName(), QFileInfo(filePath).fileName(),
                             QFileInfo(bcePath).fileName()));
            } else {
                // Документ уже заменён пустым — приводим редактор в соответствие
                reloadEditor(0);
                QMessageBox::warning(this, "Ошибка",
                    QString("Не удалось импортировать файл %1 через плагин %2.")
                        .arg(filePath, fmt->formatName()));
            }
        } else {
            QMessageBox::warning(this, "Ошибка",
                QString("Формат \".%1\" не поддерживается для чтения.").arg(ext));
        }
    });
    connect(btnSave, &QToolButton::clicked, this, [this]() {
        saveProject();
        updateFormatIndicator();
    });
    connect(btnSaveAs, &QToolButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this, "Сохранить как", QString(),
            buildSaveFilter());
        if (!filePath.isEmpty()) {
            QString ext = QFileInfo(filePath).suffix().toLower();

            if (ext == "bce" || ext.isEmpty()) {
                // Сохранение в нативном BCE формате
                if (!filePath.endsWith(".bce")) filePath += ".bce";
                if (m_document->saveAs(filePath)) {
                    setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                    updateFormatIndicator();
                    statusBar()->showMessage("Проект сохранён как: " + filePath);
                }
            } else {
                // Экспорт в другой формат через плагин
                IBookFormat *fmt = detectFormatByExtension(filePath);
                if (fmt && fmt->canWrite()) {
                    if (fmt->write(m_document, filePath)) {
                        // Сохраняем .bce копию рядом
                        QString bcePath = QFileInfo(filePath).absolutePath() + "/"
                            + QFileInfo(filePath).completeBaseName() + ".bce";
                        m_document->saveAs(bcePath);
                        setWindowTitle("BookCreateEdit — " + QFileInfo(bcePath).fileName());
                        updateFormatIndicator();
                        statusBar()->showMessage(
                            QString("Экспортировано в %1: %2").arg(fmt->formatName(), filePath));
                    } else {
                        QMessageBox::warning(this, "Ошибка",
                            "Не удалось экспортировать в " + fmt->formatName());
                    }
                } else {
                    QMessageBox::warning(this, "Ошибка",
                        QString("Формат \".%1\" не поддерживается для записи.").arg(ext));
                }
            }
        }
    });
    connect(btnPublish, &QToolButton::clicked, this, [this]() {
        PublishDialog dlg(m_document, m_project, this);
        dlg.exec();
    });
    connect(btnRestore, &QToolButton::clicked, this, [this]() {
        RestoreDialog dlg(this);
        dlg.exec();
    });
    connect(btnImport, &QToolButton::clicked, this, [this]() {
        ImportDialog dlg(m_document, this);
        dlg.setProjectDir(hasProject() ? m_project->projectDir() : QString());
        if (dlg.exec() == QDialog::Accepted) {
            // Импорт добавил страницы в документ — пересобираем редактор
            const int cur = qBound(0, m_editor->currentPage(),
                                   m_document->pageCount() - 1);
            reloadEditor(cur);
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
            reloadEditor(0);
            setWindowTitle("BookCreateEdit");
            statusBar()->showMessage("Проект закрыт");
        } else if (btn == QMessageBox::No) {
            m_document->newDocument();
            reloadEditor(0);
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
        createNewProject();
    });
    connect(actOpen, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Открыть проект", QString(),
            "Проекты BookCreateEdit (*.bceproj);;" + buildOpenFilter());
        if (filePath.isEmpty()) return;

        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == "bceproj") {
            if (!openProject(filePath))
                QMessageBox::warning(this, "Ошибка",
                    "Не удалось открыть проект:\n" + filePath);
            return;
        }
        if (ext == "bce") {
            if (!openProject(filePath))
                QMessageBox::warning(this, "Ошибка",
                    "Не удалось открыть файл:\n" + filePath);
            return;
        }

        IBookFormat *fmt = detectFormatByExtension(filePath);
        if (fmt && fmt->canRead()) {
            m_editor->flushCurrentPage();
            m_document->newDocument();
            if (fmt->read(filePath, m_document)) {
                QString bcePath = QFileInfo(filePath).absolutePath() + "/"
                    + QFileInfo(filePath).completeBaseName() + ".bce";
                m_document->saveAs(bcePath);
                reloadEditor(0);
                setWindowTitle("BookCreateEdit — " + QFileInfo(bcePath).fileName());
                updateFormatIndicator();
                statusBar()->showMessage(
                    QString("Импортировано из %1: %2")
                        .arg(fmt->formatName(), QFileInfo(filePath).fileName()));
            } else {
                // Документ уже заменён пустым — приводим редактор в соответствие
                reloadEditor(0);
                QMessageBox::warning(this, "Ошибка",
                    QString("Не удалось импортировать файл %1 через плагин %2.")
                        .arg(filePath, fmt->formatName()));
            }
        } else {
            QMessageBox::warning(this, "Ошибка",
                QString("Формат \".%1\" не поддерживается для чтения.").arg(ext));
        }
    });
    connect(actSave, &QAction::triggered, this, [this]() {
        saveProject();
        updateFormatIndicator();
    });
    connect(actSaveAs, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this, "Сохранить как", QString(),
            buildSaveFilter());
        if (!filePath.isEmpty()) {
            QString ext = QFileInfo(filePath).suffix().toLower();
            if (ext == "bce" || ext.isEmpty()) {
                if (!filePath.endsWith(".bce")) filePath += ".bce";
                if (m_document->saveAs(filePath)) {
                    setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
                    updateFormatIndicator();
                    statusBar()->showMessage("Проект сохранён как: " + filePath);
                }
            } else {
                IBookFormat *fmt = detectFormatByExtension(filePath);
                if (fmt && fmt->canWrite()) {
                    if (fmt->write(m_document, filePath)) {
                        QString bcePath = QFileInfo(filePath).absolutePath() + "/"
                            + QFileInfo(filePath).completeBaseName() + ".bce";
                        m_document->saveAs(bcePath);
                        setWindowTitle("BookCreateEdit — " + QFileInfo(bcePath).fileName());
                        updateFormatIndicator();
                        statusBar()->showMessage(
                            QString("Экспортировано в %1: %2").arg(fmt->formatName(), filePath));
                    }
                }
            }
        }
    });
    connect(actImportFile, &QAction::triggered, this, [this]() {
        ImportDialog dlg(m_document, this);
        dlg.setProjectDir(hasProject() ? m_project->projectDir() : QString());
        if (dlg.exec() == QDialog::Accepted) {
            // Импорт добавил страницы в документ — пересобираем редактор
            const int cur = qBound(0, m_editor->currentPage(),
                                   m_document->pageCount() - 1);
            reloadEditor(cur);
        }
    });
    connect(actImportProject, &QAction::triggered, this, [this]() {
        QString projPath = QFileDialog::getOpenFileName(this, "Импорт проекта", QString(),
            "Файлы проектов (*.bceproj);;Все файлы (*)");
        if (projPath.isEmpty()) return;
        if (!openProject(projPath))
            QMessageBox::warning(this, "Ошибка",
                "Не удалось открыть проект:\n" + projPath);
    });
    connect(actPublish, &QAction::triggered, this, [this]() {
        PublishDialog dlg(m_document, m_project, this);
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
            reloadEditor(0);
            setWindowTitle("BookCreateEdit");
        } else if (btn == QMessageBox::No) {
            m_document->newDocument();
            reloadEditor(0);
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

    if (m_editor)
        m_editor->applyScale(percent);
}

void MainWindow::applySettings()
{
    SettingsManager &s = SettingsManager::instance();

    int scale = s.autoScale() ? 100 : s.scaleFactor();
    applyScale(scale);

    int sidebarW = s.sidebarWidth();
    qreal k = scale / 100.0;
    m_leftPanel->setMinimumWidth(qRound(180 * k));
    m_leftPanel->setMaximumWidth(qRound(600 * k));
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

// --- Динамическая смена расширения ---

QString MainWindow::buildOpenFilter() const
{
    // Группируем расширения по плагинам для красивого фильтра
    QStringList filters;
    filters << "Проекты BookCreateEdit (*.bce)";

    QStringList formatNames = PluginManager::instance().availableFormats();
    for (const QString &name : formatNames) {
        IBookFormat *fmt = PluginManager::instance().format(name);
        if (fmt && fmt->canRead()) {
            QStringList exts = fmt->supportedExtensions();
            QString pattern;
            for (const QString &ext : exts) {
                if (!pattern.isEmpty()) pattern += " ";
                pattern += "*." + ext;
            }
            filters << QString("%1 (%2)").arg(fmt->formatName(), pattern);
        }
    }

    filters << "Все файлы (*)";
    return filters.join(";;");
}

QString MainWindow::buildSaveFilter() const
{
    QStringList filters;
    filters << "Проекты BookCreateEdit (*.bce)";

    QStringList formatNames = PluginManager::instance().availableFormats();
    for (const QString &name : formatNames) {
        IBookFormat *fmt = PluginManager::instance().format(name);
        if (fmt && fmt->canWrite()) {
            QStringList exts = fmt->supportedExtensions();
            QString pattern;
            for (const QString &ext : exts) {
                if (!pattern.isEmpty()) pattern += " ";
                pattern += "*." + ext;
            }
            filters << QString("%1 (%2)").arg(fmt->formatName(), pattern);
        }
    }

    filters << "Все файлы (*)";
    return filters.join(";;");
}

IBookFormat *MainWindow::detectFormatByExtension(const QString &filePath) const
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext.isEmpty()) return nullptr;
    return PluginManager::instance().formatByExtension(ext);
}

void MainWindow::updateFormatIndicator()
{
    if (m_document->filePath().isEmpty()) {
        m_currentFormatId.clear();
        statusBar()->showMessage("Готово");
        return;
    }

    IBookFormat *fmt = detectFormatByExtension(m_document->filePath());
    if (fmt) {
        m_currentFormatId = fmt->formatName();
        statusBar()->showMessage(
            QString("Формат: %1 | %2")
                .arg(fmt->formatName(), m_document->filePath()));
    } else {
        m_currentFormatId.clear();
        statusBar()->showMessage(m_document->filePath());
    }
}

// --- Управление проектом (.bceproj) ---

void MainWindow::createNewProject()
{
    // 0. Предлагаем сохранить текущую работу
    if (m_document->isModified()) {
        auto btn = QMessageBox::question(this, "Новый проект",
            "Сохранить изменения перед созданием нового?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (btn == QMessageBox::Cancel)
            return;
        if (btn == QMessageBox::Yes && !m_document->filePath().isEmpty())
            m_document->save();
    }

    // 1. Диалог создания проекта
    NewProjectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString root = dlg.projectRoot();
    const QString name = dlg.projectName();
    if (name.isEmpty() || root.isEmpty())
        return;

    // 2. Структура каталогов проекта (dosc.md): history, img, content, map
    QDir().mkpath(root);
    for (const QString &sub : QStringList{"history", "img", "content", "map"})
        QDir(root).mkpath(sub);

    // 3. Новый .bceproj + пустой документ (пустое оглавление — 1 страница)
    m_project->newProject(name);
    m_project->setBookTitle(name);
    m_editor->flushCurrentPage();
    m_document->newDocument();
    // Редактор сразу приводим в соответствие новому документу
    reloadEditor(0);

    const QString bcePath = root + "/" + name + ".bce";
    if (!m_document->saveAs(bcePath)) {
        QMessageBox::warning(this, "Новый проект",
            "Не удалось создать файл книги:\n" + bcePath);
        return;
    }

    m_project->addDocument(QFileInfo(bcePath).fileName(), "BCE");
    const QString projPath = root + "/" + name + ".bceproj";
    if (!m_project->save(projPath)) {
        QMessageBox::warning(this, "Новый проект",
            "Не удалось создать файл проекта:\n" + projPath);
        return;
    }

    // 4. Импорт выбранных файлов: раскладка по img/ и content/
    int copied = 0;
    for (const auto &pair : dlg.importPlan()) {
        QFileInfo srcFi(pair.first);
        if (!srcFi.exists())
            continue;
        QDir destDir(root + "/" + pair.second);

        // Не перезаписываем существующие файлы
        QString destName = srcFi.fileName();
        int n = 2;
        while (destDir.exists(destName)) {
            destName = QString("%1_%2.%3")
                .arg(srcFi.completeBaseName()).arg(n++).arg(srcFi.suffix());
        }
        if (QFile::copy(pair.first, destDir.filePath(destName)))
            ++copied;
    }

    // 5. Обновление интерфейса
    m_leftPanel->refreshContents();
    setWindowTitle("BookCreateEdit — " + QFileInfo(projPath).fileName());
    updateFormatIndicator();
    statusBar()->showMessage(
        QString("Проект создан: %1 (импортировано файлов: %2)")
            .arg(root).arg(copied));
}

bool MainWindow::hasProject() const
{
    return m_project && !m_project->filePath().isEmpty();
}

/**
 * @brief Пересобирает вкладки редактора и дерево оглавления.
 * @param selectPage Индекс открываемой страницы (по умолчанию 0).
 */
void MainWindow::reloadEditor(int selectPage)
{
    m_editor->reloadFromDocument(selectPage);
    m_leftPanel->refreshContents(selectPage);
}

void MainWindow::newProject(const QString &projectPath)
{
    m_editor->flushCurrentPage();
    m_document->newDocument();
    reloadEditor(0);

    if (!projectPath.isEmpty()) {
        QString name = QFileInfo(projectPath).completeBaseName();
        m_project->newProject(name);
        if (m_project->save(projectPath)) {
            QString bceDir = QFileInfo(projectPath).absolutePath();
            QString bcePath = bceDir + "/" + name + ".bce";
            m_document->saveAs(bcePath);
            m_project->addDocument(QFileInfo(bcePath).fileName(), "BCE");
            m_project->save();
        }
    }

    m_leftPanel->refreshContents();
    setWindowTitle("BookCreateEdit — " +
        (projectPath.isEmpty() ? QString("Новый проект")
                               : QFileInfo(projectPath).fileName()));
}

bool MainWindow::openProject(const QString &filePath)
{
    // Правки текущего документа сохраняем до его замены
    m_editor->flushCurrentPage();

    QString ext = QFileInfo(filePath).suffix().toLower();

    // .bceproj — загружаем проект, затем его основной документ
    if (ext == "bceproj") {
        if (!m_project->load(filePath))
            return false;

        QString mainDoc = m_project->mainDocumentPath();
        if (!mainDoc.isEmpty()) {
            // Путь к документу — относительно каталога проекта
            QString absDoc = mainDoc;
            if (QFileInfo(mainDoc).isRelative())
                absDoc = m_project->projectDir() + "/" + mainDoc;

            if (QFileInfo::exists(absDoc)) {
                if (!m_document->load(absDoc))
                    return false;
                reloadEditor(0);
            }
        }

        m_leftPanel->refreshContents();
        setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
        updateFormatIndicator();
        return true;
    }

    // .bce — одиночный документ без проекта
    if (!m_document->load(filePath))
        return false;

    reloadEditor(0);
    setWindowTitle("BookCreateEdit — " + QFileInfo(filePath).fileName());
    updateFormatIndicator();
    return true;
}

void MainWindow::saveProject()
{
    // 1. Сохраняем документ (.bce)
    if (m_document->filePath().isEmpty()) {
        // Документ ещё не сохранён
        QString docPath;
        if (hasProject()) {
            docPath = m_project->projectDir() + "/"
                + m_project->projectName() + ".bce";
        } else {
            docPath = QFileDialog::getSaveFileName(this, "Сохранить документ",
                QString(), "Документ BookCreateEdit (*.bce)");
            if (docPath.isEmpty()) return;
            if (!docPath.endsWith(".bce")) docPath += ".bce";
        }
        m_document->saveAs(docPath);
    } else {
        m_document->save();
    }

    // 2. Сохраняем проект (.bceproj)
    if (hasProject()) {
        // Синхронизируем список документов
        QString relDoc = m_document->filePath();
        if (relDoc.startsWith(m_project->projectDir()))
            relDoc = QDir(m_project->projectDir()).relativeFilePath(relDoc);

        if (m_project->documentPaths().contains(relDoc))
            m_project->removeDocument(relDoc);
        m_project->addDocument(relDoc, "BCE");

        m_project->save();
        statusBar()->showMessage("Проект сохранён: " + m_project->filePath());
    }
}
