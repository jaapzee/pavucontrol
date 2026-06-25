/***
  This file is part of pavucontrol.

  Copyright 2006-2008 Lennart Poettering
  Copyright 2009 Colin Guthrie

  pavucontrol is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  pavucontrol is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pavucontrol. If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef streamwidget_h
#define streamwidget_h

#include "pavucontrol.h"

#include "minimalstreamwidget.h"

class MainWindow;
class ChannelWidget;

/* Used as the ID for the unknown device item in deviceComboBox. */
#define UNKNOWN_DEVICE_NAME "#unknown#"

class StreamWidget : public MinimalStreamWidget {
  public:
    StreamWidget(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &x);
    void init(MainWindow *mainWindow);

    void setChannelMap(const pa_channel_map &m, bool can_decibel);
    void setVolume(const pa_cvolume &volume, bool force = false);
    virtual void updateChannelVolume(int channel, pa_volume_t v);

    void hideLockedChannels(bool hide = true);

    Gtk::ToggleButton *lockToggleButton, *muteToggleButton;
    Gtk::Label *directionLabel;
    Gtk::Label *inactiveLabel;
    Gtk::ComboBoxText *deviceComboBox;

    pa_channel_map channelMap;
    pa_cvolume volume;

    ChannelWidget *channelWidgets[PA_CHANNELS_MAX];

    virtual void onMuteToggleButton();
    virtual void onLockToggleButton();
    virtual void onContextTriggerEvent(gint n_press, gdouble x, gdouble y);

    sigc::connection timeoutConnection;

    bool timeoutEvent();

    virtual void executeVolumeUpdate();
    virtual void onKill(const Glib::VariantBase &parameter);
    virtual void onDeviceComboBoxChanged();

    /* Set once the underlying sink-input/source-output has gone away but is
     * still shown as recently-active history. The numeric index member on
     * the subclass is no longer valid for PA calls once this is true. */
    bool inactive;
    gint64 inactiveSince;

    /* The "module-stream-restore.id" of the underlying stream, captured
     * while it was still live. Lets a ghosted entry's controls update the
     * stream-restore database directly (by name) instead of the no-longer
     * valid numeric index, so the adjustment applies next time a stream
     * with this id reappears. */
    Glib::ustring restoreId;

    /* The matching key WirePlumber's own (PipeWire-side) stream-properties
     * restore store would use for this stream, e.g.
     * "Output/Audio:application.name:Firefox". On PipeWire systems this is
     * the store that actually governs a new stream's initial volume, and
     * it isn't kept in sync with the standard PulseAudio stream-restore
     * extension above, so it needs its own write. Empty if unknown. */
    Glib::ustring wpRestoreKey;

    void markInactive();
    void updateInactiveLabel();
    void writeRestoreEntry();
    void writeWirePlumberStateEntry();

  protected:
    MainWindow *mpMainWindow;

    Gtk::PopoverMenu contextMenu;
    void addKillMenu(const char *killLabel);
};

#endif
