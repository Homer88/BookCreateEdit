#include "docxFormat.h"
#include "BookDocument.h"
#include "../ziputil.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDebug>
#include <QtEndian>
#include <QMap>

// Minimal ZIP writer for DOCX
class SimpleZipWriter
{
public:
    struct ZipEntry {
        QString name;
        QByteArray data;
        quint32 crc;
        quint32 offset;
    };

    bool addFile(const QString &name, const QByteArray &data)
    {
        ZipEntry entry;
        entry.name = name;
        entry.data = data;
        entry.crc = computeCrc(data);
        m_entries.append(entry);
        return true;
    }

    QByteArray toArray()
    {
        QByteArray localHeaders;
        QByteArray centralDir;
        quint32 offset = 0;

        for (int i = 0; i < m_entries.size(); ++i) {
            ZipEntry &e = m_entries[i];
            e.offset = offset;

            QByteArray nameBytes = e.name.toUtf8();
            // Local file header
            localHeaders.append("PK\x03\x04", 4);
            localHeaders.append(qToLittleEndian<quint16>(20), 2);
            localHeaders.append(qToLittleEndian<quint16>(0), 2);
            localHeaders.append(qToLittleEndian<quint16>(0), 2);
            localHeaders.append(qToLittleEndian<quint16>(0), 2);
            localHeaders.append(qToLittleEndian<quint16>(0), 2);
            localHeaders.append(qToLittleEndian<quint32>(e.crc), 4);
            localHeaders.append(qToLittleEndian<quint32>(e.data.size()), 4);
            localHeaders.append(qToLittleEndian<quint32>(e.data.size()), 4);
            localHeaders.append(qToLittleEndian<quint16>(nameBytes.size()), 2);
            localHeaders.append(qToLittleEndian<quint16>(0), 2);
            localHeaders.append(nameBytes);
            localHeaders.append(e.data);

            // Central directory
            centralDir.append("PK\x01\x02", 4);
            centralDir.append(qToLittleEndian<quint16>(20), 2);
            centralDir.append(qToLittleEndian<quint16>(20), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint32>(e.crc), 4);
            centralDir.append(qToLittleEndian<quint32>(e.data.size()), 4);
            centralDir.append(qToLittleEndian<quint32>(e.data.size()), 4);
            centralDir.append(qToLittleEndian<quint16>(nameBytes.size()), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint16>(0), 2);
            centralDir.append(qToLittleEndian<quint32>(0), 4);
            centralDir.append(qToLittleEndian<quint32>(e.offset), 4);
            centralDir.append(nameBytes);

            offset += 4 + 26 + nameBytes.size() + e.data.size();
        }

        quint32 cdOffset = offset;
        quint32 cdSize = centralDir.size();

        QByteArray endRecord;
        endRecord.append("PK\x05\x06", 4);
        endRecord.append(qToLittleEndian<quint16>(0), 2);
        endRecord.append(qToLittleEndian<quint16>(0), 2);
        endRecord.append(qToLittleEndian<quint16>(m_entries.size()), 2);
        endRecord.append(qToLittleEndian<quint16>(m_entries.size()), 2);
        endRecord.append(qToLittleEndian<quint32>(cdSize), 4);
        endRecord.append(qToLittleEndian<quint32>(cdOffset), 4);
        endRecord.append(qToLittleEndian<quint16>(0), 2);

        return localHeaders + centralDir + endRecord;
    }

private:
    static quint32 computeCrc(const QByteArray &data)
    {
        quint32 crc = 0xFFFFFFFF;
        const uchar *buf = reinterpret_cast<const uchar*>(data.constData());
        for (int i = 0; i < data.size(); ++i) {
            crc ^= buf[i];
            for (int j = 0; j < 8; ++j) {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320;
                else
                    crc >>= 1;
            }
        }
        return ~crc;
    }

    QList<ZipEntry> m_entries;
};

DocxPlugin::DocxPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

DocxPlugin::~DocxPlugin()
{
    shutdown();
}

QString DocxPlugin::name() const { return "DOCX"; }
QString DocxPlugin::description() const { return "Microsoft Word Open XML format plugin"; }
QString DocxPlugin::version() const { return "1.0.0"; }
QString DocxPlugin::author() const { return "BookCreateEdit"; }
QStringList DocxPlugin::dependencies() const { return {}; }

void DocxPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("DOCX: initialized");
}

void DocxPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool DocxPlugin::isInitialized() const { return m_initialized; }

QString DocxPlugin::formatName() const { return "DOCX"; }
QString DocxPlugin::fileExtension() const { return "docx"; }
QStringList DocxPlugin::supportedExtensions() const { return {"docx"}; }
QStringList DocxPlugin::mimeTypes() const { return {"application/vnd.openxmlformats-officedocument.wordprocessingml.document"}; }

bool DocxPlugin::canRead() const { return true; }
bool DocxPlugin::canWrite() const { return true; }

