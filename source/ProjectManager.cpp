#include "ProjectManager.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>

ProjectManager::ProjectManager(QObject *parent)
    : QObject(parent)
    , m_modified(false)
{
}

ProjectManager::~ProjectManager()
{
}

void ProjectManager::newProject(const QString &projectName)
{
    m_doc.clear();
    m_filePath.clear();
    m_modified = false;

    QDomProcessingInstruction pi = m_doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    m_doc.appendChild(pi);

    QDomElement root = m_doc.createElement("bceproj");
    root.setAttribute("version", "1.0");
    m_doc.appendChild(root);

    // Блок <book>
    QDomElement book = m_doc.createElement("book");
    root.appendChild(book);
    setElementText(book, "title", "");
    setElementText(book, "year", QString::number(QDate::currentDate().year()));
    setElementText(book, "publisher", "");
    setElementText(book, "projectName", projectName);
    setElementText(book, "comments", "");

    // Блок <meta>
    QDomElement meta = m_doc.createElement("meta");
    root.appendChild(meta);
    setElementText(meta, "programVersion", "1.0.0");
    setElementText(meta, "defaultPlugins", "spellchecker,diagramrenderer");

    QDomElement settings = m_doc.createElement("settings");
    meta.appendChild(settings);
    setElementText(settings, "backupCount", "5");
    setElementText(settings, "lastPublishVersion", "0");

    QDomElement lock = m_doc.createElement("lock");
    meta.appendChild(lock);
    setElementText(lock, "enabled", "false");
    setElementText(lock, "passwordHash", "");
    setElementText(lock, "readOnlyExport", "true");

    // Блок <cover>
    QDomElement cover = m_doc.createElement("cover");
    root.appendChild(cover);
    setElementText(cover, "filePath", "");
    setElementText(cover, "mimeType", "");
    setElementText(cover, "width", "0");
    setElementText(cover, "height", "0");

    // Блок <documents>
    QDomElement docs = m_doc.createElement("documents");
    root.appendChild(docs);

    m_modified = true;
    emit modified();
}

bool ProjectManager::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ProjectManager: не удалось открыть файл:" << filePath;
        return false;
    }

    QString errorMsg;
    int errorLine, errorCol;
    if (!m_doc.setContent(&file, &errorMsg, &errorLine, &errorCol)) {
        qWarning() << "ProjectManager: ошибка парсинга XML:" << errorMsg
                    << "строка" << errorLine << "столбец" << errorCol;
        return false;
    }

    m_filePath = filePath;
    m_modified = false;
    return true;
}

bool ProjectManager::save(const QString &filePath)
{
    QString savePath = filePath.isEmpty() ? m_filePath : filePath;
    if (savePath.isEmpty()) return false;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ProjectManager: не удалось записать файл:" << savePath;
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << m_doc.toString(4);

    m_filePath = savePath;
    m_modified = false;
    return true;
}

bool ProjectManager::isModified() const { return m_modified; }
QString ProjectManager::filePath() const { return m_filePath; }

QString ProjectManager::projectDir() const
{
    if (m_filePath.isEmpty()) return QString();
    return QFileInfo(m_filePath).absolutePath();
}

// --- Информация о книге ---

QString ProjectManager::bookTitle() const
{
    return elementText(m_doc.documentElement().firstChildElement("book"), "title");
}

void ProjectManager::setBookTitle(const QString &title)
{
    setElementText(m_doc.documentElement().firstChildElement("book"), "title", title);
    m_modified = true;
    emit modified();
}

int ProjectManager::bookYear() const
{
    bool ok;
    int y = elementText(m_doc.documentElement().firstChildElement("book"), "year").toInt(&ok);
    return ok ? y : QDate::currentDate().year();
}

void ProjectManager::setBookYear(int year)
{
    setElementText(m_doc.documentElement().firstChildElement("book"), "year", QString::number(year));
    m_modified = true;
    emit modified();
}

QString ProjectManager::bookPublisher() const
{
    return elementText(m_doc.documentElement().firstChildElement("book"), "publisher");
}

