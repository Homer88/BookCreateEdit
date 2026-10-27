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

/**
 * @brief Менеджер плагинов — загружает, хранит и управляет всеми плагинами.
 *
 * Реализован как синглтон (PluginManager::instance()).
 * Загружает плагины из DLL-файлов в каталоге plugins/ через Qt Plugin System.
 *
 * После загрузки каждого плагина вызывается initialize(context),
 * где context — BookContext для доступа к текущему документу и логированию.
 *
 * Основные возможности:
 *   - loadAllPlugins() — сканирует каталог и загружает все плагины
 *   - formatByExtension(ext) — находит плагин формата по расширению файла
 *   - format(name) — находит плагин формата по имени
 *   - plugin(name) — находит любой плагин по имени
 */
class PluginManager : public QObject
{
    Q_OBJECT

public:
    /** @brief Возвращает единственный экземпляр менеджера (синглтон). */
    static PluginManager &instance();

    /**
     * @brief Устанавливает каталог для поиска плагинов (.dll/.so).
     * По умолчанию — "./plugins" относительно исполняемого файла.
     */
    void setPluginDirectory(const QString &path);

    /** @brief Возвращает текущий каталог плагинов. */
    QString pluginDirectory() const;

    /** @brief Устанавливает контекст приложения для передачи плагинам. */
    void setContext(BookContext *ctx);

    /**
     * @brief Загружает все плагины из каталога plugins/.
     *
     * Сканирует каталог на наличие DLL/SO-файлов, загружает каждый,
     * проверяет зависимости, вызывает initialize(). Генерирует сигнал
     * pluginLoaded(name) для каждого успешного плагина.
     */
    void loadAllPlugins();

    /** @brief Загружает один плагин по пути к DLL-файлу. */
    void loadPlugin(const QString &filePath);

    /** @brief Выгружает плагин по имени (вызывает shutdown() и unload()). */
    void unloadPlugin(const QString &name);

    /** @brief Возвращает список имён всех загруженных плагинов. */
    QStringList availablePlugins() const;

    /** @brief Возвращает список имён плагинов форматов (наследников IBookFormat). */
    QStringList availableFormats() const;

    /**
     * @brief Ищет плагин по имени (любой тип).
     * @return Указатель на IBookPlugin или nullptr, если не найден.
     */
    IBookPlugin *plugin(const QString &name) const;

    /**
     * @brief Ищет плагин формата по имени (например, "EPUB").
     * @return Указатель на IBookFormat или nullptr.
     */
    IBookFormat *format(const QString &name) const;

    /**
     * @brief Ищет плагин формата по расширению файла (например, "epub").
     * @return Указатель на IBookFormat или nullptr, если формат не поддерживается.
     *
     * Это основной метод для выбора плагина в диалогах «Открыть»/«Сохранить».
     */
    IBookFormat *formatByExtension(const QString &ext) const;

    /** @brief Возвращает true, если плагин с таким именем уже загружен. */
    bool isPluginLoaded(const QString &name) const;

signals:
    /** @brief Сигнал о успешной загрузке плагина. */
    void pluginLoaded(const QString &name);

    /** @brief Сигнал о выгрузке плагина. */
    void pluginUnloaded(const QString &name);

    /** @brief Сигнал об ошибке загрузки плагина. */
    void error(const QString &message);

private:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager();

    Q_DISABLE_COPY(PluginManager)

    /** @brief Внутренняя структура с информацией о загруженном плагине. */
    struct PluginInfo
    {
        QString filePath;      ///< Путь к DLL-файлу плагина
        QObject *object;       ///< Qt-объект плагина (результат qobject_cast)
        IBookPlugin *plugin;   ///< Указатель на интерфейс плагина
        QPluginLoader *loader; ///< Загрузчик Qt для управления生命周期
    };

    /** @brief Проверяет, удовлетворяет ли плагин всем зависимостям. */
    bool checkDependencies(IBookPlugin *plugin);

    /** @brief Вспомогательный метод логирования через BookContext. */
    void log(const QString &message);

    QString m_pluginDir;              ///< Каталог с плагинами
    BookContext *m_context;           ///< Контекст приложения
    QMap<QString, PluginInfo> m_plugins; ///< Загруженные плагины (имя → информация)
};

#endif // PLUGINMANAGER_H
