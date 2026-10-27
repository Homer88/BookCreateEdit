#include "machoFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QDebug>
#include <QtEndian>

static const char *BCE_MAGIC = "BCEBOOK";

static QByteArray minimalMachoStub()
{
    QByteArray macho;
    macho.resize(0x200);
    macho.fill('\0');

    // Mach-O 64-bit header (32 bytes)
    // magic: 0xFEEDFACF
    macho[0] = '\xCF'; macho[1] = '\xFA'; macho[2] = '\xED'; macho[3] = '\xFE';
    // cputype: CPU_TYPE_X86_64 (0x01000007)
    macho[4] = '\x07'; macho[5] = '\x00'; macho[6] = '\x00'; macho[7] = '\x01';
    // cpusubtype: CPU_SUBTYPE_X86_64_ALL (0x00000003)
    macho[8] = '\x03';
    // filetype: MH_EXECUTE (2)
    macho[12] = '\x02';
    // ncmds: 1
    macho[16] = '\x01';
    // sizeofcmds: 72 (segment command 72 bytes)
    macho[20] = '\x48';
    // flags: MH_DYLDLINK | MH_PIE (0x00000085)
    macho[24] = '\x85'; macho[25] = '\x00'; macho[26] = '\x00'; macho[27] = '\x00';

    // LC_SEGMENT_64 load command (72 bytes at offset 32)
    int lc = 32;
    // cmd: LC_SEGMENT_64 (0x19)
    macho[lc + 0] = '\x19';
    // cmdsize: 72
    macho[lc + 4] = '\x48';
    // segname: __TEXT (16 bytes, zero-padded)
    macho[lc + 8] = '_'; macho[lc + 9] = '_'; macho[lc + 10] = 'T'; macho[lc + 11] = 'E';
    macho[lc + 12] = 'X'; macho[lc + 13] = 'T';
    // vmaddr: 0x100000000
    macho[lc + 24] = '\x00'; macho[lc + 25] = '\x00'; macho[lc + 26] = '\x00'; macho[lc + 27] = '\x00';
    macho[lc + 28] = '\x01';
    // vmsize: 0x1000
    macho[lc + 32] = '\x00'; macho[lc + 33] = '\x10';
    // fileoff: 0
    // filesize: 0x200 (size of header block)
    macho[lc + 40] = '\x00'; macho[lc + 41] = '\x02';
    // maxprot: 7 (rwx)
    macho[lc + 44] = '\x07';
    // initprot: 5 (r-x)
    macho[lc + 48] = '\x05';
    // nsects: 1
    macho[lc + 56] = '\x01';

    // Section __text at offset 104 (32 header + 72 load command)
    int sec = lc + 72;
    // sectname: __text (16 bytes)
    macho[sec + 0] = '_'; macho[sec + 1] = '_'; macho[sec + 2] = 't'; macho[sec + 3] = 'e';
    macho[sec + 4] = 'x'; macho[sec + 5] = 't';
    // segname: __TEXT (16 bytes)
    macho[sec + 16] = '_'; macho[sec + 17] = '_'; macho[sec + 18] = 'T'; macho[sec + 19] = 'E';
    macho[sec + 20] = 'X'; macho[sec + 21] = 'T';
    // addr: 0x100000084 (vmaddr + section offset)
    macho[sec + 32] = '\x84'; macho[sec + 33] = '\x00'; macho[sec + 34] = '\x00'; macho[sec + 35] = '\x00';
    macho[sec + 36] = '\x01';
    // size: 3
    macho[sec + 40] = '\x03';
    // offset: 0x84 (32 header + 72 load command)
    macho[sec + 44] = '\x84';
    // align: 2
    macho[sec + 48] = '\x02';

    // Code at offset 0x84: xor eax,eax; ret
    macho[0x84] = '\x31'; macho[0x85] = '\xC0'; macho[0x86] = '\xC3';

    return macho;
}

MachoPlugin::MachoPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

MachoPlugin::~MachoPlugin() { shutdown(); }

QString MachoPlugin::name() const { return "Mach-O"; }
QString MachoPlugin::description() const { return "Self-contained Mach-O executable book format"; }
QString MachoPlugin::version() const { return "1.0.0"; }
QString MachoPlugin::author() const { return "BookCreateEdit"; }
QStringList MachoPlugin::dependencies() const { return {}; }

void MachoPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context) m_context->log("Mach-O: initialized");
}

void MachoPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool MachoPlugin::isInitialized() const { return m_initialized; }

QString MachoPlugin::formatName() const { return "Mach-O"; }
QString MachoPlugin::fileExtension() const { return "macho"; }
QStringList MachoPlugin::supportedExtensions() const { return {"macho"}; }
QStringList MachoPlugin::mimeTypes() const { return {"application/x-mach-binary"}; }

bool MachoPlugin::canRead() const { return false; } // APP/Mach-O — только запись (dosc.md)
bool MachoPlugin::canWrite() const { return true; }