void ProjectManager::setBookPublisher(const QString &publisher)
{
    setElementText(m_doc.documentElement().firstChildElement("book"), "publisher", publisher);
    m_modified = true;
    emit modified();
}

QString ProjectManager::projectName() const
{
    return elementText(m_doc.documentElement().firstChildElement("book"), "projectName");
}

void ProjectManager::setProjectName(const QString &name)
{
    setElementText(m_doc.documentElement().firstChildElement("book"), "projectName", name);
    m_modified = true;
    emit modified();
}

QString ProjectManager::comments() const
{
    return elementText(m_doc.documentElement().firstChildElement("book"), "comments");
}

void ProjectManager::setComments(const QString &comments)
{
    setElementText(m_doc.documentElement().firstChildElement("book"), "comments", comments);
    m_modified = true;
    emit modified();
}

// --- Метаданные ---

QString ProjectManager::programVersion() const
{
    return elementText(m_doc.documentElement().firstChildElement("meta"), "programVersion");
}

void ProjectManager::setProgramVersion(const QString &version)
{
    setElementText(m_doc.documentElement().firstChildElement("meta"), "programVersion", version);
    m_modified = true;
}

QStringList ProjectManager::defaultPlugins() const
{
    QString list = elementText(m_doc.documentElement().firstChildElement("meta"), "defaultPlugins");
    return list.isEmpty() ? QStringList() : list.split(",", QString::SkipEmptyParts);
}

void ProjectManager::setDefaultPlugins(const QStringList &plugins)
{
    setElementText(m_doc.documentElement().firstChildElement("meta"), "defaultPlugins",
                   plugins.join(","));
    m_modified = true;
}

// --- Настройки публикации ---

PublishInfo ProjectManager::publishInfo() const
{
    PublishInfo info;
    QDomElement settings = m_doc.documentElement()
        .firstChildElement("meta").firstChildElement("settings");
    info.backupCount = elementText(settings, "backupCount", "5").toInt();
    info.lastVersion = elementText(settings, "lastPublishVersion", "0").toInt();
    info.lastDate = elementText(settings, "lastPublishDate", "");
    info.lastFormat = elementText(settings, "lastPublishFormat", "");
    return info;
}

void ProjectManager::setPublishInfo(const PublishInfo &info)
{
    QDomElement meta = m_doc.documentElement().firstChildElement("meta");
    QDomElement settings = meta.firstChildElement("settings");
    if (settings.isNull()) {
        settings = m_doc.createElement("settings");
        meta.appendChild(settings);
    }
    setElementText(settings, "backupCount", QString::number(info.backupCount));
    setElementText(settings, "lastPublishVersion", QString::number(info.lastVersion));
    setElementText(settings, "lastPublishDate", info.lastDate);
    setElementText(settings, "lastPublishFormat", info.lastFormat);
    m_modified = true;
    emit modified();
}

// --- Блокировка ---

LockInfo ProjectManager::lockInfo() const
{
    LockInfo info;
    QDomElement lock = m_doc.documentElement()
        .firstChildElement("meta").firstChildElement("lock");
    info.enabled = elementText(lock, "enabled", "false") == "true";
    info.passwordHash = elementText(lock, "passwordHash");
    info.readOnlyExport = elementText(lock, "readOnlyExport", "true") == "true";
    return info;
}

void ProjectManager::setLockInfo(const LockInfo &lock)
{
    QDomElement meta = m_doc.documentElement().firstChildElement("meta");
    QDomElement lockEl = meta.firstChildElement("lock");
    if (lockEl.isNull()) {
        lockEl = m_doc.createElement("lock");
        meta.appendChild(lockEl);
    }
    setElementText(lockEl, "enabled", lock.enabled ? "true" : "false");
    setElementText(lockEl, "passwordHash", lock.passwordHash);
    setElementText(lockEl, "readOnlyExport", lock.readOnlyExport ? "true" : "false");
    m_modified = true;
    emit modified();
}

void ProjectManager::setLockPassword(const QString &password)
{
    LockInfo info = lockInfo();
    info.passwordHash = hashPassword(password);
    info.enabled = !password.isEmpty();
    setLockInfo(info);
}

