#include "elfFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QDebug>
#include <QtEndian>

static const char *BCE_MAGIC = "BCEBOOK";

static QByteArray minimalElfStub()
{
    QByteArray elf;
    elf.resize(0x200);
    elf.fill('\0');

    // ELF32 header (52 bytes)
    // e_ident[0..3]: magic 7f 45 4c 46
    elf[0] = '\x7F'; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    // e_ident[4]: class = 1 (ELFCLASS32)
    elf[4] = '\x01';
    // e_ident[5]: data = 1 (little-endian)
    elf[5] = '\x01';
    // e_ident[6]: version = 1
    elf[6] = '\x01';
    // e_ident[7]: OS/ABI = 0 (ELFOSABI_NONE)
    elf[7] = '\x00';
    // e_ident[8..15]: padding
    // e_type = 2 (ET_EXEC)
    elf[16] = '\x02'; elf[17] = '\x00';
    // e_machine = 3 (EM_386)
    elf[18] = '\x03'; elf[19] = '\x00';
    // e_version = 1
    elf[20] = '\x01';
    // e_entry = 0x08049000 (entry point in virtual memory)
    elf[24] = '\x00'; elf[25] = '\x90'; elf[26] = '\x04'; elf[27] = '\x08';
    // e_phoff = 52 (program header offset, right after ELF header)
    elf[28] = '\x34';
    // e_shoff = 0 (no section headers)
    // e_flags = 0
    // e_ehsize = 52
    elf[40] = '\x34';
    // e_phentsize = 32
    elf[42] = '\x20';
    // e_phnum = 1
    elf[44] = '\x01';
    // e_shentsize = 40
    elf[46] = '\x28';
    // e_shnum = 0
    // e_shstrndx = 0

    // Program header (32 bytes at offset 52)
    int phdr = 52;
    // p_type = 1 (PT_LOAD)
    elf[phdr + 0] = '\x01';
    // p_offset = 0 (load from start of file)
    // p_vaddr = 0x08049000
    elf[phdr + 4] = '\x00'; elf[phdr + 5] = '\x90'; elf[phdr + 6] = '\x04'; elf[phdr + 7] = '\x08';
    // p_paddr = 0x08049000
    elf[phdr + 8] = '\x00'; elf[phdr + 9] = '\x90'; elf[phdr + 10] = '\x04'; elf[phdr + 11] = '\x08';
    // p_filesz = 0x200 (whole file)
    elf[phdr + 12] = '\x00'; elf[phdr + 13] = '\x02'; elf[phdr + 14] = '\x00'; elf[phdr + 15] = '\x00';
    // p_memsz = 0x200
    elf[phdr + 16] = '\x00'; elf[phdr + 17] = '\x02'; elf[phdr + 18] = '\x00'; elf[phdr + 19] = '\x00';
    // p_flags = 5 (PF_R | PF_X)
    elf[phdr + 20] = '\x05';
    // p_align = 0x1000
    elf[phdr + 24] = '\x00'; elf[phdr + 25] = '\x10'; elf[phdr + 26] = '\x00'; elf[phdr + 27] = '\x00';

    // Code at offset 84 (52 + 32): xor eax,eax; ret
    elf[84] = '\x31'; elf[85] = '\xC0'; elf[86] = '\xC3';

    return elf;
}

ElfPlugin::ElfPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

ElfPlugin::~ElfPlugin() { shutdown(); }

QString ElfPlugin::name() const { return "ELF"; }
QString ElfPlugin::description() const { return "Self-contained ELF executable book format"; }
QString ElfPlugin::version() const { return "1.0.0"; }
QString ElfPlugin::author() const { return "BookCreateEdit"; }
QStringList ElfPlugin::dependencies() const { return {}; }

void ElfPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context) m_context->log("ELF: initialized");
}

void ElfPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool ElfPlugin::isInitialized() const { return m_initialized; }

QString ElfPlugin::formatName() const { return "ELF"; }
QString ElfPlugin::fileExtension() const { return "elf"; }
QStringList ElfPlugin::supportedExtensions() const { return {"elf"}; }
QStringList ElfPlugin::mimeTypes() const { return {"application/x-elf"}; }

bool ElfPlugin::canRead() const { return false; } // ELF — только запись (dosc.md)
bool ElfPlugin::canWrite() const { return true; }

bool ElfPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray data = file.readAll();
    file.close();

    // Verify ELF magic
    if (data.size() < 16 || data[0] != '\x7F' || data[1] != 'E' || data[2] != 'L' || data[3] != 'F') {
        if (m_context) m_context->log("ELF: not a valid ELF file");
        return false;
    }

    int idx = data.indexOf(BCE_MAGIC, 0);
    if (idx < 0) {
        if (m_context) m_context->log("ELF: no BCE marker found");
        return false;
    }

    if (idx + 15 > data.size()) {
        if (m_context) m_context->log("ELF: truncated BCE header");
        return false;
    }

    quint32 offset = qFromLittleEndian<quint32>(data.mid(idx + 7, 4));
    quint32 size = qFromLittleEndian<quint32>(data.mid(idx + 11, 4));

    if ((quint32)(offset + size) > (quint32)data.size()) {
        if (m_context) m_context->log("ELF: BCE data extends beyond file");
        return false;
    }

    QByteArray xmlData = data.mid(offset, size);
    QString xml = QString::fromUtf8(xmlData);
    if (xml.isEmpty()) {
        if (m_context) m_context->log("ELF: BCE data is empty");
        return false;
    }

    QDomDocument xmlDoc;
    QString err;
    int errLine;
    if (!xmlDoc.setContent(xml, &err, &errLine)) {
        if (m_context)
            m_context->log(QString("ELF: XML parse error at line %1: %2").arg(errLine).arg(err));
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
        m_context->log(QString("ELF: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool ElfPlugin::write(BookDocument *doc, const QString &filePath)
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

    QByteArray elfStub = minimalElfStub();

    quint32 dataOffset = (quint32)elfStub.size();
    quint32 dataSize = (quint32)xmlData.size();

    QByteArray header;
    header.append(BCE_MAGIC, 7);
    header.append(reinterpret_cast<const char*>(&dataOffset), 4);
    header.append(reinterpret_cast<const char*>(&dataSize), 4);

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    outFile.write(elfStub);
    outFile.write(header);
    outFile.write(xmlData);
    outFile.close();

    if (m_context)
        m_context->log(QString("ELF: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool ElfPlugin::checkDependencies() const { return true; }
