#ifndef BOOKDOCUMENT_H
#define BOOKDOCUMENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QDomDocument>
#include <QDomElement>
#include <QList>

/**
 * @brief Метаданные книги (заголовок, автор, язык и т.д.).
 *
 * Хранятся в корне BCE XML: <book><meta><title>...</title>...</meta></book>.
 */
struct BookMeta
{
    QString title;      ///< Заголовок книги
    QString author;     ///< Автор
    QString language;   ///< Код языка (например, "en", "ru")
    QString created;    ///< Дата создания
    QString modified;   ///< Дата последнего изменения
};

/**
 * @brief Описание стиля страницы (шрифт, размер, начертание, цвет).
 *
 * Стили хранятся в <styles> и могут применяться к страницам
 * через атрибут template тега <page>.
 */
struct PageStyle
{
    QString id;           ///< Уникальный идентификатор стиля
    QString fontFamily;   ///< Имя шрифта (например, "Times New Roman")
    int fontSize;         ///< Размер шрифта в пунктах
    bool bold;            ///< Жирное начертание
    bool italic;          ///< Курсив
    bool underline;       ///< Подчёркивание
    QString color;        ///< Цвет текста (CSS-формат, "#RRGGBB")
    QString alignment;    ///< Выравнивание ("left", "center", "right", "justify")
};

/**
 * @brief Центральный класс приложения — модель документа (электронной книги).
 *
 * Хранит структуру книги в виде QDomDocument:
 * @code
 * <book>
 *   <meta><title>...</title><author>...</author>...</meta>
 *   <styles><style id="s1">...</style>...</styles>
 *   <pages>
 *     <page id="p1" template="s1"><body>...</body></page>
 *     ...
 *   </pages>
 * </book>
 * @endcode
 *
 * Основной API:
 *   - meta()/setMeta() — работа с метаданными
 *   - pageCount()/addPage()/removePage()/movePage() — управление страницами
 *   - pageXml()/setPageXml() — чтение/запись XML-содержимого страницы
 *   - load()/save()/saveAs() — чтение/запись .bce файлов
 *   - toXml()/fromXml() — сериализация в строку и обратно
 *
 * Генерирует сигналы modified(), pageAdded(), pageRemoved(), pageChanged()
 * для оповещения UI о изменениях.
 */
class BookDocument : public QObject
{
    Q_OBJECT

public:
    explicit BookDocument(QObject *parent = nullptr);
    ~BookDocument();

    /** @brief Загружает документ из .bce файла. @return true при успехе. */
    bool load(const QString &filePath);

    /** @brief Сохраняет документ. Если filePath пустой — перезаписывает текущий файл. */
    bool save(const QString &filePath = QString());

    /** @brief Сохраняет документ по новому пути (Save As). */
    bool saveAs(const QString &filePath);

    /** @brief Возвращает true, если документ был изменён с последнего сохранения. */
    bool isModified() const;

    /** @brief Возвращает путь к текущему файлу (пустой для нового документа). */
    QString filePath() const;

    /** @brief Возвращает метаданные книги. */
    BookMeta meta() const;

    /** @brief Устанавливает метаданные книги. */
    void setMeta(const BookMeta &meta);

    /** @brief Возвращает список стилей книги. */
    QList<PageStyle> styles() const;

    /** @brief Добавляет стиль в книгу (если стиль с таким id уже есть — заменяет). */
    void addStyle(const PageStyle &style);

    /** @brief Удаляет стиль по идентификатору. */
    void removeStyle(const QString &id);

    /**
     * @brief Возвращает BCE XML содержимое страницы.
     * @param pageIndex — индекс страницы (0-based).
     * @return XML вида <page><body>...</body></page>, или пустая строка.
     */
    QString pageXml(int pageIndex) const;

    /**
     * @brief Устанавливает BCE XML содержимое страницы.
     * @param pageIndex — индекс страницы (0-based).
     * @param xml       — BCE XML с тегом <body> внутри.
     *
     * Заменяет содержимое <body> существующей страницы.
     * Генерирует сигнал pageChanged(index).
     */
    void setPageXml(int pageIndex, const QString &xml);

