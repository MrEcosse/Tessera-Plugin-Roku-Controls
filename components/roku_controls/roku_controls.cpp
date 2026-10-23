#include "roku_controls.h"

namespace esphome::roku_controls {

// The tile ids are the manifest's (tiles[].id).
void RokuControls::setup() {
  add_tile("nav", [this]() { return make_nav(this); });
  add_tile("playback", [this]() { return make_playback(this); });
  add_tile("tv", [this]() { return make_tv(this); });
  add_tile("numbers", [this]() { return make_numbers(this); });
  add_tile("channels", [this]() { return make_channels(this); });
}

}  // namespace esphome::roku_controls
