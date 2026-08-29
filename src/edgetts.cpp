/*
 * Edge TTS protocol compatibility based on edge-tts:
 * https://github.com/rany2/edge-tts
 */

#include "edgetts.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QWebSocket>

namespace
{
constexpr char trustedClientToken[] = "6A5AA1D4EAFF4E9FB37E23D68491D6F4";
constexpr char secMsGecVersion[] = "1-143.0.3650.75";
constexpr char serviceBaseUrl[] = "speech.platform.bing.com/consumer/speech/synthesize/readaloud";
constexpr char extensionOrigin[] = "chrome-extension://jdiccldimpdaibmpdkjnbmckianbfold";
constexpr int requestTimeoutMs = 30000;
}

EdgeTts::EdgeTts(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this] {
        fail(tr("Edge TTS request timed out"));
    });

}

void EdgeTts::synthesize(const QString &text, QOnlineTranslator::Language language)
{
    abort();

    m_text = sanitizeText(text);
    m_languageCode = QOnlineTranslator::languageCode(language);
    if (m_text.isEmpty()) {
        emit error(tr("Playback text is empty"));
        return;
    }
    if (m_languageCode.isEmpty() || language == QOnlineTranslator::Auto) {
        emit error(tr("A detected language is required for Edge TTS"));
        return;
    }

    m_finishing = false;
    m_audio.clear();
    requestVoices();
}

void EdgeTts::abort()
{
    m_finishing = true;
    m_timeoutTimer->stop();
    if (m_voiceReply != nullptr) {
        m_voiceReply->abort();
        m_voiceReply->deleteLater();
        m_voiceReply = nullptr;
    }
    if (m_socket != nullptr) {
        disconnect(m_socket, nullptr, this, nullptr);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_audio.clear();
}

void EdgeTts::requestVoices()
{
    QUrl url(QStringLiteral("https://%1/voices/list").arg(QString::fromLatin1(serviceBaseUrl)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("trustedclienttoken"), QString::fromLatin1(trustedClientToken));
    query.addQueryItem(QStringLiteral("Sec-MS-GEC"), secMsGec());
    query.addQueryItem(QStringLiteral("Sec-MS-GEC-Version"), QString::fromLatin1(secMsGecVersion));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", browserUserAgent());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Cookie", QStringLiteral("muid=%1;").arg(muid()).toLatin1());
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    QNetworkReply *reply = m_networkManager->get(request);
    m_voiceReply = reply;
    m_timeoutTimer->start(requestTimeoutMs);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_finishing || m_voiceReply != reply) {
            reply->deleteLater();
            return;
        }

        const QByteArray response = reply->readAll();
        const QString networkError = reply->errorString();
        const bool succeeded = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        m_voiceReply = nullptr;

        if (!succeeded) {
            fail(networkError);
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
            fail(tr("Unable to parse the Edge TTS voice list"));
            return;
        }

        const QString preferredLocale = m_languageCode.compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("en-US")
            : m_languageCode;
        QString fallbackVoice;
        QString selectedVoice;
        for (const QJsonValue &value : document.array()) {
            const QJsonObject voice = value.toObject();
            const QString locale = voice.value(QStringLiteral("Locale")).toString();
            const QString shortName = voice.value(QStringLiteral("ShortName")).toString();
            if (locale.compare(preferredLocale, Qt::CaseInsensitive) == 0) {
                selectedVoice = shortName;
                break;
            }
            if (fallbackVoice.isEmpty() && locale.startsWith(m_languageCode + QLatin1Char('-'), Qt::CaseInsensitive))
                fallbackVoice = shortName;
        }

        if (selectedVoice.isEmpty())
            selectedVoice = fallbackVoice;
        if (selectedVoice.isEmpty()) {
            fail(tr("Edge TTS has no voice for %1").arg(m_languageCode));
            return;
        }

        openSocket(selectedVoice);
    });
}

