#ifndef IBOOKFORMAT_H
#define IBOOKFORMAT_H

#include "IBookPlugin.h"

#define IBookFormat_iid "com.bookcreateedit.format/1.0"

class IBookFormat : public IBookPlugin
{
public:
    virtual ~IBookFormat() = default;

    virtual QString formatName() const = 0;
    virtual QString fileExtension() const = 0;
    virtual QStringList supportedExtensions() const = 0;
    virtual QStringList mimeTypes() const = 0;

    virtual bool canRead() const = 0;
    virtual bool canWrite() const = 0;

    virtual bool read(const QString &filePath, BookDocument *doc) = 0;
    virtual bool write(BookDocument *doc, const QString &filePath) = 0;

    virtual bool checkDependencies() const = 0;
};

Q_DECLARE_INTERFACE(IBookFormat, IBookFormat_iid)

#endif // IBOOKFORMAT_H
