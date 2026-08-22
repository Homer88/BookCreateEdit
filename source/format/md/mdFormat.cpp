#include "mdFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QRegularExpression>
#include <QDebug>

MdPlugin::MdPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

MdPlugin::~MdPlugin()
{
    shutdown();
}

QString MdPlugin::name() const { return "Markdown"; }
QString MdPlugin::description() const { return "Markdown format plugin"; }
QString MdPlugin::version() const { return "1.0.0"; }
QString MdPlugin::author() const { return "BookCreateEdit"; }
QStringList MdPlugin::dependencies() const { return {}; }

void MdPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("Markdown: initialized");
}

void MdPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool MdPlugin::isInitialized() const { return m_initialized; }

QString MdPlugin::formatName() const { return "Markdown"; }
QString MdPlugin::fileExtension() const { return "md"; }
QStringList MdPlugin::supportedExtensions() const { return {"md", "markdown"}; }
QStringList MdPlugin::mimeTypes() const { return {"text/markdown"}; }

bool MdPlugin::canRead() const { return true; }
bool MdPlugin::canWrite() const { return true; }

static QString processInlineMarkdown(const QString &text)
{
    QString result = text;

    // Bold: **text** or __text__
    result.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");
    result.replace(QRegularExpression("__(.+?)__"), "<b>\\1</b>");

    // Italic: *text* or _text_
    result.replace(QRegularExpression("\\*(.+?)\\*"), "<i>\\1</i>");
    result.replace(QRegularExpression("_(.+?)_"), "<i>\\1</i>");

    // Strikethrough: ~~text~~
    result.replace(QRegularExpression("~~(.+?)~~"), "<s>\\1</s>");

    // Inline code: `code`
    result.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");

    // Links: [text](url)
    result.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)"), "<a href=\"\\2\">\\1</a>");

    // Images: ![alt](src)
    result.replace(QRegularExpression("!\\[([^\\]]*)\\]\\(([^)]+)\\)"), "<image src=\"\\2\" alt=\"\\1\"/>");

    return result;
}

static QString stripXmlTags(const QString &xml)
{
    QString result = xml;
    result.remove(QRegularExpression("<[^>]*>"));
    return result;
}

static QDomElement findOrCreateElement(QDomElement &parent, const QString &tag, QDomDocument &doc)
{
    QDomElement el = parent.firstChildElement(tag);
    if (el.isNull()) {
        el = doc.createElement(tag);
        parent.appendChild(el);
    }
    return el;
}

