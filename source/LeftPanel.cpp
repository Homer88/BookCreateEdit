#include "LeftPanel.h"
#include "BookDocument.h"

#include <QVBoxLayout>
#include <QTabWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QHeaderView>
#include <QLineEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QVector>

/**
 * @brief Конструктор панели. Создаёт интерфейс и задаёт стилизацию виджета.
 * @param parent Родительский виджет (может быть nullptr).
 */
LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
    , m_document(nullptr)
{
    setupUI();
    // Стиль панели: светлый фон и вертикальная линия-разделитель справа
    setStyleSheet(R"(
        LeftPanel {
            background-color: #f0f0f0;
            border-right: 1px solid #c0c0c0;
        }
    )");
}

/**
 * @brief Привязывает панель к документу книги и выполняет первичное заполнение.
 *
 * Устанавливает указатель на документ, подписывается на его сигналы
 * (изменение, добавление и удаление страницы), после чего заполняет
 * все вкладки актуальными данными.
 * @param doc Указатель на документ книги (nullptr сбрасывает привязку).
 */
void LeftPanel::setDocument(BookDocument *doc)
{
    m_document = doc;
    if (m_document) {
        // При любом изменении документа обновляем содержимое панели
        connect(m_document, &BookDocument::modified, this, [this]() { refreshContents(); });
        connect(m_document, &BookDocument::pageAdded, this, [this](int) { refreshContents(); });
        connect(m_document, &BookDocument::pageRemoved, this, [this](int) { refreshContents(); });
    }
    populateFromDocument(-1);
}

/**
 * @brief Создаёт и настраивает элементы пользовательского интерфейса панели.
 *
 * Формирует вкладки: «Оглавление» (дерево страниц),
 * «Файлы» (список файлов) и «Свойства» (метаданные проекта).
 * Настраивает стили и подключает обработчики событий.
 */
void LeftPanel::setupUI()
{
    // Корневой компоновщик без отступов — панель занимает всё доступное пространство
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    // Оформление вкладок в стиле «подпапок»: активная вкладка сливается с фоном
    m_tabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #c0c0c0;
            background: white;
        }
        QTabBar::tab {
            background: #e8e8e8;
            border: 1px solid #c0c0c0;
            padding: 5px 12px;
            margin-right: 1px;
        }
        QTabBar::tab:selected {
            background: white;
            border-bottom: 1px solid white;
            font-weight: bold;
        }
    )");

    // Вкладка «Оглавление»: дерево страниц документа
    m_treeContents = new QTreeWidget(m_tabWidget);
    m_treeContents->setHeaderLabels({"Оглавление"});
    m_treeContents->setRootIsDecorated(true);
    m_treeContents->setAnimated(true);
    m_treeContents->setStyleSheet(R"(
        QTreeWidget { border: none; }
        QTreeWidget::item { padding: 3px; }
        QTreeWidget::item:selected { background-color: #d0e8ff; }
    )");
    // Клик по элементу дерева выбирает соответствующую страницу
    connect(m_treeContents, &QTreeWidget::itemClicked,
            this, &LeftPanel::onTreeItemClicked);

    // Включаем собственное контекстное меню для операций над страницами
    m_treeContents->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeContents, &QTreeWidget::customContextMenuRequested,
            this, &LeftPanel::showContextMenu);

    // Вкладка «Файлы»: простой список файлов проекта
    m_fileList = new QListWidget(m_tabWidget);
    m_fileList->setStyleSheet(R"(
        QListWidget { border: none; }
        QListWidget::item { padding: 4px; }
        QListWidget::item:selected { background-color: #d0e8ff; }
    )");

    // Вкладка «Свойства»: двухколоночное дерево пар «имя — значение» без заголовка
    m_projectSettings = new QTreeWidget(m_tabWidget);
    m_projectSettings->setHeaderHidden(true);
    m_projectSettings->setRootIsDecorated(false);
    m_projectSettings->setColumnCount(2);
    // Вторая колонка растягивается на всю оставшуюся ширину
    m_projectSettings->header()->setStretchLastSection(true);
    m_projectSettings->setStyleSheet(R"(
        QTreeWidget { border: none; }
        QTreeWidget::item { padding: 3px; }
    )");

    // Добавляем вкладки с русскими названиями
    m_tabWidget->addTab(m_treeContents, "Оглавление");
    m_tabWidget->addTab(m_fileList, "Файлы");
    m_tabWidget->addTab(m_projectSettings, "Свойства");

    layout->addWidget(m_tabWidget);
}

