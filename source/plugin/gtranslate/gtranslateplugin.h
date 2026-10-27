#ifndef GTRANSLATEPLUGIN_H
#define GTRANSLATEPLUGIN_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class BookContext;

/**
 * @brief Плагин перевода текста через Google Translate.
 *
 * Использует бесплатный API translate.googleapis.com (клиент gtx).
 * Не требует API-ключа для базового использования.
 */
class GTranslatePlugin : public QObject
{
    Q_OBJECT

public:
    explicit GTranslatePlugin(QObject *parent = nullptr);
    ~GTranslatePlugin();

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

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    /** @brief Парсит JSON-ответ Google Translate, извлекает переведённый текст. */
    QString parseTranslatedJson(const QByteArray &data);

    /** @brief Выполняет синхронный HTTP POST запрос. */
    QString doPost(const QUrl &url, const QByteArray &body);

    BookContext *m_ctx;
    QNetworkAccessManager *m_net;
    QString m_lastError;
};

#endif // GTRANSLATEPLUGIN_H
