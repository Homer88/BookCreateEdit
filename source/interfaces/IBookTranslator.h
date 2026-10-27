#ifndef IBOOKTRANSLATOR_H
#define IBOOKTRANSLATOR_H

#include "IBookPlugin.h"

/** IID для Qt-системы плагинов переводчиков. */
#define IBookTranslator_iid "com.bookcreateedit.translator/1.0"

/**
 * @brief Интерфейс плагина-переводчика текста.
 *
 * Каждый плагин перевода (Google Translate, Yandex Translate и т.д.)
 * реализует этот интерфейс. Приложение может предлагать пользователю
 * перевод текущего документа или выделенного фрагмента.
 *
 * Пример использования:
 *   @code
 *   IBookTranslator *t = pluginManager.translator("google");
 *   if (t && t->isConfigured()) {
 *       QString result = t->translate("Hello world", "en", "ru");
 *   }
 *   @endcode
 */
class IBookTranslator : public IBookPlugin
{
public:
    virtual ~IBookTranslator() = default;

    /**
     * @brief Краткое название сервиса перевода (например, "Google Translate").
     * @return Название сервиса.
     */
    virtual QString serviceName() const = 0;

    /**
     * @brief Возвращает список поддерживаемых языков (коды ISO 639-1).
     * @return Список кодов, например {"en", "ru", "de", "fr", ...}.
     */
    virtual QStringList supportedLanguages() const = 0;

    /**
     * @brief Проверяет, настроен ли плагин для работы (есть ли API-ключ и т.д.).
     * @return true, если плагин готов к переводу.
     */
    virtual bool isConfigured() const = 0;

    /**
     * @brief Переводит текст с одного языка на другой.
     * @param text — исходный текст для перевода.
     * @param fromLang — код исходного языка (ISO 639-1, например "en").
     * @param toLang — код целевого языка (ISO 639-1, например "ru").
     * @return Переведённый текст. При ошибке — пустая строка.
     */
    virtual QString translate(const QString &text, const QString &fromLang, const QString &toLang) = 0;

    /**
     * @brief Переводит HTML-фрагмент страницы, сохраняя теги.
     * @param html — исходный HTML.
     * @param fromLang — код исходного языка.
     * @param toLang — код целевого языка.
     * @return HTML с переведённым текстом (теги сохранены). При ошибке — пустая строка.
     */
    virtual QString translateHtml(const QString &html, const QString &fromLang, const QString &toLang) = 0;

    /**
     * @brief Возвращает описание последней ошибки.
     * @return Текст ошибки или пустая строка, если ошибок нет.
     */
    virtual QString lastError() const = 0;
};

Q_DECLARE_INTERFACE(IBookTranslator, IBookTranslator_iid)

#endif // IBOOKTRANSLATOR_H
