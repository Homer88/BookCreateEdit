#include "fb2Format.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDebug>

static const QString FB2_NS = "http://www.gribuser.ru/xml/fictionbook/2.0";

Fb2Plugin::Fb2Plugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

Fb2Plugin::~Fb2Plugin()
{
    shutdown();
}

QString Fb2Plugin::name() const { return "FictionBook2"; }
QString Fb2Plugin::description() const { return "FictionBook 2.0 format plugin"; }
QString Fb2Plugin::version() const { return "1.0.0"; }
QString Fb2Plugin::author() const { return "BookCreateEdit"; }
QStringList Fb2Plugin::dependencies() const { return {}; }

void Fb2Plugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("FictionBook2: initialized");
}

void Fb2Plugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool Fb2Plugin::isInitialized() const { return m_initialized; }

QString Fb2Plugin::formatName() const { return "FictionBook2"; }
QString Fb2Plugin::fileExtension() const { return "fb2"; }
QStringList Fb2Plugin::supportedExtensions() const { return {"fb2"}; }
QStringList Fb2Plugin::mimeTypes() const { return {"application/x-fictionbook+xml"}; }

bool Fb2Plugin::canRead() const { return true; }
bool Fb2Plugin::canWrite() const { return true; }

static void convertFb2NodeToBce(const QDomNode &node, QDomElement &bceParent, QDomDocument &bceDoc)
{
    if (node.isText()) {
        bceParent.appendChild(bceDoc.createTextNode(node.nodeValue()));
        return;
    }

    if (!node.isElement())
        return;

    QDomElement el = node.toElement();
    QString tag = el.localName();
    QString ns = el.namespaceURI();

    if (tag == "p") {
        QDomElement p = bceDoc.createElement("p");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), p, bceDoc);
        bceParent.appendChild(p);
    } else if (tag == "strong" || tag == "b") {
        QDomElement b = bceDoc.createElement("b");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), b, bceDoc);
        bceParent.appendChild(b);
    } else if (tag == "emphasis" || tag == "i") {
        QDomElement i = bceDoc.createElement("i");
        QDomNodeList children = el.childNodes();
        for (int i2 = 0; i2 < children.count(); ++i2)
            convertFb2NodeToBce(children.at(i2), i, bceDoc);
        bceParent.appendChild(i);
    } else if (tag == "a") {
        QDomElement a = bceDoc.createElement("a");
        QString href = el.attribute("href", el.attributeNS(FB2_NS, "href", ""));
        a.setAttribute("href", href);
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), a, bceDoc);
        bceParent.appendChild(a);
    } else if (tag == "image") {
        QDomElement img = bceDoc.createElement("image");
        QString href = el.attribute("href", el.attributeNS(FB2_NS, "href", ""));
        if (href.startsWith("#"))
            href = href.mid(1);
        img.setAttribute("src", href);
        bceParent.appendChild(img);
    } else if (tag == "section") {
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), bceParent, bceDoc);
    } else if (tag == "title") {
        QDomNodeList paragraphs = el.childNodes();
        for (int i = 0; i < paragraphs.count(); ++i) {
            QDomNode child = paragraphs.at(i);
            if (child.isElement() && child.toElement().localName() == "p") {
                QDomElement h = bceDoc.createElement("h1");
                QDomNodeList runs = child.childNodes();
                for (int j = 0; j < runs.count(); ++j) {
                    QDomNode run = runs.at(j);
                    if (run.isText())
                        h.appendChild(bceDoc.createTextNode(run.nodeValue()));
                    else if (run.isElement()) {
                        QDomElement rEl = run.toElement();
                        if (rEl.localName() == "strong" || rEl.localName() == "b") {
                            QDomElement bEl = bceDoc.createElement("b");
                            bEl.appendChild(bceDoc.createTextNode(rEl.text()));
                            h.appendChild(bEl);
                        } else {
                            h.appendChild(bceDoc.createTextNode(rEl.text()));
                        }
                    }
                }
                bceParent.appendChild(h);
            }
        }
    } else if (tag == "subtitle") {
        QDomElement h = bceDoc.createElement("h2");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), h, bceDoc);
        bceParent.appendChild(h);
    } else if (tag == "epigraph") {
        QDomElement bq = bceDoc.createElement("blockquote");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), bq, bceDoc);
        bceParent.appendChild(bq);
    } else if (tag == "cite") {
        QDomElement bq = bceDoc.createElement("blockquote");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), bq, bceDoc);
        bceParent.appendChild(bq);
    } else if (tag == "poem") {
        QDomElement bq = bceDoc.createElement("blockquote");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), bq, bceDoc);
        bceParent.appendChild(bq);
    } else if (tag == "stanza") {
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            QDomNode child = children.at(i);
            if (child.isElement() && child.toElement().localName() == "v") {
                QDomElement p = bceDoc.createElement("p");
                p.appendChild(bceDoc.createTextNode(child.toElement().text()));
                bceParent.appendChild(p);
            }
        }
    } else if (tag == "empty-line") {
        QDomElement p = bceDoc.createElement("p");
        bceParent.appendChild(p);
    } else if (tag == "text-author") {
        QDomElement p = bceDoc.createElement("p");
        QDomElement iEl = bceDoc.createElement("i");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), iEl, bceDoc);
        p.appendChild(iEl);
        bceParent.appendChild(p);
    } else if (tag == "table") {
        QDomElement table = bceDoc.createElement("table");
        QDomNodeList rows = el.childNodes();
        for (int i = 0; i < rows.count(); ++i) {
            if (rows.at(i).isElement() && rows.at(i).toElement().localName() == "tr") {
                QDomElement tr = bceDoc.createElement("row");
                QDomNodeList cells = rows.at(i).childNodes();
                for (int j = 0; j < cells.count(); ++j) {
                    if (cells.at(j).isElement() && (cells.at(j).toElement().localName() == "td" || cells.at(j).toElement().localName() == "th")) {
                        QDomElement td = bceDoc.createElement("cell");
                        QDomNodeList cellChildren = cells.at(j).childNodes();
                        for (int k = 0; k < cellChildren.count(); ++k)
                            convertFb2NodeToBce(cellChildren.at(k), td, bceDoc);
                        tr.appendChild(td);
                    }
                }
                table.appendChild(tr);
            }
        }
        bceParent.appendChild(table);
    } else {
        // Generic: process children
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb2NodeToBce(children.at(i), bceParent, bceDoc);
    }
}

