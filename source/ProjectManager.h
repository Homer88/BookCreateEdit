#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QString>
#include <QDomDocument>
#include <QDomElement>

/**
 * @brief Информация об обложке книги.
 */
struct CoverInfo
{
    QString filePath;      ///< Путь к файлу обложки (относительный или абсолютный)
    QString mimeType;      ///< MIME-тип (image/jpeg, image/png)
    int width;             ///< Ширина в пикселях
    int height;            ///< Высота в пикселях
};

/**
 * @brief Настройки блокировки проекта.
 */
struct LockInfo
{
    bool enabled;          ///< Включена ли блокировка
    QString passwordHash;  ///< Хеш пароля (SHA-256)
    bool readOnlyExport;   ///< Запрет редактирования в формате публикации
};

/**
 * @brief Настройки публикации проекта.
 */
struct PublishInfo
{
    int lastVersion;       ///< Номер последней версии публикации
    QString lastDate;      ///< Дата последней публикации
    QString lastFormat;    ///< Формат последней публикации
    int backupCount;       ///< Количество бэкапов
};

/**
 * @brief Менеджер проекта (.bceproj файл).
 *
 * Формат .bceproj — XML-файл с расширенными метаданными проекта:
 * @code
 * <bceproj version="1.0">
 *   <book>
 *     <title>Название книги</title>
 *     <year>2026</year>
 *     <publisher>Издательство</publisher>
 *     <projectName>Название проекта</projectName>
 *     <comments>Комментарии</comments>
 *   </book>
 *   <meta>
 *     <programVersion>1.0.0</programVersion>
 *     <defaultPlugins>spellchecker,diagramrenderer</defaultPlugins>
 *     <settings>
 *       <backupCount>5</backupCount>
 *       <lastPublishVersion>3</lastPublishVersion>
 *     </settings>
 *     <lock>
 *       <enabled>false</enabled>
 *       <passwordHash></passwordHash>
 *       <readOnlyExport>true</readOnlyExport>
 *     </lock>
 *   </meta>
 *   <cover>
 *     <filePath>cover.jpg</filePath>
 *     <mimeType>image/jpeg</mimeType>
 *     <width>600</width>
 *     <height>900</height>
 *   </cover>
 *   <documents>
 *     <document path="book.bce" format="BCE"/>
 *   </documents>
 * </bceproj>
 * @endcode
 */
class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject *parent = nullptr);
    ~ProjectManager();

    /** @brief Создаёт новый пустой проект с указанным именем. */
    void newProject(const QString &projectName);

    /**
     * @brief Загружает проект из .bceproj файла.
     * @param filePath — путь к .bceproj файлу.
     * @return true при успехе.
     */
    bool load(const QString &filePath);

    /**
     * @brief Сохраняет проект в .bceproj файл.
     * @param filePath — путь для сохранения (пустой = текущий путь).
     * @return true при успехе.
     */
    bool save(const QString &filePath = QString());

    /** @brief Возвращает true, если проект был изменён. */
    bool isModified() const;

    /** @brief Возвращает путь к текущему .bceproj файлу. */
    QString filePath() const;

    /** @brief Возвращает базовый каталог проекта (каталог .bceproj файла). */
    QString projectDir() const;

    // --- Информация о книге ---
    QString bookTitle() const;
    void setBookTitle(const QString &title);

    int bookYear() const;
    void setBookYear(int year);

    QString bookPublisher() const;
    void setBookPublisher(const QString &publisher);

    QString projectName() const;
    void setProjectName(const QString &name);

    QString comments() const;
    void setComments(const QString &comments);

    // --- Метаданные ---
    QString programVersion() const;
    void setProgramVersion(const QString &version);

    QStringList defaultPlugins() const;
    void setDefaultPlugins(const QStringList &plugins);

    // --- Настройки публикации ---
    PublishInfo publishInfo() const;
    void setPublishInfo(const PublishInfo &info);

    // --- Блокировка ---
    LockInfo lockInfo() const;
    void setLockInfo(const LockInfo &lock);

    /** @brief Устанавливает пароль блокировки (хеширует SHA-256). */
    void setLockPassword(const QString &password);

    /** @brief Проверяет пароль блокировки. @return true если верный. */
    bool checkLockPassword(const QString &password) const;

    /** @brief Возвращает true, если проект заблокирован. */
    bool isLocked() const;

    // --- Обложка ---
    CoverInfo coverInfo() const;
    void setCoverInfo(const CoverInfo &cover);

    // --- Документы ---
    /** @brief Возвращает список путей к документам проекта (.bce и другие). */
    QStringList documentPaths() const;

    /** @brief Добавляет путь к документу в проект. */
    void addDocument(const QString &path, const QString &format = "BCE");

    /** @brief Удаляет документ из проекта. */
    void removeDocument(const QString &path);

    /** @brief Возвращает путь к основному .bce файлу документа. */
    QString mainDocumentPath() const;

    // --- Статические утилиты ---
    /** @brief Вычисляет SHA-256 хеш строки. */
    static QString hashPassword(const QString &password);

signals:
    void modified(); ///< Сигнал об изменении проекта

private:
    void ensureStructure();  ///< Создаёт базовую XML-структуру
    QDomElement findOrCreateElement(QDomElement parent, const QString &tag);
    QString elementText(QDomElement parent, const QString &tag, const QString &defaultValue = QString()) const;
    void setElementText(QDomElement parent, const QString &tag, const QString &value);

    QDomDocument m_doc;   ///< DOM-дерево .bceproj файла
    QString m_filePath;   ///< Путь к .bceproj файлу
    bool m_modified;      ///< Флаг изменений
};

#endif // PROJECTMANAGER_H
