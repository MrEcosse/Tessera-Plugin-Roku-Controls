#pragma once
// An app's name as the name of its icon file, the way Home Assistant's `slugify` filter writes it (python-slugify with
// "_"): letters lower case, accented Latin letters without their accents, every run of anything else one "_", and no
// "_" at either end. "BBC iPlayer" is bbc_iplayer, "Disney+" disney, "Café TV" cafe_tv. The Home Assistant package
// names the files it downloads with the same filter, so the tile finds them. No LVGL and no Tessera here, so a test on a
// computer can run it.
#include <cstddef>
#include <cstdint>

namespace esphome::roku_controls {

namespace slug_detail {
// U+00C0 to U+00FF without accents ('\0': none, a separator); the two-letter ones (Æ, ß, Þ) are spelled in `pair`.
static constexpr char LATIN1[64] = {
    'a', 'a', 'a', 'a', 'a', 'a', 0,   'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',   // C0-CF
    'd', 'n', 'o', 'o', 'o', 'o', 'o', 0,   'o', 'u', 'u', 'u', 'u', 'y', 0,   0,     // D0-DF
    'a', 'a', 'a', 'a', 'a', 'a', 0,   'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',   // E0-EF
    'd', 'n', 'o', 'o', 'o', 'o', 'o', 0,   'o', 'u', 'u', 'u', 'u', 'y', 0,   'y'};  // F0-FF
inline const char *pair(unsigned code) {
  switch (code) {
    case 0xC6: case 0xE6: return "ae";
    case 0xDF: return "ss";
    case 0xDE: case 0xFE: return "th";
    default: return nullptr;
  }
}
}  // namespace slug_detail

// Writes the slug of `name` (UTF-8) into `out`, at most size - 1 characters. Returns its length; an empty name or one
// with nothing to keep gives "unknown", as Home Assistant's does.
inline size_t slugify(const char *name, char *out, size_t size) {
  if (size == 0) return 0;
  size_t n = 0;
  bool gap = false;  // a separator is due before the next letter
  auto put = [&](char ch) {
    if (gap && n > 0 && n + 1 < size) out[n++] = '_';
    gap = false;
    if (n + 1 < size) out[n++] = ch;
  };
  const auto *p = reinterpret_cast<const unsigned char *>(name ? name : "");
  while (*p) {
    const unsigned c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      put(static_cast<char>(c));
      ++p;
    } else if (c >= 'A' && c <= 'Z') {
      put(static_cast<char>(c - 'A' + 'a'));
      ++p;
    } else if (c == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) {
      // U+00C0 to U+00FF: Latin letters with accents.
      const unsigned code = 0xC0 + (p[1] - 0x80);
      if (const char *two = slug_detail::pair(code)) {
        put(two[0]);
        put(two[1]);
      } else if (const char plain = slug_detail::LATIN1[code - 0xC0]) {
        put(plain);
      } else {
        gap = true;
      }
      p += 2;
    } else {
      // Anything else (spaces, signs, other scripts): a separator. Skip the rest of a UTF-8 character.
      gap = true;
      ++p;
      while ((*p & 0xC0) == 0x80) ++p;
    }
  }
  if (n == 0) {
    const char *unknown = "unknown";
    while (*unknown && n + 1 < size) out[n++] = *unknown++;
  }
  out[n] = '\0';
  return n;
}

}  // namespace esphome::roku_controls