    /** @brief Возвращает количество страниц в книге. */
    int pageCount() const;

    /**
     * @brief Добавляет новую пустую страницу.
     * @param afterIndex — индекс, после которого вставить (-1 = в конец).
     */
    void addPage(int afterIndex = -1);

    /**
     * @brief Вставляет новую пустую страницу ПЕРЕД указанной позицией.
     * @param atIndex  Индекс, перед которым вставить (-1 или вне диапазона = в конец).
     * @param parentId id страницы-родителя для иерархии (пустая строка — корневой уровень).
     * @return Фактический индекс вставленной страницы.
     */
    int insertPage(int atIndex, const QString &parentId = QString());

    /** @brief Удаляет страницу по индексу. Дочерние страницы переходят к родителю удаляемой. */
    void removePage(int pageIndex);

    /** @brief Перемещает страницу с позиции fromIndex на позицию toIndex. */
    void movePage(int fromIndex, int toIndex);

    /** @brief Возвращает id страницы (например, "p1"). */
    QString pageId(int pageIndex) const;

    /**
     * @brief Возвращает id страницы-родителя (атрибут parent).
     * @return id родителя или пустую строку для страниц верхнего уровня.
     */
    QString pageParent(int pageIndex) const;

    /**
     * @brief Возвращает индекс последнего потомка страницы в иерархии.
     * @param pageIndex Индекс страницы-родителя.
     * @return Индекс самой глубокой/последней дочерней страницы;
     *         сам pageIndex, если потомков нет. Используется для вставки
     *         «после выбранной» с учётом всего поддерева.
     */
    int lastDescendantIndex(int pageIndex) const;

    /** @brief Возвращает id стиля-шаблона страницы (или пустую строку). */
    QString pageTemplate(int pageIndex) const;

    /** @brief Устанавливает стиль-шаблон для страницы. */
    void setPageTemplate(int pageIndex, const QString &tmpl);

    /** @brief Сериализует весь документ в XML-строку. */
    QString toXml() const;

    /** @brief Загружает документ из XML-строки (перезаписывает текущее содержимое). */
    void fromXml(const QString &xml);

    /** @brief Очищает документ и создаёт пустую книгу с одной страницей. */
    void newDocument();

    /**
     * @brief Возвращает все ссылки на изображения в документе.
     * @return Список значений атрибута src всех элементов &lt;image&gt;.
     */
    QStringList imageSources() const;

    /**
     * @brief Переписывает пути изображений (атрибут src) по карте замены.
     * @param map Карта «старый путь → новый путь»; при отсутствии точного
     *            совпадения пробуется совпадение по имени файла.
     */
    void remapImageSources(const QMap<QString, QString> &map);

signals:
    /** @brief Сигнал о любых изменениях в документе (для обновления заголовка окна). */
    void modified();

    /** @brief Сигнал о добавлении страницы. */
    void pageAdded(int index);

    /** @brief Сигнал об удалении страницы. */
    void pageRemoved(int index);

    /** @brief Сигнал об изменении содержимого страницы. */
    void pageChanged(int index);

private:
    void ensureStructure();             ///< Создаёт базовую структуру XML (<book>/<meta>/<pages>), если её нет
    QDomElement metaElement();          ///< Возвращает <meta> элемент (создаёт, если нужно)
    QDomElement stylesElement();        ///< Возвращает <styles> элемент (создаёт, если нужно)
    QDomElement pagesElement() const;   ///< Возвращает <pages> элемент (или пустой QDomElement)
    QDomElement pageElement(int index) const; ///< Возвращает <page> по индексу
    QString generatePageId() const;     ///< Генерирует уникальный id новой страницы ("p1", "p2", ...)

    QDomDocument m_doc;    ///< DOM-дерево всего документа
    QString m_filePath;    ///< Путь к файлу на диске
    bool m_modified;       ///< Флаг «есть несохранённые изменения»
    int m_nextPageId;      ///< Счётчик для генерации id страниц
};

#endif // BOOKDOCUMENT_H
