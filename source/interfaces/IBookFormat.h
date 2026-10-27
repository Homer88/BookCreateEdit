#ifndef IBOOKFORMAT_H
#define IBOOKFORMAT_H

#include "IBookPlugin.h"

/** IID для Qt-системы плагинов форматов файлов. */
#define IBookFormat_iid "com.bookcreateedit.format/1.0"

/**
 * @brief Интерфейс плагина формата файлов (чтение/запись).
 *
 * Наследует IBookPlugin и добавляет контракт для импорта/экспорта документов.
 *
 * Каждый формат (EPUB, DOCX, RTF, HLP, MOBI и т.д.) реализует этот интерфейс.
 * Плагин сообщает о поддерживаемых расширениях через supportedExtensions(),
 * а приложение решает, какие плагины показывать в диалогах «Открыть»/«Сохранить».
 *
 * Форматы BCE (.bce) — нативный формат приложения на основе XML.
 * Плагины форматов конвертируют из/в форматы файлов ↔ BCE XML.
 *
 * Пример использования (import):
 *   @code
 *   IBookFormat *fmt = pluginManager.formatForExtension("epub");
 *   if (fmt && fmt->canRead())
 *       fmt->read(filePath, document);
 *   @endcode
 *
 * Пример использования (export):
 *   @code
 *   IBookFormat *fmt = pluginManager.formatForExtension("epub");
 *   if (fmt && fmt->canWrite())
 *       fmt->write(document, filePath);
 *   @endcode
 */
class IBookFormat : public IBookPlugin
{
public:
    virtual ~IBookFormat() = default;

    /** @brief Человекочитаемое название формата (например, "EPUB", "RTF"). */
    virtual QString formatName() const = 0;

    /**
     * @brief Основное расширение файла формата (например, "epub").
     * Используется для автоподстановки расширения при сохранении.
     */
    virtual QString fileExtension() const = 0;

    /** @brief Все расширения файлов, которые умеет читать/писать этот плагин. */
    virtual QStringList supportedExtensions() const = 0;

    /** @brief MIME-типы файлов формата (например, "application/epub+zip"). */
    virtual QStringList mimeTypes() const = 0;

    /**
     * @brief Возвращает true, если плагин поддерживает чтение (импорт).
     * Если возвращает false — метод read() вызываться не будет.
     */
    virtual bool canRead() const = 0;

    /**
     * @brief Возвращает true, если плагин поддерживает запись (экспорт).
     * Если возвращает false — метод write() вызываться не будет.
     */
    virtual bool canWrite() const = 0;

    /**
     * @brief Читает файл формата и заполняет BookDocument.
     * @param filePath — путь к файлу для чтения.
     * @param doc      — целевой документ; плагин должен вызвать
     *                   doc->setMeta(), doc->addPage(), doc->setPageXml().
     * @return true при успехе, false при ошибке (формат не распознан, повреждён и т.д.).
     */
    virtual bool read(const QString &filePath, BookDocument *doc) = 0;

    /**
     * @brief Записывает BookDocument в файл указанного формата.
     * @param doc      — исходный документ для экспорта.
     * @param filePath — путь к выходному файлу.
     * @return true при успехе.
     *
     * Плагин должен сам определить формат выходного файла по расширению
     * filePath или по自己的 внутренней логике.
     */
    virtual bool write(BookDocument *doc, const QString &filePath) = 0;

    /**
     * @brief Проверяет, доступны ли все необходимые зависимости плагина.
     * @return true, если плагин готов к работе.
     *
     * Например, плагин PDF может проверить наличие библиотеки Poppler.
     */
    virtual bool checkDependencies() const = 0;
};

Q_DECLARE_INTERFACE(IBookFormat, IBookFormat_iid)

#endif // IBOOKFORMAT_H