bool Fb2Plugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDomDocument xmlDoc;
    QString errorMsg;
    int errorLine;
    if (!xmlDoc.setContent(&file, &errorMsg, &errorLine)) {
        if (m_context)
            m_context->log(QString("FictionBook2: XML parse error at line %1: %2").arg(errorLine).arg(errorMsg));
        file.close();
        return false;
    }
    file.close();

    QDomElement root = xmlDoc.documentElement();

    // Extract metadata
    BookMeta meta;
    QDomElement desc = root.firstChildElement("description");
    if (!desc.isNull()) {
        QDomElement titleInfo = desc.firstChildElement("title-info");
        if (!titleInfo.isNull()) {
            meta.title = titleInfo.firstChildElement("book-title").text();
            QDomElement author = titleInfo.firstChildElement("author");
            if (!author.isNull()) {
                QString first = author.firstChildElement("first-name").text();
                QString middle = author.firstChildElement("middle-name").text();
                QString last = author.firstChildElement("last-name").text();
                meta.author = (first + " " + middle + " " + last).simplified();
            }
            meta.language = titleInfo.firstChildElement("lang").text();
        }
    }
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    // Process body elements
    QDomNodeList bodies = root.elementsByTagName("body");
    int pageIndex = 0;

    for (int b = 0; b < bodies.count(); ++b) {
        QDomElement body = bodies.at(b).toElement();

        QDomNodeList sections = body.childNodes();
        for (int s = 0; s < sections.count(); ++s) {
            QDomNode secNode = sections.at(s);
            if (!secNode.isElement())
                continue;

            QDomElement secEl = secNode.toElement();
            if (secEl.localName() == "section") {
                QDomDocument bceDoc;
                bceDoc.setContent(QString("<page><body></body></page>"));
                QDomElement bceRoot = bceDoc.documentElement();
                QDomElement bceBody = bceRoot.firstChildElement("body");

                QDomNodeList children = secEl.childNodes();
                for (int i = 0; i < children.count(); ++i)
                    convertFb2NodeToBce(children.at(i), bceBody, bceDoc);

                doc->addPage(pageIndex - 1);
                doc->setPageXml(doc->pageCount() - 1, bceDoc.toString(-1));
                pageIndex++;
            }
        }
    }

    if (doc->pageCount() == 0) {
        doc->addPage(-1);
        QDomDocument bceDoc;
        bceDoc.setContent(QString("<page><body><p></p></body></page>"));
        doc->setPageXml(0, bceDoc.toString(-1));
    }

    if (m_context)
        m_context->log(QString("FictionBook2: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

static void convertBceToFb2(const QDomNode &node, QDomElement &fb2Parent, QDomDocument &fb2Doc)
{
    if (node.isText()) {
        fb2Parent.appendChild(fb2Doc.createTextNode(node.nodeValue()));
        return;
    }

    if (!node.isElement())
        return;

    QDomElement el = node.toElement();
    QString tag = el.tagName();

    if (tag == "p") {
        QDomElement p = fb2Doc.createElementNS(FB2_NS, "p");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb2(children.at(i), p, fb2Doc);
        fb2Parent.appendChild(p);
    } else if (tag == "b" || tag == "strong") {
        QDomElement s = fb2Doc.createElementNS(FB2_NS, "strong");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb2(children.at(i), s, fb2Doc);
        fb2Parent.appendChild(s);
    } else if (tag == "i" || tag == "em") {
        QDomElement em = fb2Doc.createElementNS(FB2_NS, "emphasis");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb2(children.at(i), em, fb2Doc);
        fb2Parent.appendChild(em);
    } else if (tag == "a") {
        QDomElement a = fb2Doc.createElementNS(FB2_NS, "a");
        a.setAttributeNS(FB2_NS, "href", el.attribute("href"));
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb2(children.at(i), a, fb2Doc);
        fb2Parent.appendChild(a);
    } else if (tag == "image") {
        QDomElement img = fb2Doc.createElementNS(FB2_NS, "image");
        img.setAttributeNS(FB2_NS, "href", "#" + el.attribute("src"));
        fb2Parent.appendChild(img);
    } else if (tag == "h1" || tag == "h2" || tag == "h3") {
        QDomElement title = fb2Doc.createElementNS(FB2_NS, "title");
        QDomElement p = fb2Doc.createElementNS(FB2_NS, "p");
        p.appendChild(fb2Doc.createTextNode(el.text()));
        title.appendChild(p);
        fb2Parent.appendChild(title);
    } else if (tag == "blockquote") {
        QDomElement bq = fb2Doc.createElementNS(FB2_NS, "cite");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb2(children.at(i), bq, fb2Doc);
        fb2Parent.appendChild(bq);
    } else if (tag == "list") {
        QString type = el.attribute("type", "ul");
        QDomNodeList items = el.elementsByTagName("li");
        for (int i = 0; i < items.count(); ++i) {
            QDomElement p = fb2Doc.createElementNS(FB2_NS, "p");
            QString prefix = (type == "ol") ? QString::number(i + 1) + ". " : "- ";
            p.appendChild(fb2Doc.createTextNode(prefix + items.at(i).toElement().text()));
            fb2Parent.appendChild(p);
        }
    } else if (tag == "table") {
        QDomElement table = fb2Doc.createElementNS(FB2_NS, "table");
        QDomNodeList rows = el.childNodes();
        for (int i = 0; i < rows.count(); ++i) {
            if (rows.at(i).isElement()) {
                QDomElement tr = fb2Doc.createElementNS(FB2_NS, "tr");
                QDomNodeList cells = rows.at(i).childNodes();
                for (int j = 0; j < cells.count(); ++j) {
                    if (cells.at(j).isElement()) {
                        QDomElement td = fb2Doc.createElementNS(FB2_NS, "td");
                        td.appendChild(fb2Doc.createTextNode(cells.at(j).toElement().text()));
                        tr.appendChild(td);
                    }
                }
                table.appendChild(tr);
            }
        }
        fb2Parent.appendChild(table);
    } else if (tag == "code" || tag == "pre") {
        QDomElement p = fb2Doc.createElementNS(FB2_NS, "p");
        QDomElement s = fb2Doc.createElementNS(FB2_NS, "strong");
        s.appendChild(fb2Doc.createTextNode("[code] " + el.text()));
        p.appendChild(s);
        fb2Parent.appendChild(p);
    } else {
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb2(children.at(i), fb2Parent, fb2Doc);
    }
}

bool Fb2Plugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    BookMeta meta = doc->meta();

    QDomDocument fb2Doc;
    fb2Doc.setContent(QString("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                      "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" "
                      "xmlns:l=\"http://www.w3.org/1999/xlink\">\n</FictionBook>"));
    QDomElement root = fb2Doc.documentElement();

    // Description
    QDomElement desc = fb2Doc.createElementNS(FB2_NS, "description");
    QDomElement titleInfo = fb2Doc.createElementNS(FB2_NS, "title-info");
    QDomElement bookTitle = fb2Doc.createElementNS(FB2_NS, "book-title");
    bookTitle.appendChild(fb2Doc.createTextNode(meta.title));
    titleInfo.appendChild(bookTitle);

    if (!meta.author.isEmpty()) {
        QDomElement author = fb2Doc.createElementNS(FB2_NS, "author");
        QStringList nameParts = meta.author.split(" ", QString::SkipEmptyParts);
        if (!nameParts.isEmpty()) {
            QDomElement firstName = fb2Doc.createElementNS(FB2_NS, "first-name");
            firstName.appendChild(fb2Doc.createTextNode(nameParts.first()));
            author.appendChild(firstName);
        }
        if (nameParts.size() > 1) {
            QDomElement lastName = fb2Doc.createElementNS(FB2_NS, "last-name");
            lastName.appendChild(fb2Doc.createTextNode(nameParts.last()));
            author.appendChild(lastName);
        }
        titleInfo.appendChild(author);
    }

    if (!meta.language.isEmpty()) {
        QDomElement lang = fb2Doc.createElementNS(FB2_NS, "lang");
        lang.appendChild(fb2Doc.createTextNode(meta.language));
        titleInfo.appendChild(lang);
    }

    desc.appendChild(titleInfo);
    root.insertBefore(desc, root.firstChild());

    // Body
    QDomElement body = fb2Doc.createElementNS(FB2_NS, "body");

    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageXml = doc->pageXml(i);
        QDomDocument bceDoc;
        if (!bceDoc.setContent(pageXml))
            continue;

        QDomElement bceRoot = bceDoc.documentElement();
        QDomElement bceBody = bceRoot.firstChildElement("body");
        if (bceBody.isNull())
            continue;

        QDomElement section = fb2Doc.createElementNS(FB2_NS, "section");

        QDomNodeList children = bceBody.childNodes();
        for (int j = 0; j < children.count(); ++j)
            convertBceToFb2(children.at(j), section, fb2Doc);

        body.appendChild(section);
    }

    root.appendChild(body);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");
    ts << fb2Doc.toString(4);
    ts.flush();
    file.close();

    if (m_context)
        m_context->log(QString("FictionBook2: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool Fb2Plugin::checkDependencies() const
{
    return true;
}
