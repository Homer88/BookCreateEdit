#include "__PLUGIN_NAME_LOWER__Format.h"
#include "BookDocument.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
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
}

void __PLUGIN_CLASS__::shutdown()
{
    m_initialized = false;
    m_context = nullptr;
}

bool __PLUGIN_CLASS__::isInitialized() const { return m_initialized; }

QString __PLUGIN_CLASS__::formatName() const { return "__PLUGIN_DISPLAY_NAME__"; }
QString __PLUGIN_CLASS__::fileExtension() const { return "__PLUGIN_NAME_LOWER__"; }
QStringList __PLUGIN_CLASS__::supportedExtensions() const { return {"__PLUGIN_NAME_LOWER__"}; }
QStringList __PLUGIN_CLASS__::mimeTypes() const { return {"application/x-__PLUGIN_NAME_LOWER__"}; }

bool __PLUGIN_CLASS__::canRead() const { return true; }
bool __PLUGIN_CLASS__::canWrite() const { return true; }

bool __PLUGIN_CLASS__::read(const QString &filePath, BookDocument *doc)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    // TODO: реализовать чтение формата __PLUGIN_DISPLAY_NAME__
    // Пример: парсинг файла и заполнение BookDocument

    if (m_context)
        m_context->log("__PLUGIN_DISPLAY_NAME__: чтение " + filePath);

    return true;
}

bool __PLUGIN_CLASS__::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc)
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    // TODO: реализовать запись формата __PLUGIN_DISPLAY_NAME__
    // Пример: сериализация BookDocument в файл

    if (m_context)
        m_context->log("__PLUGIN_DISPLAY_NAME__: запись " + filePath);

    return true;
}

bool __PLUGIN_CLASS__::checkDependencies() const
{
    return true;
}
