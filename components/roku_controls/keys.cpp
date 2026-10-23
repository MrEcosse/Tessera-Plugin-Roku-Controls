#include "roku_controls.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome::roku_controls {
namespace keys {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// A key of the Roku's remote: the command Home Assistant's Roku integration sends for it (remote.send_command), and what
// the key shows: an icon of Tessera's set, the same icon twice side by side (rewind, fast forward: Tessera's set has no
// icon for them), or a word or sign where the set has no icon at all (replay, the * key).
struct KeyDef {
  const char *command;   // nullptr: an empty place in the block
  uint32_t glyph;        // 0: words
  bool twice;            // the glyph twice, side by side
  const char *words;     // a text of the screen's translations, or a sign shown as it is
  bool sign;             // `words` is the sign itself
};

static constexpr uint32_t BACK = 0xF17B3, HOME = 0xF02DC, UP = 0xF0143, DOWN = 0xF0140, LEFT = 0xF0141, RIGHT = 0xF0142,
                          OK = 0xF012C, PLAY_PAUSE = 0xF040E, POWER = 0xF0425, VOL_DOWN = 0xF075E, MUTE = 0xF075F,
                          VOL_UP = 0xF075D, PLUS = 0xF0415, MINUS = 0xF0374, TV = 0xF0502;

// The navigation keys stand as on the remote: back and home over the arrows round OK, replay and * under them.
static constexpr KeyDef NAV[] = {
    {"back", BACK, false, nullptr, false},  {"up", UP, false, nullptr, false},       {"home", HOME, false, nullptr, false},
    {"left", LEFT, false, nullptr, false},  {"select", OK, false, nullptr, false},   {"right", RIGHT, false, nullptr, false},
    {"replay", 0, false, "key_replay", false}, {"down", DOWN, false, nullptr, false}, {"info", 0, false, "*", true},
};
static constexpr KeyDef PLAYBACK[] = {
    {"reverse", LEFT, true, nullptr, false},
    {"play", PLAY_PAUSE, false, nullptr, false},
    {"forward", RIGHT, true, nullptr, false},
};
// A Roku TV's own keys: power, its volume, the channel of its tuner, and the TV input.
static constexpr KeyDef TV_KEYS[] = {
    {"power", POWER, false, nullptr, false},         {"volume_down", VOL_DOWN, false, nullptr, false},
    {"volume_mute", MUTE, false, nullptr, false},    {"volume_up", VOL_UP, false, nullptr, false},
    {"channel_down", MINUS, false, nullptr, false},  {"channel_up", PLUS, false, nullptr, false},
    {"input_tuner", TV, false, nullptr, false},
};

enum class Shape { BLOCK3, ROW };
static constexpr int MAX_KEYS = 9;
// How long a key stays lit after a tap: at least this long, and until the next tick after it.
static constexpr uint32_t FLASH_MS = 300;

// The tile: its keys in the middle of the card. BLOCK3 is a block of three by three with each key in its own place;
// ROW is the keys in order, in one row, or in two or three on a tile too narrow for them side by side, or in a column
// down a tile taller than it is wide.
class KeysTile : public tessera::Tile {
 public:
  KeysTile(const tessera::Plugin *plugin, const KeyDef *defs, int count, Shape shape)
      : plugin_(plugin), defs_(defs), count_(std::min(count, MAX_KEYS)), shape_(shape) {}

