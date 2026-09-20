/*
 *  Copyright © 2018-2023 Hennadii Chernyshchyk <genaloner@gmail.com>
 *
 *  This file is part of Crow Translate.
 *
 *  Crow Translate is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crow Translate is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crow Translate. If not, see <https://www.gnu.org/licenses/>.
 */

#include "speakbuttons.h"
#include "ui_speakbuttons.h"

#include "edgetts.h"
#include "settings/appsettings.h"

#include <QFile>
#include <QMediaPlaylist>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#include <sys/mman.h>
#include <unistd.h>

QMediaPlayer *SpeakButtons::s_currentlyPlaying = nullptr;

SpeakButtons::SpeakButtons(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SpeakButtons)
    , m_edgeTts(new EdgeTts(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);

    connect(ui->playPauseButton, &QAbstractButton::clicked, this, &SpeakButtons::onPlayPauseButtonPressed);
    connect(ui->stopButton, &QAbstractButton::clicked, this, &SpeakButtons::stopSpeaking);
    connect(m_edgeTts, &EdgeTts::audioReady, this, &SpeakButtons::playEdgeAudio);
    connect(m_edgeTts, &EdgeTts::error, this, &SpeakButtons::showEdgeTtsError);
}

SpeakButtons::~SpeakButtons()
{
    delete ui;
}

QMediaPlayer *SpeakButtons::mediaPlayer() const
{
    return m_mediaPlayer;
}

void SpeakButtons::setMediaPlayer(QMediaPlayer *mediaPlayer)
{
    if (m_mediaPlayer != nullptr) {
        disconnect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &SpeakButtons::loadPlayerState);
        disconnect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &SpeakButtons::stateChanged);
        disconnect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &SpeakButtons::onPlayerPositionChanged);
    }

    m_mediaPlayer = mediaPlayer;
    if (m_mediaPlayer->playlist() == nullptr)
        m_mediaPlayer->setPlaylist(new QMediaPlaylist);

    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &SpeakButtons::onPlayerPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &SpeakButtons::loadPlayerState);
    connect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &SpeakButtons::stateChanged);

    loadPlayerState(m_mediaPlayer->state());
}

QMediaPlaylist *SpeakButtons::playlist()
{
    return m_mediaPlayer->playlist();
}

void SpeakButtons::setSpeakShortcut(const QKeySequence &shortcut)
{
    ui->playPauseButton->setShortcut(shortcut);
}

QKeySequence SpeakButtons::speakShortcut() const
{
    return ui->playPauseButton->shortcut();
}

QMap<QOnlineTranslator::Language, QLocale::Country> SpeakButtons::regions(QOnlineTranslator::Engine engine) const
{
    switch (engine) {
    case QOnlineTranslator::Google:
        return m_googleRegions;
    default:
        return {};
    }
}

void SpeakButtons::setRegions(QOnlineTranslator::Engine engine, QMap<QOnlineTranslator::Language, QLocale::Country> regions)
{
    switch (engine) {
    case QOnlineTranslator::Google:
        m_googleRegions = std::move(regions);
        break;
    default:
        break;
    }
}

void SpeakButtons::speak(const QString &text, QOnlineTranslator::Language lang, QOnlineTranslator::Engine engine)
{
    if (text.isEmpty()) {
        QMessageBox::information(this, tr("No text specified"), tr("Playback text is empty"));
        return;
    }

    const QLocale::Country region = engine == QOnlineTranslator::Google ? m_googleRegions.value(lang) : QLocale::AnyCountry;
    if (text == m_cachedText && lang == m_cachedLanguage && engine == m_cachedEngine && region == m_cachedRegion) {
        if (m_audioLoading)
            return;
        if (m_audioCached && m_mediaPlayer->error() == QMediaPlayer::NoError) {
            playCachedAudio();
            return;
        }
    }

    stopSpeaking();
    playlist()->clear();
    qDeleteAll(m_audioFiles);
    m_audioFiles.clear();
    m_audioCached = false;
    m_cachedText = text;
    m_cachedLanguage = lang;
    m_cachedEngine = engine;
    m_cachedRegion = region;

    if (engine == QOnlineTranslator::Bing || engine == QOnlineTranslator::Mozhi) {
        m_audioLoading = true;
        ui->stopButton->setEnabled(true);
        m_edgeTts->synthesize(text, lang);
        return;
    }

    QOnlineTts onlineTts;
    onlineTts.setRegions(m_googleRegions);

    onlineTts.generateUrls(text, engine, lang);
    if (onlineTts.error() != QOnlineTts::NoError) {
        QMessageBox::critical(this, tr("Unable to generate URLs for TTS"), onlineTts.errorString());
        return;
    }

    m_pendingMedia = onlineTts.media();
    m_audioLoading = true;
    ui->stopButton->setEnabled(true);
    downloadNextAudio();
}

