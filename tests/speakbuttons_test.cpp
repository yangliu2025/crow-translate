#include "edgetts.h"
#include "speakbuttons.h"

#include <QDataStream>
#include <QFile>
#include <QMediaPlaylist>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include <cstring>

class AudioReply : public QNetworkReply
{
public:
    AudioReply(const QNetworkRequest &request, QObject *parent)
        : QNetworkReply(parent)
    {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly);
    }

    void finish(const QByteArray &audio, NetworkError error = NoError)
    {
        if (isFinished())
            return;
        m_audio = audio;
        if (error != NoError)
            setError(error, QStringLiteral("Simulated download failure"));
        setFinished(true);
        emit readyRead();
        emit finished();
    }

    void abort() override
    {
        finish({}, OperationCanceledError);
    }

    qint64 bytesAvailable() const override
    {
        return m_audio.size() - m_position + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 size = qMin(maxSize, m_audio.size() - m_position);
        if (size <= 0)
            return -1;
        std::memcpy(data, m_audio.constData() + m_position, static_cast<size_t>(size));
        m_position += size;
        return size;
    }

private:
    QByteArray m_audio;
    qint64 m_position = 0;
};

class AudioNetworkManager : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

    QList<QUrl> requests;
    QPointer<AudioReply> pendingReply;

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        requests.append(request.url());
        pendingReply = new AudioReply(request, this);
        return pendingReply;
    }
};

class SpeakButtonsTest : public QObject
{
    Q_OBJECT

private:
    static QByteArray audioData()
    {
        QByteArray audio;
        QDataStream stream(&audio, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF", 4);
        stream << quint32(1636);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(8000)
               << quint32(16000) << quint16(2) << quint16(16);
        stream.writeRawData("data", 4);
        stream << quint32(1600);
        audio.append(QByteArray(1600, '\0'));
        return audio;
    }

    static AudioNetworkManager *setUpPlayer(SpeakButtons &buttons, QMediaPlayer &player)
    {
        buttons.setMediaPlayer(&player);
        buttons.playlist()->setParent(&player);
        delete buttons.m_networkManager;
        auto *network = new AudioNetworkManager(&buttons);
        buttons.m_networkManager = network;
        buttons.m_edgeTts->findChild<QNetworkAccessManager *>()->setProxy(
            QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), 1));
        return network;
    }

    void speak(const QString &text = QStringLiteral("hello"),
        QOnlineTranslator::Language language = QOnlineTranslator::English,
        QOnlineTranslator::Engine engine = QOnlineTranslator::Google)
    {
        m_buttons->speak(text, language, engine);
    }

    void verifyPlaybackFinished(int chunks = 1)
    {
        QSignalSpy statuses(m_player, &QMediaPlayer::mediaStatusChanged);
        QTRY_COMPARE(m_player->state(), QMediaPlayer::StoppedState);
        QCOMPARE(m_player->error(), QMediaPlayer::NoError);
        int completedChunks = 0;
        for (const auto &arguments : statuses) {
            if (arguments.first().value<QMediaPlayer::MediaStatus>() == QMediaPlayer::EndOfMedia)
                ++completedChunks;
        }
        QCOMPARE(completedChunks, chunks);
    }

    QMediaPlayer *m_player = nullptr;
    SpeakButtons *m_buttons = nullptr;
    AudioNetworkManager *m_network = nullptr;

