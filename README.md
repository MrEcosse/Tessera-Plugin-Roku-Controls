# Roku controls

A Roku's remote on a wall panel, with nothing to type. Choose the Roku from Home Assistant's
[Roku integration](https://www.home-assistant.io/integrations/roku/) and every key does what the same key on the remote
does. Five tiles, to use side by side on one panel or one at a time:

| Tile | Keys | Entity | Sizes |
|---|---|---|---|
| Roku navigation | back and home, the arrows round OK, replay and * | the Roku's remote | 1x1 to 3x3 |
| Roku playback | rewind, play/pause, fast forward | the Roku's remote | 1x1 to 4x2 |
| Roku TV keys | power, volume down, mute, volume up, channel down and up, TV | the Roku TV's remote | 1x1 to 7x2 |
| Roku TV channel numbers | 0 to 9, Clear and Enter: Enter tunes to the number | the Roku TV's media player | 1x1 to 4x4 |
| Roku channels | the Roku's apps, with their logos | the Roku's media player | 1x1 to 4x4 |

It is a companion to [AV wall panel controls](https://github.com/MrEcosse/Tessera-Plugin-AV-Wall-Panel-Controls), whose
tiles run any action you type: use those for the TV or the amplifier next to the Roku, these for the Roku itself.

**Tried on:** the plugin bench (Tessera's own firmware, built for a computer) on the CYD, the 4-inch Guition, the
4.3-inch Waveshare standing up and the 10.1-inch Guition, light and dark. The navigation, playback and channels tiles
are for every Roku; the Roku TV keys and channel numbers need a Roku TV and have not been tried on one yet.

## Set up

1. Add the Roku to Home Assistant with its Roku integration, if it is not there yet. It makes a remote
   (`remote.<name>`) and a media player (`media_player.<name>`) for it. The Roku must accept control from the network:
   on the Roku, **Settings > System > Advanced system settings > Control by mobile apps**.
2. In Tessera, open **Plugins**, choose **Roku controls** and tick your screens. Each screen builds once with the plugin.
3. In Layout, place the tiles you want from the library's Plugins group and size them by their edges.
4. In the inspector, choose each tile's entity: the Roku's remote for the navigation, playback and TV keys, its media
   player for the channel numbers and the channels.
5. For the logos on the channels tile, add the Home Assistant package (below, "The logos").
6. Make sure the screen may perform Home Assistant actions. Tessera turns this on for a screen it added to Home
   Assistant; for a screen added by hand, tick **Allow the device to perform Home Assistant actions** in the ESPHome
   integration's options for that device.

## The keys

Each key sends the Roku integration's own command with `remote.send_command`:

| Key | Command | Key | Command |
|---|---|---|---|
| Back | `back` | Rewind | `reverse` |
| Home | `home` | Play/pause | `play` |
| Up, down, left, right | `up`, `down`, `left`, `right` | Fast forward | `forward` |
| OK | `select` | Power | `power` |
| Replay | `replay` | Volume down, mute, up | `volume_down`, `volume_mute`, `volume_up` |
| * | `info` | Channel down, up | `channel_down`, `channel_up` |
| | | TV | `input_tuner` |

Tessera's icon set has no rewind, fast forward, replay or * icon yet, so rewind and fast forward show two chevrons
(« and »), * shows the sign, and replay the word (on a key too small for it, the skip-back icon).

Not here, because a Roku does not take them over the network as Home Assistant sends keys: the **colour keys** (red,
green, yellow, blue) and **Guide** of a Roku TV's remote. A Roku TV's remote is usually infrared, so an IR blaster can
send them with the tiles of AV wall panel controls.

## Roku TV channel numbers

Type a channel number on the keys; the digits show above them, and **Enter** tunes the Roku TV's tuner to that channel
(`media_player.play_media` with `media_content_type: channel`, as Home Assistant's Roku integration tunes a channel).
Clear empties the number; **Forget after** (default 10 seconds) forgets digits typed and not tuned, so the next person
starts afresh; **Most digits** (default 4) is the longest number it takes. A Roku takes digits sent one by one only
while its on-screen keyboard is open, which is why the number goes as a whole.

A Roku TV also has a select entity of its tuner's channels by name: the Selector grid of AV wall panel controls shows
those as keys.

## Roku channels

The Roku's apps (its media player's inputs: Home and every installed app) as keys; a tap starts the app, and the app
on screen is lit. Where the screen shows pictures each key has the app's logo, with its name under it where the key is
high enough; on the CYD, the 3.5-inch Waveshare and the 4-inch Hosyond, which show no pictures, the keys have the names.
When the apps do not all fit, arrows along the bottom (at the sides, on a low tile) page through them, at most eight a
page with logos.

- **Show only**: only these apps, in this order. Five of them make the shortcut keys of a remote: Netflix, Freeview
  Play, Apple TV, Spotify, Rakuten TV.
- **Leave out**: apps not to show, such as Home.
- **Names under the logos**: on by default.
- **Logo folder**: where the logos are, under Home Assistant's `www` folder; `roku_icons` unless you changed the package.

The list follows the Roku: Home Assistant's Roku integration reads its apps itself, so an app installed on the Roku
appears on the tile by its name, and its logo follows with the package below.

## The logos

Tessera fetches pictures for a plugin only from Home Assistant (its `www` folder, `/local/...`) or from public hosts a
plugin names, never from another device on your network, so no plugin can make the app reach into your house. The logos
therefore go through Home Assistant: the package in
[`home_assistant/roku_icons.yaml`](home_assistant/roku_icons.yaml) asks the Roku for its apps
(`http://<roku>:8060/query/apps`), downloads each app's icon (`/query/icon/<app id>`) into `www/roku_icons/`, named
after the app (`BBC iPlayer` is `bbc_iplayer.png`), and the tile shows `/local/roku_icons/<that name>.png`.

1. **Settings > Devices & services > Add integration > Downloader**, with the download directory `www`.
2. Copy `roku_icons.yaml` into your config's `packages` folder, and in `configuration.yaml`:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

3. Change `192.168.1.50` in it (twice) to your Roku's address, and restart Home Assistant.

It reads the Roku's apps every six hours, downloads the icons whenever the apps change and at every start, and adds a
script, **Scan Roku channels now**, to do it at once: run it from Home Assistant, or put it on the panel as a script
tile. For more than one Roku, copy the package once per Roku with its own address, sensor and folder, and give each
channels tile its folder.

An app whose name has letters outside the Latin alphabet may get another file name than the tile looks for; rename the
file to what the tile asks for (the app's name in lower case, with `_` between words) if its logo stays empty.

## Good to know

- Each tile sends only its own actions, to the entity you chose: `remote.send_command` for the keys,
  `media_player.play_media` for a channel number, `media_player.select_source` for an app.
- A tile only gets taps: no key repeats while a finger is held, and no tile scrolls (a swipe on a Tessera screen changes
  the page). The channels tile pages with taps instead.
- Nothing is stored and nothing leaves your home: the tiles use no internet service and no Home Assistant entity but the
  ones you choose, and the logos come from your own Home Assistant.

## How it works

- `tessera-plugin.yaml`: five tiles (`nav`, `playback`, `tv`, `numbers`, `channels`), each belonging to an entity of the
  Roku integration.
- `components/roku_controls/keys.cpp`: one tile for the three sets of keys, each a table of commands and icons, laid
  out in a block of three by three or in a row, finding the key under the finger (`tessera::tap_point`) and sending its
  command (`tessera::action`).
- `components/roku_controls/numbers.cpp`: the number keys; Enter sends the action with two fields (plugin API 0.12).
- `components/roku_controls/channels.cpp` and `roku_slug.h`: the apps as keys, each logo a picture place
  (`ui::picture`) showing `/local/<folder>/<slug>.png`, the slug written as Home Assistant's `slugify` writes it.
- `home_assistant/roku_icons.yaml`: the package that keeps the logos in Home Assistant.