void SpeakButtons::pauseSpeaking()
{
    m_mediaPlayer->pause();
}

void SpeakButtons::playPauseSpeaking()
{
    if (m_mediaPlayer->state() == QMediaPlayer::PlayingState)
        m_mediaPlayer->pause();
    else
        m_mediaPlayer->play();
}

void SpeakButtons::stopSpeaking()
{
    m_audioLoading = false;
    m_edgeTts->abort();
    if (m_audioReply != nullptr) {
        QNetworkReply *reply = m_audioReply;
        m_audioReply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    m_pendingMedia.clear();
    m_mediaPlayer->stop();
    ui->stopButton->setEnabled(false);
}

void SpeakButtons::loadPlayerState(QMediaPlayer::State state)
{
    switch (state) {
    case QMediaPlayer::StoppedState:
        if (s_currentlyPlaying == m_mediaPlayer)
            s_currentlyPlaying = nullptr;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        ui->stopButton->setEnabled(false);
        break;
    case QMediaPlayer::PlayingState:
        if (s_currentlyPlaying != nullptr)
            s_currentlyPlaying->pause();
        s_currentlyPlaying = m_mediaPlayer;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
        ui->stopButton->setEnabled(true);
        break;
    case QMediaPlayer::PausedState:
        if (s_currentlyPlaying == m_mediaPlayer)
            s_currentlyPlaying = nullptr;

        ui->playPauseButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        break;
    }
}

void SpeakButtons::onPlayPauseButtonPressed()
{
    if (m_mediaPlayer->state() == QMediaPlayer::StoppedState)
        emit playerMediaRequested();
    else
        playPauseSpeaking();
}

void SpeakButtons::onPlayerPositionChanged(qint64 position)
{
    if (m_mediaPlayer->duration() != 0)
        emit positionChanged(static_cast<double>(position) / static_cast<double>(m_mediaPlayer->duration()));
    else
        emit positionChanged(0);
}

void SpeakButtons::playEdgeAudio(const QByteArray &audio)
{
    if (!m_audioLoading || !cacheAudio(audio))
        return;

    m_audioLoading = false;
    m_audioCached = true;
    playCachedAudio();
}

void SpeakButtons::showEdgeTtsError(const QString &message)
{
    m_audioLoading = false;
    m_audioCached = false;
    m_pendingMedia.clear();
    playlist()->clear();
    qDeleteAll(m_audioFiles);
    m_audioFiles.clear();
    ui->stopButton->setEnabled(false);
    QMessageBox::critical(this, tr("Unable to generate audio"), message);
}

bool SpeakButtons::cacheAudio(const QByteArray &audio)
{
    if (audio.isEmpty()) {
        showEdgeTtsError(tr("No audio received"));
        return false;
    }

    const int descriptor = memfd_create("crow-tts.mp3", MFD_CLOEXEC);
    if (descriptor == -1) {
        showEdgeTtsError(tr("Unable to create an audio buffer"));
        return false;
    }

    auto *audioFile = new QFile(this);
    if (!audioFile->open(descriptor, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle)) {
        ::close(descriptor);
        delete audioFile;
        showEdgeTtsError(tr("Unable to open the audio buffer"));
        return false;
    }
    if (audioFile->write(audio) != audio.size() || !audioFile->flush()) {
        delete audioFile;
        showEdgeTtsError(tr("Unable to write to the audio buffer"));
        return false;
    }

    m_audioFiles.append(audioFile);
    playlist()->addMedia(QUrl::fromLocalFile(QStringLiteral("/proc/self/fd/%1").arg(descriptor)));
    return true;
}

void SpeakButtons::downloadNextAudio()
{
    if (m_pendingMedia.isEmpty()) {
        m_audioLoading = false;
        m_audioCached = !m_audioFiles.isEmpty();
        if (m_audioCached)
            playCachedAudio();
        return;
    }

    QNetworkRequest request = m_pendingMedia.takeFirst().request();
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_networkManager->get(request);
    m_audioReply = reply;
    QTimer::singleShot(30000, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_audioReply != reply)
            return;
        m_audioReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            showEdgeTtsError(reply->errorString());
            return;
        }
        if (cacheAudio(reply->readAll()))
            downloadNextAudio();
    });
}

void SpeakButtons::playCachedAudio()
{
    playlist()->setCurrentIndex(0);
    m_mediaPlayer->setPosition(0);
    m_mediaPlayer->play();
}
