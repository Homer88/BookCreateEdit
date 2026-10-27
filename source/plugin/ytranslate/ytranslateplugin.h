#ifndef YTRANSLATEPLUGIN_H
#define YTRANSLATEPLUGIN_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class BookContext;

/**
 * @brief Плагин перевода текста через Yandex Translate API.
 *
 * Использует официальный Yandex Cloud Translate API v2.
 * Требуется API-ключ из Yandex Cloud (секрет IAM-токена).
 *
 * Работает через:
 *   https://translate.api.cloud.yandex.net/translate/v2/translate
 *
 * API-ключ сохраняется в SettingsManager:
 *   ytranslate/apiKey — IAM-токен или API-ключ.
 */
class YTranslatePlugin : public QObject
{
    Q_OBJECT

public:
    explicit YTranslatePlugin(QObject *parent = nullptr);
    ~YTranslatePlugin();

    QString name() const;
    QString description() const;
    QString version() const;
    QString serviceName() const;
    QStringList supportedLanguages() const;
    bool isConfigured() const;

    /**
     * @brief Переводит текст с одного языка на другой.
     * @param text — исходный текст.
     * @param fromLang — код исходного языка (ISO 639-1).
     * @param toLang — код целевого языка (ISO 639-1).
     * @return Переведённый текст или пустая строка при ошибке.
     */
    QString translate(const QString &text, const QString &fromLang, const QString &toLang);

    /**
     * @brief Переводит HTML-фрагмент, сохраняя теги.
     * @param html — исходный HTML.
     * @param fromLang — код исходного языка.
     * @param toLang — код целевого языка.
     * @return HTML с переведённым текстом.
     */
    QString translateHtml(const QString &html, const QString &fromLang, const QString &toLang);

    /** @brief Возвращает описание последней ошибки. */
    QString lastError() const;

    /** @brief Устанавливает контекст приложения для логирования. */
    void setContext(BookContext *ctx);

    /** @brief Устанавливает API-ключ (IAM-токен) для доступа к Yandex Cloud. */
    void setApiKey(const QString &apiKey);

private:
    /** @brief Парсит JSON-ответ Yandex Translate API. */
    QString parseTranslatedJson(const QByteArray &data);

    /** @brief Выполняет синхронный HTTP POST запрос с авторизацией. */
    QString doPost(const QUrl &url, const QByteArray &body);

    /** @brief Загружает API-ключ из SettingsManager. */
    void loadApiKey();

    BookContext *m_ctx;
    QNetworkAccessManager *m_net;
    QString m_apiKey;
    QString m_lastError;
};

#endif // YTRANSLATEPLUGIN_H