private slots:
    void init()
    {
        qRegisterMetaType<QMediaPlayer::MediaStatus>();
        m_player = new QMediaPlayer;
        m_buttons = new SpeakButtons;
        m_network = setUpPlayer(*m_buttons, *m_player);
    }

    void cleanup()
    {
        m_buttons->stopSpeaking();
        delete m_buttons;
        delete m_player;
    }

    void repeatedGooglePlayback()
    {
        speak();
        speak();
        QCOMPARE(m_network->requests.size(), 1);
        m_network->pendingReply->finish(audioData());
        QVERIFY(m_buttons->m_audioCached);

        const QUrl audioUrl = m_buttons->playlist()->media(0).request().url();
        QVERIFY(audioUrl.toLocalFile().startsWith(QStringLiteral("/proc/self/fd/")));
        QFile audioFile(audioUrl.toLocalFile());
        QVERIFY(audioFile.open(QIODevice::ReadOnly));
        QCOMPARE(audioFile.readAll(), audioData());
        audioFile.close();

        verifyPlaybackFinished();
        speak();
        QCOMPARE(m_network->requests.size(), 1);
        QCOMPARE(m_buttons->playlist()->currentIndex(), 0);
        verifyPlaybackFinished();

        m_buttons->stopSpeaking();
        speak();
        QCOMPARE(m_network->requests.size(), 1);
        QCOMPARE(m_buttons->playlist()->media(0).request().url(), audioUrl);
    }

    void changedParametersInvalidateCache()
    {
        speak();
        m_network->pendingReply->finish(audioData());
        const QString oldPath = m_buttons->playlist()->media(0).request().url().toLocalFile();
        speak(QStringLiteral("goodbye"));
        QCOMPARE(m_network->requests.size(), 2);
        QVERIFY(!m_buttons->m_audioCached);
        QVERIFY(!QFile::exists(oldPath));
        m_network->pendingReply->finish(audioData());

        speak(QStringLiteral("goodbye"), QOnlineTranslator::German);
        QCOMPARE(m_network->requests.size(), 3);
        m_network->pendingReply->finish(audioData());

        m_buttons->setRegions(QOnlineTranslator::Google, {{QOnlineTranslator::German, QLocale::Germany}});
        speak(QStringLiteral("goodbye"), QOnlineTranslator::German);
        QCOMPARE(m_network->requests.size(), 4);
        m_network->pendingReply->finish(audioData());
        speak(QStringLiteral("goodbye"), QOnlineTranslator::German);
        QCOMPARE(m_network->requests.size(), 4);

        speak(QStringLiteral("goodbye"), QOnlineTranslator::German, QOnlineTranslator::Bing);
        QVERIFY(m_buttons->m_audioLoading);
        QVERIFY(!m_buttons->m_audioCached);
        QCOMPARE(m_buttons->playlist()->mediaCount(), 0);
    }

    void edgePlayback_data()
    {
        QTest::addColumn<int>("engine");
        QTest::newRow("Bing") << int(QOnlineTranslator::Bing);
        QTest::newRow("Mozhi") << int(QOnlineTranslator::Mozhi);
    }

    void edgePlayback()
    {
        QFETCH(int, engine);
        const auto speechEngine = static_cast<QOnlineTranslator::Engine>(engine);
        speak(QStringLiteral("hello"), QOnlineTranslator::English, speechEngine);
        m_buttons->m_edgeTts->abort();
        emit m_buttons->m_edgeTts->audioReady(audioData());
        QVERIFY(m_buttons->m_audioCached);
        const QUrl audioUrl = m_buttons->playlist()->media(0).request().url();
        verifyPlaybackFinished();

        speak(QStringLiteral("hello"), QOnlineTranslator::English, speechEngine);
        QVERIFY(!m_buttons->m_audioLoading);
        QCOMPARE(m_buttons->playlist()->media(0).request().url(), audioUrl);
        verifyPlaybackFinished();
    }

    void longTextReplaysAllChunks()
    {
        const QString text = QStringLiteral("hello ").repeated(80);
        speak(text);
        const int chunks = m_buttons->m_pendingMedia.size() + 1;
        QVERIFY(chunks > 1);
        for (int index = 0; index < chunks; ++index)
            m_network->pendingReply->finish(audioData());
        QCOMPARE(m_buttons->playlist()->mediaCount(), chunks);
        QCOMPARE(m_network->requests.size(), chunks);
        verifyPlaybackFinished(chunks);

        speak(text);
        QCOMPARE(m_buttons->playlist()->currentIndex(), 0);
        QCOMPARE(m_network->requests.size(), chunks);
        verifyPlaybackFinished(chunks);
    }

    void cancelledDownloadsCanRetry()
    {
        speak();
        QPointer<AudioReply> oldReply = m_network->pendingReply;
        speak(QStringLiteral("new text"));
        QCOMPARE(oldReply->error(), QNetworkReply::OperationCanceledError);
        QCOMPARE(m_network->requests.size(), 2);
        QCOMPARE(m_buttons->playlist()->mediaCount(), 0);

        m_buttons->stopSpeaking();
        QVERIFY(!m_buttons->m_audioLoading);
        QVERIFY(!m_buttons->m_audioCached);
        speak(QStringLiteral("new text"));
        QCOMPARE(m_network->requests.size(), 3);
        m_network->pendingReply->finish(audioData());
        QVERIFY(m_buttons->m_audioCached);
    }

    void failedDownloadsCanRetry_data()
    {
        QTest::addColumn<int>("error");
        QTest::newRow("empty audio") << int(QNetworkReply::NoError);
        QTest::newRow("network error") << int(QNetworkReply::TemporaryNetworkFailureError);
    }

    void failedDownloadsCanRetry()
    {
        QFETCH(int, error);
        const QString text = QStringLiteral("hello ").repeated(80);
        speak(text);
        m_network->pendingReply->finish(audioData());
        QTimer::singleShot(0, [] {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (auto *messageBox = qobject_cast<QMessageBox *>(widget))
                    messageBox->accept();
            }
        });
        m_network->pendingReply->finish({}, static_cast<QNetworkReply::NetworkError>(error));
        QVERIFY(!m_buttons->m_audioLoading);
        QVERIFY(!m_buttons->m_audioCached);
        QVERIFY(m_buttons->m_audioFiles.isEmpty());
        QCOMPARE(m_buttons->playlist()->mediaCount(), 0);

        speak(text);
        QCOMPARE(m_network->requests.size(), 3);
        QVERIFY(m_buttons->m_audioLoading);
    }

    void invalidAudioCanRetry()
    {
        speak();
        m_network->pendingReply->finish(QByteArrayLiteral("not audio"));
        QTRY_VERIFY(m_player->error() != QMediaPlayer::NoError);
        speak();
        QCOMPARE(m_network->requests.size(), 2);
        m_network->pendingReply->finish(audioData());
        verifyPlaybackFinished();
    }

    void sourceAndTranslationCachesAreIndependent()
    {
        QMediaPlayer translationPlayer;
        SpeakButtons translationButtons;
        AudioNetworkManager *translationNetwork = setUpPlayer(translationButtons, translationPlayer);

        speak();
        m_network->pendingReply->finish(audioData());
        translationButtons.speak(QStringLiteral("hallo"), QOnlineTranslator::German, QOnlineTranslator::Google);
        translationNetwork->pendingReply->finish(audioData());
        m_buttons->stopSpeaking();
        translationButtons.stopSpeaking();

        speak();
        QCOMPARE(m_network->requests.size(), 1);
        translationButtons.speak(QStringLiteral("hallo"), QOnlineTranslator::German, QOnlineTranslator::Google);
        QCOMPARE(translationNetwork->requests.size(), 1);
        QCOMPARE(m_buttons->m_cachedText, QStringLiteral("hello"));
        QCOMPARE(translationButtons.m_cachedText, QStringLiteral("hallo"));
        translationButtons.stopSpeaking();
    }

    void edgeSecMsGecV2Token()
    {
        QCOMPARE(
            EdgeTts::secMsGec(0),
            QStringLiteral("52BC7A17B50DFEC9-28C4394775C8751CD2F6859C030C06DB1489ACAB3B3A7651831FC26895177ED6"));
        QCOMPARE(
            EdgeTts::secMsGec(1700000000),
            QStringLiteral("2CD23A2CA0B2595A-F3C846C370E05F9CC83ACC5F995C5F5D87A4D0F14CE8936A0F902E64099E38A2"));
        QCOMPARE(
            EdgeTts::secMsGec(1793146278),
            QStringLiteral("FBEF5E395A19E4A4-97ACC9ABB9EBBFE9224280EDCC6D55116C5A304F33D3116778BC7DB1AC812FCD"));
        QCOMPARE(
            EdgeTts::secMsGec(2000000123),
            QStringLiteral("498DEBEC091E9FBB-48F566F30D363EA955F305452BB7D7029DA9E2E43FF809AEEF3ACB5964B2A49B"));
    }
};

QTEST_MAIN(SpeakButtonsTest)
#include "speakbuttons_test.moc"