/**
 * @brief Заполняет все вкладки панели данными из текущего документа.
 *
 * Очищает дерево оглавления, список файлов и таблицу свойств,
 * затем заново строит их содержимое. Дерево оглавления отражает
 * иерархию страниц: ссылка на родителя берётся из атрибута parent.
 * @param selectPageIndex Индекс страницы для выделения после перестроения
 *                        (-1 — выделение не восстанавливается).
 */
void LeftPanel::populateFromDocument(int selectPageIndex)
{
    // Полная перестройка: сначала очищаем все представления
    m_treeContents->clear();
    m_fileList->clear();
    m_projectSettings->clear();

    // Документ не задан — показываем заглушку в дереве оглавления
    if (!m_document) {
        new QTreeWidgetItem(m_treeContents, {"(нет документа)"});
        return;
    }

    // Первый проход: создаём элементы всех страниц (пока без привязки к дереву)
    const int n = m_document->pageCount();
    QVector<QTreeWidgetItem *> items(n);
    QMap<QString, int> idToRow;          // id страницы → её индекс в списке
    for (int i = 0; i < n; ++i) {
        items[i] = buildItemForPage(i);
        const QString id = m_document->pageId(i);
        if (!id.isEmpty())
            idToRow.insert(id, i);
    }

    // Второй проход: выстраиваем иерархию по атрибуту parent
    for (int i = 0; i < n; ++i) {
        QTreeWidgetItem *parentItem = nullptr;
        const QString pid = m_document->pageParent(i);
        if (!pid.isEmpty()) {
            const auto it = idToRow.constFind(pid);
            // Защита от зацикливания: страница не может быть родителем себе
            if (it != idToRow.constEnd() && it.value() != i)
                parentItem = items[it.value()];
        }
        if (parentItem)
            parentItem->addChild(items[i]);
        else
            m_treeContents->addTopLevelItem(items[i]);
    }

    // Раскрываем дерево, чтобы структура была видна сразу
    m_treeContents->expandAll();

    // Выделяем запрошенную страницу (не порождает сигналов клика)
    if (selectPageIndex >= 0 && selectPageIndex < n && items[selectPageIndex])
        m_treeContents->setCurrentItem(items[selectPageIndex]);

    // Заполняем вкладку «Свойства» метаданными книги
    BookMeta m = m_document->meta();

    QTreeWidgetItem *itemTitle = new QTreeWidgetItem(m_projectSettings, {"Название", m.title});
    QTreeWidgetItem *itemAuthor = new QTreeWidgetItem(m_projectSettings, {"Автор", m.author});
    QTreeWidgetItem *itemLang = new QTreeWidgetItem(m_projectSettings, {"Язык", m.language});
    QTreeWidgetItem *itemPages = new QTreeWidgetItem(m_projectSettings,
        {"Страниц", QString::number(m_document->pageCount())});
    // Если файл ещё не сохранён на диске, отображаем заглушку
    QString filePath = m_document->filePath().isEmpty() ? "(не сохранён)" : m_document->filePath();
    QTreeWidgetItem *itemFile = new QTreeWidgetItem(m_projectSettings,
        {"Файл", filePath});

    // Подгоняем ширину первой колонки под самый длинный заголовок поля
    m_projectSettings->resizeColumnToContents(0);
}

/**
 * @brief Создаёт элемент дерева оглавления для страницы документа.
 * @param pageIndex Индекс страницы (с нуля).
 * @return Элемент дерева с заголовком страницы; индекс сохранён в UserRole.
 */
QTreeWidgetItem *LeftPanel::buildItemForPage(int pageIndex) const
{
    QString pageId = m_document->pageId(pageIndex);
    QString pageXml = m_document->pageXml(pageIndex);

    // Заголовок по умолчанию — номер страницы и её идентификатор
    QString title = QString("Страница %1 [%2]").arg(pageIndex + 1).arg(pageId);

    // Пытаемся извлечь реальный заголовок из первого тега <h1>..<h6> в разметке
    QRegularExpression re("<h[1-6][^>]*>([^<]*)</h[1-6]>",
                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(pageXml);
    if (match.hasMatch()) {
        title = match.captured(1).trimmed();
        // Пустой заголовок внутри тега — откат к номеру страницы
        if (title.isEmpty())
            title = QString("Страница %1").arg(pageIndex + 1);
    }

    QTreeWidgetItem *item = new QTreeWidgetItem({title});
    // Индекс страницы сохраняется в UserRole для последующего выбора по клику
    item->setData(0, Qt::UserRole, pageIndex);
    item->setToolTip(0, QString("Страница %1").arg(pageIndex + 1) + " — " + pageId);
    return item;
}

/**
 * @brief Обновляет содержимое панели в ответ на изменения документа.
 *
 * Вызывается автоматически по сигналам документа (modified,
 * pageAdded, pageRemoved). Просто перечитывает данные заново.
 * @param selectPageIndex Индекс страницы для выделения (-1 — не менять).
 */
void LeftPanel::refreshContents(int selectPageIndex)
{
    populateFromDocument(selectPageIndex);
}

/**
 * @brief Обрабатывает клик по элементу дерева оглавления.
 *
 * Извлекает индекс страницы из UserRole выбранного элемента
 * и испускает сигнал pageSelected, если индекс корректен.
 * @param item Элемент дерева, по которому выполнен клик.
 * @param column Номер колонки (не используется).
 */
void LeftPanel::onTreeItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;

    // Индекс страницы был сохранён при заполнении дерева
    bool ok = false;
    int pageIndex = item->data(0, Qt::UserRole).toInt(&ok);
    // Проверяем корректность преобразования и границы диапазона страниц
    if (ok && pageIndex >= 0 && pageIndex < m_document->pageCount()) {
        emit pageSelected(pageIndex);
    }
}

