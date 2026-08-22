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
bool HlpPlugin::canWrite() const { return false; }

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

bool HlpPlugin::write(BookDocument *doc, const QString &filePath)
{
    Q_UNUSED(doc)
    Q_UNUSED(filePath)
    if (m_context)
        m_context->log("Help: write not supported (HLP is a read-only format)");
    return false;
}

bool HlpPlugin::checkDependencies() const
{
    return true;
}
