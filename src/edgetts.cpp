/*
 * Edge TTS protocol compatibility based on edge-tts:
 * https://github.com/rany2/edge-tts
 */

#include "edgetts.h"

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
#include <QVector>
#include <QWebSocket>
#include <QtEndian>

#include <array>
#include <utility>

namespace
{
constexpr char trustedClientToken[] = "6A5AA1D4EAFF4E9FB37E23D68491D6F4";
constexpr char secMsGecVersion[] = "2-143.0.3650.75";
constexpr char serviceBaseUrl[] = "speech.platform.bing.com/consumer/speech/synthesize/readaloud";
constexpr char extensionOrigin[] = "chrome-extension://jdiccldimpdaibmpdkjnbmckianbfold";
constexpr int requestTimeoutMs = 30000;

constexpr std::array<quint32, 8> sha256Initial = {
    0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
    0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U,
};

constexpr std::array<quint32, 8> sha256State = {
    0xFD9FBEA2U, 0x91243CE7U, 0xBE499094U, 0xCD2E1E0AU,
    0x6C006F49U, 0x04BD35ABU, 0xFEB0A7E4U, 0x03EFFBD2U,
};

constexpr std::array<quint32, 14> sha256Schedule = {
    0xEB24BF9EU, 0x1558691FU, 0xA0C09EE2U, 0x0CFA32BAU,
    0xA622EE8CU, 0xD1DD9491U, 0xC0DD25A0U, 0xF9E70AB6U,
    0xF0C31573U, 0x7058CE4EU, 0x7188E606U, 0x00E5AB33U,
    0xD258B060U, 0x72766564U,
};

constexpr std::array<quint32, 64> sha256RoundConstants = {
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
    0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
    0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
    0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
    0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
    0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
    0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
    0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
    0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
    0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
    0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
    0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
    0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
    0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
};

constexpr quint32 sha256Adjustment = 0xC9DC06A7U;
constexpr quint32 sha256Sigma0Adjustment = 0x059C0B80U;
constexpr quint32 sha256Sigma1Adjustment = 0x000B92C0U;

constexpr std::array<std::pair<quint32, quint32>, 14> cs64Coefficients = {{
    {0xE82E91B1U, 0xB32700D0U},
    {0x44DE24C6U, 0x88DFF51DU},
    {0x3BEB65BCU, 0x29514038U},
    {0xC89C5933U, 0xB328CBDFU},
    {0x8D1EC211U, 0xD405B4D5U},
    {0x314EF5D5U, 0x26E55F3CU},
    {0xE92491B1U, 0x607801C6U},
    {0xF0A5B9D5U, 0x82B2C1FBU},
    {0x06064CF3U, 0xFA51A3DAU},
    {0xDA79FA60U, 0x207CC464U},
    {0xBED20560U, 0x3C3C74F3U},
    {0x89F9C9F8U, 0xD2D14B61U},
    {0x35FA38C6U, 0xBB04A2C1U},
    {0xCB49E3C6U, 0xC5626167U},
}};

constexpr quint32 cs64Factor = 0x82893933U;
constexpr quint32 cs64Adjustment0 = 0xF2F90B1AU;
constexpr quint32 cs64Adjustment1 = 0x0B1A0000U;

constexpr quint32 rotr32(quint32 value, int shift)
{
    return (value >> shift) | (value << (32 - shift));
}

constexpr quint32 rol16(quint32 value)
{
    return (value << 16) | (value >> 16);
}

constexpr quint32 sigma0(quint32 value)
{
    return rotr32(value, 7) ^ rotr32(value, 18) ^ (value >> 3);
}

constexpr quint32 sigma1(quint32 value)
{
    return rotr32(value, 17) ^ rotr32(value, 19) ^ (value >> 10);
}

void expandSha256Schedule(std::array<quint32, 64> &words, int firstWord = 16)
{
    for (int index = firstWord; index < 64; ++index)
        words[index] = sigma1(words[index - 2]) + words[index - 7] + sigma0(words[index - 15]) + words[index - 16];
}

std::array<quint32, 8> sha256Rounds(std::array<quint32, 8> state, const std::array<quint32, 64> &words, int firstRound = 0)
{
    quint32 a = state[0];
    quint32 b = state[1];
    quint32 c = state[2];
    quint32 d = state[3];
    quint32 e = state[4];
    quint32 f = state[5];
    quint32 g = state[6];
    quint32 h = state[7];

    for (int index = firstRound; index < 64; ++index) {
        const quint32 k = (firstRound == 14 && index == 14) ? sha256Adjustment : sha256RoundConstants[index];
        const quint32 s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        const quint32 ch = g ^ (e & (f ^ g));
        const quint32 t1 = h + s1 + ch + k + words[index];
        const quint32 s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        const quint32 maj = (a & b) | (c & (a | b));
        const quint32 t2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    return {a, b, c, d, e, f, g, h};
}

std::array<quint32, 8> sha256Init(const QByteArray &salt)
{
    std::array<quint32, 64> words{};
    words[14] = (static_cast<quint32>(static_cast<quint8>(salt[0])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(salt[1])) << 8)
        | static_cast<quint32>(static_cast<quint8>(salt[2]));
    words[15] = qFromBigEndian<quint32>(salt.constData() + 3);

    for (int index = 16; index < 30; ++index) {
        quint32 value = sha256Schedule[index - 16];
        if (index - 2 >= 14)
            value += (index == 16) ? (sigma1(words[index - 2]) ^ sha256Sigma1Adjustment) : sigma1(words[index - 2]);
        if (index - 7 >= 14)
            value += words[index - 7];
        if (index - 15 >= 14)
            value += (index == 29) ? (sigma0(words[index - 15]) ^ sha256Sigma0Adjustment) : sigma0(words[index - 15]);
        words[index] = value;
    }

    words[30] = sigma1(words[28]) + words[23] + sigma0(words[15]) + words[14] + sha256Adjustment - sha256RoundConstants[14];
    expandSha256Schedule(words, 31);

    const std::array<quint32, 8> working = sha256Rounds(sha256State, words, 14);
    std::array<quint32, 8> result{};
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = sha256Initial[i] + working[i];
    return result;
}

QByteArray sha256Digest(const QByteArray &data)
{
    const int length = 57 + data.size();
    const int zeroPadding = (55 - (length % 64) + 64) % 64;

    QByteArray padded = data.mid(7);
    padded.append(char(0x80));
    padded.append(QByteArray(zeroPadding, '\0'));
    const quint64 bitLengthBe = qToBigEndian<quint64>(static_cast<quint64>(length) * 8ULL);
    padded.append(reinterpret_cast<const char *>(&bitLengthBe), sizeof(bitLengthBe));

    std::array<quint32, 8> state = sha256Init(data.left(7));
    for (int offset = 0; offset < padded.size(); offset += 64) {
        std::array<quint32, 64> words{};
        for (int i = 0; i < 16; ++i)
            words[i] = qFromBigEndian<quint32>(padded.constData() + offset + i * 4);
        expandSha256Schedule(words);
        const std::array<quint32, 8> working = sha256Rounds(state, words);
        for (size_t i = 0; i < state.size(); ++i)
            state[i] += working[i];
    }

    QByteArray digest(32, Qt::Uninitialized);
    for (size_t i = 0; i < state.size(); ++i)
        qToBigEndian<quint32>(state[i], digest.data() + i * 4);
    return digest;
}

QByteArray cs64Digest(const QByteArray &data, const QByteArray &digest)
{
    const quint32 keyA = qFromLittleEndian<quint32>(digest.constData()) | 1U;
    const quint32 keyB = qFromLittleEndian<quint32>(digest.constData() + 4) | 1U;
    const std::array<quint32, 2> factors = {
        keyA * cs64Factor,
        keyB * cs64Factor,
    };

    QVector<quint32> terms;
    QVector<quint32> offsets;
    terms.reserve(14 + (data.size() + 8) / 4);
    offsets.reserve(14 + (data.size() + 8) / 4);

    for (size_t index = 0; index < cs64Coefficients.size(); ++index) {
        terms.append(cs64Coefficients[index].first * factors[index % 2]);
        offsets.append(cs64Coefficients[index].second);
    }

    QByteArray padded(1, '\0');
    padded.append(data);
    while (padded.size() % 8 != 0)
        padded.append('\0');

    const int wordCount = padded.size() / 4;
    for (int index = 0; index < wordCount; ++index) {
        const quint32 word = qFromLittleEndian<quint32>(padded.constData() + index * 4);
        quint32 term = word * (index % 2 == 0 ? keyA : keyB);
        quint32 offset = rol16(word) * (index % 2 == 0 ? 0xEF0569FBU : 0xC31713DBU);
        if (index == 0) {
            term += cs64Adjustment0 * factors[0];
            offset += cs64Adjustment1;
        }
        terms.append(term);
        offsets.append(offset);
    }

    quint32 swapAcc = 0;
    quint32 swapLast = 0;
    quint32 state = 0;
    quint32 acc = 0;
    for (int index = 0; index < terms.size(); index += 2) {
        const quint32 x = terms[index];
        const quint32 y = terms[index + 1];
        quint32 t = rol16(offsets[index] + x);
        quint32 q = t * 0x76F50F34U;
        swapAcc += rol16(q) * 0xC3EFEA97U + q;
        t = rol16(offsets[index + 1] + y);
        q = t * 0x1881CE3CU;
        swapLast = rol16(q) * 0x35BD1EC9U + q;
        swapAcc += swapLast;

        state = state * keyA + x;
        state = rol16(state) * 0xCF98B111U;
        state = rol16(state) * 0x87085B9FU;
        state = rol16(state) * 0x12CEB96DU;
        state = rol16(state) * 0x257E1D83U;
        acc += state;
        t = state * keyB + y;
        t = rol16(t) * 0xA27416F5U;
        t = rol16(t) * 0xD38396FFU;
        state = rol16(rol16(t) * 0x7C932B89U) * 0xBFA49F69U;
        acc += state;
    }

    QByteArray out(8, Qt::Uninitialized);
    qToLittleEndian<quint32>(swapLast ^ state, out.data());
    qToLittleEndian<quint32>(swapAcc ^ acc, out.data() + 4);
    return out;
}
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
        const QDateTime serverDate = QDateTime::fromString(QString::fromLatin1(reply->rawHeader("Date")), Qt::RFC2822Date);
        const qint64 unixTimestamp = serverDate.isValid() ? serverDate.toSecsSinceEpoch() : QDateTime::currentSecsSinceEpoch();
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

        openSocket(selectedVoice, unixTimestamp);
    });
}

void EdgeTts::openSocket(const QString &voice, qint64 unixTimestamp)
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
    query.addQueryItem(QStringLiteral("Sec-MS-GEC"), secMsGec(unixTimestamp));
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

QString EdgeTts::secMsGec(qint64 unixTimestamp)
{
    constexpr quint64 windowsEpochSeconds = 11644473600ULL;
    constexpr quint64 ticksPerSecond = 10000000ULL;
    quint64 seconds = static_cast<quint64>(unixTimestamp) + windowsEpochSeconds;
    seconds -= seconds % 300;
    const QByteArray value = QByteArray::number(seconds * ticksPerSecond) + trustedClientToken;
    const QByteArray digest = sha256Digest(value);
    const QByteArray cs64 = cs64Digest(value, digest);
    return QStringLiteral("%1-%2").arg(
        QString::fromLatin1(cs64.toHex().toUpper()),
        QString::fromLatin1(digest.toHex().toUpper()));
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
