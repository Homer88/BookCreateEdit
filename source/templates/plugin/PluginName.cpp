#include "__PLUGIN_NAME_LOWER__.h"
#include <QDebug>

__PLUGIN_CLASS__::__PLUGIN_CLASS__(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_initialized(false)
{
}

__PLUGIN_CLASS__::~__PLUGIN_CLASS__()
{
    shutdown();
}

QString __PLUGIN_CLASS__::name() const { return "__PLUGIN_DISPLAY_NAME__"; }
QString __PLUGIN_CLASS__::description() const { return "__PLUGIN_DESCRIPTION__"; }
QString __PLUGIN_CLASS__::version() const { return "1.0.0"; }
QString __PLUGIN_CLASS__::author() const { return "Your Name"; }
QStringList __PLUGIN_CLASS__::dependencies() const { return {}; }

void __PLUGIN_CLASS__::initialize(BookContext *ctx)
{
    m_context = ctx;
    m_initialized = true;

    if (m_context)
        m_context->log("__PLUGIN_DISPLAY_NAME__ инициализирован");

    // TODO: здесь ваша логика инициализации
    // Например:
    // - Регистрация обработчиков событий
    // - Загрузка ресурсов
    // - Подключение сигналов
}

void __PLUGIN_CLASS__::shutdown()
{
    // TODO: здесь очистка ресурсов

    m_initialized = false;
    m_context = nullptr;
}

bool __PLUGIN_CLASS__::isInitialized() const { return m_initialized; }
