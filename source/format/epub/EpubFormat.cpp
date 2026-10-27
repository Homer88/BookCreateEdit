#include "EpubFormat.h"
#include "BookDocument.h"
#include "../ziputil.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDomDocument>
#include <QTextStream>
#include <QCoreApplication>
#include <QDateTime>
#include <QUuid>
#include <QDebug>
#include <QtEndian>
#include <QMap>

// Minimal ZIP writer for EPUB/DOCX
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
            localHeaders.append("PK\x03\x04", 4);           // signature
            localHeaders.append(qToLittleEndian<quint16>(20), 2); // version needed
            localHeaders.append(qToLittleEndian<quint16>(0), 2);  // flags
            localHeaders.append(qToLittleEndian<quint16>(0), 2);  // compression (stored)
            localHeaders.append(qToLittleEndian<quint16>(0), 2);  // mod time
            localHeaders.append(qToLittleEndian<quint16>(0), 2);  // mod date
            localHeaders.append(qToLittleEndian<quint32>(e.crc), 4);  // crc32
            localHeaders.append(qToLittleEndian<quint32>(e.data.size()), 4); // compressed size
            localHeaders.append(qToLittleEndian<quint32>(e.data.size()), 4); // uncompressed size
            localHeaders.append(qToLittleEndian<quint16>(nameBytes.size()), 2); // name length
            localHeaders.append(qToLittleEndian<quint16>(0), 2);  // extra field length
            localHeaders.append(nameBytes);
            localHeaders.append(e.data);

            // Central directory entry
            centralDir.append("PK\x01\x02", 4);            // signature
            centralDir.append(qToLittleEndian<quint16>(20), 2); // version made by
            centralDir.append(qToLittleEndian<quint16>(20), 2); // version needed
            centralDir.append(qToLittleEndian<quint16>(0), 2);  // flags
            centralDir.append(qToLittleEndian<quint16>(0), 2);  // compression
            centralDir.append(qToLittleEndian<quint16>(0), 2);  // mod time
            centralDir.append(qToLittleEndian<quint16>(0), 2);  // mod date
            centralDir.append(qToLittleEndian<quint32>(e.crc), 4);  // crc32
            centralDir.append(qToLittleEndian<quint32>(e.data.size()), 4); // compressed
            centralDir.append(qToLittleEndian<quint32>(e.data.size()), 4); // uncompressed
            centralDir.append(qToLittleEndian<quint16>(nameBytes.size()), 2); // name length
            centralDir.append(qToLittleEndian<quint16>(0), 2);  // extra length
            centralDir.append(qToLittleEndian<quint16>(0), 2);  // comment length
            centralDir.append(qToLittleEndian<quint16>(0), 2);  // disk start
            centralDir.append(qToLittleEndian<quint16>(0), 2);  // internal attrs
            centralDir.append(qToLittleEndian<quint32>(0), 4);  // external attrs
            centralDir.append(qToLittleEndian<quint32>(e.offset), 4); // local header offset
            centralDir.append(nameBytes);

            offset += 4 + 26 + nameBytes.size() + e.data.size();
        }

        quint32 cdOffset = offset;
        quint32 cdSize = centralDir.size();

        // End of central directory
        QByteArray endRecord;
        endRecord.append("PK\x05\x06", 4);                 // signature
        endRecord.append(qToLittleEndian<quint16>(0), 2);   // disk number
        endRecord.append(qToLittleEndian<quint16>(0), 2);   // disk with CD
        endRecord.append(qToLittleEndian<quint16>(m_entries.size()), 2); // entries on disk
        endRecord.append(qToLittleEndian<quint16>(m_entries.size()), 2); // total entries
        endRecord.append(qToLittleEndian<quint32>(cdSize), 4); // CD size
        endRecord.append(qToLittleEndian<quint32>(cdOffset), 4); // CD offset
        endRecord.append(qToLittleEndian<quint16>(0), 2);   // comment length

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

EpubFormat::EpubFormat(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

EpubFormat::~EpubFormat()
{
    shutdown();
}

QString EpubFormat::name() const { return "EPUB"; }
QString EpubFormat::description() const { return "EPUB 2.0/3.0 ebook format"; }
QString EpubFormat::version() const { return "1.0.0"; }
QString EpubFormat::author() const { return "BookCreateEdit"; }
QStringList EpubFormat::dependencies() const { return {}; }

void EpubFormat::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("EPUB: initialized");
}

