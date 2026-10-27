#include "BookDocument.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <QUuid>
#include <QSet>

/**
 * @brief Конструктор документа. Создаёт пустой (несформированный) XML-документ,
 *        сбрасывает признак изменений и счётчик идентификаторов страниц.
 * @param parent Родительский QObject (для иерархии владения Qt).
 */
BookDocument::BookDocument(QObject *parent)
    : QObject(parent)
    , m_modified(false)
    , m_nextPageId(1)
{
}

/**
 * @brief Деструктор. Специальной очистки не требуется — QDomDocument
 *        освобождает ресурсы автоматически.
 */
BookDocument::~BookDocument()
{
}

/**
 * @brief Создаёт новый пустой документ в формате BCE со структурой по умолчанию:
 *        корневой элемент <book>, секция <meta> с начальными значениями,
 *        пустые секции стилей и страниц, а также одну стартовую страницу.
 */
void BookDocument::newDocument()
{
    // Полностью сбрасываем состояние документа
    m_doc.clear();
    m_filePath.clear();
    m_nextPageId = 1;
    m_modified = false;

    // Добавляем XML-объявление: <?xml version="1.0" encoding="UTF-8"?>
    QDomProcessingInstruction pi = m_doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    m_doc.appendChild(pi);

    // Создаём корневой элемент книги с версией формата
    QDomElement root = m_doc.createElement("book");
    root.setAttribute("version", "1.0");
    m_doc.appendChild(root);

    // Секция метаданных: заголовок, автор, язык, даты создания/изменения
    QDomElement meta = m_doc.createElement("meta");
    root.appendChild(meta);

    QDomElement titleEl = m_doc.createElement("title");
    titleEl.appendChild(m_doc.createTextNode("Новая книга"));
    meta.appendChild(titleEl);

    QDomElement authorEl = m_doc.createElement("author");
    authorEl.appendChild(m_doc.createTextNode(""));
    meta.appendChild(authorEl);

    QDomElement langEl = m_doc.createElement("language");
    langEl.appendChild(m_doc.createTextNode("ru"));
    meta.appendChild(langEl);

    QDomElement createdEl = m_doc.createElement("created");
    createdEl.appendChild(m_doc.createTextNode(QDateTime::currentDateTime().toString(Qt::ISODate)));
    meta.appendChild(createdEl);

    QDomElement modifiedEl = m_doc.createElement("modified");
    modifiedEl.appendChild(m_doc.createTextNode(QDateTime::currentDateTime().toString(Qt::ISODate)));
    meta.appendChild(modifiedEl);

    // Пустые контейнеры для стилей и страниц
    QDomElement styles = m_doc.createElement("styles");
    root.appendChild(styles);

    QDomElement pages = m_doc.createElement("pages");
    root.appendChild(pages);

    // Добавляем первую страницу, чтобы документ был сразу пригоден к работе
    addPage();
}

/**
 * @brief Собирает все ссылки на изображения из всех страниц документа.
 * @return Список уникальных значений атрибута src элементов <image>.
 */
QStringList BookDocument::imageSources() const
{
    QStringList result;

    // Обходим все страницы и все элементы <image> внутри них
    int count = pageCount();
    for (int i = 0; i < count; ++i) {
        QDomElement pageEl = pageElement(i);
        if (pageEl.isNull())
            continue;
        QDomNodeList images = pageEl.elementsByTagName("image");
        for (int j = 0; j < images.count(); ++j) {
            QString src = images.at(j).toElement().attribute("src").trimmed();
            if (!src.isEmpty() && !result.contains(src))
                result.append(src);
        }
    }
    return result;
}

/**
 * @brief Заменяет пути изображений во всём документе по карте замены.
 *
 * Поиск выполняется сначала по точному совпадению src, затем по имени
 * файла (последняя часть пути) — это позволяет переписать ссылки,
 * извлечённые, например, из ZIP-контейнеров с вложенными каталогами.
 * @param map Карта «старый путь → новый путь».
 */
