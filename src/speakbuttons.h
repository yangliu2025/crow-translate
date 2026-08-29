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

#ifndef PLAYERBUTTONS_H
#define PLAYERBUTTONS_H

#include "qonlinetranslator.h"
#include "qonlinetts.h"

#include <QMediaPlayer>
#include <QWidget>

class AppSettings;
class EdgeTts;
class QTemporaryFile;

namespace Ui
{
class SpeakButtons;
}

class SpeakButtons : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(SpeakButtons)

public:
    explicit SpeakButtons(QWidget *parent = nullptr);
    ~SpeakButtons() override;

    QMediaPlayer *mediaPlayer() const;
    void setMediaPlayer(QMediaPlayer *mediaPlayer);
    QMediaPlaylist *playlist();

    void setSpeakShortcut(const QKeySequence &shortcut);
    QKeySequence speakShortcut() const;

    QMap<QOnlineTranslator::Language, QLocale::Country> regions(QOnlineTranslator::Engine engine) const;
    void setRegions(QOnlineTranslator::Engine engine, QMap<QOnlineTranslator::Language, QLocale::Country> regions);

    void speak(const QString &text, QOnlineTranslator::Language lang, QOnlineTranslator::Engine engine);
    void pauseSpeaking();
    void playPauseSpeaking();

public slots:
    void stopSpeaking();

signals:
    void playerMediaRequested();
    void stateChanged(QMediaPlayer::State state);
    void positionChanged(double progress);

private slots:
    void loadPlayerState(QMediaPlayer::State state);
    void onPlayPauseButtonPressed();
    void onPlayerPositionChanged(qint64 position);
    void playEdgeAudio(const QByteArray &audio);
    void showEdgeTtsError(const QString &message);

private:
    static QMediaPlayer *s_currentlyPlaying;

    Ui::SpeakButtons *ui;
    QMediaPlayer *m_mediaPlayer = nullptr;
    QMap<QOnlineTranslator::Language, QLocale::Country> m_googleRegions;
    EdgeTts *m_edgeTts;
    QTemporaryFile *m_edgeAudioFile = nullptr;
};

#endif // PLAYERBUTTONS_H
