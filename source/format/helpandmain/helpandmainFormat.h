#ifndef HELPMANUALFORMAT_H
#define HELPMANUALFORMAT_H

#include <QObject>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QHash>
#include <QPair>
#include "interfaces/IBookFormat.h"

struct HmTopicRef {
    QString id;
    QString href;
    QString caption;
    int icon;
    QString build;
    QList<HmTopicRef> children;
};

class HelpManualFormat : public QObject, public IBookFormat
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.bookcreateedit.format/1.0" FILE "helpandmain.json")
    Q_INTERFACES(IBookFormat)

public:
    explicit HelpManualFormat(QObject *parent = nullptr);
    ~HelpManualFormat() override;

    QString name() const override;
    QString description() const override;
    QString version() const override;
    QString author() const override;
    QStringList dependencies() const override;

    void initialize(BookContext *ctx) override;
    void shutdown() override;
    bool isInitialized() const override;

    QString formatName() const override;
    QString fileExtension() const override;
    QStringList supportedExtensions() const override;
    QStringList mimeTypes() const override;

    bool canRead() const override;
    bool canWrite() const override;

    bool read(const QString &filePath, BookDocument *doc) override;
    bool write(BookDocument *doc, const QString &filePath) override;

    bool checkDependencies() const override;

private:
    bool readProject(const QString &projectDir, BookDocument *doc);
    bool readTopicFile(const QString &topicPath, BookDocument *doc, int pageIndex);
    void flattenTopicRefs(const QDomElement &el, QList<HmTopicRef> &out);
    void collectTopicRefs(const QDomElement &el, QList<HmTopicRef> &out);
    QString extractTopicText(const QDomElement &body);
    void extractParaText(const QDomElement &para, QString &out);
    void extractListText(const QDomElement &list, QString &out);
    void extractTableText(const QDomElement &table, QString &out);

    bool writeProject(BookDocument *doc, const QString &projectDir);
    QString generateTopicXml(const QString &title, const QString &keywords, const QString &bodyHtml);

    BookContext *m_context;
    bool m_initialized;
};

#endif // HELPMANUALFORMAT_H