void BookDocument::remapImageSources(const QMap<QString, QString> &map)
{
    if (map.isEmpty())
        return;

    // Индекс по имени файла для резервного сопоставления
    QMap<QString, QString> byBase;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        byBase.insert(QFileInfo(it.key()).fileName(), it.value());

    bool changed = false;
    int count = pageCount();
    for (int i = 0; i < count; ++i) {
        QDomElement pageEl = pageElement(i);
        if (pageEl.isNull())
            continue;
        QDomNodeList images = pageEl.elementsByTagName("image");
        for (int j = 0; j < images.count(); ++j) {
            QDomElement img = images.at(j).toElement();
            QString src = img.attribute("src");
            QString base = QFileInfo(src).fileName();

            QString newSrc;
            if (map.contains(src))
                newSrc = map.value(src);
            else if (map.contains("#" + src))
                newSrc = map.value("#" + src);      // FB2/FB3: ссылка вида "#id"
            else if (byBase.contains(base))
                newSrc = byBase.value(base);         // совпадение по имени файла

            if (!newSrc.isEmpty() && newSrc != src) {
                img.setAttribute("src", newSrc);
                changed = true;
            }
        }
    }

    if (changed) {
        m_modified = true;
        emit modified();
    }
}

/**
 * @brief Загружает документ из XML-файла с проверкой корректности разбора.
 * @param filePath Путь к файлу документа.
 * @return true при успешной загрузке, false при ошибке открытия или разбора.
 */
bool BookDocument::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    // Разбираем XML; при ошибке сообщаем об неудаче (детали ошибки не используются)
    QString errorMsg;
    int errorLine, errorCol;
    if (!m_doc.setContent(&file, &errorMsg, &errorLine, &errorCol)) {
        file.close();
        return false;
    }
    file.close();

    // Загрузка успешна: запоминаем путь и сбрасываем признак изменений
    m_filePath = filePath;
    m_modified = false;

    // Восстанавливаем счётчик идентификаторов так, чтобы новые id не совпали с существующими
    QDomElement root = m_doc.documentElement();
    QDomElement pagesEl = root.firstChildElement("pages");
    m_nextPageId = pagesEl.childNodes().count() + 1;

    return true;
}

/**
 * @brief Сохраняет документ в файл. Перед записью обновляет штамп времени
 *        изменения (<modified>) в метаданных.
 * @param filePath Путь сохранения; если пуст — используется текущий путь файла.
 * @return true при успешной записи, иначе false.
 */