bool MachoPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray data = file.readAll();
    file.close();

    // Verify Mach-O magic
    if (data.size() < 32) {
        if (m_context) m_context->log("Mach-O: file too small");
        return false;
    }

    quint32 magic = qFromLittleEndian<quint32>(data.mid(0, 4));
    if (magic != 0xFEEDFACF && magic != 0xFEEDFACE) {
        if (m_context) m_context->log("Mach-O: not a valid Mach-O file");
        return false;
    }

    int idx = data.indexOf(BCE_MAGIC, 0);
    if (idx < 0) {
        if (m_context) m_context->log("Mach-O: no BCE marker found");
        return false;
    }

    if (idx + 15 > data.size()) {
        if (m_context) m_context->log("Mach-O: truncated BCE header");
        return false;
    }

    quint32 offset = qFromLittleEndian<quint32>(data.mid(idx + 7, 4));
    quint32 size = qFromLittleEndian<quint32>(data.mid(idx + 11, 4));

    if ((quint32)(offset + size) > (quint32)data.size()) {
        if (m_context) m_context->log("Mach-O: BCE data extends beyond file");
        return false;
    }

    QByteArray xmlData = data.mid(offset, size);
    QString xml = QString::fromUtf8(xmlData);
    if (xml.isEmpty()) {
        if (m_context) m_context->log("Mach-O: BCE data is empty");
        return false;
    }

    QDomDocument xmlDoc;
    QString err;
    int errLine;
    if (!xmlDoc.setContent(xml, &err, &errLine)) {
        if (m_context)
            m_context->log(QString("Mach-O: XML parse error at line %1: %2").arg(errLine).arg(err));
        return false;
    }

    QDomElement root = xmlDoc.documentElement();

    BookMeta meta;
    QDomElement metaEl = root.firstChildElement("meta");
    if (!metaEl.isNull()) {
        meta.title = metaEl.firstChildElement("title").text();
        meta.author = metaEl.firstChildElement("author").text();
        meta.language = metaEl.firstChildElement("language").text();
    }
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    QDomElement pagesEl = root.firstChildElement("pages");
    if (!pagesEl.isNull()) {
        QDomNodeList pageNodes = pagesEl.elementsByTagName("page");
        for (int i = 0; i < pageNodes.count(); ++i) {
            QDomElement pageEl = pageNodes.at(i).toElement();
            QDomElement body = pageEl.firstChildElement("body");
            QString bodyXml;
            if (!body.isNull()) {
                QTextStream ts(&bodyXml);
                ts.setCodec("UTF-8");
                body.save(ts, 2);
            } else {
                bodyXml = "<body><p></p></body>";
            }
            QString fullPage = QString("<page>%1</page>").arg(bodyXml);

            doc->addPage(i - 1);
            doc->setPageXml(doc->pageCount() - 1, fullPage);
        }
    }

    if (doc->pageCount() == 0) {
        doc->addPage(-1);
        QDomDocument bceDoc;
        bceDoc.setContent(QString("<page><body><p></p></body></page>"));
        doc->setPageXml(0, bceDoc.toString(-1));
    }

    if (m_context)
        m_context->log(QString("Mach-O: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool MachoPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc) return false;

    BookMeta meta = doc->meta();

    QDomDocument xmlDoc;
    xmlDoc.setContent(QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<bcebook/>"));
    QDomElement root = xmlDoc.documentElement();

    QDomElement metaEl = xmlDoc.createElement("meta");
    QDomElement titleEl = xmlDoc.createElement("title");
    titleEl.appendChild(xmlDoc.createTextNode(meta.title));
    metaEl.appendChild(titleEl);
    QDomElement authorEl = xmlDoc.createElement("author");
    authorEl.appendChild(xmlDoc.createTextNode(meta.author));
    metaEl.appendChild(authorEl);
    QDomElement langEl = xmlDoc.createElement("language");
    langEl.appendChild(xmlDoc.createTextNode(meta.language));
    metaEl.appendChild(langEl);
    root.appendChild(metaEl);

    QDomElement pagesEl = xmlDoc.createElement("pages");
    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageXml = doc->pageXml(i);
        QDomDocument pageDoc;
        if (pageDoc.setContent(pageXml)) {
            QDomElement pageRoot = pageDoc.documentElement();
            QDomElement body = pageRoot.firstChildElement("body");
            if (!body.isNull()) {
                QDomElement pageEl = xmlDoc.createElement("page");
                QDomElement bodyClone = body.cloneNode().toElement();
                pageEl.appendChild(bodyClone);
                pagesEl.appendChild(pageEl);
            }
        }
    }
    root.appendChild(pagesEl);

    QByteArray xmlData;
    {
        QTextStream ts(&xmlData);
        ts.setCodec("UTF-8");
        xmlDoc.save(ts, 4);
    }

    QByteArray machoStub = minimalMachoStub();

    quint32 dataOffset = (quint32)machoStub.size();
    quint32 dataSize = (quint32)xmlData.size();

    QByteArray header;
    header.append(BCE_MAGIC, 7);
    header.append(reinterpret_cast<const char*>(&dataOffset), 4);
    header.append(reinterpret_cast<const char*>(&dataSize), 4);

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    outFile.write(machoStub);
    outFile.write(header);
    outFile.write(xmlData);
    outFile.close();

    if (m_context)
        m_context->log(QString("Mach-O: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool MachoPlugin::checkDependencies() const { return true; }
