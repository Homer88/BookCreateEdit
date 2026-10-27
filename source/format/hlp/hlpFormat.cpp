#include "hlpformat.h"
#include "BookDocument.h"
#include "HtmlConverter.h"
#include "converter/rtf2html.h"
#include "converter/wmf2png.h"
#include "converter/mrb2png.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QTemporaryDir>
#include <QUuid>
#include <QProcess>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>
#include <QDebug>

extern "C" int helpdeco_convert_to_rtf(const char *hlpPath, const char *outBase);

HlpPlugin::HlpPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

HlpPlugin::~HlpPlugin()
{
    shutdown();
}

QString HlpPlugin::name() const { return "Help"; }
QString HlpPlugin::description() const { return "Windows Help (.hlp) format plugin"; }
QString HlpPlugin::version() const { return "1.0.0"; }
QString HlpPlugin::author() const { return "BookCreateEdit"; }
QStringList HlpPlugin::dependencies() const { return {}; }

void HlpPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("Help: initialized");
}

void HlpPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool HlpPlugin::isInitialized() const { return m_initialized; }

QString HlpPlugin::formatName() const { return "Help"; }
QString HlpPlugin::fileExtension() const { return "hlp"; }
QStringList HlpPlugin::supportedExtensions() const { return {"hlp", "mvb"}; }
QStringList HlpPlugin::mimeTypes() const { return {"application/x-hlp"}; }

bool HlpPlugin::canRead() const { return true; }
bool HlpPlugin::canWrite() const { return true; }