void EpubFormat::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool EpubFormat::isInitialized() const { return m_initialized; }

QString EpubFormat::formatName() const { return "EPUB"; }
QString EpubFormat::fileExtension() const { return "epub"; }
QStringList EpubFormat::supportedExtensions() const { return {"epub"}; }
QStringList EpubFormat::mimeTypes() const { return {"application/epub+zip"}; }

bool EpubFormat::canRead() const { return true; }
bool EpubFormat::canWrite() const { return true; }

bool EpubFormat::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;
    return readEpub(filePath, doc);
}

bool EpubFormat::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;
    return writeEpub(doc, filePath);
}

bool EpubFormat::checkDependencies() const { return true; }

bool EpubFormat::readEpub(const QString &path, BookDocument *doc)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray zipData = file.readAll();
    file.close();

    // Читаем ВСЕ записи архива (stored + deflate) через общий ZipUtil.
    // Раньше понимались только несжатые записи, поэтому настоящие EPUB
    // (Calibre, Sigil и пр.) не импортировались.
    QString zipError;
    QMap<QString, QByteArray> files;
    const auto entries = ZipUtil::readZip(zipData, &zipError);
    for (const ZipUtil::ZipEntry &e : entries)
        files.insert(e.name, e.data);

    if (files.isEmpty()) {
        if (m_context)
            m_context->log("EPUB: пустой или повреждённый архив (" + zipError + ")");
        return false;
    }

    // Путь к OPF берём из META-INF/container.xml
    QString opfPath;
    if (files.contains("META-INF/container.xml")) {
        QDomDocument containerDoc;
        if (containerDoc.setContent(files.value("META-INF/container.xml"))) {
            QDomElement rootfile = containerDoc.documentElement()
                .firstChildElement("rootfiles")
                .firstChildElement("rootfile");
            if (!rootfile.isNull())
                opfPath = rootfile.attribute("full-path");
        }
    }

    // Резервный вариант — поиск по имени
    if (opfPath.isEmpty()) {
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            if (it.key().endsWith("content.opf")) {
                opfPath = it.key();
                break;
            }
        }
    }

    if (opfPath.isEmpty()) {
        if (m_context)
            m_context->log("EPUB: content.opf not found");
        return false;
    }

    QByteArray opfData = files.value(opfPath);
    if (opfData.isEmpty())
        return false;

    QDomDocument opfDoc;
    if (!opfDoc.setContent(opfData))
        return false;

    QDomElement opfRoot = opfDoc.documentElement();
    QString opfDir = opfPath.left(opfPath.lastIndexOf('/') + 1);

    // Extract metadata
    BookMeta meta;
    QDomElement metadata = opfRoot.firstChildElement("metadata");
    if (!metadata.isNull()) {
        QDomElement dcTitle = metadata.firstChildElement("dc:title");
        if (!dcTitle.isNull())
            meta.title = dcTitle.text();
        QDomElement dcCreator = metadata.firstChildElement("dc:creator");
        if (!dcCreator.isNull())
            meta.author = dcCreator.text();
        QDomElement dcLang = metadata.firstChildElement("dc:language");
        if (!dcLang.isNull())
            meta.language = dcLang.text();
    }
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    // Get spine order
    QDomElement spine = opfRoot.firstChildElement("spine");
    QDomElement manifest = opfRoot.firstChildElement("manifest");

    QStringList spineOrder;
    if (!spine.isNull()) {
        QDomNodeList itemrefs = spine.elementsByTagName("itemref");
        for (int i = 0; i < itemrefs.count(); ++i) {
            QString idref = itemrefs.at(i).toElement().attribute("idref");
            // Find href in manifest
            QDomNodeList items = manifest.elementsByTagName("item");
            for (int j = 0; j < items.count(); ++j) {
                QDomElement item = items.at(j).toElement();
                if (item.attribute("id") == idref) {
                    spineOrder.append(item.attribute("href"));
                    break;
                }
            }
        }
    }

    // Read XHTML files
    for (const QString &href : spineOrder) {
        QString fullPath = opfDir + href;
        QByteArray xhtmlData = files.value(fullPath);
        if (xhtmlData.isEmpty() && files.contains(href))
            xhtmlData = files.value(href);
        if (!xhtmlData.isEmpty()) {
                QDomDocument xhtmlDoc;
                if (xhtmlDoc.setContent(xhtmlData)) {
                    QDomElement htmlEl = xhtmlDoc.documentElement();
                    QDomElement body = htmlEl.firstChildElement("body");
                    if (!body.isNull()) {
                        // Convert XHTML to BCE
                        QDomDocument bceDoc;
                        bceDoc.setContent(QString("<page><body></body></page>"));
                        QDomElement bceBody = bceDoc.documentElement().firstChildElement("body");

                        QDomNodeList children = body.childNodes();
                        for (int i = 0; i < children.count(); ++i) {
                            QDomNode child = children.at(i);
                            if (child.isElement()) {
                                QDomElement childEl = child.toElement();
                                QString tag = childEl.tagName();

                                if (tag == "h1" || tag == "h2" || tag == "h3" ||
                                    tag == "h4" || tag == "h5" || tag == "h6") {
                                    QDomElement h = bceDoc.createElement(tag);
                                    h.appendChild(bceDoc.createTextNode(childEl.text()));
                                    bceBody.appendChild(h);
                                } else if (tag == "p" || tag == "div") {
                                    QDomElement p = bceDoc.createElement("p");
                                    p.appendChild(bceDoc.createTextNode(childEl.text()));
                                    bceBody.appendChild(p);
                                } else if (tag == "strong" || tag == "b") {
                                    QDomElement b = bceDoc.createElement("b");
                                    b.appendChild(bceDoc.createTextNode(childEl.text()));
                                    bceBody.appendChild(b);
                                } else if (tag == "em" || tag == "i") {
                                    QDomElement ie = bceDoc.createElement("i");
                                    ie.appendChild(bceDoc.createTextNode(childEl.text()));
                                    bceBody.appendChild(ie);
                                } else if (tag == "a") {
                                    QDomElement a = bceDoc.createElement("a");
                                    a.setAttribute("href", childEl.attribute("href", ""));
                                    a.appendChild(bceDoc.createTextNode(childEl.text()));
                                    bceBody.appendChild(a);
                                } else if (tag == "img") {
                                    QDomElement img = bceDoc.createElement("image");
                                    img.setAttribute("src", childEl.attribute("src", ""));
                                    img.setAttribute("alt", childEl.attribute("alt", ""));
                                    bceBody.appendChild(img);
                                } else if (tag == "blockquote") {
                                    QDomElement bq = bceDoc.createElement("blockquote");
                                    bq.appendChild(bceDoc.createTextNode(childEl.text()));
                                    bceBody.appendChild(bq);
                                } else if (tag == "ul" || tag == "ol") {
                                    QDomElement list = bceDoc.createElement("list");
                                    list.setAttribute("type", tag);
                                    QDomNodeList lis = childEl.elementsByTagName("li");
                                    for (int k = 0; k < lis.count(); ++k) {
                                        QDomElement li = bceDoc.createElement("li");
                                        li.appendChild(bceDoc.createTextNode(lis.at(k).toElement().text()));
                                        list.appendChild(li);
                                    }
                                    bceBody.appendChild(list);
                                } else if (tag == "pre" || tag == "code") {
                                    QDomElement pre = bceDoc.createElement(tag);
                                    pre.appendChild(bceDoc.createTextNode(childEl.text()));
                                    bceBody.appendChild(pre);
                                } else if (tag == "table") {
                                    QDomElement table = bceDoc.createElement("table");
                                    QDomNodeList rows = childEl.elementsByTagName("tr");
                                    for (int r = 0; r < rows.count(); ++r) {
                                        QDomElement row = bceDoc.createElement("row");
                                        QDomNodeList cells = rows.at(r).toElement().elementsByTagName("td");
                                        for (int c = 0; c < cells.count(); ++c) {
                                            QDomElement cell = bceDoc.createElement("cell");
                                            cell.appendChild(bceDoc.createTextNode(cells.at(c).toElement().text()));
                                            row.appendChild(cell);
                                        }
                                        table.appendChild(row);
                                    }
                                    bceBody.appendChild(table);
                                } else {
                                    // Generic: wrap in <p>
                                    QDomElement p = bceDoc.createElement("p");
                                    p.appendChild(bceDoc.createTextNode(childEl.text()));
                                    bceBody.appendChild(p);
                                }
                            }
                        }

                        doc->addPage(doc->pageCount() - 1);
                        doc->setPageXml(doc->pageCount() - 1, bceDoc.toString(-1));
                    }
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
        m_context->log(QString("EPUB: imported %1 pages from %2").arg(doc->pageCount()).arg(path));

    return true;
}

bool EpubFormat::writeEpub(BookDocument *doc, const QString &path)
{
    if (!doc)
        return false;

    SimpleZipWriter zip;

    // mimetype (must be first, uncompressed)
    zip.addFile("mimetype", "application/epub+zip");

    // container.xml
    zip.addFile("META-INF/container.xml", containerXml().toUtf8());

    // content.opf
    zip.addFile("OEBPS/content.opf", opfXml(doc).toUtf8());

    // XHTML pages
    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageId = QString("page%1").arg(i + 1);
        QString xmlContent = doc->pageXml(i);
        QString xhtml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        xhtml += "<!DOCTYPE html>\n";
        xhtml += "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
        xhtml += "<head><title>" + pageId + "</title></head>\n";
        xhtml += "<body>\n";

        QDomDocument xmlDoc;
        if (xmlDoc.setContent(xmlContent)) {
            QDomElement body = xmlDoc.documentElement().firstChildElement("body");
            if (!body.isNull()) {
                QDomNodeList children = body.childNodes();
                for (int j = 0; j < children.count(); ++j) {
                    QTextStream stream(&xhtml);
                    stream << "  ";
                    children.at(j).save(stream, 2);
                    xhtml += "\n";
                }
            }
        }

        xhtml += "</body>\n</html>";
        zip.addFile("OEBPS/text/" + pageId + ".xhtml", xhtml.toUtf8());
    }

    // Table of contents
    QString tocXhtml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    tocXhtml += "<!DOCTYPE html>\n";
    tocXhtml += "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">\n";
    tocXhtml += "<head><title>Table of Contents</title></head>\n";
    tocXhtml += "<body>\n";
    tocXhtml += "  <nav epub:type=\"toc\">\n";
    tocXhtml += "    <h1>Table of Contents</h1>\n";
    tocXhtml += "    <ol>\n";
    for (int i = 0; i < doc->pageCount(); ++i) {
        tocXhtml += QString("      <li><a href=\"text/page%1.xhtml\">Page %2</a></li>\n")
                        .arg(i + 1).arg(i + 1);
    }
    tocXhtml += "    </ol>\n";
    tocXhtml += "  </nav>\n";
    tocXhtml += "</body>\n</html>";
    zip.addFile("OEBPS/text/toc.xhtml", tocXhtml.toUtf8());

    // Write ZIP to file
    QFile outFile(path);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    outFile.write(zip.toArray());
    outFile.close();

    if (m_context)
        m_context->log(QString("EPUB: exported %1 pages to %2").arg(doc->pageCount()).arg(path));

    return true;
}

QString EpubFormat::containerXml() const
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
           "  <rootfiles>\n"
           "    <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
           "  </rootfiles>\n"
           "</container>\n";
}

