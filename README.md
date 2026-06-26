# pavucontrol — personal fork

This is a fork of [PulseAudio Volume Control (pavucontrol)](https://gitlab.freedesktop.org/pulseaudio/pavucontrol), based on version 6.2, with a number of additional features and fixes on top of upstream.

## Changes vs upstream

### Collapse duplicate stream entries

Apps like Chromium / YouTube can create multiple simultaneous PulseAudio sink-inputs with the same identity (e.g. a stereo video stream and a mono auxiliary stream). Previously this caused two separate entries to appear in the Playback tab. Now only one entry is shown per application, regardless of how many concurrent streams it has internally.

### Recently-active stream history

A new **Recently active** spin button on the Configuration tab (set in minutes, 0 to disable) keeps stream entries visible after their underlying audio stream stops. This is useful for:

- Adjusting the volume of an app after it finishes playing, so the setting takes effect next time it starts.
- Quickly muting apps that just stopped.

When a stream reappears (e.g. the same app starts playing again) its entry is revived in place, preserving its position in the list.

Volume changes made to a recently-active (greyed-out) entry are written to both the PulseAudio stream-restore database and the WirePlumber PipeWire-side state file, so they take effect on either backend.

### Mono audio setting

A **Mono** toggle on the Configuration tab forces all audio output to mono by remixing both channels. Useful for hearing-impaired users or when using a single speaker.

### Bluetooth mic autoswitch setting

A **Bluetooth mic autoswitch** toggle on the Configuration tab controls whether PulseAudio automatically switches a Bluetooth device to the HFP/HSP headset profile when a microphone is activated. Disabling this keeps the higher-quality A2DP profile active even when recording.

### Use port device name for profile identification

When a sink or source has multiple ports, the port's device name is now used to identify the associated card profile, rather than the port's description. This avoids mismatches when profile descriptions are translated or differ between systems.

### Deprecation fix: active_profile2

Uses the `active_profile2` field (introduced in PulseAudio 13) to read the active card profile, replacing the deprecated `active_profile` field.

## Building

### With meson (development build)

```sh
meson setup build
ninja -C build
./build/src/pavucontrol
```

### Arch Linux package (installs system-wide)

```sh
cd archlinux
makepkg -si
```

This builds directly from the working tree and installs via pacman.

## Requirements

Same as upstream: `gtk4`, `gtkmm-4.0`, `libpulse`, `glibmm-2.68`, `libsigc++-3.0`, `json-glib`, `libcanberra` (optional).

Optionally depends on `wireplumber` for writing volume adjustments to the WirePlumber state store.

## Upstream

- Project page: https://freedesktop.org/software/pulseaudio/pavucontrol/
- Repository: https://gitlab.freedesktop.org/pulseaudio/pavucontrol
