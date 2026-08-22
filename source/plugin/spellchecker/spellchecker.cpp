#include "spellchecker.h"
#include <QDebug>

SpellcheckerPlugin::SpellcheckerPlugin(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

SpellcheckerPlugin::~SpellcheckerPlugin()
{
    shutdown();
}

QString SpellcheckerPlugin::name() const { return "Spellchecker"; }
QString SpellcheckerPlugin::description() const { return "Plugin Spellchecker"; }
QString SpellcheckerPlugin::version() const { return "1.0.0"; }
QString SpellcheckerPlugin::author() const { return "Your Name"; }
QStringList SpellcheckerPlugin::dependencies() const { return {}; }

void SpellcheckerPlugin::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;

    if (m_context)
        m_context->log("Spellchecker РёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°РЅ");

    // TODO: Р·РґРµСЃСЊ РІР°С€Р° Р»РѕРіРёРєР° РёРЅРёС†РёР°Р»РёР·Р°С†РёРё
    // РќР°РїСЂРёРјРµСЂ:
    // - Р РµРіРёСЃС‚СЂР°С†РёСЏ РѕР±СЂР°Р±РѕС‚С‡РёРєРѕРІ СЃРѕР±С‹С‚РёР№
    // - Р—Р°РіСЂСѓР·РєР° СЂРµСЃСѓСЂСЃРѕРІ
    // - РџРѕРґРєР»СЋС‡РµРЅРёРµ СЃРёРіРЅР°Р»РѕРІ
}

void SpellcheckerPlugin::shutdown()
{
    // TODO: Р·РґРµСЃСЊ РѕС‡РёСЃС‚РєР° СЂРµСЃСѓСЂСЃРѕРІ

    m_initialized = false;
    m_context = nullptr;
}

bool SpellcheckerPlugin::isInitialized() const { return m_initialized; }

