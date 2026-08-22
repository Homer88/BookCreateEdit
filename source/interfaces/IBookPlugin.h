#ifndef IBOOKPLUGIN_H
#define IBOOKPLUGIN_H

#include <QString>
#include <QStringList>

class BookDocument;

class BookContext
{
public:
    virtual ~BookContext() = default;
    virtual BookDocument *currentDocument() const = 0;
    virtual void log(const QString &message) = 0;
};

#define IBookPlugin_iid "com.bookcreateedit.plugin/1.0"

class IBookPlugin
{
public:
    virtual ~IBookPlugin() = default;

    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString version() const = 0;
    virtual QString author() const = 0;
    virtual QStringList dependencies() const = 0;

    virtual void initialize(BookContext *ctx) = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;
};

Q_DECLARE_INTERFACE(IBookPlugin, IBookPlugin_iid)

#endif // IBOOKPLUGIN_H