bool DocxPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray zipData = file.readAll();
    file.close();

    // Читаем ВСЕ записи архива (stored + deflate) через общий ZipUtil.
    // Настоящие DOCX-файлы сжаты deflate — раньше они не импортировались.
    QMap<QString, QByteArray> files;
    const auto entries = ZipUtil::readZip(zipData);
    for (const ZipUtil::ZipEntry &e : entries)
        files.insert(e.name, e.data);

    // Find document.xml
    QByteArray docXmlData = files.value("word/document.xml");

    if (docXmlData.isEmpty()) {
        if (m_context)
            m_context->log("DOCX: word/document.xml not found");
        return false;
    }

    QDomDocument xmlDoc;
    QString errorMsg;
    int errorLine;
    if (!xmlDoc.setContent(docXmlData, &errorMsg, &errorLine)) {
        if (m_context)
            m_context->log(QString("DOCX: XML parse error at line %1: %2").arg(errorLine).arg(errorMsg));
        return false;
    }

    QDomElement root = xmlDoc.documentElement();

    // Extract document properties (meta)
    BookMeta meta;
    // Try to read from app.xml or core.xml
    if (files.contains("docProps/core.xml")) {
        QByteArray coreData = files.value("docProps/core.xml");
        QDomDocument coreDoc;
        if (coreDoc.setContent(coreData)) {
            QDomElement coreRoot = coreDoc.documentElement();
            QDomElement dcTitle = coreRoot.firstChildElement("dc:title");
            if (!dcTitle.isNull())
                meta.title = dcTitle.text();
            QDomElement dcCreator = coreRoot.firstChildElement("dc:creator");
            if (!dcCreator.isNull())
                meta.author = dcCreator.text();
        }
    }

    if (meta.title.isEmpty()) {
        QFileInfo fi(filePath);
        meta.title = fi.completeBaseName();
    }
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    // Parse document.xml for paragraphs
    // Find w:body
    QDomElement body = root.firstChildElement("w:body");
    if (body.isNull())
        body = root.firstChildElement("body");

    if (!body.isNull()) {
        QDomNodeList children = body.childNodes();
        QString currentParagraph;
        bool hasContent = false;

        for (int i = 0; i < children.count(); ++i) {
            QDomNode child = children.at(i);
            if (!child.isElement())
                continue;

            QDomElement childEl = child.toElement();
            QString localName = childEl.localName();
            QString tagName = childEl.tagName();

            // Page break
            if (localName == "sectPr" || tagName == "w:sectPr") {
                if (hasContent) {
                    doc->addPage(doc->pageCount() - 1);
                    doc->setPageXml(doc->pageCount() - 1,
                        QString("<page><body>%1</body></page>").arg(currentParagraph));
                    currentParagraph.clear();
                    hasContent = false;
                }
                continue;
            }

            if (localName == "p" || tagName == "w:p") {
                // Determine paragraph style
                QString styleId;
                QDomElement pPr = childEl.firstChildElement("w:pPr");
                if (!pPr.isNull()) {
                    QDomElement pStyle = pPr.firstChildElement("w:pStyle");
                    if (!pStyle.isNull())
                        styleId = pStyle.attribute("w:val", pStyle.attribute("val", ""));
                }

                // Extract text with formatting
                QString paraContent;
                QDomNodeList runs = childEl.childNodes();
                for (int j = 0; j < runs.count(); ++j) {
                    QDomNode runNode = runs.at(j);
                    if (!runNode.isElement())
                        continue;

                    QDomElement runEl = runNode.toElement();
                    QString runLocalName = runEl.localName();
                    QString runTagName = runEl.tagName();

                    if (runLocalName == "r" || runTagName == "w:r") {
                        // Check formatting
                        bool isBold = false;
                        bool isItalic = false;
                        bool isUnderline = false;

                        QDomElement rPr = runEl.firstChildElement("w:rPr");
                        if (!rPr.isNull()) {
                            isBold = !rPr.firstChildElement("w:b").isNull();
                            isItalic = !rPr.firstChildElement("w:i").isNull();
                            isUnderline = !rPr.firstChildElement("w:u").isNull();
                        }

                        // Get text
                        QDomElement tEl = runEl.firstChildElement("w:t");
                        if (tEl.isNull())
                            tEl = runEl.firstChildElement("t");
                        if (!tEl.isNull()) {
                            QString text = tEl.text();
                            if (isBold) text = "<b>" + text + "</b>";
                            if (isItalic) text = "<i>" + text + "</i>";
                            if (isUnderline) text = "<u>" + text + "</u>";
                            paraContent += text;
                        }

                        // Check for tab
                        QDomElement tabEl = runEl.firstChildElement("w:tab");
                        if (!tabEl.isNull())
                            paraContent += "\t";
                    }
                }

                if (!paraContent.isEmpty()) {
                    // Map style to heading
                    QString tag = "p";
                    if (styleId.contains("Heading1") || styleId == "1")
                        tag = "h1";
                    else if (styleId.contains("Heading2") || styleId == "2")
                        tag = "h2";
                    else if (styleId.contains("Heading3") || styleId == "3")
                        tag = "h3";
                    else if (styleId.contains("Heading4") || styleId == "4")
                        tag = "h4";

                    currentParagraph += "<" + tag + ">" + paraContent + "</" + tag + ">\n";
                    hasContent = true;
                }
            }
        }

        // Add last page
        if (hasContent) {
            doc->addPage(doc->pageCount() - 1);
            doc->setPageXml(doc->pageCount() - 1,
                QString("<page><body>%1</body></page>").arg(currentParagraph));
        }
    }

    if (doc->pageCount() == 0) {
        doc->addPage(-1);
        QDomDocument bceDoc;
        bceDoc.setContent(QString("<page><body><p></p></body></page>"));
        doc->setPageXml(0, bceDoc.toString(-1));
    }

    if (m_context)
        m_context->log(QString("DOCX: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

static QString xmlEscaped(const QString &text)
{
    return text.toHtmlEscaped();
}

bool DocxPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    BookMeta meta = doc->meta();

    SimpleZipWriter zip;

    // [Content_Types].xml
    QString contentTypes = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
        "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
        "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
        "  <Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>\n"
        "  <Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>\n"
        "  <Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>\n"
        "</Types>";
    zip.addFile("[Content_Types].xml", contentTypes.toUtf8());

    // _rels/.rels
    QString rels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>\n"
        "  <Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>\n"
        "</Relationships>";
    zip.addFile("_rels/.rels", rels.toUtf8());

    // docProps/core.xml
    QString coreXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
        "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
        "  <dc:title>" + xmlEscaped(meta.title) + "</dc:title>\n"
        "  <dc:creator>" + xmlEscaped(meta.author) + "</dc:creator>\n"
        "  <dc:language>" + xmlEscaped(meta.language) + "</dc:language>\n"
        "</cp:coreProperties>";
    zip.addFile("docProps/core.xml", coreXml.toUtf8());

    // docProps/app.xml
    QString appXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\">\n"
        "  <Application>BookCreateEdit</Application>\n"
        "</Properties>";
    zip.addFile("docProps/app.xml", appXml.toUtf8());

    // word/_rels/document.xml.rels
    QString docRels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        "</Relationships>";
    zip.addFile("word/_rels/document.xml.rels", docRels.toUtf8());

    // word/document.xml
    QString documentXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
        "  <w:body>\n";

    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageXml = doc->pageXml(i);
        QDomDocument bceDoc;
        if (!bceDoc.setContent(pageXml))
            continue;

        QDomElement bceRoot = bceDoc.documentElement();
        QDomElement bceBody = bceRoot.firstChildElement("body");
        if (bceBody.isNull())
            continue;

        QDomNodeList children = bceBody.childNodes();
        for (int j = 0; j < children.count(); ++j) {
            QDomElement el = children.at(j).toElement();
            QString tag = el.tagName();

            // Determine style
            QString styleAttr = "";
            if (tag == "h1") styleAttr = " w:val=\"Heading1\"";
            else if (tag == "h2") styleAttr = " w:val=\"Heading2\"";
            else if (tag == "h3") styleAttr = " w:val=\"Heading3\"";
            else if (tag == "h4") styleAttr = " w:val=\"Heading4\"";

            documentXml += "    <w:p>\n";
            if (!styleAttr.isEmpty()) {
                documentXml += "      <w:pPr><w:pStyle" + styleAttr + "/></w:pPr>\n";
            }

            // Extract inline content from the element
            QDomNodeList children2 = el.childNodes();
            for (int k = 0; k < children2.count(); ++k) {
                QDomNode child = children2.at(k);
                if (child.isText()) {
                    QString text = child.nodeValue();
                    if (!text.isEmpty()) {
                        documentXml += "      <w:r><w:t>" + xmlEscaped(text) + "</w:t></w:r>\n";
                    }
                } else if (child.isElement()) {
                    QDomElement childEl = child.toElement();
                    QString childTag = childEl.tagName();
                    QString text = childEl.text();

                    QString rPr = "";
                    if (childTag == "b" || childTag == "strong")
                        rPr = "<w:rPr><w:b/></w:rPr>";
                    else if (childTag == "i" || childTag == "em")
                        rPr = "<w:rPr><w:i/></w:rPr>";
                    else if (childTag == "u")
                        rPr = "<w:rPr><w:u w:val=\"single\"/></w:rPr>";

                    if (!text.isEmpty()) {
                        documentXml += "      <w:r>" + rPr + "<w:t>" + xmlEscaped(text) + "</w:t></w:r>\n";
                    }
                }
            }

            documentXml += "    </w:p>\n";
        }
    }

    documentXml += "  </w:body>\n"
                   "</w:document>";
    zip.addFile("word/document.xml", documentXml.toUtf8());

    // Write ZIP to file
    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    outFile.write(zip.toArray());
    outFile.close();

    if (m_context)
        m_context->log(QString("DOCX: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool DocxPlugin::checkDependencies() const
{
    return true;
}
