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

#include <QDir>
#include <QMediaPlaylist>
#include <QMessageBox>
#include <QTemporaryFile>

QMediaPlayer *SpeakButtons::s_currentlyPlaying = nullptr;

SpeakButtons::SpeakButtons(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SpeakButtons)
    , m_edgeTts(new EdgeTts(this))
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

    if (engine == QOnlineTranslator::Bing || engine == QOnlineTranslator::Mozhi) {
        playlist()->clear();
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

    // Use playlist to split long queries due engines limit
    const QList<QMediaContent> media = onlineTts.media();
    playlist()->clear();
    playlist()->addMedia(media);
    m_mediaPlayer->play();
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
    m_edgeTts->abort();
    m_mediaPlayer->stop();
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
    playlist()->clear();
    delete m_edgeAudioFile;
    m_edgeAudioFile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/crow-edge-tts-XXXXXX.mp3"), this);
    if (!m_edgeAudioFile->open() || m_edgeAudioFile->write(audio) != audio.size()) {
        showEdgeTtsError(tr("Unable to create a temporary audio file"));
        return;
    }
    m_edgeAudioFile->close();

    playlist()->addMedia(QUrl::fromLocalFile(m_edgeAudioFile->fileName()));
    m_mediaPlayer->play();
}

void SpeakButtons::showEdgeTtsError(const QString &message)
{
    QMessageBox::critical(this, tr("Unable to generate audio"), message);
}
