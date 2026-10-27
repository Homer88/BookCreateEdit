#include "visualFormat.h"
#include "BookDocument.h"
#include "HtmlConverter.h"
#include "../ziputil.h"

#include <QFile>
#include <QFileInfo>
#include <QDomDocument>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>
#include <QtEndian>
#include <QDebug>
#include <QMap>

// ---------------------------------------------------------------------------
// Минимальный ZIP-райтер (метод stored, без сжатия) — как в EPUB/DOCX.
// ---------------------------------------------------------------------------
class MshcZipWriter
{
public:
    struct Entry {
        QString name;
        QByteArray data;
        quint32 crc;
        quint32 offset;
    };

    void addFile(const QString &name, const QByteArray &data)
    {
        Entry e;
        e.name = name;
        e.data = data;
        e.crc = computeCrc(data);
        m_entries.append(e);
    }

    QByteArray toArray()
    {
        QByteArray localHeaders;
        QByteArray centralDir;
        quint32 offset = 0;

        for (int i = 0; i < m_entries.size(); ++i) {
            Entry &e = m_entries[i];
            e.offset = offset;

            QByteArray nameBytes = e.name.toUtf8();

            localHeaders.append("PK\x03\x04", 4);
            localHeaders.append(qToLittleEndian<quint16>(20), 2);   // версия
            localHeaders.append(qToLittleEndian<quint16>(0), 2);    // флаги
            localHeaders.append(qToLittleEndian<quint16>(0), 2);    // stored
            localHeaders.append(qToLittleEndian<quint16>(0), 2);    // время
            localHeaders.append(qToLittleEndian<quint16>(0), 2);    // дата
            localHeaders.append(qToLittleEndian<quint32>(e.crc), 4);
            localHeaders.append(qToLittleEndian<quint32>(e.data.size()), 4);
            localHeaders.append(qToLittleEndian<quint32>(e.data.size()), 4);
            localHeaders.append(qToLittleEndian<quint16>(nameBytes.size()), 2);
            localHeaders.append(qToLittleEndian<quint16>(0), 2);
            localHeaders.append(nameBytes);
            localHeaders.append(e.data);

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
            centralDir.append(qToLittleEndian<quint32>(0), 4);
            centralDir.append(qToLittleEndian<quint32>(e.offset), 4);
            centralDir.append(nameBytes);

            offset += 4 + 26 + nameBytes.size() + e.data.size();
        }

        quint32 cdOffset = offset;
        quint32 cdSize = centralDir.size();

        QByteArray end;
        end.append("PK\x05\x06", 4);
        end.append(qToLittleEndian<quint16>(0), 2);
        end.append(qToLittleEndian<quint16>(0), 2);
        end.append(qToLittleEndian<quint16>(m_entries.size()), 2);
        end.append(qToLittleEndian<quint16>(m_entries.size()), 2);
        end.append(qToLittleEndian<quint32>(cdSize), 4);
        end.append(qToLittleEndian<quint32>(cdOffset), 4);
        end.append(qToLittleEndian<quint16>(0), 2);

        return localHeaders + centralDir + end;
    }

private:
    static quint32 computeCrc(const QByteArray &data)
    {
        quint32 crc = 0xFFFFFFFF;
        const uchar *buf = reinterpret_cast<const uchar *>(data.constData());
        for (int i = 0; i < data.size(); ++i) {
            crc ^= buf[i];
            for (int j = 0; j < 8; ++j)
                crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
        }
        return ~crc;
    }

    QList<Entry> m_entries;
};

/**
 * @brief Разбирает ZIP-контейнер и возвращает все записи с их содержимым.
 *
 * Через общий ZipUtil поддерживаются и stored, и deflate — это позволяет
 * читать .mshc, собранные сторонними инструментами (раньше понимались
 * только несжатые записи).
 */
static QMap<QString, QByteArray> readMshcEntries(const QByteArray &data,
                                                 QString *error = nullptr)
{
    QMap<QString, QByteArray> out;
    const auto entries = ZipUtil::readZip(data, error);
    for (const ZipUtil::ZipEntry &e : entries)
        if (!e.data.isEmpty())
            out.insert(e.name, e.data);
    return out;
}

// ---------------------------------------------------------------------------

VisualPlugin::VisualPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

VisualPlugin::~VisualPlugin() { shutdown(); }

