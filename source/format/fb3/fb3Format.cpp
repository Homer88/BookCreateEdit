#include "fb3Format.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QRegularExpression>
#include <QDebug>

static const QString FB3_NS = "http://www.gribuser.ru/xml/fictionbook/3.0";

Fb3Plugin::Fb3Plugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

Fb3Plugin::~Fb3Plugin()
{
    shutdown();
}

QString Fb3Plugin::name() const { return "FictionBook3"; }
QString Fb3Plugin::description() const { return "FictionBook 3.0 format plugin"; }
QString Fb3Plugin::version() const { return "1.0.0"; }
QString Fb3Plugin::author() const { return "BookCreateEdit"; }
QStringList Fb3Plugin::dependencies() const { return {}; }

void Fb3Plugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("FictionBook3: initialized");
}

void Fb3Plugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool Fb3Plugin::isInitialized() const { return m_initialized; }

QString Fb3Plugin::formatName() const { return "FictionBook3"; }
QString Fb3Plugin::fileExtension() const { return "fb3"; }
QStringList Fb3Plugin::supportedExtensions() const { return {"fb3"}; }
QStringList Fb3Plugin::mimeTypes() const { return {"application/x-fictionbook3+xml"}; }

bool Fb3Plugin::canRead() const { return true; }
bool Fb3Plugin::canWrite() const { return true; }

static void convertFb3NodeToBce(const QDomNode &node, QDomElement &bceParent, QDomDocument &bceDoc)
{
    if (node.isText()) {
        bceParent.appendChild(bceDoc.createTextNode(node.nodeValue()));
        return;
    }

    if (!node.isElement())
        return;

    QDomElement el = node.toElement();
    QString tag = el.localName();

    if (tag == "p") {
        QDomElement p = bceDoc.createElement("p");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), p, bceDoc);
        bceParent.appendChild(p);
    } else if (tag == "strong" || tag == "b") {
        QDomElement b = bceDoc.createElement("b");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), b, bceDoc);
        bceParent.appendChild(b);
    } else if (tag == "emphasis" || tag == "i") {
        QDomElement ie = bceDoc.createElement("i");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), ie, bceDoc);
        bceParent.appendChild(ie);
    } else if (tag == "a") {
        QDomElement a = bceDoc.createElement("a");
        QString href = el.attribute("href", el.attributeNS(FB3_NS, "href", ""));
        a.setAttribute("href", href);
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), a, bceDoc);
        bceParent.appendChild(a);
    } else if (tag == "image") {
        QDomElement img = bceDoc.createElement("image");
        QString href = el.attribute("href", el.attributeNS(FB3_NS, "href", ""));
        if (href.startsWith("#"))
            href = href.mid(1);
        img.setAttribute("src", href);
        bceParent.appendChild(img);
    } else if (tag == "section") {
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), bceParent, bceDoc);
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
            convertFb3NodeToBce(children.at(i), h, bceDoc);
        bceParent.appendChild(h);
    } else if (tag == "epigraph") {
        QDomElement bq = bceDoc.createElement("blockquote");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), bq, bceDoc);
        bceParent.appendChild(bq);
    } else if (tag == "cite") {
        QDomElement bq = bceDoc.createElement("blockquote");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), bq, bceDoc);
        bceParent.appendChild(bq);
    } else if (tag == "poem") {
        QDomElement bq = bceDoc.createElement("blockquote");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), bq, bceDoc);
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
            convertFb3NodeToBce(children.at(i), iEl, bceDoc);
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
                            convertFb3NodeToBce(cellChildren.at(k), td, bceDoc);
                        tr.appendChild(td);
                    }
                }
                table.appendChild(tr);
            }
        }
        bceParent.appendChild(table);
    } else {
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertFb3NodeToBce(children.at(i), bceParent, bceDoc);
    }
}

