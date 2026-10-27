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
        m_context->log(QString::fromUtf8("Doc инициализирован"));
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
    Q_UNUSED(filePath);
    Q_UNUSED(doc);
    // Чтение бинарного OLE2-формата .doc пока не реализовано.
    // Раньше здесь возвращался true без разбора файла — книга молча
    // оставалась пустой. Теперь честно сообщаем об ошибке импорта.
    if (m_context)
        m_context->log(QString::fromUtf8("Doc: чтение формата .doc не реализовано"));
    return false;
}

bool DocPlugin::write(BookDocument *doc, const QString &filePath)
{
    if (!m_initialized || !doc) {
        Q_UNUSED(doc);
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    // TODO: реализовать запись формата Doc
    if (m_context)
        m_context->log("Doc: запись " + filePath);

    file.close();
    return true;
}

bool DocPlugin::checkDependencies() const
{
    return true;
}