bool BookDocument::save(const QString &filePath)
{
    // Если путь не указан явно — сохраняем по прежнему месту
    QString path = filePath.isEmpty() ? m_filePath : filePath;
    if (path.isEmpty())
        return false;

    // Обновляем время изменения в метаданных (создаём элемент, если его нет)
    QDomElement root = m_doc.documentElement();
    QDomElement meta = root.firstChildElement("meta");
    QDomElement modifiedEl = meta.firstChildElement("modified");
    if (!modifiedEl.isNull()) {
        modifiedEl.firstChild().setNodeValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    } else {
        modifiedEl = m_doc.createElement("modified");
        modifiedEl.appendChild(m_doc.createTextNode(QDateTime::currentDateTime().toString(Qt::ISODate)));
        meta.appendChild(modifiedEl);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    // Записываем XML с отступом в 2 пробела для читаемости
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << m_doc.toString(2);
    file.close();

    // После успешного сохранения фиксируем путь и сбрасываем флаг изменений
    m_filePath = path;
    m_modified = false;
    return true;
}

/**
 * @brief Сохраняет документ под новым именем (простой делегат к save()).
 * @param filePath Новый путь файла.
 * @return true при успешной записи.
 */
bool BookDocument::saveAs(const QString &filePath)
{
    return save(filePath);
}

/**
 * @brief Проверяет наличие несохранённых изменений.
 * @return true, если документ изменён после последней загрузки/сохранения.
 */
bool BookDocument::isModified() const
{
    return m_modified;
}

/**
 * @brief Возвращает путь к файлу, связанному с документом.
 * @return Путь файла или пустую строку, если документ ещё не сохранялся.
 */
QString BookDocument::filePath() const
{
    return m_filePath;
}

/**
 * @brief Считывает метаданные книги из XML-документа.
 * @return Структура BookMeta с полями title, author, language, created, modified.
 */
BookMeta BookDocument::meta() const
{
    BookMeta m;
    QDomElement root = m_doc.documentElement();
    QDomElement meta = root.firstChildElement("meta");
    // Последовательно извлекаем текстовое содержимое каждого тега метаданных
    m.title = meta.firstChildElement("title").text();
    m.author = meta.firstChildElement("author").text();
    m.language = meta.firstChildElement("language").text();
    m.created = meta.firstChildElement("created").text();
    m.modified = meta.firstChildElement("modified").text();
    return m;
}

/**
 * @brief Обновляет метаданные книги (заголовок, автора, язык).
 *        Отсутствующие теги создаются автоматически.
 * @param meta Новые значения метаданных.
 */
void BookDocument::setMeta(const BookMeta &meta)
{
    QDomElement root = m_doc.documentElement();
    QDomElement metaEl = root.firstChildElement("meta");

    // Локальная лямбда: устанавливает текст тега, создавая элемент при необходимости
    auto setText = [this, &metaEl](const QString &tag, const QString &value) {
        QDomElement el = metaEl.firstChildElement(tag);
        if (el.isNull()) {
            el = m_doc.createElement(tag);
            metaEl.appendChild(el);
        }
        // Если текстового узла ещё нет — создаём его, иначе обновляем значение на месте
        if (el.firstChild().isNull())
            el.appendChild(m_doc.createTextNode(value));
        else
            el.firstChild().setNodeValue(value);
    };

    setText("title", meta.title);
    setText("author", meta.author);
    setText("language", meta.language);

    m_modified = true;
    emit modified();
}

/**
 * @brief Возвращает список всех стилей страниц, описанных в документе.
 *        Атрибуты, отсутствующие в XML, заменяются значениями по умолчанию.
 * @return Список структур PageStyle.
 */
QList<PageStyle> BookDocument::styles() const
{
    QList<PageStyle> list;
    QDomElement root = m_doc.documentElement();
    QDomElement stylesEl = root.firstChildElement("styles");
    QDomNodeList nodes = stylesEl.childNodes();
    // Обходим все дочерние узлы секции <styles> и выбираем элементы <style>
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement el = nodes.at(i).toElement();
        if (el.tagName() == "style") {
            PageStyle s;
            // Читаем атрибуты стиля; вторые аргументы attribute() — значения по умолчанию
            s.id = el.attribute("id");
            s.fontFamily = el.attribute("font", "Times New Roman");
            s.fontSize = el.attribute("size", "12").toInt();
            s.bold = el.attribute("bold", "false") == "true";
            s.italic = el.attribute("italic", "false") == "true";
            s.underline = el.attribute("underline", "false") == "true";
            s.color = el.attribute("color", "#000000");
            s.alignment = el.attribute("alignment", "left");
            list.append(s);
        }
    }
    return list;
}

/**
 * @brief Добавляет новый стиль или заменяет существующий с тем же id.
 * @param style Описатель стиля страницы.
 */
void BookDocument::addStyle(const PageStyle &style)
{
    QDomElement root = m_doc.documentElement();
    QDomElement stylesEl = root.firstChildElement("styles");

    // Если стиль с таким id уже существует — удаляем его (логика "вставка или замена")
    QDomNodeList existing = stylesEl.elementsByTagName("style");
    for (int i = 0; i < existing.count(); ++i) {
        if (existing.at(i).toElement().attribute("id") == style.id) {
            stylesEl.removeChild(existing.at(i));
            break;
        }
    }

    // Формируем новый элемент <style> со всеми атрибутами оформления
    QDomElement el = m_doc.createElement("style");
    el.setAttribute("id", style.id);
    el.setAttribute("font", style.fontFamily);
    el.setAttribute("size", QString::number(style.fontSize));
    el.setAttribute("bold", style.bold ? "true" : "false");
    el.setAttribute("italic", style.italic ? "true" : "false");
    el.setAttribute("underline", style.underline ? "true" : "false");
    el.setAttribute("color", style.color);
    el.setAttribute("alignment", style.alignment);
    stylesEl.appendChild(el);

    m_modified = true;
    emit modified();
}

/**
 * @brief Удаляет стиль с указанным идентификатором.
 * @param id Идентификатор стиля; если стиль не найден, ничего не происходит.
 */
