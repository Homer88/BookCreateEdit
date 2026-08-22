#ifndef Diagramrenderer_PLUGIN_H
#define Diagramrenderer_PLUGIN_H

#include <QObject>
#include "interfaces/IBookPlugin.h"

class DiagramrendererPlugin : public QObject, public IBookPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.bookcreateedit.plugin/1.0" FILE "diagramrenderer.json")
    Q_INTERFACES(IBookPlugin)

public:
    explicit DiagramrendererPlugin(QObject *parent = nullptr);
    ~DiagramrendererPlugin() override;

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

#endif // Diagramrenderer_PLUGIN_H

