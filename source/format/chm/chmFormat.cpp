#include "chmFormat.h"
#include "BookDocument.h"
#include "HtmlConverter.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QRegularExpression>
#include <QtEndian>
#include <QDebug>

static const char *ITSF_MAGIC = "ITSF";
static const char *ITSP_MAGIC = "ITSP";
static const char *PMGL_MAGIC = "PMGL";

struct ChmDirEntry {
    QString name;
    quint32 offset;
    quint32 length;
    quint16 section;
    quint8  flags;
};

static QByteArray readEntire(QIODevice &dev)
{
    return dev.readAll();
}

static bool parseItsfHeader(const QByteArray &data, quint32 &dirOffset, quint32 &dirLength)
{
    if (data.size() < 96) return false;
    if (data.mid(0, 4) != ITSF_MAGIC) return false;

    dirOffset = qFromLittleEndian<quint32>(data.mid(32, 4));
    dirLength = qFromLittleEndian<quint32>(data.mid(36, 4));
    return true;
}

static bool parseDirectory(const QByteArray &data, int baseOffset, int dirLength, QList<ChmDirEntry> &entries)
{
    int pos = baseOffset;
    int endPos = baseOffset + dirLength;

    while (pos + 84 <= endPos) {
        if (data.mid(pos, 4) == PMGL_MAGIC) {
            quint32 pageSize = qFromLittleEndian<quint32>(data.mid(pos + 4, 4));
            quint32 numEntries = qFromLittleEndian<quint32>(data.mid(pos + 8, 4));
            int entryStart = pos + 16;

            for (quint32 i = 0; i < numEntries && entryStart < endPos; ++i) {
                quint32 entryLen = qFromLittleEndian<quint32>(data.mid(entryStart, 4));
                if (entryLen == 0 || entryStart + entryLen > endPos) break;

                quint32 nameLen = qFromLittleEndian<quint32>(data.mid(entryStart + 4, 4));
                quint32 entryOffset = qFromLittleEndian<quint32>(data.mid(entryStart + 8, 4));
                quint32 entryLength = qFromLittleEndian<quint32>(data.mid(entryStart + 12, 4));

                if (entryStart + 20 + nameLen > endPos) break;
                QString name = QString::fromUtf8(data.mid(entryStart + 20, nameLen));
                if (name.endsWith('\0'))
                    name.chop(1);

                ChmDirEntry e;
                e.name = name;
                e.offset = entryOffset;
                e.length = entryLength;
                e.section = 0;
                e.flags = 0;
                entries.append(e);

                entryStart += 20 + ((nameLen + 3) & ~3);
            }
            pos += pageSize;
        } else if (data.mid(pos, 4) == ITSP_MAGIC) {
            pos += 84;
        } else {
            break;
        }
    }
    return !entries.isEmpty();
}

static QString stripHtmlTagsForSplit(const QString &html)
{
    QString result = html;
    result.remove(QRegularExpression("<[^>]*>"));
    return result.trimmed();
}

ChmPlugin::ChmPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

ChmPlugin::~ChmPlugin() { shutdown(); }

QString ChmPlugin::name() const { return "CHM"; }
QString ChmPlugin::description() const { return "Compiled HTML Help (.chm) format plugin"; }
QString ChmPlugin::version() const { return "1.0.0"; }
QString ChmPlugin::author() const { return "BookCreateEdit"; }
QStringList ChmPlugin::dependencies() const { return {}; }

void ChmPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context) m_context->log("CHM: initialized");
}

void ChmPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool ChmPlugin::isInitialized() const { return m_initialized; }

QString ChmPlugin::formatName() const { return "CHM"; }
QString ChmPlugin::fileExtension() const { return "chm"; }
QStringList ChmPlugin::supportedExtensions() const { return {"chm"}; }
QStringList ChmPlugin::mimeTypes() const { return {"application/x-chm"}; }

bool ChmPlugin::canRead() const { return true; }
bool ChmPlugin::canWrite() const { return true; }