  void create(const tessera::TileContext &c) override {
    std::snprintf(entity_, sizeof entity_, "%s", c.entity ? c.entity : "");
    if (!entity_[0]) {
      hint_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_width(hint_, c.width);
      lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
      ui::set_text(hint_, plugin_->text("no_remote"));
      lv_obj_set_pos(hint_, 0, (c.height - ui::line_height(Font::BODY)) / 2);
      return;
    }

    // The plan: columns and rows. Keys as large as the room allows, at most 14 mm high and half again as wide as high;
    // the gap narrows before the keys do.
    if (shape_ == Shape::BLOCK3) {
      cols_ = rows_ = 3;
    } else if (c.height > c.width) {
      cols_ = 1;
      rows_ = count_;
    } else {
      cols_ = count_;
      rows_ = 1;
      // Rows of their own when the keys would come out narrower than a finger side by side.
      while (cols_ > 1 && (c.width - (cols_ - 1) * ui::px(3)) / cols_ < ui::mm(7) && rows_ < 3) {
        ++rows_;
        cols_ = (count_ + rows_ - 1) / rows_;
      }
    }
    auto fit = [&]() {
      kw_ = (c.width - (cols_ - 1) * gap_) / cols_;
      kh_ = (c.height - (rows_ - 1) * gap_) / rows_;
    };
    gap_ = ui::px(8);
    fit();
    if (std::min(kw_, kh_) < ui::mm(9)) {
      gap_ = ui::px(3);
      fit();
    }
    kh_ = std::max(1, std::min(kh_, ui::mm(14)));
    kw_ = std::max(1, std::min(kw_, kh_ * 3 / 2));
    left_ = (c.width - (cols_ * kw_ + (cols_ - 1) * gap_)) / 2;
    top_ = (c.height - (rows_ * kh_ + (rows_ - 1) * gap_)) / 2;
    const int radius = std::min(ui::px(ui::large() ? 12 : 8), std::min(kw_, kh_) / 2);

    const int room = std::min(kw_, kh_) * 3 / 4;
    const Font face = ui::text_width(ui::icon(OK), Font::ICON) <= room ? Font::ICON : Font::ICON_SMALL;
    const int line = ui::line_height(face);

    for (int i = 0; i < count_; ++i) {
      const KeyDef &def = defs_[i];
      Key &key = keys_[i];
      if (!def.command) continue;
      key.block = ui::block(c.parent, theme::BUTTON);
      lv_obj_set_size(key.block, kw_, kh_);
      lv_obj_set_pos(key.block, left_ + (i % cols_) * (kw_ + gap_), top_ + (i / cols_) * (kh_ + gap_));
      lv_obj_set_style_radius(key.block, radius, 0);
      if (def.glyph) {
        // One icon in the middle, or the same one twice in one label, drawn closer together than their letter boxes:
        // rewind and fast forward.
        // Two chevrons take about 1.4 of one's width: the smaller face where they would not fit the key otherwise.
        Font own = face;
        if (def.twice && ui::text_width(ui::icon(def.glyph), face) * 7 / 5 > room) own = Font::ICON_SMALL;
        lv_obj_t *label = ui::label(key.block, own, theme::INK);
        lv_obj_set_width(label, kw_);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 0, (kh_ - ui::line_height(own)) / 2);
        if (def.twice) {
          // A chevron's ink is about a third of its letter box: pulled this close, the two touch, as on a remote.
          const int glyph_w = ui::text_width(ui::icon(def.glyph), own);
          lv_obj_set_style_text_letter_space(label, -glyph_w * 5 / 8, 0);
          ui::set_text(label, ui::icon(def.glyph) + ui::icon(def.glyph));
        } else {
          ui::set_text(label, ui::icon(def.glyph));
        }
        key.labels[key.count++] = label;
      } else {
        // Words, or the sign: the larger face where it fits the key with a margin, else the smaller one.
        std::string text = def.sign ? std::string(def.words) : std::string(plugin_->text(def.words));
        const int pad = ui::px(4);
        // The * sign in the large face of headings (Roboto's asterisk is small); words in the face of a key's words.
        const Font big = def.sign ? Font::HEADLINE : Font::BODY_LARGE;
        Font words = ui::text_width(text, big) + ui::px(6) <= kw_ - 2 * pad ? big : Font::BODY;
        // A word too long for a key of a single cell: the icon nearest to it instead (replay: skip back).
        if (!def.sign && ui::text_width(text, Font::BODY) + ui::px(2) > kw_) {
          text = ui::icon(0xF04AE);  // skip-previous
          words = face;
        }
        lv_obj_t *label = ui::label(key.block, words, theme::INK);
        lv_obj_set_width(label, kw_);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 0, (kh_ - ui::line_height(words)) / 2);
        ui::set_text(label, text);
        key.labels[key.count++] = label;
      }
    }
    paint();
  }

  // The remote's state: keys go grey while it is unavailable.
  void on_state(JsonObjectConst data) override {
    const bool away = std::strcmp(data["state"] | "", "unavailable") == 0;
    if (away == away_) return;
    away_ = away;
    paint();
  }

  void on_tick(uint32_t) override {
    if (flash_ >= 0 && esphome::millis() - flash_at_ >= FLASH_MS) {
      flash_ = -1;
      paint();
    }
  }

  void on_theme() override {
    if (hint_) ui::set_color(hint_, theme::MUTED);
    paint();
  }

  void on_tap() override {
    const int i = key_at(tessera::tap_point());
    if (i < 0 || !entity_[0]) return;
    flash_ = i;
    flash_ok_ = tessera::action("remote.send_command", entity_, "command", defs_[i].command);
    flash_at_ = esphome::millis();
    paint();
  }

 private:
  struct Key {
    lv_obj_t *block = nullptr;
    lv_obj_t *labels[2] = {nullptr, nullptr};
    int count = 0;
  };

  // The key under the finger, with half the gap around it; -1 for an empty place or off the block.
  int key_at(tessera::Point p) const {
    if (p.x < 0 || p.y < 0) return -1;
    const int x = p.x - left_ + gap_ / 2, y = p.y - top_ + gap_ / 2;
    if (x < 0 || y < 0) return -1;
    const int col = x / (kw_ + gap_), row = y / (kh_ + gap_);
    if (col >= cols_ || row >= rows_) return -1;
    const int i = row * cols_ + col;
    return i < count_ && defs_[i].command ? i : -1;
  }

  void paint() {
    for (int i = 0; i < count_; ++i) {
      Key &key = keys_[i];
      if (!key.block) continue;
      theme::Role ink = away_ ? theme::SUBTLE : theme::INK;
      if (i == flash_) {
        // A tap: the pressed blue while it goes out, red when it could not be sent.
        if (flash_ok_) ui::set_fill(key.block, Tone::NORMAL, theme::ACCENT_PRESSED);
        else ui::set_fill(key.block, Tone::ALERT, theme::BUTTON);
        ink = theme::ON_ACCENT;
      } else {
        ui::set_fill(key.block, Tone::NORMAL, theme::BUTTON);
      }
      for (int l = 0; l < key.count; ++l) ui::set_color(key.labels[l], ink);
    }
  }

  const tessera::Plugin *plugin_;
  const KeyDef *defs_;
  int count_;
  Shape shape_;
  Key keys_[MAX_KEYS];
  int cols_ = 1, rows_ = 1, kw_ = 1, kh_ = 1, gap_ = 0, left_ = 0, top_ = 0;
  char entity_[128] = "";
  bool away_ = false;
  int flash_ = -1;
  bool flash_ok_ = false;
  uint32_t flash_at_ = 0;
  lv_obj_t *hint_ = nullptr;
};

}  // namespace keys

tessera::Tile *make_nav(const tessera::Plugin *plugin) {
  return new keys::KeysTile(plugin, keys::NAV, 9, keys::Shape::BLOCK3);
}
tessera::Tile *make_playback(const tessera::Plugin *plugin) {
  return new keys::KeysTile(plugin, keys::PLAYBACK, 3, keys::Shape::ROW);
}
tessera::Tile *make_tv(const tessera::Plugin *plugin) {
  return new keys::KeysTile(plugin, keys::TV_KEYS, 7, keys::Shape::ROW);
}

}  // namespace esphome::roku_controls
