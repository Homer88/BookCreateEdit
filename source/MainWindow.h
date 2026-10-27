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
class IBookFormat;
class ProjectManager;

/**
 * @brief Главное окно приложения BookCreateEdit.
 *
 * Реализует BookContext — передаётся плагинам для доступа к документу и логированию.
 *
 * Компоновка окна:
 *   ┌─────────────────────────────┐
 *   │        RibbonWidget         │  ← лента (вкладки: Файл, Вставка, Формат и т.д.)
 *   ├──────────┬──────────────────┤
 *   │ LeftPanel│   EditorTabs     │  ← дерево оглавления | редактор (WYSIWYG + XML)
 *   │          │                  │
 *   ├──────────┴──────────────────┤
 *   │       StatusBar             │  ← строка состояния
 *   └─────────────────────────────┘
 *
 * Управляет:
 *   - Меню (File, Edit, View, Help)
 *   - Горячие клавиши (Ctrl+N/O/S/S/Q/Z/Y/X/C/V/A/B/I/U/F)
 *   - Динамическим масштабированием (SettingsManager)
 *   - Темами (Light/Dark/System через ThemeManager)
 */
class MainWindow : public QMainWindow, public BookContext
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /** @brief Возвращает текущий открытый документ (или nullptr). */
    BookDocument *currentDocument() const override;

    /** @brief Записывает сообщение в строку состояния (лог). */
    void log(const QString &message) override;

public slots:
    /** @brief Применяет настройки из SettingsDialog (шрифт, масштаб, тема). */
    void applySettings();

    /**
     * @brief Открывает .bceproj проект или одиночный .bce файл.
     * @param filePath — путь к файлу (.bceproj или .bce).
     * @return true при успехе.
     */
    bool openProject(const QString &filePath);

private:
    void setupUI();           ///< Создаёт виджеты (ribbon, left panel, editor, splitter)
    void setupMenuBar();      ///< Создаёт меню «Файл», «Правка», «Вид», «Справка»
    void setupStatusBar();    ///< Настраивает строку состояния
    void applyStyle();        ///< Применяет тему (Light/Dark) через ThemeManager
    void applyScale(int percent); ///< Изменяет масштаб UI

    /** @brief Собирает фильтр OpenFileDialog из всех доступных плагинов форматов. */
    QString buildOpenFilter() const;

    /** @brief Собряет фильтр SaveFileDialog из всех плагинов форматов + BCE. */
    QString buildSaveFilter() const;

    /**
     * @brief Определяет IBookFormat по расширению файла.
     * @param filePath — путь к файлу.
     * @return Найденный плагин формата или nullptr.
     */
    IBookFormat *detectFormatByExtension(const QString &filePath) const;

    /** @brief Обновляет индикатор текущего формата в статус-баре. */
    void updateFormatIndicator();

    // --- Управление проектом (.bceproj) ---

    /**
     * @brief Создаёт новый проект (.bceproj + .bce).
     * @param projectPath — путь для нового .bceproj файла (без него — только документ).
     */
    void newProject(const QString &projectPath = QString());

    /** @brief Сохраняет документ (.bce) и проект (.bceproj, если открыт). */
    void saveProject();

    /**
     * @brief Открывает диалог создания проекта и разворачивает его:
     *        каталоги history/img/content/map, .bceproj, .bce,
     *        импорт выбранных файлов по подкаталогам, пустое оглавление.
     */
    void createNewProject();

    /** @brief Возвращает true, если открыт .bceproj проект. */
    bool hasProject() const;

    /**
     * @brief Перестраивает редактор и дерево после полной замены документа.
     * @param selectPage Индекс страницы для открытия (по умолчанию 0).
     *
     * Обязателен после load()/newDocument()/импорта плагином: иначе вкладки
     * редактора остаются от старого содержимого, переключение страниц
     * молча игнорируется, а текст уходит в несуществующий индекс.
     */
    void reloadEditor(int selectPage = 0);

    RibbonWidget *m_ribbon;   ///< Виджет ленты (ribbon) с вкладками и кнопками
    LeftPanel *m_leftPanel;   ///< Левая панель (дерево оглавления, мета, настройки)
    QSplitter *m_splitter;    ///< Разделитель между левой панелью и редактором
    EditorTabs *m_editor;     ///< Центральный редактор (вкладки WYSIWYG + XML)
    BookDocument *m_document; ///< Текущий документ (модель данных)
    ProjectManager *m_project; ///< Менеджер проекта (.bceproj), nullptr если не открыт
    QFont m_baseFont;         ///< Базовый шрифт приложения (для масштабирования)
    QString m_currentFormatId; ///< Идентификатор текущего формата (например, "EPUB")
};

#endif // MAINWINDOW_H