void BookDocument::removeStyle(const QString &id)
{
    QDomElement root = m_doc.documentElement();
    QDomElement stylesEl = root.firstChildElement("styles");
    QDomNodeList nodes = stylesEl.elementsByTagName("style");
    // Ищем первый стиль с совпадающим id и удаляем его
    for (int i = 0; i < nodes.count(); ++i) {
        if (nodes.at(i).toElement().attribute("id") == id) {
            stylesEl.removeChild(nodes.at(i));
            m_modified = true;
            emit modified();
            return;
        }
    }
}

/**
 * @brief Возвращает XML-содержимое (внутреннюю разметку) страницы.
 * @param pageIndex Индекс страницы (с нуля).
 * @return Текст внутреннего элемента страницы или пустую строку, если страницы нет.
 */
QString BookDocument::pageXml(int pageIndex) const
{
    QDomElement pageEl = pageElement(pageIndex);
    if (pageEl.isNull())
        return QString();

    // Собираем внутреннюю разметку страницы ЦЕЛИКОМ (все дочерние узлы),
    // сохраняя структуру <body>, абзацы, картинки и форматирование.
    // Раньше здесь брали только первый дочерний элемент и его .text(),
    // из-за чего при наличии пробельных узлов (после записи файла
    // с отступами) страница загружалась пустой, а разметка терялась.
    QString inner;
    for (QDomNode n = pageEl.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QTextStream ts(&inner);
        ts.setCodec("UTF-8");
        n.save(ts, 2);
        inner += "\n";
    }
    return inner.trimmed();
}

/**
 * @brief Заменяет содержимое страницы новым XML-фрагментом.
 *        Если переданный XML некорректен, он сохраняется как простой текстовый узел.
 * @param pageIndex Индекс страницы (с нуля); игнорируется, если страница не найдена.
 * @param xml XML-строка нового содержимого страницы.
 */
void BookDocument::setPageXml(int pageIndex, const QString &xml)
{
    QDomElement pageEl = pageElement(pageIndex);
    if (pageEl.isNull())
        return;

    // Полностью очищаем старое содержимое страницы
    while (!pageEl.firstChild().isNull())
        pageEl.removeChild(pageEl.firstChild());

    // Пробуем разобрать строку как самостоятельный XML-документ
    QDomDocument fragDoc;
    if (fragDoc.setContent(xml)) {
        // Переносим всех потомков разобранного корня в текущий документ (глубокое копирование)
        QDomNodeList children = fragDoc.documentElement().childNodes();
        for (int i = 0; i < children.count(); ++i)
            pageEl.appendChild(m_doc.importNode(children.at(i), true));
    } else {
        // Резервный вариант: сохраняем исходную строку как обычный текст
        pageEl.appendChild(m_doc.createTextNode(xml));
    }

    m_modified = true;
    emit pageChanged(pageIndex);
    emit modified();
}

/**
 * @brief Возвращает количество страниц в документе.
 * @return Число дочерних элементов секции <pages>.
 */
int BookDocument::pageCount() const
{
    return pagesElement().childNodes().count();
}

/**
 * @brief Добавляет новую пустую страницу с телом <body><p/></body>.
 * @param afterIndex Индекс, после которого вставить страницу;
 *                   если отрицательный или вне диапазона — в конец списка.
 */
void BookDocument::addPage(int afterIndex)
{
    // Сохраняем прежнюю семантику: afterIndex < 0 означает добавление в конец
    insertPage(afterIndex < 0 ? -1 : afterIndex + 1);
}

/**
 * @brief Вставляет новую пустую страницу перед указанной позицией.
 * @param atIndex  Индекс, перед которым вставить; -1/вне диапазона — в конец.
 * @param parentId id страницы-родителя (пустая строка — верхний уровень).
 * @return Фактический индекс вставленной страницы.
 */
int BookDocument::insertPage(int atIndex, const QString &parentId)
{
    QDomElement pagesEl = pagesElement();
    QDomElement pageEl = m_doc.createElement("page");
    pageEl.setAttribute("id", generatePageId());
    pageEl.setAttribute("template", "default");
    // Иерархия страниц: ссылка на родителя хранится атрибутом parent
    if (!parentId.isEmpty())
        pageEl.setAttribute("parent", parentId);

    // Создаём стандартное содержимое новой страницы: тело с одним пустым абзацем
    QDomElement body = m_doc.createElement("body");
    QDomElement p = m_doc.createElement("p");
    p.appendChild(m_doc.createTextNode(""));
    body.appendChild(p);
    pageEl.appendChild(body);

    const int count = pagesEl.childNodes().count();
    int newIndex;
    if (atIndex < 0 || atIndex >= count) {
        // Вставка в конец списка
        pagesEl.appendChild(pageEl);
        newIndex = count;
    } else {
        pagesEl.insertBefore(pageEl, pagesEl.childNodes().at(atIndex));
        newIndex = atIndex;
    }

    m_nextPageId++;
    m_modified = true;
    emit pageAdded(newIndex);
    emit modified();
    return newIndex;
}

