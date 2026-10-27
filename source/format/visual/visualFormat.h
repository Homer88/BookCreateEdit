#ifndef VisualFORMAT_H
#define VisualFORMAT_H

#include <QObject>
#include "interfaces/IBookFormat.h"

/**
 * @brief Плагин формата Visual Studio Help (.mshc).
 *
 * MSHC (Microsoft Help Container) — ZIP-контейнер справки
 * Microsoft Help System (Visual Studio 2010+), содержит:
 *   - book.mshc.xml  — метаданные пакета (название, автор, версия, язык)
 *   - toc.xml        — оглавление (список тем в порядке следования)
 *   - content/*.xhtml — страницы книги (темы справки)
 *
 * Читает и записывает контейнер через собственный минимальный
 * ZIP-райтер/ридер (записи без сжатия, метод stored).
 */
class VisualPlugin : public QObject, public IBookFormat
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.bookcreateedit.format/1.0" FILE "visual.json")
    Q_INTERFACES(IBookFormat IBookPlugin)

public:
    explicit VisualPlugin(QObject *parent = nullptr);
    ~VisualPlugin() override;

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
    /** @brief Извлекает XML-метаданные пакета из контейнера. */
    void parseMetadataXml(const QString &xml, class BookMeta &meta);

    /** @brief Формирует XML метаданных пакета из документа. */
    QString metadataXml(const class BookMeta &meta) const;

    /** @brief Формирует toc.xml из списка тем. */
    QString tocXml(const class BookMeta &meta, const QStringList &titles,
                   const QStringList &files) const;

    /** @brief Оборачивает фрагмент страницы в полноценный XHTML. */
    QString wrapXhtml(const QString &title, const QString &bodyHtml) const;

    /** @brief Выделяет содержимое <body> из XHTML. */
    QString extractBody(const QString &xhtml) const;

    BookContext *m_context;   ///< Контекст приложения
    bool m_initialized;       ///< Флаг инициализации
};

#endif // VisualFORMAT_H
