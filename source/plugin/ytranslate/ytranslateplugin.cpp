#include "ytranslateplugin.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>
#include <QSettings>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDebug>

YTranslatePlugin::YTranslatePlugin(QObject *parent)
    : QObject(parent)
    , m_ctx(nullptr)
    , m_net(new QNetworkAccessManager(this))
{
    loadApiKey();
}

YTranslatePlugin::~YTranslatePlugin() {}

QString YTranslatePlugin::name() const { return "Yandex Translate"; }
QString YTranslatePlugin::description() const { return "Перевод текста через Yandex Translate API v2"; }
QString YTranslatePlugin::version() const { return "1.0.0"; }
QString YTranslatePlugin::serviceName() const { return "Yandex Translate"; }
QString YTranslatePlugin::lastError() const { return m_lastError; }

void YTranslatePlugin::setContext(BookContext *ctx) { m_ctx = ctx; }
void YTranslatePlugin::setApiKey(const QString &apiKey) { m_apiKey = apiKey; }

void YTranslatePlugin::loadApiKey()
{
    QSettings s("BookCreateEdit.ini", QSettings::IniFormat);
    m_apiKey = s.value("ytranslate/apiKey", "").toString();
}

bool YTranslatePlugin::isConfigured() const
{
    return !m_apiKey.isEmpty();
}

QStringList YTranslatePlugin::supportedLanguages() const
{
    return {
        "az","sq","am","en","ar","hy","af","eu","ba","be","bn","my",
        "bg","bs","cy","hu","vi","ht","nl","el","ka","da","he","id",
        "ga","it","es","kk","ca","qy","zh","ko","ku","ky","la","lv",
        "lt","mg","ms","ml","mt","mk","mn","de","ne","no","fa","pl",
        "pt","ro","ru","sr","si","sk","sl","sw","tg","ta","tt","te",
        "th","tr","uz","uk","ur","fi","fr","hi","hr","ce","sv","eo",
        "et","jv","ja"
    };
}

QString YTranslatePlugin::parseTranslatedJson(const QByteArray &data)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        m_lastError = "JSON parse error: " + err.errorString();
        return QString();
    }

    QJsonObject root = doc.object();
    if (root.contains("error")) {
        QJsonObject error = root["error"].toObject();
        m_lastError = error["message"].toString();
        if (m_lastError.isEmpty())
            m_lastError = "Yandex API error";
        return QString();
    }

    QJsonArray translations = root["translations"].toArray();
    if (translations.isEmpty()) {
        m_lastError = "Empty translations array";
        return QString();
    }

    return translations[0].toObject()["text"].toString();
}

QString YTranslatePlugin::doPost(const QUrl &url, const QByteArray &body)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Api-Key " + m_apiKey).toUtf8());

    QNetworkReply *reply = m_net->post(req, body);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() {
        timer.stop();
        loop.quit();
    });

    timer.start(30000);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        m_lastError = reply->errorString();
        reply->deleteLater();
        return QString();
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    return QString::fromUtf8(data);
}

QString YTranslatePlugin::translate(const QString &text, const QString &fromLang, const QString &toLang)
{
    if (text.trimmed().isEmpty()) return text;

    if (m_apiKey.isEmpty()) {
        m_lastError = "API-ключ Yandex Translate не настроен";
        return QString();
    }

    QUrl url("https://translate.api.cloud.yandex.net/translate/v2/translate");

    QJsonObject jsonBody;
    jsonBody["sourceLanguageCode"] = fromLang;
    jsonBody["targetLanguageCode"] = toLang;
    jsonBody["texts"] = QJsonArray{text};

    QJsonDocument doc(jsonBody);
    QString response = doPost(url, doc.toJson(QJsonDocument::Compact));
    if (response.isEmpty()) return QString();

    return parseTranslatedJson(response.toUtf8());
}

QString YTranslatePlugin::translateHtml(const QString &html, const QString &fromLang, const QString &toLang)
{
    if (html.trimmed().isEmpty()) return html;

    if (m_apiKey.isEmpty()) {
        m_lastError = "API-ключ Yandex Translate не настроен";
        return QString();
    }

    QRegularExpression tagRe("<[^>]*>");
    QRegularExpressionMatchIterator it = tagRe.globalMatch(html);

    QStringList segments;
    int lastPos = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        if (m.capturedStart() > lastPos)
            segments.append(html.mid(lastPos, m.capturedStart() - lastPos));
        segments.append(m.captured(0));
        lastPos = m.capturedEnd();
    }
    if (lastPos < html.size())
        segments.append(html.mid(lastPos));

    QStringList textsToTranslate;
    QList<int> textIndices;
    for (int i = 0; i < segments.size(); ++i) {
        if (!segments[i].startsWith('<')) {
            QString trimmed = segments[i].trimmed();
            if (!trimmed.isEmpty()) {
                textsToTranslate.append(trimmed);
                textIndices.append(i);
            }
        }
    }

    if (textsToTranslate.isEmpty()) return html;

    QStringList translatedTexts;
    for (const QString &t : textsToTranslate) {
        translatedTexts.append(translate(t, fromLang, toLang));
    }

    for (int i = 0; i < textIndices.size(); ++i) {
        if (!translatedTexts[i].isEmpty())
            segments[textIndices[i]] = translatedTexts[i];
    }

    return segments.join("");
}