bool ChmPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray data = readEntire(file);
    file.close();

    quint32 dirOffset = 0, dirLength = 0;
    if (!parseItsfHeader(data, dirOffset, dirLength)) {
        if (m_context) m_context->log("CHM: invalid ITSF header");
        return false;
    }

    QList<ChmDirEntry> entries;
    if (!parseDirectory(data, dirOffset, dirLength, entries)) {
        if (m_context) m_context->log("CHM: no directory entries found");
        return false;
    }

    BookMeta meta;
    QFileInfo fi(filePath);
    meta.title = fi.completeBaseName();

    while (doc->pageCount() > 0)
        doc->removePage(0);

    QStringList htmlFiles;
    for (const ChmDirEntry &e : entries) {
        if (e.name.endsWith(".html", Qt::CaseInsensitive) ||
            e.name.endsWith(".htm", Qt::CaseInsensitive)) {
            if ((int)(e.offset + e.length) <= data.size()) {
                htmlFiles.append(e.name);
            }
        }
    }

    std::sort(htmlFiles.begin(), htmlFiles.end());

    for (int i = 0; i < htmlFiles.size(); ++i) {
        for (const ChmDirEntry &e : entries) {
            if (e.name == htmlFiles.at(i)) {
                QByteArray htmlData = data.mid(e.offset, e.length);
                QString html = QString::fromUtf8(htmlData);

                if (html.startsWith("LZXPRESS")) {
                    if (m_context)
                        m_context->log("CHM: compressed data not yet supported for " + e.name);
                    continue;
                }

                QDomDocument htmlDoc;
                if (htmlDoc.setContent(html)) {
                    QDomElement body = htmlDoc.documentElement().firstChildElement("body");
                    if (!body.isNull()) {
                        QDomNodeList children = body.childNodes();
                        QString bceBody;
                        for (int c = 0; c < children.count(); ++c) {
                            QTextStream ts(&bceBody);
                            children.at(c).save(ts, 0);
                        }
                        QString bcePage = QString("<page><body>%1</body></page>").arg(bceBody);
                        doc->addPage(doc->pageCount() - 1);
                        doc->setPageXml(doc->pageCount() - 1, bcePage);
                    } else {
                        QString bcePage = HtmlConverter::htmlToBcePage(html);
                        doc->addPage(doc->pageCount() - 1);
                        doc->setPageXml(doc->pageCount() - 1, bcePage);
                    }
                } else {
                    QString bcePage = HtmlConverter::htmlToBcePage(html);
                    doc->addPage(doc->pageCount() - 1);
                    doc->setPageXml(doc->pageCount() - 1, bcePage);
                }
                break;
            }
        }
    }

    if (doc->pageCount() == 0) {
        for (const ChmDirEntry &e : entries) {
            if (e.name.endsWith(".html", Qt::CaseInsensitive) ||
                e.name.endsWith(".htm", Qt::CaseInsensitive)) {
                QByteArray htmlData = data.mid(e.offset, e.length);
                QString html = QString::fromUtf8(htmlData);
                QString bcePage = HtmlConverter::htmlToBcePage(html);
                doc->addPage(doc->pageCount() - 1);
                doc->setPageXml(doc->pageCount() - 1, bcePage);
            }
        }
    }

    if (doc->pageCount() == 0) {
        doc->addPage(-1);
        QDomDocument bceDoc;
        bceDoc.setContent(QString("<page><body><p></p></body></page>"));
        doc->setPageXml(0, bceDoc.toString(-1));
    }

    doc->setMeta(meta);

    if (m_context)
        m_context->log(QString("CHM: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

static void appendDwordLE(QByteArray &buf, quint32 val)
{
    buf.append(static_cast<char>(val & 0xFF));
    buf.append(static_cast<char>((val >> 8) & 0xFF));
    buf.append(static_cast<char>((val >> 16) & 0xFF));
    buf.append(static_cast<char>((val >> 24) & 0xFF));
}

static void appendWordLE(QByteArray &buf, quint16 val)
{
    buf.append(static_cast<char>(val & 0xFF));
    buf.append(static_cast<char>((val >> 8) & 0xFF));
}

static void appendQwordLE(QByteArray &buf, quint64 val)
{
    appendDwordLE(buf, static_cast<quint32>(val & 0xFFFFFFFF));
    appendDwordLE(buf, static_cast<quint32>((val >> 32) & 0xFFFFFFFF));
}

bool ChmPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc) return false;

    BookMeta meta = doc->meta();
    if (meta.title.isEmpty())
        meta.title = QFileInfo(filePath).completeBaseName();

    QList<QByteArray> htmlPages;
    QStringList fileNames;

    for (int i = 0; i < doc->pageCount(); ++i) {
        QString bceXml = doc->pageXml(i);
        QString pageHtml = HtmlConverter::bcePageToHtml(bceXml);

        QString fullHtml;
        fullHtml += "<html>\n<head>\n";
        fullHtml += QString("<title>%1 - Page %2</title>\n").arg(meta.title).arg(i + 1);
        fullHtml += "</head>\n<body>\n";
        fullHtml += pageHtml;
        fullHtml += "\n</body>\n</html>";

        QString name = QString("/page%1.html").arg(i + 1, 4, 10, QChar('0'));
        htmlPages.append(fullHtml.toUtf8());
        fileNames.append(name);
    }

    QByteArray tocHtml;
    tocHtml += "<html>\n<head>\n";
    tocHtml += "<title>Table of Contents</title>\n";
    tocHtml += "</head>\n<body>\n";
    tocHtml += "<h1>" + meta.title.toUtf8() + "</h1>\n";
    tocHtml += "<ul>\n";
    for (int i = 0; i < fileNames.size(); ++i) {
        tocHtml += QString("<li><a href=\"%1\">Page %2</a></li>\n")
                       .arg(fileNames.at(i)).arg(i + 1).toUtf8();
    }
    tocHtml += "</ul>\n</body>\n</html>";
    htmlPages.prepend(tocHtml);
    fileNames.prepend("/toc.html");

    const quint32 itsfSize = 96;
    const quint32 itspSize = 84;
    const quint32 pageChunkSize = 4096;

    QList<quint32> dataOffsets;
    quint32 dataCurrentOffset = 0;
    for (int i = 0; i < htmlPages.size(); ++i) {
        dataOffsets.append(dataCurrentOffset);
        dataCurrentOffset += static_cast<quint32>(htmlPages.at(i).size());
    }

    QList<QByteArray> dirEntries;
    for (int i = 0; i < fileNames.size(); ++i) {
        QByteArray nameUtf8 = fileNames.at(i).toUtf8();
        quint32 nameLen = static_cast<quint32>(nameUtf8.size() + 1);
        quint32 paddedNameLen = (nameLen + 3) & ~3;

        QByteArray entryBuf;
        appendDwordLE(entryBuf, 20 + paddedNameLen);
        appendDwordLE(entryBuf, nameLen);
        appendDwordLE(entryBuf, dataOffsets.at(i));
        appendDwordLE(entryBuf, static_cast<quint32>(htmlPages.at(i).size()));
        entryBuf.append(nameUtf8);
        entryBuf.append('\0');
        while (entryBuf.size() % 4 != 0)
            entryBuf.append('\0');
        dirEntries.append(entryBuf);
    }

    QList<QByteArray> pmglPages;
    QByteArray currentPageData;
    int currentPageEntries = 0;

    for (int i = 0; i < dirEntries.size(); ++i) {
        currentPageData.append(dirEntries.at(i));
        currentPageEntries++;

        if (currentPageData.size() > (int)(pageChunkSize - 16) || i == dirEntries.size() - 1) {
            QByteArray pmglPage;
            pmglPage.append(PMGL_MAGIC, 4);
            appendDwordLE(pmglPage, static_cast<quint32>(pageChunkSize - 16 - currentPageData.size()));
            appendDwordLE(pmglPage, 0);
            appendDwordLE(pmglPage, static_cast<quint32>(currentPageEntries));
            pmglPage.append(currentPageData);
            while (pmglPage.size() < (int)pageChunkSize)
                pmglPage.append('\0');
            pmglPages.append(pmglPage);
            currentPageData.clear();
            currentPageEntries = 0;
        }
    }

    if (pmglPages.isEmpty()) {
        QByteArray emptyPage;
        emptyPage.append(PMGL_MAGIC, 4);
        appendDwordLE(emptyPage, pageChunkSize - 16);
        appendDwordLE(emptyPage, 0);
        appendDwordLE(emptyPage, 0);
        while (emptyPage.size() < (int)pageChunkSize)
            emptyPage.append('\0');
        pmglPages.append(emptyPage);
    }

    quint32 dirStartOffset = itsfSize;
    quint32 dirTotalLength = itspSize + static_cast<quint32>(pmglPages.size()) * pageChunkSize;

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QByteArray itsfHeader(itsfSize, '\0');
    itsfHeader.replace(0, 4, ITSF_MAGIC, 4);
    appendDwordLE(itsfHeader, 3);
    appendDwordLE(itsfHeader, itsfSize);
    appendDwordLE(itsfHeader, 0);
    appendDwordLE(itsfHeader, 0x0409);
    appendDwordLE(itsfHeader, 0);
    appendDwordLE(itsfHeader, 0);
    appendDwordLE(itsfHeader, dirStartOffset);
    appendDwordLE(itsfHeader, dirTotalLength);
    appendDwordLE(itsfHeader, 0);
    appendDwordLE(itsfHeader, 0);
    appendDwordLE(itsfHeader, 0);
    appendDwordLE(itsfHeader, 0);
    appendDwordLE(itsfHeader, 0);
    appendDwordLE(itsfHeader, 0);
    appendQwordLE(itsfHeader, 0);
    appendQwordLE(itsfHeader, 0);
    appendQwordLE(itsfHeader, 0);
    outFile.write(itsfHeader);

    QByteArray itspHeader(itspSize, '\0');
    itspHeader.replace(0, 4, ITSP_MAGIC, 4);
    appendDwordLE(itspHeader, 1);
    appendDwordLE(itspHeader, itspSize);
    appendDwordLE(itspHeader, 0);
    appendDwordLE(itspHeader, pageChunkSize);
    appendDwordLE(itspHeader, 2);
    appendDwordLE(itspHeader, 0);
    appendDwordLE(itspHeader, 0);
    appendDwordLE(itspHeader, static_cast<quint32>(pmglPages.size()));
    appendDwordLE(itspHeader, 0);
    appendDwordLE(itspHeader, static_cast<quint32>(0xFFFFFFFF));
    appendDwordLE(itspHeader, static_cast<quint32>(dirEntries.size()));
    appendDwordLE(itspHeader, 0);
    appendDwordLE(itspHeader, 0);
    appendDwordLE(itspHeader, 0);
    outFile.write(itspHeader);

    for (const QByteArray &pmgl : pmglPages)
        outFile.write(pmgl);

    for (int i = 0; i < htmlPages.size(); ++i)
        outFile.write(htmlPages.at(i));

    outFile.close();

    if (m_context)
        m_context->log(QString("CHM: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool ChmPlugin::checkDependencies() const { return true; }
