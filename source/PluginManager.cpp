#include "PluginManager.h"
#include "interfaces/IBookPlugin.h"
#include "interfaces/IBookFormat.h"

#include <QPluginLoader>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

PluginManager &PluginManager::instance()
{
    static PluginManager instance;
    return instance;
}

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
{
    m_pluginDir = QCoreApplication::applicationDirPath() + "/plugins";
}

PluginManager::~PluginManager()
{
    QStringList names = m_plugins.keys();
    for (const QString &name : names)
        unloadPlugin(name);
}

void PluginManager::setPluginDirectory(const QString &path)
{
    m_pluginDir = path;
}

QString PluginManager::pluginDirectory() const
{
    return m_pluginDir;
}

void PluginManager::setContext(BookContext *ctx)
{
    m_context = ctx;
}

void PluginManager::loadAllPlugins()
{
    QDir dir(m_pluginDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        log(QString("Создана директория плагинов: %1").arg(m_pluginDir));
        return;
    }

    QStringList filters;
#ifdef Q_OS_WIN
    filters << "*.dll";
#elif defined(Q_OS_MACOS)
    filters << "*.dylib";
#else
    filters << "*.so";
#endif

    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fi : fileList) {
        loadPlugin(fi.absoluteFilePath());
    }

    log(QString("Загружено плагинов: %1").arg(m_plugins.count()));
}

void PluginManager::loadPlugin(const QString &filePath)
{
    QPluginLoader *loader = new QPluginLoader(filePath, this);

    QJsonObject metaData = loader->metaData();

    // Обычная DLL без метаданных Qt-плагина (например, git.dll) —
    // пропускаем молча, это не ошибка.
    if (metaData.value("IID").toString().isEmpty()) {
        delete loader;
        return;
    }

    QString pluginId = metaData.value("MetaData").toObject().value("id").toString();
    if (pluginId.isEmpty())
        pluginId = QFileInfo(filePath).baseName();

    if (m_plugins.contains(pluginId)) {
        log(QString("Плагин уже загружен: %1").arg(pluginId));
        delete loader;
        return;
    }

    if (!loader->load()) {
        QString errMsg = loader->errorString();
        log(QString("Ошибка загрузки плагина %1: %2").arg(filePath, errMsg));
        emit error(errMsg);
        delete loader;
        return;
    }

    QObject *obj = loader->instance();
    if (!obj) {
        log(QString("Не удалось создать экземпляр плагина: %1").arg(filePath));
        loader->unload();
        delete loader;
        return;
    }

    IBookPlugin *plugin = qobject_cast<IBookPlugin *>(obj);
    if (!plugin) {
        log(QString("Плагин не реализует IBookPlugin: %1").arg(filePath));
        loader->unload();
        delete loader;
        return;
    }

    if (!checkDependencies(plugin)) {
        log(QString("Не выполнены зависимости плагина: %1").arg(plugin->name()));
        loader->unload();
        delete loader;
        return;
    }

    plugin->initialize(m_context);

    PluginInfo info;
    info.filePath = filePath;
    info.object = obj;
    info.plugin = plugin;
    info.loader = loader;
    m_plugins[pluginId] = info;

    log(QString("Загружен плагин: %1 v%2").arg(plugin->name(), plugin->version()));
    emit pluginLoaded(pluginId);
}

void PluginManager::unloadPlugin(const QString &name)
{
    if (!m_plugins.contains(name))
        return;

    PluginInfo info = m_plugins.take(name);
    info.plugin->shutdown();

    info.loader->unload();
    delete info.loader;

    log(QString("Выгружен плагин: %1").arg(name));
    emit pluginUnloaded(name);
}

QStringList PluginManager::availablePlugins() const
{
    return m_plugins.keys();
}

QStringList PluginManager::availableFormats() const
{
    QStringList formats;
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        IBookFormat *fmt = qobject_cast<IBookFormat *>(it.value().object);
        if (fmt)
            formats.append(fmt->formatName());
    }
    return formats;
}

IBookPlugin *PluginManager::plugin(const QString &name) const
{
    if (!m_plugins.contains(name))
        return nullptr;
    return m_plugins[name].plugin;
}

IBookFormat *PluginManager::format(const QString &name) const
{
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        IBookFormat *fmt = qobject_cast<IBookFormat *>(it.value().object);
        if (fmt && fmt->formatName() == name)
            return fmt;
    }
    return nullptr;
}

IBookFormat *PluginManager::formatByExtension(const QString &ext) const
{
    QString lowerExt = ext.toLower();
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        IBookFormat *fmt = qobject_cast<IBookFormat *>(it.value().object);
        if (fmt && fmt->supportedExtensions().contains(lowerExt))
            return fmt;
    }
    return nullptr;
}

bool PluginManager::isPluginLoaded(const QString &name) const
{
    return m_plugins.contains(name);
}

bool PluginManager::checkDependencies(IBookPlugin *plugin)
{
    QStringList deps = plugin->dependencies();
    for (const QString &dep : deps) {
        if (!m_plugins.contains(dep)) {
            log(QString("Зависимость не найдена: %1 (требуется плагином %2)")
                .arg(dep, plugin->name()));
            return false;
        }
    }
    return true;
}

void PluginManager::log(const QString &message)
{
    if (m_context)
        m_context->log(message);
    else
        qDebug() << "[PluginManager]" << message;
}
