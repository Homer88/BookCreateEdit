#include "diagramrenderer.h"
#include <QDebug>

DiagramrendererPlugin::DiagramrendererPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

DiagramrendererPlugin::~DiagramrendererPlugin()
{
    shutdown();
}

QString DiagramrendererPlugin::name() const { return "Diagramrenderer"; }
QString DiagramrendererPlugin::description() const { return "Plugin Diagramrenderer"; }
QString DiagramrendererPlugin::version() const { return "1.0.0"; }
QString DiagramrendererPlugin::author() const { return "Your Name"; }
QStringList DiagramrendererPlugin::dependencies() const { return {}; }

void DiagramrendererPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;

    if (m_context)
        m_context->log("Diagramrenderer РёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°РЅ");

    // TODO: Р·РґРµСЃСЊ РІР°С€Р° Р»РѕРіРёРєР° РёРЅРёС†РёР°Р»РёР·Р°С†РёРё
    // РќР°РїСЂРёРјРµСЂ:
    // - Р РµРіРёСЃС‚СЂР°С†РёСЏ РѕР±СЂР°Р±РѕС‚С‡РёРєРѕРІ СЃРѕР±С‹С‚РёР№
    // - Р—Р°РіСЂСѓР·РєР° СЂРµСЃСѓСЂСЃРѕРІ
    // - РџРѕРґРєР»СЋС‡РµРЅРёРµ СЃРёРіРЅР°Р»РѕРІ
}

void DiagramrendererPlugin::shutdown()
{
    // TODO: Р·РґРµСЃСЊ РѕС‡РёСЃС‚РєР° СЂРµСЃСѓСЂСЃРѕРІ

    m_initialized = false;
    m_context = nullptr;
}

bool DiagramrendererPlugin::isInitialized() const { return m_initialized; }

