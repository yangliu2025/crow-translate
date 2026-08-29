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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "qonlinetts.h"

#include <QDialog>

class MainWindow;
class AbstractAutostartManager;
class QOnlineTranslator;
class QMediaPlayer;
class QMediaPlaylist;
class ShortcutItem;
#ifdef WITH_PORTABLE_MODE
class QCheckBox;
#endif

namespace Ui
{
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(SettingsDialog)

public:
    explicit SettingsDialog(MainWindow *parent = nullptr);
    ~SettingsDialog() override;

public slots:
    void accept() override;

private slots:
    void setCurrentPage(int index);

    void onProxyTypeChanged(int type);
    void onWindowModeChanged(int mode);

    void onTrayIconTypeChanged(int type);
    void selectCustomTrayIcon();
    void setCustomTrayIconPreview(const QString &iconPath);

    void selectOcrLanguagesPath();
    void onOcrLanguagesPathChanged(const QString &path);
    void onTesseractParametersCurrentItemChanged();

    void onGoogleLanguageSelectionChanged(int languageIndex);
    void saveGoogleEngineRegion(int region);
    void detectGoogleTextLanguage();
    void speakGoogleTestText();
    void testMozhiInstance();

    void loadShortcut(ShortcutItem *item);
    void updateAcceptButton();
    void acceptCurrentShortcut();
    void clearCurrentShortcut();
    void resetCurrentShortcut();
    void resetAllShortcuts();
    void restoreDefaults();

private:
    void addLocale(const QLocale &locale);
    void activateCompactMode();
    void loadSettings();

    Ui::SettingsDialog *ui;

    // Manage platform-dependant autostart
    AbstractAutostartManager *m_autostartManager;

    // Test voice
    QOnlineTranslator *m_googleTranslator;

#ifdef WITH_PORTABLE_MODE
    QCheckBox *m_portableCheckbox;
#endif
};

#endif // SETTINGSDIALOG_H
