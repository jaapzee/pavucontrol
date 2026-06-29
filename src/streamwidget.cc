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

#include "streamwidget.h"
#include "mainwindow.h"
#include "channelwidget.h"

#include <pulse/ext-stream-restore.h>
#include <pulse/volume.h>

#if HAVE_PULSE_MESSAGING_API
#include <cstring>
#include <json-glib/json-glib.h>
#endif

#include "i18n.h"

/*** StreamWidget ***/
StreamWidget::StreamWidget(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& x) :
    MinimalStreamWidget(cobject),
    inactive(false),
    inactiveSince(0),
    mpMainWindow(NULL) {

    /* MinimalStreamWidget member variables. */
    channelsVBox = x->get_widget<Gtk::Box>("streamChannelsVBox");
    nameLabel = x->get_widget<Gtk::Label>("streamNameLabel");
    boldNameLabel = x->get_widget<Gtk::Label>("streamBoldNameLabel");
    iconImage = x->get_widget<Gtk::Image>("streamIconImage");

    lockToggleButton = x->get_widget<Gtk::ToggleButton>("streamLockToggleButton");
    muteToggleButton = x->get_widget<Gtk::ToggleButton>("streamMuteToggleButton");
    directionLabel = x->get_widget<Gtk::Label>("directionLabel");
    inactiveLabel = x->get_widget<Gtk::Label>("streamInactiveLabel");
    deviceComboBox = x->get_widget<Gtk::ComboBoxText>("deviceComboBox");

    muteToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &StreamWidget::onMuteToggleButton));
    lockToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &StreamWidget::onLockToggleButton));
    deviceComboBox->signal_changed().connect(sigc::mem_fun(*this, &StreamWidget::onDeviceComboBoxChanged));

    for (unsigned i = 0; i < PA_CHANNELS_MAX; i++)
        channelWidgets[i] = NULL;
}

void StreamWidget::init(MainWindow* mainWindow) {
    mpMainWindow = mainWindow;

    MinimalStreamWidget::init();
}

void StreamWidget::addKillMenu(const char* killLabel) {
    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(3);
    gesture->set_exclusive(true);
    gesture->signal_pressed().connect(sigc::mem_fun(*this, &StreamWidget::onContextTriggerEvent));
    this->add_controller(gesture);

    const std::string actionName = "kill", groupName="streamwidget";
    auto action = Gio::SimpleAction::create(actionName);
    action->set_enabled(true);
    action->signal_activate().connect(sigc::mem_fun(*this, &StreamWidget::onKill));

    auto group = Gio::SimpleActionGroup::create();
    group->add_action(action);

    insert_action_group(groupName, group);

    auto menuModel = Gio::Menu::create();
    menuModel->append(killLabel, groupName + "." + actionName);
    contextMenu.set_menu_model(menuModel);
    contextMenu.set_parent(*this);
}


void StreamWidget::onContextTriggerEvent(gint n_press, gdouble x, gdouble y) {
    if (n_press == 1) {
        contextMenu.set_pointing_to(Gdk::Rectangle {(int) x, (int) y, 0 , 0});
        contextMenu.popup();
    }
}

void StreamWidget::setChannelMap(const pa_channel_map &m, bool can_decibel) {
    channelMap = m;

    ChannelWidget::create(this, m, can_decibel, channelWidgets);

    for (int i = 0; i < m.channels; i++) {
        ChannelWidget *cw = channelWidgets[i];
        channelsVBox->prepend(*cw);
        cw->unreference();
    }

    channelWidgets[m.channels-1]->setBaseVolume(PA_VOLUME_NORM);

    lockToggleButton->set_sensitive(m.channels > 1);
    hideLockedChannels(lockToggleButton->get_active());
}

void StreamWidget::setVolume(const pa_cvolume &v, bool force) {
    g_assert(v.channels == channelMap.channels);

    volume = v;

    if (timeoutConnection.empty() || force) { /* do not update the volume when a volume change is still in flux */
        for (int i = 0; i < volume.channels; i++)
            channelWidgets[i]->setVolume(volume.values[i]);
    }
}

void StreamWidget::updateChannelVolume(int channel, pa_volume_t v) {
    pa_cvolume n;
    g_assert(channel < volume.channels);

    n = volume;
    if (lockToggleButton->get_active()) {
        for (int i = 0; i < n.channels; i++)
            n.values[i] = v;
    } else
        n.values[channel] = v;

    setVolume(n, true);

    if (timeoutConnection.empty())
        timeoutConnection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &StreamWidget::timeoutEvent), 100);
}

void StreamWidget::hideLockedChannels(bool hide) {
    for (int i = 0; i < channelMap.channels - 1; i++)
        channelWidgets[i]->set_visible(!hide);

    channelWidgets[channelMap.channels - 1]->channelLabel->set_visible(!hide);
}