bool HlpPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        if (m_context)
            m_context->log("Help: failed to create temp dir");
        return false;
    }

    const QString outBase = QDir(tmpDir.path()).filePath("helpreader_out");

    int result = helpdeco_convert_to_rtf(
        filePath.toLocal8Bit().constData(),
        outBase.toLocal8Bit().constData());

    if (result == 0) {
        if (m_context)
            m_context->log("Help: failed to decompile " + filePath);
        return false;
    }

    QFile rtfFile(outBase + ".rtf");
    if (!rtfFile.open(QIODevice::ReadOnly)) {
        if (m_context)
            m_context->log("Help: failed to read decompiled RTF");
        return false;
    }

    const QString rtfText = QString::fromLatin1(rtfFile.readAll());
    rtfFile.close();

    if (rtfText.isEmpty()) {
        if (m_context)
            m_context->log("Help: decompiled RTF is empty");
        return false;
    }

    QFileInfo fi(filePath);
    BookMeta meta;
    meta.title = fi.completeBaseName();
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    const QStringList rawTopics = Rtf::splitTopics(rtfText);
    if (rawTopics.isEmpty()) {
        if (m_context)
            m_context->log("Help: no topics found in RTF");
        return false;
    }

    for (int i = 0; i < rawTopics.size(); ++i) {
        const QString trimmed = rawTopics.at(i).trimmed();
        if (trimmed.isEmpty())
            continue;

        Rtf::Meta rtfMeta;
        QString html = Rtf::toHtml(trimmed, rtfText, &rtfMeta);

        QString title = rtfMeta.title.isEmpty()
            ? Rtf::headingOf(trimmed)
            : rtfMeta.title;
        if (title.isEmpty())
            title = QString("Topic %1").arg(i + 1);

        QString bcePage = HtmlConverter::htmlToBcePage(html);

        doc->addPage(doc->pageCount() - 1);
        int pi = doc->pageCount() - 1;
        doc->setPageXml(pi, bcePage);
    }

    if (doc->pageCount() == 0) {
        doc->addPage(-1);
        QDomDocument bceDoc;
        bceDoc.setContent(QString("<page><body><p></p></body></page>"));
        doc->setPageXml(0, bceDoc.toString(-1));
    }

    if (m_context)
        m_context->log(QString("Help: imported %1 topics from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

static QByteArray buildRtfFromBce(const QString &bceXml, const QString &rtfAll)
{
    QDomDocument doc;
    if (!doc.setContent(bceXml))
        return QByteArray();

    QByteArray rtf;
    rtf.append("{\\rtf1\\ansi\\deff0\\fonttbl{\\f0\\froman\\fcharset0 Times New Roman;}{\\f1\\fswiss\\fcharset0 Arial;}{\\f2\\fmodern\\fcharset0 Courier New;}}\n");
    rtf.append("{\\colortbl;\\red0\\green0\\blue0;\\red255\\green0\\blue0;\\red0\\green0\\blue255;}\n");
    rtf.append("\\f0\\fs20\n");

    QDomElement body = doc.documentElement().firstChildElement("body");
    if (body.isNull())
        body = doc.documentElement();

    QDomNodeList children = body.childNodes();
    for (int i = 0; i < children.count(); ++i) {
        QDomElement el = children.at(i).toElement();
        if (el.isNull()) continue;
        QString tag = el.tagName().toLower();

        if (tag == "p") {
            if (!rtf.endsWith("\\par\n")) rtf.append(" ");
            QByteArray span;
            QDomNodeList inner = el.childNodes();
            for (int j = 0; j < inner.count(); ++j) {
                QDomElement ie = inner.at(j).toElement();
                if (ie.isNull()) {
                    span.append(inner.at(j).toElement().text().toUtf8());
                } else {
                    QString itag = ie.tagName().toLower();
                    QByteArray innerRtf;
                    QDomNodeList innernodes = ie.childNodes();
                    for (int k = 0; k < innernodes.count(); ++k) {
                        innerRtf.append(innernodes.at(k).toElement().text().toUtf8());
                    }
                    if (itag == "b" || itag == "strong")
                        span.append("{\\b " + innerRtf + "}");
                    else if (itag == "i" || itag == "em")
                        span.append("{\\i " + innerRtf + "}");
                    else if (itag == "u")
                        span.append("{\\ul " + innerRtf + "}");
                    else if (itag == "strike" || itag == "s")
                        span.append("{\\strike " + innerRtf + "}");
                    else if (itag == "code")
                        span.append("{\\f2\\fs18 " + innerRtf + "}");
                    else if (itag == "a") {
                        QString href = ie.attribute("href");
                        span.append("{\\v #{" + href.toUtf8() + "}" + innerRtf + "}");
                    }
                    else
                        span.append(innerRtf);
                }
            }
            if (span.isEmpty()) span = el.text().toUtf8();
            rtf.append(span + "\\par\n");
        } else if (tag.startsWith("h") && tag.size() == 2) {
            int level = tag[1].digitValue();
            int sz = 32 - (level - 1) * 4;
            QByteArray text = el.text().toUtf8();
            rtf.append(QString("\\pard\\b\\fs%1 %2\\b0\\fs20\\par\n").arg(sz).arg(QString::fromUtf8(text)));
        } else if (tag == "list") {
            QDomNodeList items = el.elementsByTagName("li");
            for (int j = 0; j < items.count(); ++j) {
                QByteArray itemText = items.at(j).toElement().text().toUtf8();
                rtf.append("\\tab\\bullet\\tab " + itemText + "\\par\n");
            }
        } else if (tag == "code" || tag == "pre") {
            QByteArray text = el.text().toUtf8();
            rtf.append("\\pard{\\f2\\fs18 " + text + "}\\par\n");
        } else if (tag == "table") {
            QDomNodeList rows = el.elementsByTagName("row");
            for (int j = 0; j < rows.count(); ++j) {
                QDomNodeList cells = rows.at(j).toElement().elementsByTagName("cell");
                for (int k = 0; k < cells.count(); ++k) {
                    if (k > 0) rtf.append(" \\tab ");
                    rtf.append(cells.at(k).toElement().text().toUtf8());
                }
                rtf.append("\\par\n");
            }
        } else if (tag == "hr") {
            rtf.append("\\pard\\brdrb\\brdrs\\brdrw10\\brsp20 \\par\n");
        } else if (tag == "blockquote") {
            QByteArray text = el.text().toUtf8();
            rtf.append("\\pard\\li720\\fi-360 " + text + "\\par\n");
        } else {
            QByteArray text = el.text().toUtf8();
            if (!text.trimmed().isEmpty())
                rtf.append(text + "\\par\n");
        }
    }

    return rtf;
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

static void appendFileHeader(QByteArray &buf, quint32 reservedSpace, quint32 usedSpace, quint8 flags = 0x04)
{
    appendDwordLE(buf, reservedSpace);
    appendDwordLE(buf, usedSpace);
    buf.append(static_cast<char>(flags));
}

static QByteArray buildSystemData(const QString &title)
{
    QByteArray sys;
    appendWordLE(sys, 0x036C);
    appendWordLE(sys, 1);
    appendWordLE(sys, 1);
    appendDwordLE(sys, 0);
    appendWordLE(sys, 0);

    QByteArray titleBytes = title.toUtf8();
    appendWordLE(sys, 3);
    appendWordLE(sys, static_cast<quint16>(titleBytes.size() + 1));
    sys.append(titleBytes);
    sys.append('\0');

    return sys;
}

static QByteArray buildFontData()
{
    QByteArray font;
    appendWordLE(font, 1);
    appendWordLE(font, 1);
    appendWordLE(font, 16);
    appendWordLE(font, 20);
    appendWordLE(font, 0);
    appendWordLE(font, 0);
    appendWordLE(font, 0);
    appendWordLE(font, 0);

    font.append("Arial\0", 6);

    font.append(static_cast<char>(0));
    font.append(static_cast<char>(20));
    font.append(static_cast<char>(3));
    appendWordLE(font, 0);
    font.append(static_cast<char>(0));
    font.append(static_cast<char>(0));
    font.append(static_cast<char>(0));
    font.append(static_cast<char>(0));
    font.append(static_cast<char>(0));
    font.append(static_cast<char>(0));

    return font;
}

static QByteArray buildDirData(const QList<QByteArray> &fileNames,
                                const QList<quint32> &fileOffsets)
{
    QByteArray btree;

    quint16 magic = 0x293B;
    quint16 flags = 0x0602;
    quint16 pageSize = 0x0400;
    appendWordLE(btree, magic);
    appendWordLE(btree, flags);
    appendWordLE(btree, pageSize);

    QByteArray structure("dir");
    structure.append(16 - structure.size(), '\0');
    btree.append(structure);

    appendWordLE(btree, 0);
    appendWordLE(btree, 0);
    appendWordLE(btree, 0);
    appendWordLE(btree, static_cast<quint16>(0xFFFF));
    appendWordLE(btree, 1);
    appendWordLE(btree, 1);
    appendDwordLE(btree, static_cast<quint32>(fileNames.size()));

    quint16 nodeUnknown = 0;
    appendWordLE(btree, nodeUnknown);
    appendWordLE(btree, static_cast<quint16>(fileNames.size()));
    appendWordLE(btree, static_cast<quint16>(0xFFFF));
    appendWordLE(btree, static_cast<quint16>(0xFFFF));

    for (int i = 0; i < fileNames.size(); ++i) {
        btree.append(fileNames.at(i));
        btree.append('\0');
        appendDwordLE(btree, fileOffsets.at(i));
    }

    return btree;
}

bool HlpPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc) return false;

    BookMeta meta = doc->meta();
    if (meta.title.isEmpty())
        meta.title = QFileInfo(filePath).completeBaseName();

    QList<QByteArray> topicRtfs;
    for (int i = 0; i < doc->pageCount(); ++i) {
        QString bceXml = doc->pageXml(i);
        QByteArray rtf = buildRtfFromBce(bceXml, QString());
        if (!rtf.isEmpty())
            topicRtfs.append(rtf);
    }
    if (topicRtfs.isEmpty()) {
        topicRtfs.append("{\\rtf1\\ansi Hello.\\par}");
    }

    QByteArray systemData = buildSystemData(meta.title);
    QByteArray fontData = buildFontData();

    QByteArray topicData;
    {
        QByteArray blockHdr;
        appendDwordLE(blockHdr, 12);
        appendDwordLE(blockHdr, 12);
        appendDwordLE(blockHdr, 12);
        topicData.append(blockHdr);

        quint32 topicOffset = 12;

        for (int i = 0; i < topicRtfs.size(); ++i) {
            const QByteArray &rtf = topicRtfs.at(i);
            quint32 dataLen1 = 19 + 24 + rtf.size();
            quint32 dataLen2 = rtf.size();
            quint32 blockSize = dataLen1;

            QByteArray link;
            appendDwordLE(link, blockSize);
            appendDwordLE(link, dataLen2);
            appendDwordLE(link, (i > 0) ? (topicOffset - topicRtfs.at(i-1).size() - 43) : 0);
            appendDwordLE(link, 0);
            appendDwordLE(link, dataLen1);
            link.append(static_cast<char>(0x02));

            appendDwordLE(link, blockSize);
            appendWordLE(link, static_cast<quint16>((i > 0) ? (i - 1) : 0));
            appendWordLE(link, 0);
            appendWordLE(link, static_cast<quint16>(i + 1));
            appendWordLE(link, 0);
            appendDwordLE(link, 0);
            appendDwordLE(link, 0);
            appendDwordLE(link, (i + 1 < topicRtfs.size()) ? (topicOffset + blockSize) : 0);

            link.append(rtf);

            topicData.append(link);
            topicOffset += blockSize;
        }
    }

    QList<QByteArray> internalFiles;
    QList<QByteArray> internalNames;
    internalNames.append("|SYSTEM");
    internalNames.append("|FONT");
    internalNames.append("|TOPIC");

    internalFiles.append(systemData);
    internalFiles.append(fontData);
    internalFiles.append(topicData);

    QByteArray dirData = buildDirData(internalNames, QList<quint32>());

    QList<quint32> internalOffsets;
    quint32 fileOffset = 16;

    for (int i = 0; i < internalFiles.size(); ++i) {
        internalOffsets.append(fileOffset);
        quint32 usedSpace = static_cast<quint32>(internalFiles.at(i).size());
        quint32 reservedSpace = usedSpace + 9;
        fileOffset += reservedSpace;
    }
    internalOffsets.append(fileOffset);
    quint32 dirUsedSpace = static_cast<quint32>(dirData.size());
    quint32 dirReservedSpace = dirUsedSpace + 9;
    fileOffset += dirReservedSpace;

    dirData.clear();
    dirData = buildDirData(internalNames, internalOffsets);

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QByteArray helpHeader;
    appendDwordLE(helpHeader, 0x00035F3F);
    appendDwordLE(helpHeader, fileOffset - dirReservedSpace);
    appendDwordLE(helpHeader, static_cast<quint32>(0xFFFFFFFF));
    appendDwordLE(helpHeader, fileOffset);
    outFile.write(helpHeader);

    for (int i = 0; i < internalFiles.size(); ++i) {
        QByteArray fhdr;
        quint32 usedSpace = static_cast<quint32>(internalFiles.at(i).size());
        quint32 reservedSpace = usedSpace + 9;
        appendFileHeader(fhdr, reservedSpace, usedSpace);
        outFile.write(fhdr);
        outFile.write(internalFiles.at(i));
    }

    {
        QByteArray fhdr;
        appendFileHeader(fhdr, dirReservedSpace, dirUsedSpace);
        outFile.write(fhdr);
        outFile.write(dirData);
    }

    outFile.close();

    if (m_context)
        m_context->log(QString("Help: exported %1 topics to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool HlpPlugin::checkDependencies() const
{
    return true;
}
