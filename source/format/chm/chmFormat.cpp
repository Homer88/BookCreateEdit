#include "chmFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

ChmPlugin::ChmPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

ChmPlugin::~ChmPlugin()
{
    shutdown();
}

QString ChmPlugin::name() const { return "Chm"; }
QString ChmPlugin::description() const { return "Plugin Chm"; }
QString ChmPlugin::version() const { return "1.0.0"; }
QString ChmPlugin::author() const { return "Your Name"; }
QStringList ChmPlugin::dependencies() const { return {}; }

void ChmPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("Chm РёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°РЅ");
}

void ChmPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool ChmPlugin::isInitialized() const { return m_initialized; }

QString ChmPlugin::formatName() const { return "Chm"; }
QString ChmPlugin::fileExtension() const { return "chm"; }
QStringList ChmPlugin::supportedExtensions() const { return {"chm"}; }
QStringList ChmPlugin::mimeTypes() const { return {"application/x-chm"}; }

bool ChmPlugin::canRead() const { return true; }
bool ChmPlugin::canWrite() const { return true; }

bool ChmPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    // TODO: СЂРµР°Р»РёР·РѕРІР°С‚СЊ С‡С‚РµРЅРёРµ С„РѕСЂРјР°С‚Р° Chm
    // РџСЂРёРјРµСЂ: РїР°СЂСЃРёРЅРі С„Р°Р№Р»Р° Рё Р·Р°РїРѕР»РЅРµРЅРёРµ BookDocument

    if (m_context)
        m_context->log("Chm: С‡С‚РµРЅРёРµ " + filePath);

    return true;
}

bool ChmPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    // TODO: СЂРµР°Р»РёР·РѕРІР°С‚СЊ Р·Р°РїРёСЃСЊ С„РѕСЂРјР°С‚Р° Chm
    // РџСЂРёРјРµСЂ: СЃРµСЂРёР°Р»РёР·Р°С†РёСЏ BookDocument РІ С„Р°Р№Р»

    if (m_context)
        m_context->log("Chm: Р·Р°РїРёСЃСЊ " + filePath);

    return true;
}

bool ChmPlugin::checkDependencies() const
{
    return true;
}

