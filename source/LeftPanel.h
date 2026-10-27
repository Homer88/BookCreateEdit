#ifndef LEFTPANEL_H
#define LEFTPANEL_H

#include <QWidget>

class QTabWidget;
class QTreeWidget;
class QListWidget;
class QTreeWidgetItem;
class QLineEdit;
class BookDocument;

/**
 * @brief Левая панель — содержит дерево оглавления, метаданные и настройки проекта.
 *
 * Компоновка:
 *   ┌──────────────────────┐
 *   │ [Оглавление] [Файл]  │  ← QTabWidget с вкладками
 *   ├──────────────────────┤
 *   │  ▸ Глава 1           │  ← QTreeWidget (дерево оглавления)
 *   │    ▸ Параграф 1.1    │
 *   │  ▸ Глава 2           │
 *   │                      │
 *   │  Заголовок: [______] │  ← Поля метаданных
 *   │  Автор:     [______] │
 *   │  Язык:      [______] │
 *   └──────────────────────┘
 *
 * Взаимодействие:
 *   - Клик по элементу дерева → сигнал pageSelected(index)
 *   - Контекстное меню → переименование/удаление/перемещение страниц
 *   - Изменение полей метаданных → обновление BookDocument::setMeta()
 */
class LeftPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LeftPanel(QWidget *parent = nullptr);

    /** @brief Устанавливает документ для отображения в дереве оглавления. */
    void setDocument(BookDocument *doc);

    /** @brief Возвращает текущий документ. */
    BookDocument *document() const { return m_document; }

    /** @brief Возвращает дерево оглавления (для доступа извне). */
    QTreeWidget *treeOfContents() const { return m_treeContents; }

signals:
    /** @brief Сигнал о выборе страницы в дереве оглавления. */
    void pageSelected(int pageIndex);

    /** @brief Сигнал о переименовании страницы. */
    void pageRenamed(int pageIndex, const QString &newTitle);

    /**
     * @brief Запрос на вставку новой страницы относительно выбранной.
     * @param pageIndex Якорная страница (-1 — операция над концом книги).
     * @param mode      Режим вставки: 0 — до выбранной, 1 — после выбранной
     *                  (с учётом её поддерева), 2 — дочерняя страница.
     */
    void pageInsertRequested(int pageIndex, int mode);

    /** @brief Запрос на удаление страницы (до фактического изменения документа). */
    void pageDeleteRequested(int pageIndex);

public slots:
    /**
     * @brief Перестраивает дерево оглавления из текущего документа.
     * @param selectPageIndex Индекс страницы, которую нужно выделить после
     *                        перестроения (-1 — не менять выделение).
     */
    void refreshContents(int selectPageIndex = -1);

private slots:
    void onTreeItemClicked(QTreeWidgetItem *item, int column); ///< Обработка клика по элементу дерева
    void onMetaEdited();        ///< Сохранение изменённых метаданных в документ
    void showContextMenu(const QPoint &pos); ///< Показ контекстного меню (переименовать/удалить/переместить)

private:
    void setupUI();              ///< Создаёт QTabWidget, дерево, поля метаданных
    void populateFromDocument(int selectPageIndex); ///< Заполняет дерево из BookDocument::pageCount()/pageXml()
    QTreeWidgetItem *buildItemForPage(int pageIndex) const; ///< Создаёт элемент дерева для страницы (заголовок, UserRole, подсказка)

    QTabWidget *m_tabWidget;     ///< Вкладки панели (Оглавление, Файл)
    QTreeWidget *m_treeContents;  ///< Дерево оглавления (страницы книги)
    QListWidget *m_fileList;     ///< Список файлов проекта
    QTreeWidget *m_projectSettings; ///< Настройки проекта

    QLineEdit *m_edtTitle;    ///< Поле ввода заголовка
    QLineEdit *m_edtAuthor;   ///< Поле ввода автора
    QLineEdit *m_edtLanguage; ///< Поле ввода языка

    BookDocument *m_document; ///< Текущий документ
};

#endif // LEFTPANEL_H
