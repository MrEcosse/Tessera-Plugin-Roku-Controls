#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::roku_controls {

// The plugin: an ESPHome component and a tessera::Plugin. Its five tile types each come from one of these.
tessera::Tile *make_nav(const tessera::Plugin *plugin);       // keys.cpp: back, home, the arrows, OK, replay, *
tessera::Tile *make_playback(const tessera::Plugin *plugin);  // keys.cpp: rewind, play/pause, fast forward
tessera::Tile *make_tv(const tessera::Plugin *plugin);        // keys.cpp: a Roku TV's power, volume, channel, TV input
tessera::Tile *make_numbers(const tessera::Plugin *plugin);   // numbers.cpp: a channel number, tuned on Enter
tessera::Tile *make_channels(const tessera::Plugin *plugin);  // channels.cpp: the apps, with their logos

class RokuControls : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace esphome::roku_controls
