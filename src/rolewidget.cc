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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rolewidget.h"

#include <pulse/ext-stream-restore.h>
#include <pulse/volume.h>
#include <glib.h>

#include "i18n.h"

#if HAVE_PULSE_MESSAGING_API
static void writeWirePlumberMetadata(pa_volume_t pa_vol, bool muted) {
    double cubic = pa_sw_volume_to_linear(pa_vol);
    gchar vol_str[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(vol_str, sizeof(vol_str), "%.6f", cubic);
    gchar *json = g_strdup_printf(
        "{\"mute\":%s,\"volumes\":[%s],\"channels\":[\"MONO\"]}",
        muted ? "true" : "false",
        vol_str
    );
    gchar *argv[] = {
        (gchar *)"pw-metadata",
        (gchar *)"-n", (gchar *)"route-settings",
        (gchar *)"0",
        (gchar *)"restore.stream.Output/Audio.media.role:Notification",
        json,
        (gchar *)"Spa:String:JSON",
        NULL
    };
    g_spawn_async(NULL, argv, NULL,
                  (GSpawnFlags)(G_SPAWN_SEARCH_PATH |
                                G_SPAWN_STDOUT_TO_DEV_NULL |
                                G_SPAWN_STDERR_TO_DEV_NULL),
                  NULL, NULL, NULL, NULL);
    g_free(json);
}
#endif

RoleWidget::RoleWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    StreamWidget(cobject, x) {

    lockToggleButton->hide();
    directionLabel->hide();
    deviceComboBox->hide();
}

RoleWidget* RoleWidget::create() {
    RoleWidget* w;
    Glib::RefPtr<Gtk::Builder> x = Gtk::Builder::create_from_resource("/org/pulseaudio/pavucontrol/ui/streamwidget.ui", "streamWidget");
    w = Gtk::Builder::get_widget_derived<RoleWidget>(x, "streamWidget");
    w->reference();
    return w;
}

void RoleWidget::onMuteToggleButton() {
    StreamWidget::onMuteToggleButton();

    executeVolumeUpdate();
}

void RoleWidget::executeVolumeUpdate() {
    pa_ext_stream_restore_info info;

    if (updating)
        return;

    info.name = role.c_str();
    info.channel_map.channels = 1;
    info.channel_map.map[0] = PA_CHANNEL_POSITION_MONO;
    info.volume = volume;
    info.device = device == "" ? NULL : device.c_str();
    info.mute = muteToggleButton->get_active();

    pa_operation* o;
    if (!(o = pa_ext_stream_restore_write(get_context(), PA_UPDATE_REPLACE, &info, 1, TRUE, NULL, NULL))) {
        show_error(this, _("pa_ext_stream_restore_write() failed"));
        return;
    }
    pa_operation_unref(o);

    writeWirePlumberStateEntry();

#if HAVE_PULSE_MESSAGING_API
    writeWirePlumberMetadata(volume.values[0], info.mute);
#endif

    uint32_t idx = mpMainWindow->eventRoleSinkInputIndex;
    if (idx != PA_INVALID_INDEX) {
        pa_cvolume v;
        pa_cvolume_set(&v, mpMainWindow->eventRoleSinkInputChannels, volume.values[0]);
        if ((o = pa_context_set_sink_input_volume(get_context(), idx, &v, NULL, NULL)))
            pa_operation_unref(o);
        if ((o = pa_context_set_sink_input_mute(get_context(), idx, info.mute, NULL, NULL)))
            pa_operation_unref(o);
    }
}

