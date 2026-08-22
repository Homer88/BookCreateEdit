#ifndef EPUBFORMAT_H
#define EPUBFORMAT_H

#include <QObject>
#include "interfaces/IBookFormat.h"

class EpubFormat : public QObject, public IBookFormat
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.bookcreateedit.format/1.0" FILE "epub.json")
    Q_INTERFACES(IBookFormat)

public:
    explicit EpubFormat(QObject *parent = nullptr);
    ~EpubFormat() override;

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
    bool readEpub(const QString &path, BookDocument *doc);
    bool writeEpub(BookDocument *doc, const QString &path);
    QString containerXml() const;
    QString opfXml(const BookDocument *doc) const;
    QString xhtmlPage(const QString &id, const QString &content) const;

    BookContext *m_context;
    bool m_initialized;
};

#endif // EPUBFORMAT_H