void EdgeTts::openSocket(const QString &voice)
{
    auto *socket = new QWebSocket(QString::fromLatin1(extensionOrigin), QWebSocketProtocol::VersionLatest, this);
    m_socket = socket;

    connect(socket, &QWebSocket::connected, this, [this, socket, voice] {
        if (m_socket == socket)
            sendSynthesisRequests(voice);
    });
    connect(socket, &QWebSocket::binaryMessageReceived, this, &EdgeTts::handleBinaryMessage);
    connect(socket, &QWebSocket::textMessageReceived, this, &EdgeTts::handleTextMessage);
    connect(socket, qOverload<QAbstractSocket::SocketError>(&QWebSocket::error), this, [this, socket](QAbstractSocket::SocketError) {
        if (m_socket == socket && !m_finishing)
            fail(socket->errorString());
    });
    connect(socket, &QWebSocket::disconnected, this, [this, socket] {
        if (m_socket != socket)
            return;
        if (!m_finishing) {
            fail(tr("Edge TTS connection closed before audio was received"));
            return;
        }
        m_socket = nullptr;
        socket->deleteLater();
    });

    QUrl url(QStringLiteral("wss://%1/edge/v1").arg(QString::fromLatin1(serviceBaseUrl)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("TrustedClientToken"), QString::fromLatin1(trustedClientToken));
    query.addQueryItem(QStringLiteral("ConnectionId"), connectionId());
    query.addQueryItem(QStringLiteral("Sec-MS-GEC"), secMsGec());
    query.addQueryItem(QStringLiteral("Sec-MS-GEC-Version"), QString::fromLatin1(secMsGecVersion));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", browserUserAgent());
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Cookie", QStringLiteral("muid=%1;").arg(muid()).toLatin1());
    socket->open(request);
    m_timeoutTimer->start(requestTimeoutMs);
}

void EdgeTts::sendSynthesisRequests(const QString &voice)
{
    const QString currentTimestamp = timestamp();
    const QString speechConfig = QStringLiteral(
        "X-Timestamp:%1\r\n"
        "Content-Type:application/json; charset=utf-8\r\n"
        "Path:speech.config\r\n\r\n"
        "{\"context\":{\"synthesis\":{\"audio\":{\"metadataoptions\":{"
        "\"sentenceBoundaryEnabled\":\"false\",\"wordBoundaryEnabled\":\"false\"},"
        "\"outputFormat\":\"audio-24khz-48kbitrate-mono-mp3\"}}}}\r\n")
                                     .arg(currentTimestamp);

    const QString ssml = QStringLiteral(
        "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='en-US'>"
        "<voice name='%1'><prosody pitch='+0Hz' rate='+0%' volume='+0%'>%2</prosody></voice></speak>")
                             .arg(voice, m_text.toHtmlEscaped());
    const QString ssmlRequest = QStringLiteral(
        "X-RequestId:%1\r\n"
        "Content-Type:application/ssml+xml\r\n"
        "X-Timestamp:%2Z\r\n"
        "Path:ssml\r\n\r\n%3")
                                    .arg(connectionId(), currentTimestamp, ssml);

    m_socket->sendTextMessage(speechConfig);
    m_socket->sendTextMessage(ssmlRequest);
}

void EdgeTts::handleBinaryMessage(const QByteArray &message)
{
    if (sender() != m_socket)
        return;

    m_timeoutTimer->start(requestTimeoutMs);
    if (message.size() < 2) {
        fail(tr("Edge TTS returned an invalid audio frame"));
        return;
    }

    const auto first = static_cast<quint8>(message.at(0));
    const auto second = static_cast<quint8>(message.at(1));
    const int headerLength = (first << 8) | second;
    if (headerLength <= 0 || message.size() < headerLength + 2) {
        fail(tr("Edge TTS returned an invalid audio header"));
        return;
    }

    const QByteArray headers = message.mid(2, headerLength);
    if (!headers.contains("Path:audio")) {
        fail(tr("Edge TTS returned an unexpected binary frame"));
        return;
    }

    m_audio.append(message.mid(headerLength + 2));
}

void EdgeTts::handleTextMessage(const QString &message)
{
    if (sender() != m_socket)
        return;

    m_timeoutTimer->start(requestTimeoutMs);
    if (!message.contains(QStringLiteral("Path:turn.end")))
        return;

    if (m_audio.isEmpty()) {
        fail(tr("Edge TTS returned no audio"));
        return;
    }

    m_finishing = true;
    m_timeoutTimer->stop();
    m_socket->close();
    emit audioReady(m_audio);
}

void EdgeTts::fail(const QString &message)
{
    if (m_finishing)
        return;

    m_finishing = true;
    m_timeoutTimer->stop();
    if (m_voiceReply != nullptr) {
        m_voiceReply->abort();
        m_voiceReply->deleteLater();
        m_voiceReply = nullptr;
    }
    if (m_socket != nullptr) {
        QWebSocket *socket = m_socket;
        m_socket = nullptr;
        disconnect(socket, nullptr, this, nullptr);
        socket->abort();
        socket->deleteLater();
    }
    emit error(message);
}

QByteArray EdgeTts::browserUserAgent()
{
    return QByteArrayLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                             "(KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36 Edg/143.0.0.0");
}

QString EdgeTts::connectionId()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

QString EdgeTts::muid()
{
    return connectionId().toUpper();
}

QString EdgeTts::secMsGec()
{
    constexpr quint64 windowsEpochSeconds = 11644473600ULL;
    constexpr quint64 ticksPerSecond = 10000000ULL;
    quint64 seconds = static_cast<quint64>(QDateTime::currentSecsSinceEpoch()) + windowsEpochSeconds;
    seconds -= seconds % 300;
    const QByteArray value = QByteArray::number(seconds * ticksPerSecond) + trustedClientToken;
    return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex().toUpper());
}

QString EdgeTts::timestamp()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString date = QLocale(QLocale::English, QLocale::UnitedStates).toString(now, QStringLiteral("ddd MMM dd yyyy HH:mm:ss"));
    return date + QStringLiteral(" GMT+0000 (Coordinated Universal Time)");
}

QString EdgeTts::sanitizeText(QString text)
{
    for (int index = 0; index < text.size(); ++index) {
        const ushort code = text.at(index).unicode();
        if (code <= 8 || (code >= 11 && code <= 12) || (code >= 14 && code <= 31))
            text[index] = QLatin1Char(' ');
    }
    return text.trimmed();
}
