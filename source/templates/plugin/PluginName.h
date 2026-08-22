#ifndef __PLUGIN_NAME_UPPER___PLUGIN_H
#define __PLUGIN_NAME_UPPER___PLUGIN_H

#include <QObject>
#include "interfaces/IBookPlugin.h"

class __PLUGIN_CLASS__ : public QObject, public IBookPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.bookcreateedit.plugin/1.0" FILE "__PLUGIN_NAME_LOWER__.json")
    Q_INTERFACES(IBookPlugin)

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

private:
    BookContext *m_context;
    bool m_initialized;
};

#endif // __PLUGIN_NAME_UPPER___PLUGIN_H
