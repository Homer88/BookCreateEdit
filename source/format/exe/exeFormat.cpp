#include "exeFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QDebug>
#include <QtEndian>

static const char *BCE_MAGIC = "BCEBOOK";

static QByteArray minimalPeStub()
{
    QByteArray pe;
    pe.resize(0x400);
    pe.fill('\0');

    // DOS header
    pe[0] = 'M'; pe[1] = 'Z';
    pe[0x3C] = (char)0x80; pe[0x3D] = 0x00; pe[0x3E] = 0x00; pe[0x3F] = 0x00;

    int peOff = 0x80;
    pe[peOff] = 'P'; pe[peOff+1] = 'E'; pe[peOff+2] = '\0'; pe[peOff+3] = '\0';

    int coff = peOff + 4;
    pe[coff] = '\x4C'; pe[coff+1] = '\x01';  // Machine: i386
    pe[coff+2] = '\x01'; pe[coff+3] = '\x00';  // NumberOfSections = 1
    pe[coff+16] = '\xE0'; pe[coff+17] = '\x00'; // SizeOfOptionalHeader = 224
    pe[coff+18] = '\x0F'; pe[coff+19] = '\x01'; // Characteristics

    int opt = coff + 20;
    pe[opt] = '\x0B'; pe[opt+1] = '\x01';  // Magic PE32

    // EntryPoint = 0x1000
    pe[opt+16] = '\x00'; pe[opt+17] = '\x10'; pe[opt+18] = '\x00'; pe[opt+19] = '\x00';
    // BaseOfCode = 0x1000
    pe[opt+20] = '\x00'; pe[opt+21] = '\x10'; pe[opt+22] = '\x00'; pe[opt+23] = '\x00';
    // ImageBase = 0x00400000
    pe[opt+28] = '\x00'; pe[opt+29] = '\x00'; pe[opt+30] = '\x40'; pe[opt+31] = '\x00';
    // SectionAlignment = 0x1000
    pe[opt+32] = '\x00'; pe[opt+33] = '\x10'; pe[opt+34] = '\x00'; pe[opt+35] = '\x00';
    // FileAlignment = 0x200
    pe[opt+36] = '\x00'; pe[opt+37] = '\x02'; pe[opt+38] = '\x00'; pe[opt+39] = '\x00';
    // SizeOfImage = 0x3000
    pe[opt+56] = '\x00'; pe[opt+57] = '\x30'; pe[opt+58] = '\x00'; pe[opt+59] = '\x00';
    // SizeOfHeaders = 0x200
    pe[opt+60] = '\x00'; pe[opt+61] = '\x02'; pe[opt+62] = '\x00'; pe[opt+63] = '\x00';
    // Subsystem = 3 (CONSOLE)
    pe[opt+68] = '\x03'; pe[opt+69] = '\x00';
    // NumberOfRvaAndSizes = 16
    pe[opt+92] = '\x10'; pe[opt+93] = '\x00'; pe[opt+94] = '\x00'; pe[opt+95] = '\x00';

    int sec = opt + 224;
    pe[sec] = '.'; pe[sec+1] = 't'; pe[sec+2] = 'e'; pe[sec+3] = 'x'; pe[sec+4] = 't';
    // VirtualAddress = 0x1000
    pe[sec+12] = '\x00'; pe[sec+13] = '\x10'; pe[sec+14] = '\x00'; pe[sec+15] = '\x00';
    // SizeOfRawData = 0x200
    pe[sec+16] = '\x00'; pe[sec+17] = '\x02'; pe[sec+18] = '\x00'; pe[sec+19] = '\x00';
    // PointerToRawData = 0x200
    pe[sec+20] = '\x00'; pe[sec+21] = '\x02'; pe[sec+22] = '\x00'; pe[sec+23] = '\x00';
    // Characteristics = 0x60000020
    pe[sec+36] = '\x20'; pe[sec+37] = '\x00'; pe[sec+38] = '\x00'; pe[sec+39] = '\x60';

    // Code at 0x200: xor eax,eax; ret
    pe[0x200] = '\x33'; pe[0x201] = '\xC0'; pe[0x202] = '\xC3';

    return pe;
}

ExePlugin::ExePlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

ExePlugin::~ExePlugin() { shutdown(); }

QString ExePlugin::name() const { return "Exe"; }
QString ExePlugin::description() const { return "Self-contained executable book format"; }
QString ExePlugin::version() const { return "1.0.0"; }
QString ExePlugin::author() const { return "BookCreateEdit"; }
QStringList ExePlugin::dependencies() const { return {}; }

void ExePlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context) m_context->log("Exe: initialized");
}

void ExePlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool ExePlugin::isInitialized() const { return m_initialized; }

QString ExePlugin::formatName() const { return "Exe"; }
QString ExePlugin::fileExtension() const { return "exe"; }
QStringList ExePlugin::supportedExtensions() const { return {"exe"}; }
QStringList ExePlugin::mimeTypes() const { return {"application/x-exe"}; }

bool ExePlugin::canRead() const { return false; } // EXE — только запись (dosc.md)
bool ExePlugin::canWrite() const { return true; }

bool ExePlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray data = file.readAll();
    file.close();

    int idx = data.indexOf(BCE_MAGIC, 0);
    if (idx < 0) {
        if (m_context) m_context->log("Exe: no BCE marker found");
        return false;
    }

    if (idx + 7 + 8 > data.size()) {
        if (m_context) m_context->log("Exe: truncated BCE header");
        return false;
    }

    quint32 offset = qFromLittleEndian<quint32>(data.mid(idx + 7, 4));
    quint32 size = qFromLittleEndian<quint32>(data.mid(idx + 11, 4));

    if ((quint32)(offset + size) > (quint32)data.size()) {
        if (m_context) m_context->log("Exe: BCE data extends beyond file");
        return false;
    }

    QByteArray xmlData = data.mid(offset, size);
    QString xml = QString::fromUtf8(xmlData);
    if (xml.isEmpty()) {
        if (m_context) m_context->log("Exe: BCE data is empty");
        return false;
    }

    QDomDocument xmlDoc;
    QString err;
    int errLine;
    if (!xmlDoc.setContent(xml, &err, &errLine)) {
        if (m_context)
            m_context->log(QString("Exe: XML parse error at line %1: %2").arg(errLine).arg(err));
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
        m_context->log(QString("Exe: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool ExePlugin::write(BookDocument *doc, const QString &filePath)
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

    QByteArray peStub = minimalPeStub();

    quint32 dataOffset = (quint32)peStub.size();
    quint32 dataSize = (quint32)xmlData.size();

    QByteArray header;
    header.append(BCE_MAGIC, 7);
    header.append(reinterpret_cast<const char*>(&dataOffset), 4);
    header.append(reinterpret_cast<const char*>(&dataSize), 4);

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    outFile.write(peStub);
    outFile.write(header);
    outFile.write(xmlData);
    outFile.close();

    if (m_context)
        m_context->log(QString("Exe: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool ExePlugin::checkDependencies() const { return true; }
