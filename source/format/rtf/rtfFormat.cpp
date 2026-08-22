#include "rtfFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QStack>
#include <QRegularExpression>
#include <QDebug>

RtfPlugin::RtfPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

RtfPlugin::~RtfPlugin()
{
    shutdown();
}

QString RtfPlugin::name() const { return "RTF"; }
QString RtfPlugin::description() const { return "Rich Text Format plugin"; }
QString RtfPlugin::version() const { return "1.0.0"; }
QString RtfPlugin::author() const { return "BookCreateEdit"; }
QStringList RtfPlugin::dependencies() const { return {}; }

void RtfPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("RTF: initialized");
}

void RtfPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool RtfPlugin::isInitialized() const { return m_initialized; }

QString RtfPlugin::formatName() const { return "RTF"; }
QString RtfPlugin::fileExtension() const { return "rtf"; }
QStringList RtfPlugin::supportedExtensions() const { return {"rtf"}; }
QStringList RtfPlugin::mimeTypes() const { return {"application/rtf", "text/rtf"}; }

bool RtfPlugin::canRead() const { return true; }
bool RtfPlugin::canWrite() const { return true; }

struct RtfState {
    bool bold = false;
    bool italic = false;
    bool underline = false;
};

static QString rtfToPlainText(const QString &rtf)
{
    QString result;
    QStack<RtfState> stateStack;
    stateStack.push(RtfState());
    bool inControlWord = false;
    QString controlBuffer;
    bool skipGroup = false;
    int skipDepth = 0;

    int i = 0;
    int len = rtf.length();

    while (i < len) {
        QChar ch = rtf.at(i);

        if (ch == '{') {
            stateStack.push(stateStack.top());
            if (skipGroup)
                skipDepth++;
            i++;
            continue;
        }

        if (ch == '}') {
            if (!stateStack.isEmpty())
                stateStack.pop();
            if (skipGroup) {
                if (skipDepth > 0)
                    skipDepth--;
                else
                    skipGroup = false;
            }
            i++;
            continue;
        }

        if (skipGroup) {
            i++;
            continue;
        }

        if (ch == '\\') {
            i++;
            if (i >= len)
                break;

            QChar next = rtf.at(i);

            // Skip font table, stylesheet, color table, etc.
            if (next == '{' || next == '}') {
                // Could be a special char
                i++;
                continue;
            }

            if (next == '\'') {
                // Hex character: \'XX
                i++;
                if (i + 1 < len) {
                    QString hex = rtf.mid(i, 2);
                    bool ok;
                    int code = hex.toInt(&ok, 16);
                    if (ok) {
                        // Simple Windows-1251 to Unicode conversion
                        if (code >= 0xC0 && code <= 0xFF)
                            result += QChar(0x0410 + (code - 0xC0));
                        else if (code >= 0x80 && code <= 0xBF)
                            result += QChar(0x0400 + (code - 0x80));
                        else
                            result += QChar(code);
                    }
                    i += 2;
                }
                continue;
            }

            if (next.isLetter()) {
                // Control word
                controlBuffer.clear();
                while (i < len && rtf.at(i).isLetter()) {
                    controlBuffer += rtf.at(i);
                    i++;
                }

                // Skip optional numeric parameter
                QString numStr;
                if (i < len && (rtf.at(i) == '-' || rtf.at(i).isDigit())) {
                    if (rtf.at(i) == '-') {
                        numStr += rtf.at(i);
                        i++;
                    }
                    while (i < len && rtf.at(i).isDigit()) {
                        numStr += rtf.at(i);
                        i++;
                    }
                }

                // Skip space after control word
                if (i < len && rtf.at(i) == ' ')
                    i++;

                // Process control word
                if (controlBuffer == "par" || controlBuffer == "line" || controlBuffer == "page" || controlBuffer == "sect") {
                    result += "\n";
                } else if (controlBuffer == "tab") {
                    result += "\t";
                } else if (controlBuffer == "b" || controlBuffer == "b0") {
                    // Bold - ignore for plain text
                } else if (controlBuffer == "i" || controlBuffer == "i0") {
                    // Italic - ignore for plain text
                } else if (controlBuffer == "ul" || controlBuffer == "ul0") {
                    // Underline - ignore for plain text
                } else if (controlBuffer == "bullet") {
                    result += "*";
                } else if (controlBuffer == "emdash") {
                    result += QChar(0x2014);
                } else if (controlBuffer == "endash") {
                    result += QChar(0x2013);
                } else if (controlBuffer == "lquote") {
                    result += QChar(0x2018);
                } else if (controlBuffer == "rquote") {
                    result += QChar(0x2019);
                } else if (controlBuffer == "ldblquote") {
                    result += QChar(0x201C);
                } else if (controlBuffer == "rdblquote") {
                    result += QChar(0x201D);
                } else if (controlBuffer == "u") {
                    // Unicode character
                    bool ok;
                    int code = numStr.toInt(&ok);
                    if (ok) {
                        if (code < 0)
                            code += 65536;
                        result += QChar(code);
                    }
                } else if (controlBuffer == "fonttbl" || controlBuffer == "stylesheet" || controlBuffer == "colortbl" || controlBuffer == "info") {
                    skipGroup = true;
                    skipDepth = 0;
                }
                continue;
            }

            if (next == '\\') {
                result += '\\';
                i++;
                continue;
            }

            if (next == '{') {
                result += '{';
                i++;
                continue;
            }

            if (next == '}') {
                result += '}';
                i++;
                continue;
            }

            // Unknown control - skip
            continue;
        }

        // Regular character
        result += ch;
        i++;
    }

    // Clean up: remove excessive blank lines
    result.replace(QRegularExpression("\n{3,}"), "\n\n");
    return result.trimmed();
}