/**
 * @brief Удаляет страницу по индексу. Ничего не делает при некорректном индексе.
 * @param pageIndex Индекс удаляемой страницы (с нуля).
 *
 * Дочерние страницы удаляемой переходят под её родителя
 * (или становятся корневыми), чтобы дерево не «осиротело».
 */
void BookDocument::removePage(int pageIndex)
{
    QDomElement pagesEl = pagesElement();
    QDomNodeList nodes = pagesEl.childNodes();
    if (pageIndex < 0 || pageIndex >= nodes.count())
        return;

    const QDomElement removed = nodes.at(pageIndex).toElement();
    const QString removedId = removed.attribute("id");
    const QString grandId = removed.attribute("parent");

    // Переназначаем родителя всем прямым потомкам удаляемой страницы
    if (!removedId.isEmpty()) {
        for (int i = 0; i < nodes.count(); ++i) {
            QDomElement child = nodes.at(i).toElement();
            if (!child.isNull() && child.attribute("parent") == removedId) {
                if (grandId.isEmpty())
                    child.removeAttribute("parent");
                else
                    child.setAttribute("parent", grandId);
            }
        }
    }

    pagesEl.removeChild(nodes.at(pageIndex));
    m_modified = true;
    emit pageRemoved(pageIndex);
    emit modified();
}

/**
 * @brief Перемещает страницу с одной позиции списка на другую.
 * @param fromIndex Исходный индекс страницы.
 * @param toIndex   Целевой индекс; после перемещения страница окажется на этой позиции.
 */
void BookDocument::movePage(int fromIndex, int toIndex)
{
    QDomElement pagesEl = pagesElement();
    QDomNodeList nodes = pagesEl.childNodes();
    // Проверяем валидность обоих индексов и осмысленность операции
    if (fromIndex < 0 || fromIndex >= nodes.count())
        return;
    if (toIndex < 0 || toIndex >= nodes.count())
        return;
    if (fromIndex == toIndex)
        return;

    // Клонируем узел, чтобы безопасно вырезать его из текущей позиции
    QDomNode node = nodes.at(fromIndex).cloneNode(true);
    pagesEl.removeChild(nodes.at(fromIndex));

    // После удаления список смещается, поэтому заново получаем дочерние узлы
    QDomNodeList updatedNodes = pagesEl.childNodes();
    if (toIndex > fromIndex) {
        // Движение вперёд: из-за удалённого элемента ориентируемся на позицию toIndex - 1
        if (toIndex - 1 < updatedNodes.count())
            pagesEl.insertAfter(updatedNodes.at(toIndex - 1), node);
        else
            pagesEl.appendChild(node);
    } else {
        // Движение назад: вставляем перед узлом, стоявшим на целевой позиции
        pagesEl.insertBefore(updatedNodes.at(toIndex), node);
    }

    m_modified = true;
    emit modified();
}

/**
 * @brief Возвращает идентификатор страницы.
 * @param pageIndex Индекс страницы (с нуля).
 * @return Значение атрибута id или пустую строку, если страницы нет.
 */
QString BookDocument::pageId(int pageIndex) const
{
    QDomElement el = pageElement(pageIndex);
    return el.isNull() ? QString() : el.attribute("id");
}

/**
 * @brief Возвращает id страницы-родителя.
 * @param pageIndex Индекс страницы (с нуля).
 * @return Значение атрибута parent или пустую строку для корневых страниц.
 */
QString BookDocument::pageParent(int pageIndex) const
{
    QDomElement el = pageElement(pageIndex);
    return el.isNull() ? QString() : el.attribute("parent");
}

/**
 * @brief Находит индекс последнего потомка страницы во всей иерархии.
 * @param pageIndex Индекс страницы-родителя (с нуля).
 * @return Индекс последней страницы поддерева; сам pageIndex, если потомков нет.
 */