bool ProjectManager::checkLockPassword(const QString &password) const
{
    return lockInfo().passwordHash == hashPassword(password);
}

bool ProjectManager::isLocked() const
{
    return lockInfo().enabled;
}

// --- Обложка ---

CoverInfo ProjectManager::coverInfo() const
{
    CoverInfo info;
    QDomElement cover = m_doc.documentElement().firstChildElement("cover");
    info.filePath = elementText(cover, "filePath");
    info.mimeType = elementText(cover, "mimeType");
    info.width = elementText(cover, "width", "0").toInt();
    info.height = elementText(cover, "height", "0").toInt();
    return info;
}

void ProjectManager::setCoverInfo(const CoverInfo &cover)
{
    QDomElement coverEl = m_doc.documentElement().firstChildElement("cover");
    if (coverEl.isNull()) {
        coverEl = m_doc.createElement("cover");
        m_doc.documentElement().appendChild(coverEl);
    }
    setElementText(coverEl, "filePath", cover.filePath);
    setElementText(coverEl, "mimeType", cover.mimeType);
    setElementText(coverEl, "width", QString::number(cover.width));
    setElementText(coverEl, "height", QString::number(cover.height));
    m_modified = true;
    emit modified();
}

// --- Документы ---

QStringList ProjectManager::documentPaths() const
{
    QStringList paths;
    QDomElement docs = m_doc.documentElement().firstChildElement("documents");
    QDomElement doc = docs.firstChildElement("document");
    while (!doc.isNull()) {
        paths << doc.attribute("path");
        doc = doc.nextSiblingElement("document");
    }
    return paths;
}

void ProjectManager::addDocument(const QString &path, const QString &format)
{
    QDomElement docs = m_doc.documentElement().firstChildElement("documents");
    QDomElement docEl = m_doc.createElement("document");
    docEl.setAttribute("path", path);
    docEl.setAttribute("format", format);
    docs.appendChild(docEl);
    m_modified = true;
    emit modified();
}

void ProjectManager::removeDocument(const QString &path)
{
    QDomElement docs = m_doc.documentElement().firstChildElement("documents");
    QDomElement doc = docs.firstChildElement("document");
    while (!doc.isNull()) {
        if (doc.attribute("path") == path) {
            docs.removeChild(doc);
            m_modified = true;
            emit modified();
            return;
        }
        doc = doc.nextSiblingElement("document");
    }
}

QString ProjectManager::mainDocumentPath() const
{
    QStringList paths = documentPaths();
    return paths.isEmpty() ? QString() : paths.first();
}

// --- Утилиты ---

QString ProjectManager::hashPassword(const QString &password)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(password.toUtf8());
    return hash.result().toHex();
}

// --- Внутренние методы ---

void ProjectManager::ensureStructure()
{
    if (m_doc.documentElement().isNull()) {
        QDomProcessingInstruction pi = m_doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
        m_doc.appendChild(pi);

        QDomElement root = m_doc.createElement("bceproj");
        root.setAttribute("version", "1.0");
        m_doc.appendChild(root);
    }
}

QDomElement ProjectManager::findOrCreateElement(QDomElement parent, const QString &tag)
{
    QDomElement el = parent.firstChildElement(tag);
    if (el.isNull()) {
        el = m_doc.createElement(tag);
        parent.appendChild(el);
    }
    return el;
}

QString ProjectManager::elementText(QDomElement parent, const QString &tag, const QString &defaultValue) const
{
    if (parent.isNull()) return defaultValue;
    QDomElement el = parent.firstChildElement(tag);
    if (el.isNull()) return defaultValue;
    return el.text();
}

void ProjectManager::setElementText(QDomElement parent, const QString &tag, const QString &value)
{
    if (parent.isNull()) return;
    QDomElement el = parent.firstChildElement(tag);
    if (el.isNull()) {
        el = m_doc.createElement(tag);
        parent.appendChild(el);
    }
    // Удаляем все дочерние текстовые узлы
    while (!el.firstChild().isNull())
        el.removeChild(el.firstChild());
    el.appendChild(m_doc.createTextNode(value));
}
