#include "helpandmainFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDomDocument>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>

HelpManualFormat::HelpManualFormat(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

HelpManualFormat::~HelpManualFormat()
{
    shutdown();
}

QString HelpManualFormat::name() const { return "HelpManual"; }
QString HelpManualFormat::description() const { return "Help & Manual (EC Software) формат"; }
QString HelpManualFormat::version() const { return "1.0.0"; }
QString HelpManualFormat::author() const { return "BookCreateEdit"; }
QStringList HelpManualFormat::dependencies() const { return {}; }

void HelpManualFormat::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("Help & Manual формат инициализирован");
}

void HelpManualFormat::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool HelpManualFormat::isInitialized() const { return m_initialized; }

QString HelpManualFormat::formatName() const { return "Help & Manual"; }
QString HelpManualFormat::fileExtension() const { return "hmxp"; }
QStringList HelpManualFormat::supportedExtensions() const { return {"hmxp"}; }
QStringList HelpManualFormat::mimeTypes() const { return {"application/x-helpmanual"}; }

bool HelpManualFormat::canRead() const { return true; }
bool HelpManualFormat::canWrite() const { return true; }

bool HelpManualFormat::checkDependencies() const { return true; }

// ==================== READ ====================

bool HelpManualFormat::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFileInfo fi(filePath);
    QString projectDir;

    if (fi.isDir()) {
        projectDir = fi.absoluteFilePath();
    } else {
        projectDir = fi.absolutePath();
    }

    if (m_context)
        m_context->log("HelpManual: чтение проекта из " + projectDir);

    return readProject(projectDir, doc);
}