bool Fb3Plugin::read(const QString &filePath, BookDocument *doc)
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
            m_context->log(QString("FictionBook3: XML parse error at line %1: %2").arg(errorLine).arg(errorMsg));
        file.close();
        return false;
    }
    file.close();

    QDomElement root = xmlDoc.documentElement();

    // Extract metadata
    BookMeta meta;
    QDomNodeList titleNodes = root.elementsByTagNameNS(FB3_NS, "title");
    if (!titleNodes.isEmpty())
        meta.title = titleNodes.at(0).toElement().text();
    QDomNodeList authorNodes = root.elementsByTagNameNS(FB3_NS, "author");
    if (!authorNodes.isEmpty()) {
        QDomElement authorEl = authorNodes.at(0).toElement();
        QDomNodeList firstNodes = authorEl.elementsByTagNameNS(FB3_NS, "first-name");
        QDomNodeList middleNodes = authorEl.elementsByTagNameNS(FB3_NS, "middle-name");
        QDomNodeList lastNodes = authorEl.elementsByTagNameNS(FB3_NS, "last-name");
        QString first = firstNodes.isEmpty() ? QString() : firstNodes.at(0).toElement().text();
        QString middle = middleNodes.isEmpty() ? QString() : middleNodes.at(0).toElement().text();
        QString last = lastNodes.isEmpty() ? QString() : lastNodes.at(0).toElement().text();
        meta.author = (first + " " + middle + " " + last).simplified();
    }
    QDomNodeList langNodes = root.elementsByTagNameNS(FB3_NS, "lang");
    if (!langNodes.isEmpty())
        meta.language = langNodes.at(0).toElement().text();
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    // Process body elements
    QDomNodeList bodies = root.elementsByTagNameNS(FB3_NS, "body");
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
                    convertFb3NodeToBce(children.at(i), bceBody, bceDoc);

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
        m_context->log(QString("FictionBook3: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

static void convertBceToFb3(const QDomNode &node, QDomElement &fb3Parent, QDomDocument &fb3Doc)
{
    if (node.isText()) {
        fb3Parent.appendChild(fb3Doc.createTextNode(node.nodeValue()));
        return;
    }

    if (!node.isElement())
        return;

    QDomElement el = node.toElement();
    QString tag = el.tagName();

    if (tag == "p") {
        QDomElement p = fb3Doc.createElementNS(FB3_NS, "p");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb3(children.at(i), p, fb3Doc);
        fb3Parent.appendChild(p);
    } else if (tag == "b" || tag == "strong") {
        QDomElement s = fb3Doc.createElementNS(FB3_NS, "strong");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb3(children.at(i), s, fb3Doc);
        fb3Parent.appendChild(s);
    } else if (tag == "i" || tag == "em") {
        QDomElement em = fb3Doc.createElementNS(FB3_NS, "emphasis");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb3(children.at(i), em, fb3Doc);
        fb3Parent.appendChild(em);
    } else if (tag == "a") {
        QDomElement a = fb3Doc.createElementNS(FB3_NS, "a");
        a.setAttributeNS(FB3_NS, "href", el.attribute("href"));
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb3(children.at(i), a, fb3Doc);
        fb3Parent.appendChild(a);
    } else if (tag == "image") {
        QDomElement img = fb3Doc.createElementNS(FB3_NS, "image");
        img.setAttributeNS(FB3_NS, "href", "#" + el.attribute("src"));
        fb3Parent.appendChild(img);
    } else if (tag == "h1" || tag == "h2" || tag == "h3") {
        QDomElement title = fb3Doc.createElementNS(FB3_NS, "title");
        QDomElement p = fb3Doc.createElementNS(FB3_NS, "p");
        p.appendChild(fb3Doc.createTextNode(el.text()));
        title.appendChild(p);
        fb3Parent.appendChild(title);
    } else if (tag == "blockquote") {
        QDomElement bq = fb3Doc.createElementNS(FB3_NS, "cite");
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb3(children.at(i), bq, fb3Doc);
        fb3Parent.appendChild(bq);
    } else if (tag == "list") {
        QDomNodeList items = el.elementsByTagName("li");
        for (int i = 0; i < items.count(); ++i) {
            QDomElement p = fb3Doc.createElementNS(FB3_NS, "p");
            p.appendChild(fb3Doc.createTextNode(items.at(i).toElement().text()));
            fb3Parent.appendChild(p);
        }
    } else if (tag == "code" || tag == "pre") {
        QDomElement p = fb3Doc.createElementNS(FB3_NS, "p");
        QDomElement s = fb3Doc.createElementNS(FB3_NS, "strong");
        s.appendChild(fb3Doc.createTextNode("[code] " + el.text()));
        p.appendChild(s);
        fb3Parent.appendChild(p);
    } else {
        QDomNodeList children = el.childNodes();
        for (int i = 0; i < children.count(); ++i)
            convertBceToFb3(children.at(i), fb3Parent, fb3Doc);
    }
}

bool Fb3Plugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    BookMeta meta = doc->meta();

    QDomDocument fb3Doc;
    fb3Doc.setContent(QString("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                      "<fb3 xmlns=\"http://www.gribuser.ru/xml/fictionbook/3.0\"\n"
                      "     xmlns:l=\"http://www.w3.org/1999/xlink\">\n</fb3>"));
    QDomElement root = fb3Doc.documentElement();

    if (!meta.title.isEmpty()) {
        QDomElement titleEl = fb3Doc.createElementNS(FB3_NS, "title");
        titleEl.appendChild(fb3Doc.createTextNode(meta.title));
        root.insertBefore(titleEl, root.firstChild());
    }

    if (!meta.author.isEmpty()) {
        QDomElement authorEl = fb3Doc.createElementNS(FB3_NS, "author");
        QStringList nameParts = meta.author.split(" ", QString::SkipEmptyParts);
        if (!nameParts.isEmpty()) {
            QDomElement first = fb3Doc.createElementNS(FB3_NS, "first-name");
            first.appendChild(fb3Doc.createTextNode(nameParts.first()));
            authorEl.appendChild(first);
        }
        if (nameParts.size() > 1) {
            QDomElement last = fb3Doc.createElementNS(FB3_NS, "last-name");
            last.appendChild(fb3Doc.createTextNode(nameParts.last()));
            authorEl.appendChild(last);
        }
        root.insertBefore(authorEl, root.firstChild());
    }

    if (!meta.language.isEmpty()) {
        QDomElement langEl = fb3Doc.createElementNS(FB3_NS, "lang");
        langEl.appendChild(fb3Doc.createTextNode(meta.language));
        root.insertBefore(langEl, root.firstChild());
    }

    QDomElement body = fb3Doc.createElementNS(FB3_NS, "body");

    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageXml = doc->pageXml(i);
        QDomDocument bceDoc;
        if (!bceDoc.setContent(pageXml))
            continue;

        QDomElement bceRoot = bceDoc.documentElement();
        QDomElement bceBody = bceRoot.firstChildElement("body");
        if (bceBody.isNull())
            continue;

        QDomElement section = fb3Doc.createElementNS(FB3_NS, "section");

        QDomNodeList children = bceBody.childNodes();
        for (int j = 0; j < children.count(); ++j)
            convertBceToFb3(children.at(j), section, fb3Doc);

        body.appendChild(section);
    }

    root.appendChild(body);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");
    ts << fb3Doc.toString(4);
    ts.flush();
    file.close();

    if (m_context)
        m_context->log(QString("FictionBook3: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool Fb3Plugin::checkDependencies() const
{
    return true;
}
