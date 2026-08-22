#include "pdfformat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

PdfPlugin::PdfPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

PdfPlugin::~PdfPlugin()
{
    shutdown();
}

QString PdfPlugin::name() const { return "Pdf"; }
QString PdfPlugin::description() const { return "Plugin Pdf"; }
QString PdfPlugin::version() const { return "1.0.0"; }
QString PdfPlugin::author() const { return "Your Name"; }
QStringList PdfPlugin::dependencies() const { return {}; }

void PdfPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("Pdf РёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°РЅ");
}

void PdfPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool PdfPlugin::isInitialized() const { return m_initialized; }

QString PdfPlugin::formatName() const { return "Pdf"; }
QString PdfPlugin::fileExtension() const { return "pdf"; }
QStringList PdfPlugin::supportedExtensions() const { return {"pdf"}; }
QStringList PdfPlugin::mimeTypes() const { return {"application/x-pdf"}; }

bool PdfPlugin::canRead() const { return true; }
bool PdfPlugin::canWrite() const { return true; }

bool PdfPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    // TODO: СЂРµР°Р»РёР·РѕРІР°С‚СЊ С‡С‚РµРЅРёРµ С„РѕСЂРјР°С‚Р° Pdf
    // РџСЂРёРјРµСЂ: РїР°СЂСЃРёРЅРі С„Р°Р№Р»Р° Рё Р·Р°РїРѕР»РЅРµРЅРёРµ BookDocument

    if (m_context)
        m_context->log("Pdf: С‡С‚РµРЅРёРµ " + filePath);

    return true;
}

bool PdfPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    // TODO: СЂРµР°Р»РёР·РѕРІР°С‚СЊ Р·Р°РїРёСЃСЊ С„РѕСЂРјР°С‚Р° Pdf
    // РџСЂРёРјРµСЂ: СЃРµСЂРёР°Р»РёР·Р°С†РёСЏ BookDocument РІ С„Р°Р№Р»

    if (m_context)
        m_context->log("Pdf: Р·Р°РїРёСЃСЊ " + filePath);

    return true;
}

bool PdfPlugin::checkDependencies() const
{
    return true;
}