/**
 * @brief Применяет значения из полей редактирования метаданных к документу.
 *
 * Считывает текущие метаданные, заменяет поля «Название», «Автор»
 * и «Язык» значениями из элементов ввода (если они существуют)
 * и записывает метаданные обратно в документ.
 */
void LeftPanel::onMetaEdited()
{
    if (!m_document) return;

    BookMeta m = m_document->meta();
    // Если поле редактирования отсутствует, оставляем прежнее значение
    m.title = m_edtTitle ? m_edtTitle->text() : m.title;
    m.author = m_edtAuthor ? m_edtAuthor->text() : m.author;
    m.language = m_edtLanguage ? m_edtLanguage->text() : m.language;
    m_document->setMeta(m);
}

/**
 * @brief Показывает контекстное меню дерева оглавления.
 *
 * При клике по странице доступны операции вставки «до», «после»
 * и «дочерняя страница», а также удаление и переименование.
 * На пустом месте дерева предлагается добавить страницу в конец книги.
 * Все операции со структурой выполняет MainWindow (по сигналам),
 * чтобы синхронно перестроить вкладки редактора.
 * @param pos Позиция курсора в координатах области просмотра дерева.
 */
void LeftPanel::showContextMenu(const QPoint &pos)
{
    // Определяем элемент дерева, над которым вызвано меню (может быть nullptr)
    QTreeWidgetItem *item = m_treeContents->itemAt(pos);
    QMenu menu;

    QAction *actAddBefore = nullptr;
    QAction *actAddAfter = nullptr;
    QAction *actAddChild = nullptr;
    QAction *actAddEnd = nullptr;
    QAction *actDelete = nullptr;
    QAction *actRename = nullptr;

    if (item) {
        // Операции вставки относительно выбранной страницы
        actAddBefore = menu.addAction("Добавить страницу до");
        actAddAfter  = menu.addAction("Добавить страницу после");
        actAddChild  = menu.addAction("Добавить дочернюю страницу");
        menu.addSeparator();
        actDelete = menu.addAction("Удалить страницу");
        // Запрет удаления единственной страницы документа
        if (!m_document || m_document->pageCount() <= 1)
            actDelete->setEnabled(false);
        actRename = menu.addAction("Переименовать");
    } else {
        actAddEnd = menu.addAction("Добавить страницу в конец");
    }

    // Показываем меню в глобальных координатах; выход при отмене выбора
    QAction *selected = menu.exec(m_treeContents->viewport()->mapToGlobal(pos));
    if (!selected) return;

    const int anchor = item ? item->data(0, Qt::UserRole).toInt() : -1;

    if (selected == actAddBefore) {
        emit pageInsertRequested(anchor, 0);   // до выбранной
    } else if (selected == actAddAfter) {
        emit pageInsertRequested(anchor, 1);   // после выбранной
    } else if (selected == actAddChild) {
        emit pageInsertRequested(anchor, 2);   // дочерняя страница
    } else if (selected == actAddEnd) {
        emit pageInsertRequested(-1, 1);       // в конец книги
    } else if (selected == actDelete && item) {
        emit pageDeleteRequested(anchor);
    } else if (selected == actRename && item) {
        if (m_document) {
            bool ok;
            // Диалог ввода нового названия с предзаполнением текущего текста
            QString newTitle = QInputDialog::getText(this, "Переименовать",
                "Название:", QLineEdit::Normal, item->text(0), &ok);
            // Применяем только непустое подтверждённое название
            if (ok && !newTitle.isEmpty()) {
                emit pageRenamed(anchor, newTitle);
            }
        }
    }
}
