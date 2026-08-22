#include "mobiFormat.h"
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

static const int PALM_DB_HEADER_SIZE = 84;
static const int PALM_RECORD_ENTRY_SIZE = 8;
static const int COMPRESSION_PALMDOC = 2;

static QByteArray palmdocDecompress(const QByteArray &compressed)
{
    QByteArray output;
    output.reserve(compressed.size() * 3);
    const uchar *src = reinterpret_cast<const uchar*>(compressed.constData());
    int srcLen = compressed.size();
    int pos = 0;

    while (pos < srcLen) {
        uchar flag = src[pos++];
        if (flag == 0) break;
        if (flag >= 0x80) {
            if (pos + 1 >= srcLen) break;
            int dist = ((flag & 0x7F) << 8) | src[pos++];
            dist += 1;
            if (pos >= srcLen) break;
            int len = src[pos++] + 3;
            int start = output.size() - dist;
            if (start < 0) break;
            for (int i = 0; i < len; ++i)
                output.append(output.at(start + (i % dist)));
        } else {
            int len = flag + 1;
            if (pos + len > srcLen) len = srcLen - pos;
            output.append(reinterpret_cast<const char*>(src + pos), len);
            pos += len;
        }
    }
    return output;
}

static QByteArray palmdocCompress(const QByteArray &input)
{
    QByteArray output;
    output.reserve(input.size());
    const uchar *src = reinterpret_cast<const uchar*>(input.constData());
    int srcLen = input.size();
    int pos = 0;

    while (pos < srcLen) {
        int bestLen = 0, bestDist = 0;
        int searchStart = qMax(0, pos - 2048);
        for (int d = 1; d <= pos - searchStart; ++d) {
            int len = 0;
            int sp = pos - d;
            while (pos + len < srcLen && len < 10 && src[sp + (len % d)] == src[pos + len])
                len++;
            if (len >= 3 && len > bestLen) { bestLen = len; bestDist = d; }
        }
        if (bestLen >= 3) {
            int d1 = bestDist - 1;
            output.append(static_cast<char>(0x80 | ((d1 >> 8) & 0x7F)));
            output.append(static_cast<char>(d1 & 0xFF));
            output.append(static_cast<char>((bestLen - 3) & 0xFF));
            pos += bestLen;
        } else {
            int litLen = 1;
            while (pos + litLen < srcLen && litLen < 128) litLen++;
            output.append(static_cast<char>(litLen - 1));
            output.append(reinterpret_cast<const char*>(src + pos), litLen);
            pos += litLen;
        }
    }
    output.append(static_cast<char>(0));
    return output;
}

static void parseExth(const QByteArray &data, int offset, BookMeta &meta)
{
    if (offset + 12 > data.size()) return;
    if (data.mid(offset, 4) != "EXTH") return;
    int count = qFromBigEndian<quint32>(data.mid(offset + 8, 4));
    int pos = offset + 12;
    for (int i = 0; i < count && pos + 8 <= data.size(); ++i) {
        int type = qFromBigEndian<quint32>(data.mid(pos, 4));
        int len = qFromBigEndian<quint32>(data.mid(pos + 4, 4));
        if (pos + 8 + len > data.size()) break;
        QByteArray val = data.mid(pos + 8, len);
        if (type == 1) meta.author = QString::fromUtf8(val);
        else if (type == 5) meta.title = QString::fromUtf8(val);
        else if (type == 104) meta.language = QString::fromUtf8(val);
        pos += 8 + len;
    }
}

static QString stripHtmlTags(const QString &html)
{
    QString result = html;
    result.remove(QRegularExpression("<[^>]*>"));
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&nbsp;", " ");
    return result.trimmed();
}

MobiPlugin::MobiPlugin(QObject *parent)
    : QObject(parent), m_context(nullptr), m_initialized(false) {}
