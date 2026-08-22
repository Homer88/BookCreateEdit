#ifndef __PLUGIN_NAME_UPPER__FORMAT_H
#define __PLUGIN_NAME_UPPER__FORMAT_H

#include <QObject>
#include "interfaces/IBookFormat.h"

class __PLUGIN_CLASS__ : public QObject, public IBookFormat
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.bookcreateedit.format/1.0" FILE "__PLUGIN_NAME_LOWER__.json")
    Q_INTERFACES(IBookFormat)

public:
    explicit __PLUGIN_CLASS__(QObject *parent = nullptr);
    ~__PLUGIN_CLASS__() override;

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
    BookContext *m_context;
    bool m_initialized;
};

#endif // __PLUGIN_NAME_UPPER__FORMAT_H
