#include "roku_controls.h"
#include <cstdio>
#include "esphome/core/hal.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace esphome::roku_controls {
namespace numbers {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// The keys: 0 to 9 are the digits, then Clear and Enter, which a person may leave out.
enum { CLEAR = 10, ENTER = 11, KEY_COUNT = 12 };
static constexpr uint32_t CLEAR_GLYPH = 0xF0156;  // close
static constexpr uint32_t ENTER_GLYPH = 0xF012C;  // check

// How the keys follow each other: a telephone's (1 2 3, 4 5 6, 7 8 9, then Clear 0 Enter) in three columns, and the
// digits in order with Clear and Enter at the end when the tile is wide and the keys go in rows of four or six.
static constexpr int PHONE[KEY_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9, CLEAR, 0, ENTER};
static constexpr int INORDER[KEY_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, CLEAR, ENTER};

// How long a key stays lit after a tap: at least this long, and until the next tick after it.
static constexpr uint32_t FLASH_MS = 300;
// The most digits the pad holds, whatever the option says.
static constexpr int CODE_MAX = 8;

// The tile: a Roku TV's number keys. The digits gather on the line above the keys and Enter tunes the TV's tuner to
// that channel (media_player.play_media, media_content_type channel, as Home Assistant's Roku integration does). A
// Roku takes digits it is sent one by one only while its on-screen keyboard is open, so a channel goes as a whole.
class PadTile : public tessera::Tile {
 public:
  explicit PadTile(const tessera::Plugin *plugin) : plugin_(plugin) {
    for (int &s : slot_key_) s = -1;
  }

