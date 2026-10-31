# Changelog

## 0.1.0
- First version: five tiles for a Roku through Home Assistant's Roku integration, with nothing to type. Navigation
  (back, home, the arrows round OK, replay and *), playback (rewind, play/pause, fast forward), a Roku TV's keys (power,
  volume, mute, channel, TV), a Roku TV's channel numbers tuned on Enter, and the Roku's apps as keys with their logos,
  which a Home Assistant automation blueprint keeps in Home Assistant's www folder.
- Builds on Tessera 0.4.91 and later (plugin API 0.24): every action sends its fields as a JSON object. The apps tile
  makes its keys, arrows, page number and hint once and keeps one list of apps.
