#include "BookDocument.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QUuid>

BookDocument::BookDocument(QObject *parent)
    : QObject(parent)
    , m_modified(false)
    , m_nextPageId(1)
{
}

BookDocument::~BookDocument()
{
}

void BookDocument::newDocument()
{
    m_doc.clear();
    m_filePath.clear();
    m_nextPageId = 1;
    m_modified = false;

    QDomProcessingInstruction pi = m_doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    m_doc.appendChild(pi);

    QDomElement root = m_doc.createElement("book");
    root.setAttribute("version", "1.0");
    m_doc.appendChild(root);

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

    QDomElement styles = m_doc.createElement("styles");
    root.appendChild(styles);

    QDomElement pages = m_doc.createElement("pages");
    root.appendChild(pages);

    addPage();
}

bool BookDocument::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QString errorMsg;
    int errorLine, errorCol;
    if (!m_doc.setContent(&file, &errorMsg, &errorLine, &errorCol)) {
        file.close();
        return false;
    }
    file.close();

    m_filePath = filePath;
    m_modified = false;

    QDomElement root = m_doc.documentElement();
    QDomElement pagesEl = root.firstChildElement("pages");
    m_nextPageId = pagesEl.childNodes().count() + 1;

    return true;
}

bool BookDocument::save(const QString &filePath)
{
    QString path = filePath.isEmpty() ? m_filePath : filePath;
    if (path.isEmpty())
        return false;

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

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << m_doc.toString(2);
    file.close();

    m_filePath = path;
    m_modified = false;
    return true;
}

bool BookDocument::saveAs(const QString &filePath)
{
    return save(filePath);
}

bool BookDocument::isModified() const
{
    return m_modified;
}

QString BookDocument::filePath() const
{
    return m_filePath;
}

BookMeta BookDocument::meta() const
{
    BookMeta m;
    QDomElement root = m_doc.documentElement();
    QDomElement meta = root.firstChildElement("meta");
    m.title = meta.firstChildElement("title").text();
    m.author = meta.firstChildElement("author").text();
    m.language = meta.firstChildElement("language").text();
    m.created = meta.firstChildElement("created").text();
    m.modified = meta.firstChildElement("modified").text();
    return m;
}