  void create(const tessera::TileContext &c) override {
    std::snprintf(entity_, sizeof entity_, "%s", c.entity ? c.entity : "");
    on_enter_ = true;
    show_clear_ = true;
    show_enter_ = true;
    max_ = std::max(1, std::min(c.options["max"] | 4, CODE_MAX));
    idle_ = std::max(0, c.options["idle"] | 10);
    const bool show_code = true;

    if (!entity_[0]) {
      // No Roku yet: say what to do, in the middle.
      hint_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_width(hint_, c.width);
      lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
      ui::set_text(hint_, plugin_->text("no_player"));
      lv_obj_set_pos(hint_, 0, (c.height - ui::line_height(Font::BODY)) / 2);
      return;
    }
    enter_valid_ = true;

    // The line of digits above the keys.
    int top = 0;
    if (show_code) {
      display_ = ui::label(c.parent, Font::TITLE, theme::MUTED);
      lv_obj_set_width(display_, c.width);
      lv_obj_set_style_text_align(display_, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(display_, 0, 0);
      top = ui::line_height(Font::TITLE) + ui::px(6);
    }
    layout(c.width, c.height - top, top);

    // Words on a key where the key is high enough for them, else the smaller face.
    const Font word = ui::line_height(Font::BODY_LARGE) + ui::px(4) <= kh_ ? Font::BODY_LARGE : Font::BODY;
    const int room = std::min(kw_, kh_) * 3 / 4;
    const Font face = ui::text_width(ui::icon(ENTER_GLYPH), Font::ICON) <= room ? Font::ICON : Font::ICON_SMALL;
    const int radius = std::min(ui::px(ui::large() ? 12 : 8), std::min(kw_, kh_) / 2);

    for (int i = 0; i < count_; ++i) {
      Key &key = keys_[i];
      const bool icon = key.id == CLEAR || key.id == ENTER;
      const Font font = icon ? face : word;
      key.block = ui::block(c.parent, theme::BUTTON);
      lv_obj_set_size(key.block, kw_, kh_);
      lv_obj_set_pos(key.block, pos_x(key.slot), pos_y(key.slot));
      lv_obj_set_style_radius(key.block, radius, 0);
      key.label = ui::label(key.block, font, theme::INK);
      lv_obj_set_width(key.label, kw_);
      lv_obj_set_style_text_align(key.label, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(key.label, 0, (kh_ - ui::line_height(font)) / 2);
      if (key.id == CLEAR) ui::set_text(key.label, ui::icon(CLEAR_GLYPH));
      else if (key.id == ENTER) ui::set_text(key.label, ui::icon(ENTER_GLYPH));
      else {
        const char digit[2] = {static_cast<char>('0' + key.id), 0};
        ui::set_text(key.label, digit);
      }
    }
    show();
    paint();
  }

  // The entity's state: one that is not there greys the keys.
  void on_state(JsonObjectConst data) override {
    const char *state = data["state"] | "";
    if (std::strcmp(state, state_) == 0) return;
    std::snprintf(state_, sizeof state_, "%s", state);
    paint();
  }

  void on_tick(uint32_t) override {
    bool changed = false;
    if (flash_ >= 0 && esphome::millis() - flash_at_ >= FLASH_MS) {
      flash_ = -1;
      changed = true;
    }
    // The number nobody finished: gone after the idle time, so the next person starts afresh.
    if (idle_ > 0 && len_ > 0 && esphome::millis() - last_tap_ >= static_cast<uint32_t>(idle_) * 1000u) {
      forget();
      show();
    }
    if (changed) paint();
  }

  void on_theme() override {
    if (hint_) ui::set_color(hint_, theme::MUTED);
    show();
    paint();
  }

  void on_tap() override {
    const int i = key_at(tessera::tap_point());
    if (i < 0) return;
    last_tap_ = esphome::millis();
    bool ok = true;
    const int id = keys_[i].id;
    if (id == CLEAR) {
      forget();
    } else if (id == ENTER) {
      ok = press_enter();
    } else {
      ok = press_digit(id);
    }
    show();
    flash_ = i;
    flash_ok_ = ok;
    flash_at_ = esphome::millis();
    paint();
  }

 private:
  struct Key {
    int id = 0;
    int slot = 0;   // its place in the block, row by row
    lv_obj_t *block = nullptr, *label = nullptr;
  };

  // The plan of the keys: the telephone's three columns when they come out large enough (7 mm), else whichever of the
  // arrangements gives the largest keys. A key is never higher than 14 mm nor wider than half again its height, and the
  // gap narrows before the keys do.
  struct Plan {
    int cols = 1, rows = 1, kw = 1, kh = 1, gap = 0;
    bool phone = false;
    int size() const { return std::min(kw, kh); }
  };

  Plan plan(int cols, bool phone, int n, int width, int height) const {
    Plan p;
    p.cols = cols;
    p.rows = (n + cols - 1) / cols;
    p.phone = phone;
    p.gap = ui::px(8);
    auto fit = [&]() {
      p.kw = (width - (cols - 1) * p.gap) / cols;
      p.kh = (height - (p.rows - 1) * p.gap) / p.rows;
    };
    fit();
    if (std::min(p.kw, p.kh) < ui::mm(9)) {
      p.gap = ui::px(3);
      fit();
    }
    p.kh = std::max(1, std::min(p.kh, ui::mm(14)));
    p.kw = std::max(1, std::min(p.kw, p.kh * 3 / 2));
    return p;
  }

  // Which keys there are, and where they stand in the plan chosen for this much room.
  void layout(int width, int height, int top) {
    const int shown = 10 + (show_clear_ ? 1 : 0) + (show_enter_ ? 1 : 0);
    const Plan phone = plan(3, true, KEY_COUNT, width, height);
    Plan best = phone;
    if (phone.size() < ui::mm(7)) {
      for (int cols : {4, 6}) {
        const Plan other = plan(cols, false, shown, width, height);
        if (other.size() > best.size()) best = other;
      }
    }
    cols_ = best.cols;
    rows_ = best.rows;
    kw_ = best.kw;
    kh_ = best.kh;
    gap_ = best.gap;
    left_ = (width - (cols_ * kw_ + (cols_ - 1) * gap_)) / 2;
    top_ = top + (height - (rows_ * kh_ + (rows_ - 1) * gap_)) / 2;

    // The keys in the order of the plan. The telephone keeps a key's place empty; the rows close up over it.
    const int *order = best.phone ? PHONE : INORDER;
    int slot = 0;
    for (int k = 0; k < KEY_COUNT; ++k) {
      const int id = order[k];
      const bool there = (id != CLEAR || show_clear_) && (id != ENTER || show_enter_);
      if (!there) {
        if (best.phone) ++slot;
        continue;
      }
      keys_[count_].id = id;
      keys_[count_].slot = slot;
      slot_key_[slot] = count_;
      ++count_;
      ++slot;
    }
  }

  int pos_x(int slot) const { return left_ + (slot % cols_) * (kw_ + gap_); }
  int pos_y(int slot) const { return top_ + (slot / cols_) * (kh_ + gap_); }

  // The key under the finger: its own size and half the gap around it, so a tap between two keys still counts for the
  // nearer one. A place with no key, or a point off the block, gives -1.
  int key_at(tessera::Point p) const {
    if (count_ == 0 || p.x < 0 || p.y < 0) return -1;
    const int x = p.x - left_ + gap_ / 2, y = p.y - top_ + gap_ / 2;
    if (x < 0 || y < 0) return -1;
    const int col = x / (kw_ + gap_), row = y / (kh_ + gap_);
    if (col >= cols_ || row >= rows_) return -1;
    return slot_key_[row * cols_ + col];
  }

  void forget() {
    code_[0] = '\0';
    len_ = 0;
  }

  // Tunes the TV to the number typed: two fields, so the action of plugin API 0.12 with a JSON object of them. False
  // when there is nothing to tune or it could not be sent.
  bool tune() const {
    if (!entity_[0] || len_ == 0) return false;
    JsonDocument fields;
    fields["media_content_type"] = "channel";
    fields["media_content_id"] = code_;
    return tessera::action("media_player.play_media", entity_, fields.as<JsonVariantConst>());
  }

  bool press_digit(int digit) {
    // A number that has reached its length waits for Enter.
    if (len_ >= max_) return false;
    code_[len_++] = static_cast<char>('0' + digit);
    code_[len_] = '\0';
    return true;
  }

  bool press_enter() {
    const bool sent = tune();
    if (sent) forget();
    return sent;
  }

  // The digits typed so far, above the keys, or the words that say what to do.
  void show() {
    if (!display_) return;
    if (len_ > 0) {
      ui::set_color(display_, theme::INK);
      ui::set_text(display_, code_);
    } else {
      ui::set_color(display_, theme::MUTED);
      ui::set_text(display_, plugin_->text("type_number"));
    }
  }

  void paint() {
    const bool away = std::strcmp(state_, "unavailable") == 0;
    for (int i = 0; i < count_; ++i) {
      Key &key = keys_[i];
      if (i == flash_) {
        // A tap: the pressed blue while it goes out, red when it could not be sent.
        if (flash_ok_) ui::set_fill(key.block, Tone::NORMAL, theme::ACCENT_PRESSED);
        else ui::set_fill(key.block, Tone::ALERT, theme::BUTTON);
        ui::set_color(key.label, theme::ON_ACCENT);
        continue;
      }
      ui::set_fill(key.block, Tone::NORMAL, theme::BUTTON);
      if (away) ui::set_color(key.label, theme::SUBTLE);
      else ui::set_color(key.label, theme::INK);
    }
  }

  const tessera::Plugin *plugin_;
  Key keys_[KEY_COUNT];
  int slot_key_[16];
  int count_ = 0;
  int cols_ = 1, rows_ = 1, kw_ = 1, kh_ = 1, gap_ = 0, left_ = 0, top_ = 0;

  bool on_enter_ = false, show_clear_ = true, show_enter_ = false, enter_valid_ = false;
  int max_ = 3, idle_ = 10;

  char code_[CODE_MAX + 1] = "";
  int len_ = 0;
  uint32_t last_tap_ = 0;

  char entity_[128] = "";
  char state_[32] = "";
  int flash_ = -1;
  bool flash_ok_ = false;
  uint32_t flash_at_ = 0;
  lv_obj_t *hint_ = nullptr, *display_ = nullptr;
};

}  // namespace numbers

tessera::Tile *make_numbers(const tessera::Plugin *plugin) { return new numbers::PadTile(plugin); }

}  // namespace esphome::roku_controls