bool HelpManualFormat::readProject(const QString &projectDir, BookDocument *doc)
{
    QDir dir(projectDir);

    // Find .hmxp file
    QStringList hmxpFiles = dir.entryList({"*.hmxp"}, QDir::Files);
    if (hmxpFiles.isEmpty()) {
        if (m_context) m_context->log("HelpManual: нет .hmxp файла в " + projectDir);
        return false;
    }

    QString hmxpPath = dir.absoluteFilePath(hmxpFiles.first());

    QFile hmxpFile(hmxpPath);
    if (!hmxpFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QDomDocument hmxpDoc;
    QString errMsg; int errLine;
    if (!hmxpDoc.setContent(&hmxpFile, &errMsg, &errLine)) {
        if (m_context) m_context->log(QString("HelpManual: ошибка парсинга .hmxp: %1 (строка %2)").arg(errMsg).arg(errLine));
        return false;
    }
    hmxpFile.close();

    QDomElement root = hmxpDoc.documentElement();
    QDomElement config = root.firstChildElement("config");

    // Parse project metadata
    BookMeta meta;
    if (!config.isNull()) {
        QDomNodeList groups = config.elementsByTagName("config-group");
        for (int i = 0; i < groups.count(); ++i) {
            QDomElement group = groups.at(i).toElement();
            if (group.attribute("name") == "project") {
                QDomNodeList values = group.elementsByTagName("config-value");
                for (int j = 0; j < values.count(); ++j) {
                    QDomElement val = values.at(j).toElement();
                    QString name = val.attribute("name");
                    QString value = val.text();
                    if (name == "title") meta.title = value;
                    else if (name == "author") meta.author = value;
                    else if (name == "lcid") meta.language = value;
                }
                break;
            }
        }
    }
    meta.created = QDateTime::currentDateTime().toString(Qt::ISODate);
    doc->setMeta(meta);

    // Parse TOC - first check xi:include, then direct map
    QList<HmTopicRef> orderedTopics;

    // Method 1: xi:include in .hmxp
    QDomNodeList includes = root.elementsByTagName("xi:include");
    if (includes.count() > 0) {
        for (int i = 0; i < includes.count(); ++i) {
            QDomElement inc = includes.at(i).toElement();
            QString href = inc.attribute("href");
            if (!href.isEmpty()) {
                QString tocPath = dir.absoluteFilePath(href);
                QFile tocFile(tocPath);
                if (tocFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QDomDocument tocDoc;
                    if (tocDoc.setContent(&tocFile)) {
                        QDomElement tocRoot = tocDoc.documentElement();
                        collectTopicRefs(tocRoot, orderedTopics);
                    }
                }
            }
        }
    }

    // Method 2: Direct TOC file
    if (orderedTopics.isEmpty()) {
        QString tocPath = dir.absoluteFilePath("Maps/table_of_contents.xml");
        QFile tocFile(tocPath);
        if (tocFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QDomDocument tocDoc;
            if (tocDoc.setContent(&tocFile)) {
                QDomElement tocRoot = tocDoc.documentElement();
                collectTopicRefs(tocRoot, orderedTopics);
            }
        }
    }

    // Method 3: Scan Topics/ directory
    if (orderedTopics.isEmpty()) {
        QDir topicsDir(dir.absoluteFilePath("Topics"));
        QStringList topicFiles = topicsDir.entryList({"*.xml"}, QDir::Files, QDir::Name);
        for (const QString &f : topicFiles) {
            HmTopicRef info;
            QString baseName = QFileInfo(f).baseName();
            info.href = baseName;
            info.id = QString::number(qHash(baseName));
            info.caption = baseName;
            orderedTopics.append(info);
        }
    }

    // Read each topic as a page
    int pageCount = 0;
    for (int i = 0; i < orderedTopics.size(); ++i) {
        const HmTopicRef &topic = orderedTopics.at(i);
        QString topicPath = dir.absoluteFilePath("Topics/" + topic.href + ".xml");

        doc->addPage(pageCount > 0 ? pageCount - 1 : -1);
        int pageIndex = doc->pageCount() - 1;

        if (QFile::exists(topicPath)) {
            readTopicFile(topicPath, doc, pageIndex);
        } else {
            // Topic file not found, create empty page with caption
            QDomDocument pageDoc;
            pageDoc.setContent(QString("<page/>"));
            QDomElement pageRoot = pageDoc.documentElement();
            QDomElement h1 = pageDoc.createElement("h1");
            h1.appendChild(pageDoc.createTextNode(topic.caption.isEmpty() ? topic.href : topic.caption));
            pageRoot.appendChild(h1);
            doc->setPageXml(pageIndex, pageDoc.toString(-1));
        }
        pageCount++;
    }

    if (m_context)
        m_context->log(QString("HelpManual: загружено %1 топиков из %2").arg(pageCount).arg(projectDir));

    return true;
}

void HelpManualFormat::collectTopicRefs(const QDomElement &el, QList<HmTopicRef> &out)
{
    QDomNodeList children = el.childNodes();
    for (int i = 0; i < children.count(); ++i) {
        QDomElement child = children.at(i).toElement();
        if (child.tagName() == "topicref") {
            HmTopicRef info;
            info.href = child.attribute("href");
            info.id = child.attribute("id");
            info.build = child.attribute("build");
            info.icon = child.attribute("icon", "0").toInt();
            QDomElement caption = child.firstChildElement("caption");
            if (!caption.isNull())
                info.caption = caption.text();
            if (!info.href.isEmpty())
                out.append(info);
            collectTopicRefs(child, out);
        }
    }
}

bool HelpManualFormat::readTopicFile(const QString &topicPath, BookDocument *doc, int pageIndex)
{
    QFile file(topicPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QDomDocument topicDoc;
    QString errMsg; int errLine;
    if (!topicDoc.setContent(&file, &errMsg, &errLine)) {
        if (m_context) m_context->log(QString("HelpManual: ошибка парсинга %1: %2").arg(topicPath, errMsg));
        return false;
    }
    file.close();

    QDomElement topicRoot = topicDoc.documentElement();
    QDomElement titleEl = topicRoot.firstChildElement("title");
    QDomElement bodyEl = topicRoot.firstChildElement("body");
    QDomElement keywordsEl = topicRoot.firstChildElement("keywords");

    QString titleText;
    if (!titleEl.isNull())
        titleText = titleEl.text();

    QString keywords;
    if (!keywordsEl.isNull()) {
        QDomNodeList kws = keywordsEl.elementsByTagName("keyword");
        QStringList kwList;
        for (int i = 0; i < kws.count(); ++i)
            kwList.append(kws.at(i).toElement().text());
        keywords = kwList.join(", ");
    }

    QString bodyHtml;
    if (!bodyEl.isNull())
        bodyHtml = extractTopicText(bodyEl);

    // Build page XML in BookDocument format
    QDomDocument pageDoc;
    pageDoc.setContent(QString("<page/>"));
    QDomElement pageRoot = pageDoc.documentElement();

    if (!titleText.isEmpty()) {
        QDomElement h1 = pageDoc.createElement("h1");
        h1.appendChild(pageDoc.createTextNode(titleText));
        pageRoot.appendChild(h1);
    }

    if (!keywords.isEmpty()) {
        QDomElement kwEl = pageDoc.createElement("keywords");
        kwEl.appendChild(pageDoc.createTextNode(keywords));
        pageRoot.appendChild(kwEl);
    }

    QDomElement contentEl = pageDoc.createElement("content");
    contentEl.appendChild(pageDoc.createCDATASection(bodyHtml));
    pageRoot.appendChild(contentEl);

    doc->setPageXml(pageIndex, pageDoc.toString(-1));
    return true;
}

QString HelpManualFormat::extractTopicText(const QDomElement &body)
{
    QString result;
    QDomNodeList children = body.childNodes();
    for (int i = 0; i < children.count(); ++i) {
        QDomElement el = children.at(i).toElement();
        QString tag = el.tagName();

        if (tag == "header") {
            QDomNodeList headerChildren = el.childNodes();
            for (int j = 0; j < headerChildren.count(); ++j) {
                QDomElement hEl = headerChildren.at(j).toElement();
                if (hEl.tagName() == "para") {
                    QString text;
                    extractParaText(hEl, text);
                    result += "<h1>" + text.trimmed() + "</h1>\n";
                }
            }
        } else if (tag == "para") {
            // Check if para contains a table
            QDomElement tableEl = el.firstChildElement("table");
            if (!tableEl.isNull()) {
                extractTableText(tableEl, result);
            } else {
                QString text;
                extractParaText(el, text);
                if (!text.trimmed().isEmpty()) {
                    QString cls = el.attribute("styleclass");
                    if (cls.startsWith("Heading")) {
                        int level = 1;
                        QRegularExpression re("(\\d+)");
                        QRegularExpressionMatch m = re.match(cls);
                        if (m.hasMatch()) level = m.captured(1).toInt();
                        if (level < 1) level = 1;
                        if (level > 6) level = 6;
                        result += "<h" + QString::number(level) + ">" + text.trimmed() + "</h" + QString::number(level) + ">\n";
                    } else if (cls == "Code Example") {
                        result += "<pre>" + text.trimmed().toHtmlEscaped() + "</pre>\n";
                    } else {
                        result += "<p>" + text.trimmed() + "</p>\n";
                    }
                }
            }
        } else if (tag == "list") {
            extractListText(el, result);
        } else if (tag == "table") {
            extractTableText(el, result);
        } else if (tag == "image") {
            QString src = el.attribute("src");
            QString alt;
            QDomElement titleChild = el.firstChildElement("title");
            if (!titleChild.isNull()) alt = titleChild.text();
            result += "<p><img src=\"" + src + "\" alt=\"" + alt + "\"/></p>\n";
        }
    }
    return result;
}

void HelpManualFormat::extractParaText(const QDomElement &para, QString &out)
{
    QDomNodeList children = para.childNodes();
    for (int i = 0; i < children.count(); ++i) {
        QDomNode node = children.at(i);
        if (node.isText()) {
            out += node.toText().data();
        } else if (node.isElement()) {
            QDomElement el = node.toElement();
            QString tag = el.tagName();

            if (tag == "text") {
                QString text = el.text();
                QString style = el.attribute("style");

                bool bold = style.contains("font-weight:bold");
                bool italic = style.contains("font-style:italic");
                bool underline = style.contains("text-decoration:underline");

                if (bold) text = "<b>" + text + "</b>";
                if (italic) text = "<i>" + text + "</i>";
                if (underline) text = "<u>" + text + "</u>";

                out += text;
            } else if (tag == "tab") {
                out += "\t";
            } else if (tag == "br") {
                out += "<br/>";
            } else if (tag == "image") {
                QString src = el.attribute("src");
                out += "<img src=\"" + src + "\"/>";
            } else if (tag == "a" || tag == "link") {
                QString href = el.attribute("href");
                out += "<a href=\"" + href + "\">" + el.text() + "</a>";
            } else if (tag == "anchor") {
                QString id = el.attribute("id");
                out += "<a name=\"" + id + "\"></a>";
            } else if (tag == "include") {
                // Skip includes
            } else {
                out += el.text();
            }
        }
    }
}

void HelpManualFormat::extractListText(const QDomElement &list, QString &out)
{
    QString listType = list.attribute("listtype", "bullet");
    QString tag = (listType == "number" || listType == "decimal" || listType == "ordered") ? "ol" : "ul";
    out += "<" + tag + ">\n";

    QDomNodeList items = list.elementsByTagName("li");
    for (int i = 0; i < items.count(); ++i) {
        QDomElement li = items.at(i).toElement();
        QString text;
        QDomNodeList children = li.childNodes();
        for (int j = 0; j < children.count(); ++j) {
            QDomNode node = children.at(j);
            if (node.isText()) {
                text += node.toText().data();
            } else if (node.isElement()) {
                QDomElement el = node.toElement();
                if (el.tagName() == "text") {
                    QString t = el.text();
                    QString style = el.attribute("style");
                    if (style.contains("font-style:italic")) t = "<i>" + t + "</i>";
                    if (style.contains("font-weight:bold")) t = "<b>" + t + "</b>";
                    text += t;
                } else if (el.tagName() == "list") {
                    extractListText(el, text);
                } else if (el.tagName() == "link" || el.tagName() == "a") {
                    text += "<a href=\"" + el.attribute("href") + "\">" + el.text() + "</a>";
                } else {
                    text += el.text();
                }
            }
        }
        out += "  <li>" + text.trimmed() + "</li>\n";
    }
    out += "</" + tag + ">\n";
}

void HelpManualFormat::extractTableText(const QDomElement &table, QString &out)
{
    out += "<table>\n";

    QDomNodeList rows = table.elementsByTagName("tr");
    for (int i = 0; i < rows.count(); ++i) {
        QDomElement tr = rows.at(i).toElement();
        out += "  <tr>\n";
        QDomNodeList cells = tr.childNodes();
        for (int j = 0; j < cells.count(); ++j) {
            QDomElement td = cells.at(j).toElement();
            if (td.tagName() == "td") {
                int colspan = td.attribute("colspan", "1").toInt();
                out += "    <td" + (colspan > 1 ? QString(" colspan=\"%1\"").arg(colspan) : QString()) + ">";
                QDomNodeList cellChildren = td.childNodes();
                for (int k = 0; k < cellChildren.count(); ++k) {
                    QDomNode node = cellChildren.at(k);
                    if (node.isElement()) {
                        QDomElement el = node.toElement();
                        if (el.tagName() == "para") {
                            QString text;
                            extractParaText(el, text);
                            out += text.trimmed();
                        } else if (el.tagName() == "text") {
                            out += el.text();
                        } else if (el.tagName() == "image") {
                            out += "<img src=\"" + el.attribute("src") + "\"/>";
                        } else if (el.tagName() == "link" || el.tagName() == "a") {
                            out += "<a href=\"" + el.attribute("href") + "\">" + el.text() + "</a>";
                        } else {
                            out += el.text();
                        }
                    } else if (node.isText()) {
                        out += node.toText().data();
                    }
                }
                out += "</td>\n";
            }
        }
        out += "  </tr>\n";
    }
    out += "</table>\n";
}

// ==================== WRITE ====================

bool HelpManualFormat::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFileInfo fi(filePath);
    QString projectDir;

    if (fi.isDir()) {
        projectDir = fi.absoluteFilePath();
    } else {
        projectDir = fi.absolutePath();
    }

    QDir dir(projectDir);
    if (!dir.exists())
        QDir().mkpath(projectDir);

    if (m_context)
        m_context->log("HelpManual: запись в " + projectDir);

    return writeProject(doc, projectDir);
}

bool HelpManualFormat::writeProject(BookDocument *doc, const QString &projectDir)
{
    QDir dir(projectDir);
    BookMeta meta = doc->meta();

    // Find project name from existing .hmxp or use title
    QString projectName;
    QStringList existing = dir.entryList({"*.hmxp"}, QDir::Files);
    if (!existing.isEmpty()) {
        projectName = QFileInfo(existing.first()).baseName();
    } else {
        projectName = meta.title.isEmpty() ? "project" : meta.title;
        projectName.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
    }

    // Create directories
    QDir().mkpath(dir.absoluteFilePath("Topics"));
    QDir().mkpath(dir.absoluteFilePath("Maps"));
    QDir().mkpath(dir.absoluteFilePath("image"));

    // ===== Write .hmxp =====
    QString hmxpPath = dir.absoluteFilePath(projectName + ".hmxp");
    QFile hmxpFile(hmxpPath);
    if (!hmxpFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream ts(&hmxpFile);
    ts.setCodec("UTF-8");
    ts << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ts << "<helpproject xmlns:xi=\"http://www.w3.org/2001/XInclude\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"helpproject.xsd\" version=\"2.0\" isskin=\"false\" isrepository=\"false\">\n";
    ts << "  <config>\n";
    ts << "    <config-group name=\"project\">\n";
    ts << "      <config-value name=\"title\">" << meta.title.toHtmlEscaped() << "</config-value>\n";
    ts << "      <config-value name=\"author\">" << meta.author.toHtmlEscaped() << "</config-value>\n";
    ts << "      <config-value name=\"lcid\">" << meta.language.toHtmlEscaped() << "</config-value>\n";
    ts << "      <config-value name=\"charset\">UTF8_CHARSET</config-value>\n";
    ts << "      <config-value name=\"deffont\">Arial,10,0</config-value>\n";
    ts << "      <config-value name=\"searchpath\">.\\;.\\image\\</config-value>\n";
    ts << "      <config-value name=\"contents\">HTMLcover</config-value>\n";
    ts << "      <config-value name=\"make0include\">CHM,HTML,HXS,EBOOK</config-value>\n";
    ts << "      <config-value name=\"make0status\">*</config-value>\n";
    ts << "    </config-group>\n";
    ts << "    <styleclasses>\n";
    ts << "      <styleclass name=\"Normal\" shortcut=\"0\">\n";
    ts << "        <style-set media=\"screen\" type=\"text\">font-family:Arial; font-size:10pt;</style-set>\n";
    ts << "      </styleclass>\n";
    ts << "      <styleclass name=\"Heading1\" parentclass=\"Normal\" nextclass=\"Normal\" shortcut=\"0\">\n";
    ts << "        <style-set media=\"screen\" type=\"text\">font-size:16pt; font-weight:bold;</style-set>\n";
    ts << "      </styleclass>\n";
    ts << "      <styleclass name=\"Heading2\" parentclass=\"Normal\" nextclass=\"Normal\" shortcut=\"0\">\n";
    ts << "        <style-set media=\"screen\" type=\"text\">font-size:14pt; font-weight:bold;</style-set>\n";
    ts << "      </styleclass>\n";
    ts << "      <styleclass name=\"Heading3\" parentclass=\"Normal\" nextclass=\"Normal\" shortcut=\"0\">\n";
    ts << "        <style-set media=\"screen\" type=\"text\">font-size:12pt; font-weight:bold;</style-set>\n";
    ts << "      </styleclass>\n";
    ts << "      <styleclass name=\"Code Example\" nextclass=\"Normal\" shortcut=\"0\">\n";
    ts << "        <style-set media=\"screen\" type=\"para\">white-space:nowrap; page-break-inside:avoid;</style-set>\n";
    ts << "        <style-set media=\"screen\" type=\"text\">font-family:'Courier New'; font-size:8pt;</style-set>\n";
    ts << "      </styleclass>\n";
    ts << "      <styleclass name=\"Normal_body_text\" parentclass=\"Normal\" nextclass=\"Normal\" shortcut=\"0\">\n";
    ts << "        <style-set media=\"screen\" type=\"text\">font-family:'Times New Roman'; font-size:11pt;</style-set>\n";
    ts << "      </styleclass>\n";
    ts << "    </styleclasses>\n";
    ts << "  </config>\n";
    ts << "  <xi:include parse=\"xml\" href=\"Maps/table_of_contents.xml\"/>\n";
    ts << "  <project-files>\n";
    ts << "    <file href=\"Topics/*.xml\" build=\"ALL\" filetype=\"topic\"/>\n";
    ts << "    <file href=\"Baggage/*.*\" build=\"ALL\" filetype=\"baggage\"/>\n";
    ts << "  </project-files>\n";
    ts << "</helpproject>\n";
    hmxpFile.close();

    // ===== Write Topics =====
    QStringList topicFileNames;
    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageId = doc->pageId(i);
        QString pageXml = doc->pageXml(i);
        QDomDocument pageDoc;
        pageDoc.setContent(pageXml);
        QDomElement pageRoot = pageDoc.documentElement();

        QString title;
        QDomElement h1 = pageRoot.firstChildElement("h1");
        if (!h1.isNull())
            title = h1.text();

        QString keywords;
        QDomElement kwEl = pageRoot.firstChildElement("keywords");
        if (!kwEl.isNull())
            keywords = kwEl.text();

        QString content;
        QDomElement contentEl = pageRoot.firstChildElement("content");
        if (!contentEl.isNull())
            content = contentEl.text();

        QString topicFileName = "HTML." + pageId;
        topicFileNames.append(topicFileName);

        QString topicPath = dir.absoluteFilePath("Topics/" + topicFileName + ".xml");
        QFile topicFile(topicPath);
        if (!topicFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            continue;

        QTextStream tts(&topicFile);
        tts.setCodec("UTF-8");
        tts << generateTopicXml(title, keywords, content);
        topicFile.close();
    }

    // ===== Write TOC =====
    QString tocPath = dir.absoluteFilePath("Maps/table_of_contents.xml");
    QFile tocFile(tocPath);
    if (tocFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream tts(&tocFile);
        tts.setCodec("UTF-8");
        tts << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        tts << "<map xmlns:xsi=\"http://www.w3.org/2001/XInclude\"";

        if (!topicFileNames.isEmpty())
            tts << " defaulttopic=\"" << topicFileNames.first() << "\"";

        tts << ">\n";

        for (int i = 0; i < topicFileNames.size(); ++i) {
            QString pageXml = doc->pageXml(i);
            QDomDocument pageDoc;
            pageDoc.setContent(pageXml);
            QDomElement pageRoot = pageDoc.documentElement();
            QString title;
            QDomElement h1 = pageRoot.firstChildElement("h1");
            if (!h1.isNull())
                title = h1.text();
            if (title.isEmpty())
                title = topicFileNames.at(i);

            QString topicId = QString::number(qHash(topicFileNames.at(i)));
            tts << "  <topicref type=\"topic\" id=\"" << topicId
                << "\" build=\"ALL\" icon=\"0\" href=\"" << topicFileNames.at(i) << "\">\n";
            tts << "    <caption>" << title.toHtmlEscaped() << "</caption>\n";
            tts << "  </topicref>\n";
        }

        tts << "</map>\n";
        tocFile.close();
    }

    if (m_context)
        m_context->log(QString("HelpManual: записано %1 топиков в %2").arg(doc->pageCount()).arg(projectDir));

    return true;
}

QString HelpManualFormat::generateTopicXml(const QString &title, const QString &keywords, const QString &bodyHtml)
{
    QString xml;
    QTextStream ts(&xml);
    ts.setCodec("UTF-8");

    ts << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ts << "<?xml-stylesheet type=\"text/xsl\" href=\"../helpproject.xsl\" ?>\n";
    ts << "<topic template=\"Default\" status=\"Draft\" lasteditedby=\"BookCreateEdit\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"../helpproject.xsd\">\n";

    if (!title.isEmpty())
        ts << "  <title>" << title.toHtmlEscaped() << "</title>\n";

    if (!keywords.isEmpty()) {
        ts << "  <keywords>\n";
        QStringList kwList = keywords.split(",", QString::SkipEmptyParts);
        for (const QString &kw : kwList) {
            ts << "    <keyword>" << kw.trimmed().toHtmlEscaped() << "</keyword>\n";
        }
        ts << "  </keywords>\n";
    }

    ts << "  <body>\n";

    if (!bodyHtml.isEmpty()) {
        QDomDocument htmlDoc;
        QString htmlBody = "<root>" + bodyHtml + "</root>";
        if (htmlDoc.setContent(htmlBody)) {
            QDomElement root = htmlDoc.documentElement();
            QDomNodeList children = root.childNodes();
            for (int i = 0; i < children.count(); ++i) {
                QDomElement el = children.at(i).toElement();
                QString tag = el.tagName();

                if (tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
                    int level = tag.mid(1).toInt();
                    QString cls = "Heading" + QString::number(level);
                    ts << "    <header>\n";
                    ts << "      <para styleclass=\"" << cls << "\" style=\"text-align:left; text-indent:0px; margin-top:19px; line-height:1.00;\">\n";
                    ts << "        <text styleclass=\"" << cls << "\" style=\"font-size:" << (18 - level) << "pt; font-weight:bold;\">" << el.text().toHtmlEscaped() << "</text>\n";
                    ts << "      </para>\n";
                    ts << "    </header>\n";
                } else if (tag == "p") {
                    ts << "    <para styleclass=\"Normal_body_text\" style=\"text-align:justify; text-indent:0px; margin-top:0px; margin-right:38px; margin-bottom:0px; margin-left:76px; line-height:1.00;\">\n";
                    ts << "      <text styleclass=\"body_text\" style=\"font-family:'Times New Roman'; font-size:11pt;\">" << el.text().toHtmlEscaped() << "</text>\n";
                    ts << "    </para>\n";
                } else if (tag == "ul" || tag == "ol") {
                    ts << "    <list id=\"" << QString::number(qHash(el.text().left(20))) << "\" type=\"" << (tag == "ol" ? "ol" : "ul")
                       << "\" listtype=\"" << (tag == "ol" ? "number" : "bullet")
                       << "\" formatstring=\"&#183;\" levelreset=\"true\" legalstyle=\"false\" startfrom=\"1\" styleclass=\"List\" style=\"text-align:justify; margin-left:76px;\">\n";
                    QDomNodeList lis = el.elementsByTagName("li");
                    for (int j = 0; j < lis.count(); ++j) {
                        ts << "      <li styleclass=\"List\" style=\"text-align:justify; margin-left:76px;\">";
                        ts << "<text styleclass=\"List\" style=\"font-family:'Times New Roman'; font-size:11pt;\">" << lis.at(j).toElement().text().toHtmlEscaped() << "</text>";
                        ts << "</li>\n";
                    }
                    ts << "    </list>\n";
                } else if (tag == "table") {
                    QDomNodeList rows = el.elementsByTagName("tr");
                    int rowCount = rows.count();
                    int colCount = 0;
                    if (rowCount > 0) {
                        QDomNodeList firstRowCells = rows.at(0).toElement().elementsByTagName("td");
                        colCount = firstRowCells.count();
                    }
                    ts << "    <para styleclass=\"Normal_cmd\" style=\"text-align:justify;\">\n";
                    ts << "      <table rowcount=\"" << rowCount << "\" colcount=\"" << colCount
                       << "\" style=\"width:auto; cell-padding:1px; cell-spacing:1px; border-width:0px; background-color:#000000;\">\n";
                    for (int r = 0; r < rows.count(); ++r) {
                        ts << "        <tr style=\"vertical-align:top\">\n";
                        QDomNodeList cells = rows.at(r).toElement().elementsByTagName("td");
                        for (int c = 0; c < cells.count(); ++c) {
                            ts << "          <td style=\"background-color:#ffffff;\">\n";
                            ts << "            <para styleclass=\"\" style=\"text-align:justify; margin-left:76px;\">\n";
                            ts << "              <text styleclass=\"Normal\" style=\"vertical-align:baseline;\">" << cells.at(c).toElement().text().toHtmlEscaped() << "</text>\n";
                            ts << "            </para>\n";
                            ts << "          </td>\n";
                        }
                        ts << "        </tr>\n";
                    }
                    ts << "      </table>\n";
                    ts << "    </para>\n";
                } else if (tag == "pre") {
                    ts << "    <para styleclass=\"Code Example\" style=\"white-space:nowrap; page-break-inside:avoid;\">\n";
                    ts << "      <text styleclass=\"Code Example\" style=\"font-family:'Courier New'; font-size:8pt;\">" << el.text().toHtmlEscaped() << "</text>\n";
                    ts << "    </para>\n";
                } else if (tag == "img") {
                    ts << "    <para styleclass=\"Normal\" style=\"text-align:center;\">\n";
                    ts << "      <image src=\"" << el.attribute("src").toHtmlEscaped() << "\"/>\n";
                    ts << "    </para>\n";
                } else if (tag == "a") {
                    ts << "    <para styleclass=\"Normal_body_text\" style=\"text-align:justify; margin-left:76px;\">\n";
                    ts << "      <link type=\"weblink\" href=\"" << el.attribute("href").toHtmlEscaped() << "\">" << el.text().toHtmlEscaped() << "</link>\n";
                    ts << "    </para>\n";
                }
            }
        }
    }

    ts << "  </body>\n";
    ts << "</topic>\n";

    return xml;
}