void BookDocument::setMeta(const BookMeta &meta)
{
    QDomElement root = m_doc.documentElement();
    QDomElement metaEl = root.firstChildElement("meta");

    auto setText = [this, &metaEl](const QString &tag, const QString &value) {
        QDomElement el = metaEl.firstChildElement(tag);
        if (el.isNull()) {
            el = m_doc.createElement(tag);
            metaEl.appendChild(el);
        }
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

QList<PageStyle> BookDocument::styles() const
{
    QList<PageStyle> list;
    QDomElement root = m_doc.documentElement();
    QDomElement stylesEl = root.firstChildElement("styles");
    QDomNodeList nodes = stylesEl.childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QDomElement el = nodes.at(i).toElement();
        if (el.tagName() == "style") {
            PageStyle s;
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

void BookDocument::addStyle(const PageStyle &style)
{
    QDomElement root = m_doc.documentElement();
    QDomElement stylesEl = root.firstChildElement("styles");

    QDomNodeList existing = stylesEl.elementsByTagName("style");
    for (int i = 0; i < existing.count(); ++i) {
        if (existing.at(i).toElement().attribute("id") == style.id) {
            stylesEl.removeChild(existing.at(i));
            break;
        }
    }

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

void BookDocument::removeStyle(const QString &id)
{
    QDomElement root = m_doc.documentElement();
    QDomElement stylesEl = root.firstChildElement("styles");
    QDomNodeList nodes = stylesEl.elementsByTagName("style");
    for (int i = 0; i < nodes.count(); ++i) {
        if (nodes.at(i).toElement().attribute("id") == id) {
            stylesEl.removeChild(nodes.at(i));
            m_modified = true;
            emit modified();
            return;
        }
    }
}

QString BookDocument::pageXml(int pageIndex) const
{
    QDomElement pageEl = pageElement(pageIndex);
    if (pageEl.isNull())
        return QString();
    return pageEl.firstChild().isNull() ? QString() : pageEl.firstChild().toElement().text();
}

void BookDocument::setPageXml(int pageIndex, const QString &xml)
{
    QDomElement pageEl = pageElement(pageIndex);
    if (pageEl.isNull())
        return;

    while (!pageEl.firstChild().isNull())
        pageEl.removeChild(pageEl.firstChild());

    QDomDocument fragDoc;
    if (fragDoc.setContent(xml)) {
        QDomNodeList children = fragDoc.documentElement().childNodes();
        for (int i = 0; i < children.count(); ++i)
            pageEl.appendChild(m_doc.importNode(children.at(i), true));
    } else {
        pageEl.appendChild(m_doc.createTextNode(xml));
    }

    m_modified = true;
    emit pageChanged(pageIndex);
    emit modified();
}

int BookDocument::pageCount() const
{
    return pagesElement().childNodes().count();
}

void BookDocument::addPage(int afterIndex)
{
    QDomElement pagesEl = pagesElement();
    QDomElement pageEl = m_doc.createElement("page");
    pageEl.setAttribute("id", generatePageId());
    pageEl.setAttribute("template", "default");

    QDomElement body = m_doc.createElement("body");
    QDomElement p = m_doc.createElement("p");
    p.appendChild(m_doc.createTextNode(""));
    body.appendChild(p);
    pageEl.appendChild(body);

    if (afterIndex < 0 || afterIndex >= pagesEl.childNodes().count()) {
        pagesEl.appendChild(pageEl);
    } else {
        QDomNode ref = pagesEl.childNodes().at(afterIndex);
        pagesEl.insertAfter(pageEl, ref);
    }

    m_nextPageId++;
    m_modified = true;
    emit pageAdded(afterIndex + 1);
    emit modified();
}

void BookDocument::removePage(int pageIndex)
{
    QDomElement pagesEl = pagesElement();
    QDomNodeList nodes = pagesEl.childNodes();
    if (pageIndex < 0 || pageIndex >= nodes.count())
        return;

    pagesEl.removeChild(nodes.at(pageIndex));
    m_modified = true;
    emit pageRemoved(pageIndex);
    emit modified();
}

void BookDocument::movePage(int fromIndex, int toIndex)
{
    QDomElement pagesEl = pagesElement();
    QDomNodeList nodes = pagesEl.childNodes();
    if (fromIndex < 0 || fromIndex >= nodes.count())
        return;
    if (toIndex < 0 || toIndex >= nodes.count())
        return;
    if (fromIndex == toIndex)
        return;

    QDomNode node = nodes.at(fromIndex).cloneNode(true);
    pagesEl.removeChild(nodes.at(fromIndex));

    QDomNodeList updatedNodes = pagesEl.childNodes();
    if (toIndex > fromIndex) {
        if (toIndex - 1 < updatedNodes.count())
            pagesEl.insertAfter(updatedNodes.at(toIndex - 1), node);
        else
            pagesEl.appendChild(node);
    } else {
        pagesEl.insertBefore(updatedNodes.at(toIndex), node);
    }

    m_modified = true;
    emit modified();
}

QString BookDocument::pageId(int pageIndex) const
{
    QDomElement el = pageElement(pageIndex);
    return el.isNull() ? QString() : el.attribute("id");
}

QString BookDocument::pageTemplate(int pageIndex) const
{
    QDomElement el = pageElement(pageIndex);
    return el.isNull() ? QString("default") : el.attribute("template", "default");
}

void BookDocument::setPageTemplate(int pageIndex, const QString &tmpl)
{
    QDomElement el = pageElement(pageIndex);
    if (!el.isNull()) {
        el.setAttribute("template", tmpl);
        m_modified = true;
        emit modified();
    }
}

QString BookDocument::toXml() const
{
    return m_doc.toString(2);
}

void BookDocument::fromXml(const QString &xml)
{
    m_doc.setContent(xml);
    m_modified = true;
    emit modified();
}

QDomElement BookDocument::pagesElement() const
{
    return m_doc.documentElement().firstChildElement("pages");
}

QDomElement BookDocument::pageElement(int index) const
{
    QDomNodeList nodes = pagesElement().childNodes();
    if (index < 0 || index >= nodes.count())
        return QDomElement();
    return nodes.at(index).toElement();
}

QString BookDocument::generatePageId() const
{
    return QString("p%1").arg(m_nextPageId);
}

void BookDocument::ensureStructure()
{
    QDomElement root = m_doc.documentElement();
    if (root.isNull()) {
        root = m_doc.createElement("book");
        root.setAttribute("version", "1.0");
        m_doc.appendChild(root);
    }
    if (root.firstChildElement("meta").isNull())
        root.appendChild(m_doc.createElement("meta"));
    if (root.firstChildElement("styles").isNull())
        root.appendChild(m_doc.createElement("styles"));
    if (root.firstChildElement("pages").isNull())
        root.appendChild(m_doc.createElement("pages"));
}