bool MdPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");
    QString text = ts.readAll();
    file.close();

    if (text.isEmpty())
        return false;

    QFileInfo fi(filePath);
    BookMeta meta;
    meta.title = fi.completeBaseName();
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    QStringList lines = text.split('\n');

    // Collect sections: split by headings (# or ##)
    struct Section {
        QString heading;
        int level;
        QStringList contentLines;
    };

    QList<Section> sections;
    Section current;
    current.level = 0;

    bool inCodeBlock = false;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);

        if (line.trimmed().startsWith("```")) {
            inCodeBlock = !inCodeBlock;
            if (!inCodeBlock)
                current.contentLines.append(line);
            else
                current.contentLines.append(line);
            continue;
        }

        if (inCodeBlock) {
            current.contentLines.append(line);
            continue;
        }

        QRegularExpression headingRe("^(#{1,6})\\s+(.+)$");
        QRegularExpressionMatch headingMatch = headingRe.match(line);

        if (headingMatch.hasMatch()) {
            int level = headingMatch.captured(1).length();
            QString heading = headingMatch.captured(2).trimmed();

            // Split on # or ## (level 1 or 2) only
            if (level <= 2) {
                if (!current.contentLines.isEmpty() || !current.heading.isEmpty()) {
                    sections.append(current);
                }
                current = Section();
                current.heading = heading;
                current.level = level;
                continue;
            }
        }

        current.contentLines.append(line);
    }

    if (!current.contentLines.isEmpty() || !current.heading.isEmpty()) {
        sections.append(current);
    }

    if (sections.isEmpty()) {
        Section s;
        s.contentLines = lines;
        sections.append(s);
    }

    // Convert each section to a BCE page
    for (int si = 0; si < sections.size(); ++si) {
        const Section &sec = sections.at(si);

        QDomDocument bceDoc;
        bceDoc.setContent(QString("<page><body></body></page>"));
        QDomElement root = bceDoc.documentElement();
        QDomElement body = root.firstChildElement("body");

        if (!sec.heading.isEmpty()) {
            QDomElement h1 = bceDoc.createElement("h1");
            h1.appendChild(bceDoc.createTextNode(sec.heading));
            body.appendChild(h1);
        }

        bool inCode = false;
        QString codeContent;
        bool inList = false;
        QString listType;
        QDomElement listEl;

        for (const QString &line : sec.contentLines) {
            QString trimmed = line.trimmed();

            // Fenced code block
            if (trimmed.startsWith("```")) {
                if (inCode) {
                    QDomElement pre = bceDoc.createElement("pre");
                    pre.appendChild(bceDoc.createTextNode(codeContent));
                    body.appendChild(pre);
                    codeContent.clear();
                    inCode = false;
                } else {
                    inCode = true;
                }
                continue;
            }

            if (inCode) {
                if (!codeContent.isEmpty())
                    codeContent += "\n";
                codeContent += line;
                continue;
            }

            // List items
            QRegularExpression ulRe("^[-*+]\\s+(.+)$");
            QRegularExpression olRe("^\\d+\\.\\s+(.+)$");
            QRegularExpressionMatch ulMatch = ulRe.match(trimmed);
            QRegularExpressionMatch olMatch = olRe.match(trimmed);

            if (ulMatch.hasMatch()) {
                if (!inList || listType != "ul") {
                    if (inList) body.appendChild(listEl);
                    inList = true;
                    listType = "ul";
                    listEl = bceDoc.createElement("list");
                    listEl.setAttribute("type", "ul");
                }
                QDomElement li = bceDoc.createElement("li");
                li.appendChild(bceDoc.createTextNode(processInlineMarkdown(ulMatch.captured(1))));
                listEl.appendChild(li);
                continue;
            }

            if (olMatch.hasMatch()) {
                if (!inList || listType != "ol") {
                    if (inList) body.appendChild(listEl);
                    inList = true;
                    listType = "ol";
                    listEl = bceDoc.createElement("list");
                    listEl.setAttribute("type", "ol");
                }
                QDomElement li = bceDoc.createElement("li");
                li.appendChild(bceDoc.createTextNode(processInlineMarkdown(olMatch.captured(1))));
                listEl.appendChild(li);
                continue;
            }

            // Close list if we hit non-list content
            if (inList && !trimmed.isEmpty()) {
                body.appendChild(listEl);
                inList = false;
            }

            // Empty line = paragraph separator
            if (trimmed.isEmpty()) {
                continue;
            }

            // Blockquote
            if (trimmed.startsWith("> ")) {
                QDomElement bq = bceDoc.createElement("blockquote");
                bq.appendChild(bceDoc.createTextNode(processInlineMarkdown(trimmed.mid(2))));
                body.appendChild(bq);
                continue;
            }

            // Horizontal rule
            if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
                QDomElement hr = bceDoc.createElement("hr");
                body.appendChild(hr);
                continue;
            }

            // Normal paragraph
            QDomElement p = bceDoc.createElement("p");
            p.appendChild(bceDoc.createTextNode(processInlineMarkdown(trimmed)));
            body.appendChild(p);
        }

        if (inList) {
            body.appendChild(listEl);
        }

        doc->addPage(si - 1);
        doc->setPageXml(doc->pageCount() - 1, bceDoc.toString(-1));
    }

    if (m_context)
        m_context->log(QString("Markdown: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool MdPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");

    BookMeta m = doc->meta();
    if (!m.title.isEmpty())
        ts << "# " << m.title << "\n\n";

    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageXml = doc->pageXml(i);
        QDomDocument bceDoc;
        if (bceDoc.setContent(pageXml)) {
            QDomElement root = bceDoc.documentElement();
            QDomElement body = root.firstChildElement("body");
            if (body.isNull())
                continue;

            QDomNodeList children = body.childNodes();
            for (int j = 0; j < children.count(); ++j) {
                QDomElement el = children.at(j).toElement();
                QString tag = el.tagName();

                if (tag == "h1") ts << "# " << el.text() << "\n\n";
                else if (tag == "h2") ts << "## " << el.text() << "\n\n";
                else if (tag == "h3") ts << "### " << el.text() << "\n\n";
                else if (tag == "h4") ts << "#### " << el.text() << "\n\n";
                else if (tag == "h5") ts << "##### " << el.text() << "\n\n";
                else if (tag == "h6") ts << "###### " << el.text() << "\n\n";
                else if (tag == "p") {
                    QString text = el.text();
                    ts << text << "\n\n";
                }
                else if (tag == "b" || tag == "strong")
                    ts << "**" << el.text() << "**\n\n";
                else if (tag == "i" || tag == "em")
                    ts << "*" << el.text() << "*\n\n";
                else if (tag == "u")
                    ts << "__" << el.text() << "__\n\n";
                else if (tag == "s" || tag == "del")
                    ts << "~~" << el.text() << "~~\n\n";
                else if (tag == "code")
                    ts << "`" << el.text() << "`\n\n";
                else if (tag == "pre")
                    ts << "```\n" << el.text() << "\n```\n\n";
                else if (tag == "a")
                    ts << "[" << el.text() << "](" << el.attribute("href") << ")\n\n";
                else if (tag == "image")
                    ts << "![" << el.attribute("alt") << "](" << el.attribute("src") << ")\n\n";
                else if (tag == "blockquote")
                    ts << "> " << el.text() << "\n\n";
                else if (tag == "list") {
                    QString listType = el.attribute("type", "ul");
                    QDomNodeList items = el.elementsByTagName("li");
                    for (int k = 0; k < items.count(); ++k) {
                        if (listType == "ol")
                            ts << (k + 1) << ". " << items.at(k).toElement().text() << "\n";
                        else
                            ts << "- " << items.at(k).toElement().text() << "\n";
                    }
                    ts << "\n";
                }
                else if (tag == "hr")
                    ts << "---\n\n";
                else
                    ts << el.text() << "\n\n";
            }
        }
    }

    ts.flush();
    file.close();

    if (m_context)
        m_context->log(QString("Markdown: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool MdPlugin::checkDependencies() const
{
    return true;
}
