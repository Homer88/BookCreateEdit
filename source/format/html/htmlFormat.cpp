#include "htmlFormat.h"
#include "BookDocument.h"
#include "HtmlConverter.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>
#include <QRegularExpression>
#include <QDebug>

HtmlPlugin::HtmlPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

HtmlPlugin::~HtmlPlugin()
{
    shutdown();
}

QString HtmlPlugin::name() const { return "Html"; }
QString HtmlPlugin::description() const { return "HTML format plugin"; }
QString HtmlPlugin::version() const { return "1.0.0"; }
QString HtmlPlugin::author() const { return "BookCreateEdit"; }
QStringList HtmlPlugin::dependencies() const { return {}; }

void HtmlPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("Html: initialized");
}

void HtmlPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool HtmlPlugin::isInitialized() const { return m_initialized; }

QString HtmlPlugin::formatName() const { return "Html"; }
QString HtmlPlugin::fileExtension() const { return "html"; }
QStringList HtmlPlugin::supportedExtensions() const { return {"html", "htm"}; }
QStringList HtmlPlugin::mimeTypes() const { return {"text/html"}; }

bool HtmlPlugin::canRead() const { return true; }
bool HtmlPlugin::canWrite() const { return true; }

static QStringList splitHtmlByHeadings(const QString &html)
{
    QStringList sections;
    QRegularExpression re("<h([1-6])[^>]*>(.*?)</h\\1>", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

    int lastPos = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(html);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        if (match.capturedStart() > lastPos) {
            QString before = html.mid(lastPos, match.capturedStart() - lastPos).trimmed();
            if (!before.isEmpty())
                sections.append(before);
        }
        lastPos = match.capturedStart();
    }
    if (lastPos < html.size()) {
        QString tail = html.mid(lastPos).trimmed();
        if (!tail.isEmpty())
            sections.append(tail);
    }

    if (sections.isEmpty() && !html.trimmed().isEmpty())
        sections.append(html.trimmed());

    return sections;
}

bool HtmlPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");
    QString html = ts.readAll();
    file.close();

    if (html.isEmpty())
        return false;

    QFileInfo fi(filePath);
    BookMeta meta;
    meta.title = fi.completeBaseName();
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    if (html.contains(QRegularExpression("<h[1-6][^>]*>", QRegularExpression::CaseInsensitiveOption))) {
        QStringList sections = splitHtmlByHeadings(html);
        for (int i = 0; i < sections.size(); ++i) {
            QString bceXml = HtmlConverter::htmlToBcePage(sections.at(i));
            doc->addPage(i - 1);
            doc->setPageXml(doc->pageCount() - 1, bceXml);
        }
    } else {
        QString bceXml = HtmlConverter::htmlToBcePage(html);
        doc->addPage(-1);
        doc->setPageXml(0, bceXml);
    }

    if (m_context)
        m_context->log(QString("Html: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool HtmlPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");
    ts << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"UTF-8\">\n";

    BookMeta m = doc->meta();
    if (!m.title.isEmpty())
        ts << "<title>" << m.title << "</title>\n";
    if (!m.author.isEmpty())
        ts << "<meta name=\"author\" content=\"" << m.author << "\">\n";

    ts << "<style>\n";
    ts << "body { font-family: sans-serif; max-width: 900px; margin: 0 auto; padding: 20px; }\n";
    ts << "pre { background: #f5f5f5; padding: 10px; overflow-x: auto; }\n";
    ts << "code { background: #f5f5f5; padding: 2px 4px; }\n";
    ts << "blockquote { border-left: 4px solid #ccc; margin-left: 0; padding-left: 15px; color: #555; }\n";
    ts << "table { border-collapse: collapse; width: 100%; }\n";
    ts << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    ts << "details { margin: 10px 0; } details summary { cursor: pointer; font-weight: bold; }\n";
    ts << "</style>\n";
    ts << "</head>\n<body>\n";

    for (int i = 0; i < doc->pageCount(); ++i) {
        QString bceXml = doc->pageXml(i);
        QString pageHtml = HtmlConverter::bcePageToHtml(bceXml);
        ts << pageHtml << "\n\n";
    }

    ts << "</body>\n</html>\n";
    ts.flush();
    file.close();

    if (m_context)
        m_context->log(QString("Html: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool HtmlPlugin::checkDependencies() const
{
    return true;
}