void StreamWidget::onMuteToggleButton() {

    lockToggleButton->set_sensitive(!muteToggleButton->get_active());

    for (int i = 0; i < channelMap.channels; i++)
        channelWidgets[i]->set_sensitive(!muteToggleButton->get_active());
}

void StreamWidget::onLockToggleButton() {
    hideLockedChannels(lockToggleButton->get_active());
}

bool StreamWidget::timeoutEvent() {
    executeVolumeUpdate();
    return false;
}

void StreamWidget::executeVolumeUpdate() {
}

void StreamWidget::onKill(const Glib::VariantBase& parameter) {
}

void StreamWidget::onDeviceComboBoxChanged() {
}

void StreamWidget::markInactive() {
    inactive = true;
    inactiveSince = g_get_monotonic_time();
    updateInactiveLabel();
    inactiveLabel->show();
}

void StreamWidget::writeRestoreEntry() {
    if (!restoreId.empty()) {
        pa_ext_stream_restore_info info;
        pa_operation *o;

        Glib::ustring deviceId = deviceComboBox->get_active_id();

        info.name = restoreId.c_str();
        info.channel_map = channelMap;
        info.volume = volume;
        info.device = (deviceId.empty() || deviceId == UNKNOWN_DEVICE_NAME) ? NULL : deviceId.c_str();
        info.mute = muteToggleButton->get_active();

        if (!(o = pa_ext_stream_restore_write(get_context(), PA_UPDATE_REPLACE, &info, 1, TRUE, NULL, NULL)))
            show_error(this, _("pa_ext_stream_restore_write() failed"));
        else
            pa_operation_unref(o);
    }

    /* Best-effort: on PipeWire systems, the call above updates the
     * standard PulseAudio stream-restore database, but that database is
     * not what actually governs a new stream's initial volume there (see
     * writeWirePlumberStateEntry()). Update WirePlumber's own store too. */
    writeWirePlumberStateEntry();
}

void StreamWidget::writeWirePlumberStateEntry() {
#if HAVE_PULSE_MESSAGING_API
    if (wpRestoreKey.empty())
        return;

    gchar *dir = g_build_filename(g_get_user_state_dir(), "wireplumber", NULL);
    gchar *path = g_build_filename(dir, "stream-properties", NULL);

    GKeyFile *kf = g_key_file_new();
    GKeyFileFlags flags = (GKeyFileFlags) (G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS);
    /* Not present yet on a system that never ran WirePlumber's
     * stream-restore is fine; we start a fresh keyfile in that case. */
    g_key_file_load_from_file(kf, path, flags, NULL);

    gchar *existing = g_key_file_get_string(kf, "stream-properties", wpRestoreKey.c_str(), NULL);

    JsonObject *obj = NULL;
    if (existing) {
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, existing, -1, NULL)) {
            JsonNode *root = json_parser_get_root(parser);
            if (root && JSON_NODE_HOLDS_OBJECT(root))
                obj = json_object_ref(json_node_get_object(root));
        }
        g_object_unref(parser);
        g_free(existing);
    }
    if (!obj)
        obj = json_object_new();

    /* Preserve any other fields already in the entry (e.g. WirePlumber's
     * own "target" / "channelMap") and only touch what we actually know. */
    if (!json_object_has_member(obj, "volume"))
        json_object_set_double_member(obj, "volume", 1.0);
    json_object_set_boolean_member(obj, "mute", muteToggleButton->get_active());

    JsonArray *channelVolumes = json_array_new();
    for (int i = 0; i < volume.channels; i++)
        json_array_add_double_element(channelVolumes, pa_sw_volume_to_linear(volume.values[i]));
    json_object_set_array_member(obj, "channelVolumes", channelVolumes);

    JsonNode *outNode = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(outNode, obj);

    JsonGenerator *gen = json_generator_new();
    json_generator_set_root(gen, outNode);
    gchar *serialized = json_generator_to_data(gen, NULL);

    g_key_file_set_string(kf, "stream-properties", wpRestoreKey.c_str(), serialized);

    g_mkdir_with_parents(dir, 0700);

    GError *err = NULL;
    if (!g_key_file_save_to_file(kf, path, &err)) {
        g_debug("Failed to update WirePlumber stream-properties state: %s", err->message);
        g_error_free(err);
    }

    g_free(serialized);
    g_object_unref(gen);
    json_node_free(outNode);
    json_object_unref(obj);
    g_key_file_free(kf);
    g_free(path);
    g_free(dir);
#endif
}

void StreamWidget::updateInactiveLabel() {
    gint64 elapsedMinutes = (g_get_monotonic_time() - inactiveSince) / G_USEC_PER_SEC / 60;
    gchar *text, *markup;

    if (elapsedMinutes < 1)
        text = g_strdup(_("stopped less than a minute ago"));
    else
        text = g_strdup_printf(_("stopped %d min ago"), (int) elapsedMinutes);

    markup = g_markup_printf_escaped("<i>(%s)</i>", text);
    inactiveLabel->set_markup(markup);
    g_free(text);
    g_free(markup);
}