int BookDocument::lastDescendantIndex(int pageIndex) const
{
    const int n = pageCount();
    if (pageIndex < 0 || pageIndex >= n)
        return pageIndex;

    // Собираем множество id всего поддерева: расширяем его, пока
    // встречаются страницы, чей родитель уже принадлежит поддереву
    QSet<QString> subtree;
    subtree.insert(pageId(pageIndex));
    bool grew = true;
    while (grew) {
        grew = false;
        for (int i = 0; i < n; ++i) {
            const QString id = pageId(i);
            if (id.isEmpty() || subtree.contains(id))
                continue;
            if (subtree.contains(pageParent(i))) {
                subtree.insert(id);
                grew = true;
            }
        }
    }

    int last = pageIndex;
    for (int i = pageIndex + 1; i < n; ++i)
        if (subtree.contains(pageId(i)))
            last = i;
    return last;
}

/**
 * @brief Возвращает имя шаблона страницы.
 * @param pageIndex Индекс страницы (с нуля).
 * @return Значение атрибута template; "default" — если атрибут или страница отсутствуют.
 */
QString BookDocument::pageTemplate(int pageIndex) const
{
    QDomElement el = pageElement(pageIndex);
    return el.isNull() ? QString("default") : el.attribute("template", "default");
}

/**
 * @brief Устанавливает шаблон страницы. Ничего не делает, если страница не найдена.
 * @param pageIndex Индекс страницы (с нуля).
 * @param tmpl      Имя шаблона.
 */
void BookDocument::setPageTemplate(int pageIndex, const QString &tmpl)
{
    QDomElement el = pageElement(pageIndex);
    if (!el.isNull()) {
        el.setAttribute("template", tmpl);
        m_modified = true;
        emit modified();
    }
}

/**
 * @brief Сериализует весь документ в строку XML.
 * @return XML-представление документа с отступами в 2 пробела.
 */
QString BookDocument::toXml() const
{
    return m_doc.toString(2);
}

/**
 * @brief Загружает документ напрямую из строки XML (без обращения к файлу).
 *        Документ всегда помечается как изменённый.
 * @param xml Строка с XML-содержимым документа.
 */
void BookDocument::fromXml(const QString &xml)
{
    m_doc.setContent(xml);
    m_modified = true;
    emit modified();
}

/**
 * @brief Возвращает элемент-контейнер <pages> корневого узла документа.
 * @return Элемент <pages>; может быть пустым (null), если структура повреждена.
 */
QDomElement BookDocument::pagesElement() const
{
    return m_doc.documentElement().firstChildElement("pages");
}

/**
 * @brief Возвращает элемент страницы по её индексу.
 * @param index Индекс страницы (с нуля).
 * @return Элемент <page> или пустой (null) элемент при выходе за границы.
 */
QDomElement BookDocument::pageElement(int index) const
{
    QDomNodeList nodes = pagesElement().childNodes();
    if (index < 0 || index >= nodes.count())
        return QDomElement();
    return nodes.at(index).toElement();
}

/**
 * @brief Генерирует следующий уникальный идентификатор страницы вида "p<номер>".
 * @return Строка-идентификатор, например "p1", "p2", ...
 */
QString BookDocument::generatePageId() const
{
    return QString("p%1").arg(m_nextPageId);
}

/**
 * @brief Гарантирует наличие обязательных элементов структуры документа:
 *        корня <book> и секций <meta>, <styles>, <pages>.
 *        Недостающие части создаются, существующие не затрагиваются.
 */
void BookDocument::ensureStructure()
{
    // Создаём корень, если документ пустой
    QDomElement root = m_doc.documentElement();
    if (root.isNull()) {
        root = m_doc.createElement("book");
        root.setAttribute("version", "1.0");
        m_doc.appendChild(root);
    }
    // Достраиваем отсутствующие обязательные секции
    if (root.firstChildElement("meta").isNull())
        root.appendChild(m_doc.createElement("meta"));
    if (root.firstChildElement("styles").isNull())
        root.appendChild(m_doc.createElement("styles"));
    if (root.firstChildElement("pages").isNull())
        root.appendChild(m_doc.createElement("pages"));
}
