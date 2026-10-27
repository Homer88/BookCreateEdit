#include "gtranslateplugin.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDebug>

GTranslatePlugin::GTranslatePlugin(QObject *parent)
    : QObject(parent)
    , m_ctx(nullptr)
    , m_net(new QNetworkAccessManager(this))
{
}

GTranslatePlugin::~GTranslatePlugin() {}

QString GTranslatePlugin::name() const { return "Google Translate"; }
QString GTranslatePlugin::description() const { return "Перевод текста через Google Translate API"; }
QString GTranslatePlugin::version() const { return "1.0.0"; }
QString GTranslatePlugin::serviceName() const { return "Google Translate"; }
QString GTranslatePlugin::lastError() const { return m_lastError; }
bool GTranslatePlugin::isConfigured() const { return true; }

void GTranslatePlugin::setContext(BookContext *ctx) { m_ctx = ctx; }

QStringList GTranslatePlugin::supportedLanguages() const
{
    return {
        "af","sq","am","ar","hy","az","eu","be","bn","bs","bg","ca","ceb",
        "zh-CN","zh-TW","co","hr","cs","da","nl","eo","et","fi","fr","fy",
        "gl","ka","de","el","gu","ht","ha","haw","he","hi","hmn","hu",
        "is","ig","id","ga","it","ja","jv","kn","kk","km","rw","ko","ku",
        "ky","lo","la","lv","lt","lb","mk","mg","ms","ml","mt","mi","mr",
        "mn","my","ne","no","ny","or","ps","fa","pl","pt","pa","ro","ru",
        "sm","gd","sr","st","sn","sd","si","sk","sl","so","es","su","sw",
        "sv","tl","tg","ta","tt","te","th","tr","tk","uk","ur","ug","uz",
        "vi","cy","xh","yi","yo","zu"
    };
}

QString GTranslatePlugin::parseTranslatedJson(const QByteArray &data)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        m_lastError = "JSON parse error: " + err.errorString();
        return QString();
    }

    QJsonArray outer = doc.array();
    if (outer.isEmpty()) {
        m_lastError = "Empty response from Google Translate";
        return QString();
    }

    QString result;
    QJsonArray sentences = outer[0].toArray();
    for (const QJsonValue &v : sentences) {
        QJsonArray pair = v.toArray();
        if (pair.size() >= 1)
            result += pair[0].toString();
    }
    return result;
}

QString GTranslatePlugin::doPost(const QUrl &url, const QByteArray &body)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setRawHeader("User-Agent", "Mozilla/5.0");

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

QString GTranslatePlugin::translate(const QString &text, const QString &fromLang, const QString &toLang)
{
    if (text.trimmed().isEmpty()) return text;

    QUrl url("https://translate.googleapis.com/translate_a/single");
    QByteArray body = "client=gtx&sl=" + fromLang.toUtf8()
                    + "&tl=" + toLang.toUtf8()
                    + "&dt=t&q=" + QUrl::toPercentEncoding(text);

    QString response = doPost(url, body);
    if (response.isEmpty()) return QString();

    return parseTranslatedJson(response.toUtf8());
}

QString GTranslatePlugin::translateHtml(const QString &html, const QString &fromLang, const QString &toLang)
{
    if (html.trimmed().isEmpty()) return html;

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

void GTranslatePlugin::onReplyFinished(QNetworkReply *reply)
{
    Q_UNUSED(reply);
}