QString VisualPlugin::name() const { return "VSHelp"; }
QString VisualPlugin::description() const
{
    return "Visual Studio Help (.mshc) format plugin";
}
QString VisualPlugin::version() const { return "1.0.0"; }
QString VisualPlugin::author() const { return "BookCreateEdit"; }
QStringList VisualPlugin::dependencies() const { return {}; }

void VisualPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("VSHelp: initialized");
}

void VisualPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool VisualPlugin::isInitialized() const { return m_initialized; }

QString VisualPlugin::formatName() const { return "VS Help"; }
QString VisualPlugin::fileExtension() const { return "mshc"; }
QStringList VisualPlugin::supportedExtensions() const { return {"mshc", "vshelp"}; }
QStringList VisualPlugin::mimeTypes() const { return {"application/x-mshc"}; }

bool VisualPlugin::canRead() const { return true; }
bool VisualPlugin::canWrite() const { return true; }

void VisualPlugin::parseMetadataXml(const QString &xml, BookMeta &meta)
{
    QDomDocument doc;
    if (!doc.setContent(xml))
        return;

    QDomElement root = doc.documentElement();
    auto textOf = [&root](const QString &tag) -> QString {
        QDomNodeList nodes = root.elementsByTagName(tag);
        return nodes.isEmpty() ? QString()
                               : nodes.at(0).toElement().text().trimmed();
    };

    QString t = textOf("title");
    if (!t.isEmpty()) meta.title = t;
    QString a = textOf("author");
    if (!a.isEmpty()) meta.author = a;
    QString l = textOf("locale");
    if (l.isEmpty()) l = textOf("language");
    if (!l.isEmpty()) meta.language = l.left(2);
}

QString VisualPlugin::metadataXml(const BookMeta &meta) const
{
    QString xml;
    xml += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    xml += "<package xmlns=\"http://schemas.microsoft.com/help/2010/msxlp\" ";
    xml += "version=\"1.0\">\n";
    xml += QString("  <title>%1</title>\n").arg(meta.title.toHtmlEscaped());
    xml += QString("  <author>%1</author>\n").arg(meta.author.toHtmlEscaped());
    xml += QString("  <locale>%1</locale>\n")
               .arg(meta.language.isEmpty() ? "ru" : meta.language);
    xml += QString("  <version>1.0</version>\n");
    xml += QString("  <created>%1</created>\n")
               .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    xml += "</package>\n";
    return xml;
}

QString VisualPlugin::tocXml(const BookMeta &meta,
                             const QStringList &titles,
                             const QStringList &files) const
{
    QString xml;
    xml += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    xml += "<toc title=\"" + meta.title.toHtmlEscaped() + "\">\n";
    for (int i = 0; i < titles.size() && i < files.size(); ++i) {
        xml += QString("  <topic src=\"%1\" title=\"%2\"/>\n")
                   .arg(files.at(i), titles.at(i).toHtmlEscaped());
    }
    xml += "</toc>\n";
    return xml;
}

QString VisualPlugin::wrapXhtml(const QString &title, const QString &bodyHtml) const
{
    QString x;
    x += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    x += "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" ";
    x += "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n";
    x += "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n<head>\n";
    x += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\n";
    x += "<title>" + title.toHtmlEscaped() + "</title>\n";
    x += "</head>\n<body>\n";
    x += bodyHtml;
    x += "\n</body>\n</html>\n";
    return x;
}

QString VisualPlugin::extractBody(const QString &xhtml) const
{
    QDomDocument doc;
    if (!doc.setContent(xhtml)) {
        // Фолбэк: грубое извлечение регуляркой
        QRegularExpression re("<body[^>]*>(.*)</body>",
                              QRegularExpression::DotMatchesEverythingOption);
        auto m = re.match(xhtml);
        return m.hasMatch() ? m.captured(1) : xhtml;
    }
    QDomElement body = doc.documentElement().firstChildElement("body");
    if (body.isNull())
        return xhtml;

    QString out;
    QTextStream ts(&out);
    QDomNodeList children = body.childNodes();
    for (int i = 0; i < children.count(); ++i)
        children.at(i).save(ts, 0);
    return out;
}

