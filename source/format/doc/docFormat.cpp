#include "docFormat.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

DocPlugin::DocPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

DocPlugin::~DocPlugin()
{
    shutdown();
}

QString DocPlugin::name() const { return "Doc"; }
QString DocPlugin::description() const { return "Plugin Doc"; }
QString DocPlugin::version() const { return "1.0.0"; }
QString DocPlugin::author() const { return "Your Name"; }
QStringList DocPlugin::dependencies() const { return {}; }

void DocPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;
    if (m_context)
        m_context->log("Doc РёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°РЅ");
}

void DocPlugin::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool DocPlugin::isInitialized() const { return m_initialized; }

QString DocPlugin::formatName() const { return "Doc"; }
QString DocPlugin::fileExtension() const { return "doc"; }
QStringList DocPlugin::supportedExtensions() const { return {"doc"}; }
QStringList DocPlugin::mimeTypes() const { return {"application/x-doc"}; }

bool DocPlugin::canRead() const { return true; }
bool DocPlugin::canWrite() const { return true; }

bool DocPlugin::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    // TODO: СЂРµР°Р»РёР·РѕРІР°С‚СЊ С‡С‚РµРЅРёРµ С„РѕСЂРјР°С‚Р° Doc
    // РџСЂРёРјРµСЂ: РїР°СЂСЃРёРЅРі С„Р°Р№Р»Р° Рё Р·Р°РїРѕР»РЅРµРЅРёРµ BookDocument

    if (m_context)
        m_context->log("Doc: С‡С‚РµРЅРёРµ " + filePath);

    return true;
}

bool DocPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    // TODO: СЂРµР°Р»РёР·РѕРІР°С‚СЊ Р·Р°РїРёСЃСЊ С„РѕСЂРјР°С‚Р° Doc
    // РџСЂРёРјРµСЂ: СЃРµСЂРёР°Р»РёР·Р°С†РёСЏ BookDocument РІ С„Р°Р№Р»

    if (m_context)
        m_context->log("Doc: Р·Р°РїРёСЃСЊ " + filePath);

    return true;
}

bool DocPlugin::checkDependencies() const
{
    return true;
}

