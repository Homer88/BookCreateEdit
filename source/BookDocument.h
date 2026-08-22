#ifndef BOOKDOCUMENT_H
#define BOOKDOCUMENT_H

#include <QObject>
#include <QString>
#include <QDomDocument>
#include <QDomElement>
#include <QList>

struct BookMeta
{
    QString title;
    QString author;
    QString language;
    QString created;
    QString modified;
};

struct PageStyle
{
    QString id;
    QString fontFamily;
    int fontSize;
    bool bold;
    bool italic;
    bool underline;
    QString color;
    QString alignment;
};

class BookDocument : public QObject
{
    Q_OBJECT

public:
    explicit BookDocument(QObject *parent = nullptr);
    ~BookDocument();

    bool load(const QString &filePath);
    bool save(const QString &filePath = QString());
    bool saveAs(const QString &filePath);
    bool isModified() const;
    QString filePath() const;

    BookMeta meta() const;
    void setMeta(const BookMeta &meta);

    QList<PageStyle> styles() const;
    void addStyle(const PageStyle &style);
    void removeStyle(const QString &id);

    QString pageXml(int pageIndex) const;
    void setPageXml(int pageIndex, const QString &xml);
    int pageCount() const;
    void addPage(int afterIndex = -1);
    void removePage(int pageIndex);
    void movePage(int fromIndex, int toIndex);

    QString pageId(int pageIndex) const;
    QString pageTemplate(int pageIndex) const;
    void setPageTemplate(int pageIndex, const QString &tmpl);

    QString toXml() const;
    void fromXml(const QString &xml);

    void newDocument();

signals:
    void modified();
    void pageAdded(int index);
    void pageRemoved(int index);
    void pageChanged(int index);

private:
    void ensureStructure();
    QDomElement metaElement();
    QDomElement stylesElement();
    QDomElement pagesElement() const;
    QDomElement pageElement(int index) const;
    QString generatePageId() const;

    QDomDocument m_doc;
    QString m_filePath;
    bool m_modified;
    int m_nextPageId;
};

#endif // BOOKDOCUMENT_H
