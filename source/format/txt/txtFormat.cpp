#include "txtFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QTextCodec>
#include <QRegularExpression>
#include <QDebug>

TxtPlugin::TxtPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

TxtPlugin::~TxtPlugin()
{
    shutdown();
}

QString TxtPlugin::name() const { return "Txt"; }
QString TxtPlugin::description() const { return "Plain text format plugin"; }
QString TxtPlugin::version() const { return "1.0.0"; }
QString TxtPlugin::author() const { return "BookCreateEdit"; }
QStringList TxtPlugin::dependencies() const { return {}; }

void TxtPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("Txt: initialized");
}

void TxtPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool TxtPlugin::isInitialized() const { return m_initialized; }

QString TxtPlugin::formatName() const { return "Txt"; }
QString TxtPlugin::fileExtension() const { return "txt"; }
QStringList TxtPlugin::supportedExtensions() const { return {"txt"}; }
QStringList TxtPlugin::mimeTypes() const { return {"text/plain"}; }

bool TxtPlugin::canRead() const { return true; }
bool TxtPlugin::canWrite() const { return true; }

static QString stripXmlTags(const QString &xml)
{
    QString result = xml;
    QRegularExpression tagRe("<[^>]*>");
    result.remove(tagRe);
    return result;
}

bool TxtPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");
    QString text = ts.readAll();
    file.close();

    if (text.isEmpty())
        return false;

    QFileInfo fi(filePath);
    BookMeta meta;
    meta.title = fi.completeBaseName();
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    QStringList paragraphs = text.split("\n\n", QString::SkipEmptyParts);

    if (paragraphs.isEmpty()) {
        doc->addPage(-1);
        QDomDocument bceDoc;
        bceDoc.setContent(QString("<page><body><p></p></body></page>"));
        doc->setPageXml(0, bceDoc.toString(-1));
    } else {
        for (int i = 0; i < paragraphs.size(); ++i) {
            QString para = paragraphs.at(i).trimmed();
            if (para.isEmpty())
                continue;

            doc->addPage(i - 1);
            int pi = doc->pageCount() - 1;

            QStringList lines = para.split('\n', QString::SkipEmptyParts);
            QString bceBody;
            for (const QString &line : lines) {
                bceBody += "<p>" + line.toHtmlEscaped() + "</p>\n";
            }

            QString pageXml = QString("<page id=\"p%1\"><body>%2</body></page>")
                                  .arg(pi + 1)
                                  .arg(bceBody);
            doc->setPageXml(pi, pageXml);
        }
    }

    if (m_context)
        m_context->log(QString("Txt: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool TxtPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");

    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageXml = doc->pageXml(i);
        QDomDocument bceDoc;
        if (bceDoc.setContent(pageXml)) {
            QDomElement root = bceDoc.documentElement();
            QDomElement body = root.firstChildElement("body");
            if (!body.isNull()) {
                QDomNodeList children = body.childNodes();
                for (int j = 0; j < children.count(); ++j) {
                    QString text = stripXmlTags(children.at(j).toElement().text());
                    if (!text.trimmed().isEmpty()) {
                        ts << text.trimmed() << "\n\n";
                    }
                }
            }
        }
    }

    ts.flush();
    file.close();

    if (m_context)
        m_context->log(QString("Txt: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool TxtPlugin::checkDependencies() const
{
    return true;
}