bool VisualPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QByteArray zip = file.readAll();
    file.close();

    // Контейнер должен начинаться с сигнатуры ZIP
    if (!zip.startsWith("PK\x03\x04")) {
        if (m_context)
            m_context->log("VSHelp: not a ZIP container");
        return false;
    }

    QString zipError;
    const QMap<QString, QByteArray> entries = readMshcEntries(zip, &zipError);
    if (entries.isEmpty()) {
        if (m_context)
            m_context->log("VSHelp: нет читаемых записей в контейнере (" + zipError + ")");
        return false;
    }

    BookMeta meta;
    meta.title = QFileInfo(filePath).completeBaseName();

    while (doc->pageCount() > 0)
        doc->removePage(0);

    // 1. Метаданные пакета (*book*.mshc.xml / *.xml без content/)
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        if ((it.key().endsWith(".mshc.xml") || it.key() == "book.mshc.xml") &&
                !it.key().startsWith("content/")) {
            parseMetadataXml(QString::fromUtf8(it.value()), meta);
            break;
        }
    }
    if (meta.title.isEmpty())
        meta.title = QFileInfo(filePath).completeBaseName();

    // 2. Оглавление toc.xml → порядок тем
    QStringList orderedFiles;
    QStringList orderedTitles;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        if (it.key().endsWith("toc.xml", Qt::CaseInsensitive)) {
            QDomDocument tocDoc;
            if (tocDoc.setContent(it.value())) {
                QDomNodeList topics =
                    tocDoc.documentElement().elementsByTagName("topic");
                for (int i = 0; i < topics.count(); ++i) {
                    QDomElement t = topics.at(i).toElement();
                    QString src = t.attribute("src");
                    if (!src.isEmpty()) {
                        orderedFiles.append(src);
                        orderedTitles.append(t.attribute("title"));
                    }
                }
            }
            break;
        }
    }

    // 3. Темы: XHTML из content/
    QStringList pageNames;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QString n = it.key();
        if (!n.startsWith("content/") &&
            !n.contains("/content/", Qt::CaseInsensitive))
            continue;
        if (!n.endsWith(".xhtml", Qt::CaseInsensitive) &&
            !n.endsWith(".html", Qt::CaseInsensitive) &&
            !n.endsWith(".htm", Qt::CaseInsensitive))
            continue;
        pageNames.append(n);
    }
    pageNames.sort();

    // Порядок: сначала темы из toc.xml, затем остальные по алфавиту
    QStringList finalOrder;
    for (const QString &f : orderedFiles) {
        for (const QString &p : pageNames) {
            if (p.endsWith(f, Qt::CaseInsensitive) ||
                    f.endsWith(p, Qt::CaseInsensitive)) {
                finalOrder.append(p);
                break;
            }
        }
    }
    for (const QString &p : pageNames)
        if (!finalOrder.contains(p))
            finalOrder.append(p);

    int added = 0;
    for (const QString &name : finalOrder) {
        if (!entries.contains(name))
            continue;
        QString xhtml = QString::fromUtf8(entries.value(name));
        QString body = extractBody(xhtml);
        QString bcePage = HtmlConverter::htmlToBcePage(body);
        doc->addPage(doc->pageCount() - 1);
        doc->setPageXml(doc->pageCount() - 1, bcePage);
        ++added;
    }

    // 4. Совсем пусто — создаём одну пустую страницу
    if (doc->pageCount() == 0) {
        doc->addPage(-1);
        doc->setPageXml(0, QString("<page><body><p></p></body></page>"));
    }

    doc->setMeta(meta);

    if (m_context)
        m_context->log(QString("VSHelp: imported %1 topics from %2")
                           .arg(added).arg(filePath));

    return true;
}

bool VisualPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    BookMeta meta = doc->meta();
    if (meta.title.isEmpty())
        meta.title = QFileInfo(filePath).completeBaseName();

    MshcZipWriter zip;

    // Метаданные пакета
    zip.addFile("book.mshc.xml", metadataXml(meta).toUtf8());

    // Темы
    QStringList titles;
    QStringList files;
    for (int i = 0; i < doc->pageCount(); ++i) {
        QString bceXml = doc->pageXml(i);
        QString body = HtmlConverter::bcePageToHtml(bceXml);
        QString title = QString("%1 — %2").arg(meta.title).arg(i + 1);
        QString fname = QString("content/page%1.xhtml")
                            .arg(i + 1, 4, 10, QChar('0'));
        zip.addFile(fname, wrapXhtml(title, body).toUtf8());
        titles.append(QString("Страница %1").arg(i + 1));
        files.append(fname);
    }

    // Оглавление
    zip.addFile("toc.xml", tocXml(meta, titles, files).toUtf8());

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    outFile.write(zip.toArray());
    outFile.close();

    if (m_context)
        m_context->log(QString("VSHelp: exported %1 pages to %2")
                           .arg(doc->pageCount()).arg(filePath));

    return true;
}

bool VisualPlugin::checkDependencies() const { return true; }