QString EpubFormat::opfXml(const BookDocument *doc) const
{
    BookMeta meta = doc->meta();
    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString opf = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    opf += "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"bookid\">\n";
    opf += "  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n";
    opf += "    <dc:identifier id=\"bookid\">urn:uuid:bookcreateedit-" + QUuid::createUuid().toString() + "</dc:identifier>\n";
    opf += "    <dc:title>" + meta.title.toHtmlEscaped() + "</dc:title>\n";
    opf += "    <dc:creator>" + meta.author.toHtmlEscaped() + "</dc:creator>\n";
    opf += "    <dc:language>" + meta.language.toHtmlEscaped() + "</dc:language>\n";
    opf += "    <dc:date>" + now + "</dc:date>\n";
    opf += "    <meta property=\"dcterms:modified\">" + now + "</meta>\n";
    opf += "  </metadata>\n";
    opf += "  <manifest>\n";
    opf += "    <item id=\"toc\" href=\"text/toc.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n";
    for (int i = 0; i < doc->pageCount(); ++i) {
        opf += QString("    <item id=\"page%1\" href=\"text/page%1.xhtml\" media-type=\"application/xhtml+xml\"/>\n").arg(i + 1);
    }
    opf += "  </manifest>\n";
    opf += "  <spine>\n";
    for (int i = 0; i < doc->pageCount(); ++i) {
        opf += QString("    <itemref idref=\"page%1\"/>\n").arg(i + 1);
    }
    opf += "  </spine>\n";
    opf += "</package>\n";
    return opf;
}
