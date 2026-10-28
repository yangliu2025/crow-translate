#ifndef EDGETTS_H
#define EDGETTS_H

#include "qonlinetranslator.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QWebSocket;

class EdgeTts : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(EdgeTts)
    friend class SpeakButtonsTest;

public:
    explicit EdgeTts(QObject *parent = nullptr);

    void synthesize(const QString &text, QOnlineTranslator::Language language);
    void abort();

signals:
    void audioReady(const QByteArray &audio);
    void error(const QString &message);

private:
    void requestVoices();
    void openSocket(const QString &voice, qint64 unixTimestamp);
    void sendSynthesisRequests(const QString &voice);
    void handleBinaryMessage(const QByteArray &message);
    void handleTextMessage(const QString &message);
    void fail(const QString &message);

    static QByteArray browserUserAgent();
    static QString connectionId();
    static QString muid();
    static QString secMsGec(qint64 unixTimestamp = QDateTime::currentSecsSinceEpoch());
    static QString timestamp();
    static QString sanitizeText(QString text);

    QNetworkAccessManager *m_networkManager;
    QWebSocket *m_socket = nullptr;
    QTimer *m_timeoutTimer;
    QPointer<QNetworkReply> m_voiceReply;
    QByteArray m_audio;
    QString m_text;
    QString m_languageCode;
    bool m_finishing = false;
};

#endif // EDGETTS_H
