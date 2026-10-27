#ifndef ChmFORMAT_H
#define ChmFORMAT_H

#include <QObject>
#include "interfaces/IBookFormat.h"

class ChmPlugin : public QObject, public IBookFormat
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.bookcreateedit.format/1.0" FILE "chm.json")
    Q_INTERFACES(IBookFormat IBookPlugin)

public:
    explicit ChmPlugin(QObject *parent = nullptr);
    ~ChmPlugin() override;

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

#endif // ChmFORMAT_H

