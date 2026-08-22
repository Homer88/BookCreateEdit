#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QDir>

class QPluginLoader;
class IBookPlugin;
class IBookFormat;
class BookContext;

class PluginManager : public QObject
{
    Q_OBJECT

public:
    static PluginManager &instance();

    void setPluginDirectory(const QString &path);
    QString pluginDirectory() const;

    void setContext(BookContext *ctx);

    void loadAllPlugins();
    void loadPlugin(const QString &filePath);
    void unloadPlugin(const QString &name);

    QStringList availablePlugins() const;
    QStringList availableFormats() const;

    IBookPlugin *plugin(const QString &name) const;
    IBookFormat *format(const QString &name) const;
    IBookFormat *formatByExtension(const QString &ext) const;

    bool isPluginLoaded(const QString &name) const;

signals:
    void pluginLoaded(const QString &name);
    void pluginUnloaded(const QString &name);
    void error(const QString &message);

private:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager();

    Q_DISABLE_COPY(PluginManager)

    struct PluginInfo
    {
        QString filePath;
        QObject *object;
        IBookPlugin *plugin;
        QPluginLoader *loader;
    };

    bool checkDependencies(IBookPlugin *plugin);
    void log(const QString &message);

    QString m_pluginDir;
    BookContext *m_context;
    QMap<QString, PluginInfo> m_plugins;
};

#endif // PLUGINMANAGER_H