bool RtfPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file.readAll();
    file.close();

    // Detect encoding: look for \ansi or \ansi
    QString text = QString::fromLocal8Bit(data);

    // Check for RTF header
    if (!text.startsWith("{\\rtf"))
        return false;

    QString plainText = rtfToPlainText(text);

    if (plainText.isEmpty())
        return false;

    QFileInfo fi(filePath);
    BookMeta meta;
    meta.title = fi.completeBaseName();
    doc->setMeta(meta);

    while (doc->pageCount() > 0)
        doc->removePage(0);

    // Split into paragraphs by double newlines (sections)
    QStringList paragraphs = plainText.split("\n\n", QString::SkipEmptyParts);

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
                QString escapedLine = line.toHtmlEscaped();
                bceBody += "<p>" + escapedLine + "</p>\n";
            }

            QString pageXml = QString("<page id=\"p%1\"><body>%2</body></page>")
                                  .arg(pi + 1)
                                  .arg(bceBody);
            doc->setPageXml(pi, pageXml);
        }
    }

    if (m_context)
        m_context->log(QString("RTF: imported %1 pages from %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

static QString escapeRtfText(const QString &text)
{
    QString result;
    for (int i = 0; i < text.length(); ++i) {
        QChar ch = text.at(i);
        ushort code = ch.unicode();

        if (code == '\\')
            result += "\\\\";
        else if (code == '{')
            result += "\\{";
        else if (code == '}')
            result += "\\}";
        else if (code == '\n')
            result += "\\line\n";
        else if (code == '\r')
            continue;
        else if (code == '\t')
            result += "\\tab ";
        else if (code >= 0x80) {
            // Use Unicode escape
            result += QString("\\u%1?").arg(code > 32767 ? (int)code - 65536 : (int)code);
        } else {
            result += ch;
        }
    }
    return result;
}

bool RtfPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QTextStream ts(&file);
    ts.setCodec("UTF-8");

    BookMeta m = doc->meta();

    ts << "{\\rtf1\\ansi\\ansicpg1251\\deff0";
    ts << "{\\fonttbl{\\f0 Times New Roman;}{\\f1 Arial;}}";
    ts << "{\\colortbl;\\red0\\green0\\blue0;}";
    ts << "{\\info{\\title " << escapeRtfText(m.title) << "}"
        << "{\\author " << escapeRtfText(m.author) << "}}";

    for (int i = 0; i < doc->pageCount(); ++i) {
        QString pageXml = doc->pageXml(i);
        QDomDocument bceDoc;
        if (bceDoc.setContent(pageXml)) {
            QDomElement root = bceDoc.documentElement();
            QDomElement body = root.firstChildElement("body");
            if (body.isNull())
                continue;

            QDomNodeList children = body.childNodes();
            for (int j = 0; j < children.count(); ++j) {
                QDomElement el = children.at(j).toElement();
                QString tag = el.tagName();

                if (tag == "p") {
                    ts << "\\pard\\f0 ";
                    ts << escapeRtfText(el.text());
                    ts << "\\par\n";
                } else if (tag == "b" || tag == "strong") {
                    ts << "\\pard\\f0\\b " << escapeRtfText(el.text()) << "\\b0\\par\n";
                } else if (tag == "i" || tag == "em") {
                    ts << "\\pard\\f0\\i " << escapeRtfText(el.text()) << "\\i0\\par\n";
                } else if (tag == "u") {
                    ts << "\\pard\\f0\\ul " << escapeRtfText(el.text()) << "\\ul0\\par\n";
                } else if (tag == "h1") {
                    ts << "\\pard\\f1\\b\\fs48 " << escapeRtfText(el.text()) << "\\b0\\fs24\\par\n";
                } else if (tag == "h2") {
                    ts << "\\pard\\f1\\b\\fs36 " << escapeRtfText(el.text()) << "\\b0\\fs24\\par\n";
                } else if (tag == "h3") {
                    ts << "\\pard\\f1\\b\\fs30 " << escapeRtfText(el.text()) << "\\b0\\fs24\\par\n";
                } else if (tag == "a") {
                    ts << "\\pard\\f0\\ul " << escapeRtfText(el.text()) << "\\ul0\\par\n";
                } else if (tag == "list") {
                    QString listType = el.attribute("type", "ul");
                    QDomNodeList items = el.elementsByTagName("li");
                    for (int k = 0; k < items.count(); ++k) {
                        QString prefix = (listType == "ol") ? QString::number(k + 1) + ". " : "\\bullet  ";
                        ts << "\\pard\\f0 " << prefix << escapeRtfText(items.at(k).toElement().text()) << "\\par\n";
                    }
                } else if (tag == "blockquote") {
                    ts << "\\pard\\f0\\i " << escapeRtfText(el.text()) << "\\i0\\par\n";
                } else {
                    ts << "\\pard\\f0 " << escapeRtfText(el.text()) << "\\par\n";
                }
            }
        }
    }

    ts << "}";
    ts.flush();
    file.close();

    if (m_context)
        m_context->log(QString("RTF: exported %1 pages to %2").arg(doc->pageCount()).arg(filePath));

    return true;
}

bool RtfPlugin::checkDependencies() const
{
    return true;
}