MobiPlugin::~MobiPlugin() { shutdown(); }
QString MobiPlugin::name() const { return "MOBI"; }
QString MobiPlugin::description() const { return "MOBI ebook format plugin"; }
QString MobiPlugin::version() const { return "1.0.0"; }
QString MobiPlugin::author() const { return "BookCreateEdit"; }
QStringList MobiPlugin::dependencies() const { return {}; }
void MobiPlugin::initialize(BookContext *ctx) { m_context = ctx; m_initialized = true; if (m_context) m_context->log("MOBI: initialized"); }
void MobiPlugin::shutdown() { m_initialized = false; m_context = nullptr; }
bool MobiPlugin::isInitialized() const { return m_initialized; }
QString MobiPlugin::formatName() const { return "MOBI"; }
QString MobiPlugin::fileExtension() const { return "mobi"; }
QStringList MobiPlugin::supportedExtensions() const { return {"mobi", "prc"}; }
QStringList MobiPlugin::mimeTypes() const { return {"application/x-mobipocket-ebook"}; }
bool MobiPlugin::canRead() const { return true; }
bool MobiPlugin::canWrite() const { return true; }

bool MobiPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray data = file.readAll();
    file.close();

    if (data.size() < PALM_DB_HEADER_SIZE) {
        if (m_context) m_context->log("MOBI: file too small");
        return false;
    }

    quint16 numRecords = qFromBigEndian<quint16>(data.mid(80, 2));
    if (numRecords == 0) {
        if (m_context) m_context->log("MOBI: no records");
        return false;
    }

    QList<quint32> offsets;
    int tablePos = PALM_DB_HEADER_SIZE;
    for (int i = 0; i < numRecords; ++i) {
        offsets.append(qFromBigEndian<quint32>(data.mid(tablePos, 4)));
        tablePos += PALM_RECORD_ENTRY_SIZE;
    }

    // Find MOBI header
    int mobiOff = -1;
    for (int i = static_cast<int>(offsets[0]); i < qMin(static_cast<int>(offsets[0]) + 1024, data.size() - 4); ++i) {
        if (data[i]=='M' && data[i+1]=='O' && data[i+2]=='B' && data[i+3]=='I') { mobiOff = i; break; }
    }
    if (mobiOff < 0) {
        if (m_context) m_context->log("MOBI: no MOBI header");
        return false;
    }

    quint32 hdrLen = qFromBigEndian<quint32>(data.mid(mobiOff + 4, 4));
    quint32 enc = qFromBigEndian<quint32>(data.mid(mobiOff + 12, 4));
    quint32 exthFlags = qFromBigEndian<quint32>(data.mid(mobiOff + 128, 4));
    bool hasExth = (exthFlags & 0x40) != 0;

    // Compression type at PalmDB header + 80 (after records table, it is actually in the record 0 header)
    // Actually compression is first 2 bytes of record 0 data
    quint16 compression = qFromBigEndian<quint16>(data.mid(offsets[0], 2));

    // Parse EXTH
    BookMeta meta;
    if (hasExth) {
        int exthPos = mobiOff + hdrLen;
        parseExth(data, exthPos, meta);
    }
    if (meta.title.isEmpty()) {
        QFileInfo fi(filePath);
        meta.title = fi.completeBaseName();
    }
    doc->setMeta(meta);

    while (doc->pageCount() > 0) doc->removePage(0);

    // Get text record offsets (skip record 0 = header+first text)
    // Record 0 contains MOBI header. Text records start from the PalmDB record data.
    // Each record after the header contains compressed text.
    // First text record starts right after MOBI+EXTH header in record 0.

    // Actually: PalmDB record 0 contains PalmDoc header (16 bytes) + MOBI header + EXTH + first chunk of text
    // Text records: record 1..N contain compressed text
    // PalmDoc header (first 16 bytes of record 0):
    //   0-1: compression type (1=none, 2=PalmDoc)
    //   2-3: text record count
    //   4-5: record size (usually 4096)
    //   6-7: attributes
    //   8-11: created
    //   12-15: modified

    int rec0 = offsets[0];
    quint16 textRecCount = qFromBigEndian<quint16>(data.mid(rec0 + 2, 2));
    quint16 recSize = qFromBigEndian<quint16>(data.mid(rec0 + 4, 2));
    if (recSize == 0) recSize = 4096;

    // Collect all text from records 1..textRecCount
    QByteArray allText;
    for (int i = 1; i <= textRecCount && i < numRecords; ++i) {
        int start = offsets[i];
        int end = (i + 1 < numRecords) ? offsets[i + 1] : data.size();
        QByteArray recData = data.mid(start, end - start);

        if (compression == COMPRESSION_PALMDOC)
            allText.append(palmdocDecompress(recData));
        else
            allText.append(recData);
    }

    // Determine encoding
    QString encoding = (enc == 65001) ? "UTF-8" : "Windows-1252";
    QString htmlText = QString::fromLatin1(allText);
    if (enc == 65001)
        htmlText = QString::fromUtf8(allText);

    // Split HTML by <h1>-<h6> or <mbp:pagebreak/>
    QStringList sections;
    QRegularExpression pageBreakRe("<mbp:pagebreak[^>]*/?>", QRegularExpression::CaseInsensitiveOption);
    htmlText.replace(pageBreakRe, "\n<!-- PAGE BREAK -->\n");

    // Split on headings or page breaks
    QRegularExpression headingRe("<h([1-6])[^>]*>", QRegularExpression::CaseInsensitiveOption);
    QStringList parts;
    int lastPos = 0;
    QRegularExpressionMatchIterator it = headingRe.globalMatch(htmlText);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        if (m.capturedStart() > lastPos) {
            QString before = htmlText.mid(lastPos, m.capturedStart() - lastPos).trimmed();
            if (!before.isEmpty()) parts.append(before);
        }
        lastPos = m.capturedStart();
    }
    if (lastPos < htmlText.size()) {
        QString tail = htmlText.mid(lastPos).trimmed();
        if (!tail.isEmpty()) parts.append(tail);
    }

    if (parts.isEmpty()) {
        QRegularExpression pbIt("<!-- PAGE BREAK -->");
        parts = htmlText.split(pbIt);
    }

    // Filter empty
    QStringList filtered;
    for (const QString &s : parts) {
        QString trimmed = s.trimmed();
        if (!trimmed.isEmpty() && trimmed != "<!-- PAGE BREAK -->")
            filtered.append(trimmed);
    }
    if (filtered.isEmpty()) filtered.append(htmlText.trimmed());

    for (int i = 0; i < filtered.size(); ++i) {
        QString bcePage = HtmlConverter::htmlToBcePage(filtered.at(i));
        doc->addPage(i - 1);
        doc->setPageXml(doc->pageCount() - 1, bcePage);
    }

    if (doc->pageCount() == 0) {
        doc->addPage(-1);
        QDomDocument bceDoc;
        bceDoc.setContent(QString("<page><body><p></p></body></page>"));
        doc->setPageXml(0, bceDoc.toString(-1));
    }

    if (m_context)
        m_context->log(QString("MOBI: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));
    return true;
}

bool MobiPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc) return false;

    BookMeta meta = doc->meta();

    // Build HTML from pages
    QString html;
    for (int i = 0; i < doc->pageCount(); ++i) {
        QString bceXml = doc->pageXml(i);
        QString pageHtml = HtmlConverter::bcePageToHtml(bceXml);
        if (i > 0) html += "<mbp:pagebreak/>\n";
        html += pageHtml + "\n";
    }

    QByteArray htmlBytes = html.toUtf8();
    QByteArray textRecord = palmdocCompress(htmlBytes);

    // Build MOBI structure
    int textRecSize = 4096;
    QList<QByteArray> textRecords;
    for (int pos = 0; pos < textRecord.size(); pos += textRecSize) {
        textRecords.append(textRecord.mid(pos, qMin(textRecSize, textRecord.size() - pos)));
    }
    if (textRecords.isEmpty()) textRecords.append(QByteArray());

    // EXTH header
    QByteArray exth;
    exth.append("EXTH", 4);
    QList<QPair<int, QByteArray>> exthRecords;
    if (!meta.title.isEmpty()) exthRecords.append({5, meta.title.toUtf8()});
    if (!meta.author.isEmpty()) exthRecords.append({1, meta.author.toUtf8()});
    if (!meta.language.isEmpty()) exthRecords.append({104, meta.language.toUtf8()});
    exth.append(QByteArray(4, '\0')); // placeholder for count
    qToBigEndian(static_cast<quint32>(exthRecords.size()), exth.data() + 4);
    for (auto &r : exthRecords) {
        exth.append(QByteArray(4, '\0'));
        qToBigEndian(static_cast<quint32>(r.first), exth.data() + exth.size() - 4);
        exth.append(QByteArray(4, '\0'));
        qToBigEndian(static_cast<quint32>(r.second.size() + 8), exth.data() + exth.size() - 4);
        exth.append(r.second);
    }

    // MOBI header (232 bytes minimum)
    QByteArray mobiHdr(232, '\0');
    mobiHdr.replace(0, 4, "MOBI");
    qToBigEndian(static_cast<quint32>(232), mobiHdr.data() + 4);
    qToBigEndian(static_cast<quint32>(2), mobiHdr.data() + 8);   // mobi type
    qToBigEndian(static_cast<quint32>(65001), mobiHdr.data() + 12); // UTF-8
    qToBigEndian(static_cast<quint32>(textRecords.size()), mobiHdr.data() + 16); // text record count
    qToBigEndian(static_cast<quint32>(textRecSize), mobiHdr.data() + 20); // record size

    QByteArray fullName = meta.title.toUtf8();
    int fullNameOff = mobiHdr.size() + exth.size();
    qToBigEndian(static_cast<quint32>(fullNameOff), mobiHdr.data() + 80);
    qToBigEndian(static_cast<quint32>(fullName.size()), mobiHdr.data() + 84);
    // EXTH flag
    quint32 exthFlag = exthRecords.isEmpty() ? 0 : 0x40;
    qToBigEndian(exthFlag, mobiHdr.data() + 128);
    // first image index = 0 (no images)
    qToBigEndian(static_cast<quint32>(0), mobiHdr.data() + 108);

    // PalmDB record 0: PalmDoc header (16) + MOBI header + EXTH + full name
    QByteArray palmDocHdr(16, '\0');
    qToBigEndian(static_cast<quint16>(COMPRESSION_PALMDOC), palmDocHdr.data());
    qToBigEndian(static_cast<quint16>(textRecords.size()), palmDocHdr.data() + 2);
    qToBigEndian(static_cast<quint16>(textRecSize), palmDocHdr.data() + 4);

    QByteArray rec0 = palmDocHdr + mobiHdr + exth + fullName;
    // Pad rec0 to 4096
    if (rec0.size() < textRecSize) rec0.append(textRecSize - rec0.size(), '\0');

    // Collect all records
    QList<QByteArray> allRecords;
    allRecords.append(rec0);
    for (const QByteArray &r : textRecords) allRecords.append(r);

    // PalmDB header
    QByteArray palmHdr(PALM_DB_HEADER_SIZE, '\0');
    QByteArray dbName = meta.title.left(31).toUtf8();
    palmHdr.replace(0, qMin(dbName.size(), 31), dbName);
    qToBigEndian(static_cast<quint32>(0x01000000), palmHdr.data() + 60); // type BOOK
    qToBigEndian(static_cast<quint32>(0x4D4F4249), palmHdr.data() + 64); // creator MOBI
    qToBigEndian(static_cast<quint16>(allRecords.size()), palmHdr.data() + 80);

    // Calculate offsets
    quint32 recListOffset = PALM_DB_HEADER_SIZE + allRecords.size() * PALM_RECORD_ENTRY_SIZE;
    qToBigEndian(recListOffset, palmHdr.data() + 76);

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    outFile.write(palmHdr);

    // Write record offsets table
    quint32 offset = recListOffset;
    for (const QByteArray &r : allRecords) {
        outFile.write(QByteArray(reinterpret_cast<const char*>(&offset), 4));
        outFile.write(QByteArray::fromHex("00000000")); // attributes + id
        offset += r.size();
    }

    // Write records
    for (const QByteArray &r : allRecords)
        outFile.write(r);

    outFile.close();

    if (m_context)
        m_context->log(QString("MOBI: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));
    return true;
}

bool MobiPlugin::checkDependencies() const { return true; }
