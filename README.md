# Matrix Weather Clock

A wall clock on a 64x32 HUB75 LED matrix that also shows local weather and US National Weather Service (NWS)
emergency weather alerts. Runs on the Seengreat **RGB Matrix HUB75 S3** controller (ESP32-S3), fetches weather from
Open-Meteo and alerts from api.weather.gov, plays a chime through the board's speaker output when a new alert
arrives, and is configured entirely through its own web interface.

> Status: feature-complete firmware that builds cleanly; first hardware bring-up still pending. See
> [Troubleshooting](#troubleshooting) for the settings most likely to need adjusting on a new panel.

## Features

- **Clock** in a fixed-cell 7x11 digit font (no jitter), 12/24-hour, AM/PM or seconds, blinking colon. Time comes
  from NTP and is kept in the on-board PCF85063 real-time clock, so the display is correct right after a power cut.
- **Weather** from [Open-Meteo](https://open-meteo.com/) (free, no API key): current temperature, feels-like,
  humidity, wind, gusts, conditions with a weather icon, and a three-day forecast screen with highs and lows.
  Imperial or metric units.
- **NWS alerts** from [api.weather.gov](https://www.weather.gov/documentation/services-web-api) for your
  coordinates: severity-coloured scrolling banner (red for Extreme/Severe, orange Moderate, yellow Minor), a flashing
  frame for the first minute, automatic expiry, cancellation and de-duplication of updates, a severity filter and an
  ignore list for event types you do not care about.
- **Chime** on new alerts through the ES8311 codec and speaker connector: three tone patterns, volume, quiet hours,
  optional repeat while an alert stays unacknowledged.
- **Display control**: manual brightness, day/night schedule, night mode (very dim, clock only), gamma, colours,
  page order and timing.
- **Web UI** with setup access point and captive portal, mDNS (`matrixclock.local`), REST API, WiFi scanner,
  test buttons (panel pattern, fake alert, chime), log viewer and over-the-air firmware update.
- **Lightning**: live strikes from the Blitzortung.org network (optional, via the community MQTT relay). Strikes within
  your radius show a bolt next to the clock, a flash across the panel, a "LIGHTNING 8MI NW 3M" page and an optional chime.
- **Alarms and timer**: four alarms with weekday selection and label, a countdown timer; snooze or stop with the wheel.
- **Messages**: `POST /api/message` scrolls any text on the clock with an optional chime, for Home Assistant, scripts or
  phone shortcuts.
- **More screens**: 12-hour temperature and rain-chance graph, sunrise/sunset page, brightness that follows the sun.
- **Polish**: pages slide in, rain/snow/lightning animate behind the weather, holiday colour themes with confetti, snow,
  hearts or sparkles on the day.
- **Pushbullet**: new weather alerts, nearby lightning and alarms pushed to your phone; pushes sent to the clock
  (from the phone app, IFTTT or scripts) scroll on the panel.
- **Live view** of the panel in the web UI, settings backup and restore.
- **Buttons**: the board's thumb-wheel switch changes pages, acknowledges alerts, stops or snoozes alarms, refreshes data.

## Hardware

| Part | Notes |
|---|---|
| [Seengreat RGB Matrix HUB75 S3](https://seengreat.com/wiki/214/rgb-matrix-hub75-s3) (SKU 260612) | ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB PSRAM, native USB-C, ES8311 codec, ES7210 mic ADC, speaker amp, PCF85063 RTC, PCA9557 thumb-wheel switch, micro-SD. [Controller used in this build (Amazon)](https://www.amazon.com/dp/B0H69DTZVH) |
| P4 64x32 HUB75 panel (P4-256x128-2121-A5 or similar) | 1/16 scan, 3-in-1 SMD, 256x128 mm. Any 64x32 1/16-scan panel should work. [Panel used in this build (AliExpress)](https://s.click.aliexpress.com/e/_c3olggmN) |
| 5 V power supply | The board has a second USB-C and a VH-4P screw terminal for panel power (5 V / 4 A max). A 64x32 panel at full white can draw close to that |
| Speaker (optional) | 4-8 ohm on the board's speaker connector, needed for the chime |
| CR1220 / LIR battery (optional) | On the SH1.0 connector to keep the RTC running without power |

**Where the parts came from**

| Controller | Panel |
|---|---|
| <a href="https://www.amazon.com/dp/B0H69DTZVH"><img src="https://m.media-amazon.com/images/I/61VzwaWpPfL._AC_SL350_.jpg" width="260" alt="Seengreat RGB Matrix HUB75 S3 controller board on Amazon"></a> | <a href="https://s.click.aliexpress.com/e/_c3olggmN"><img src="https://ae-pic-a1.aliexpress-media.com/kf/S68fe89304a9f4e888e5167fd0c0d50d3P.jpg_350x350.jpg" width="260" alt="P4 64x32 HUB75 LED matrix panel on AliExpress"></a> |
| [Seengreat RGB Matrix HUB75 S3 (Amazon)](https://www.amazon.com/dp/B0H69DTZVH) | [P4 256x128 mm 64x32 module (AliExpress)](https://s.click.aliexpress.com/e/_c3olggmN) |

### Wiring

Plug the panel's HUB75 input into the board's box header with the supplied ribbon cable (or push the board straight
onto the panel's pin header). Feed the panel from the board's panel-power USB-C or the screw terminal. Connect the
programming USB-C to your computer.

Pin map used by the firmware (`include/pins.h`):

| Function | GPIO | Function | GPIO |
|---|---|---|---|
| HUB75 R1 G1 B1 | 5 4 6 | I2S MCLK / BCLK / LRCK | 38 / 48 / 21 |
| HUB75 R2 G2 B2 | 15 7 17 | I2S data ESP→codec / codec→ESP | 14 / 47 |
| HUB75 A B C D E | 8 18 10 9 16 | Speaker amp enable | 3 |
| HUB75 LAT / OE / CLK | 11 / 13 / 12 | I2C SDA / SCL (RTC, codec, expander) | 1 / 2 |
| micro-SD SPI (unused) | 42 41 40 39 | BOOT button | 0 |

## Install from your browser

The easiest way to flash a board is the web installer, which uses Web Serial (Chrome or Edge on a computer):

1. Open **https://icbizlabs.github.io/MatrixClock/** (published by the build workflow from the `installer/` folder;
   it goes live once GitHub Pages is enabled for the repository).
2. Connect the board's programming USB-C port, click **Install Matrix Clock**, pick the serial port and choose "erase"
   on a first install. If the port is missing, hold BOOT, tap RST, release BOOT and try again.
3. When it reboots, join the `MatrixClock-XXXX` network and open http://4.3.2.1/ to finish setup.

<img src="docs/ui/installer.png" width="560" alt="Web installer page with the Install Matrix Clock button">

The same page works from your own machine, because browsers treat `localhost` as a secure origin:

```sh
cd installer
python -m http.server 8000      # then open http://localhost:8000/ in Chrome or Edge
```

`installer/` also holds the images directly: `matrix-clock-<version>-factory.bin` (whole flash, write at offset 0 with
esptool) and `matrix-clock-<version>-ota.bin` (upload from the clock's Update tab). Every push to `main` rebuilds the
firmware on GitHub Actions, republishes the installer and, through the manifest, offers the new version to every clock
that has automatic updates on; the images are also attached to each workflow run as an artifact. `python tools/make_installer.py --build` regenerates the folder locally.

## Automatic updates

Once a clock runs version 0.3.0 or later it keeps itself current: every six hours (configurable on the Update tab) it
reads the installer's `manifest.json`, and when a newer version is listed it downloads the OTA image over HTTPS, checks
the MD5 from the manifest, installs it with a progress bar on the panel and reboots. Automatic installs wait while an
alarm or timer is running. The Update tab shows the installed and latest versions and has "Check now" and "Install"
buttons; automatic installation can be switched off to install manually. The manifest URL is configurable, so a fork
can point its clocks at its own GitHub Pages site.

## Building and flashing

Requirements: [PlatformIO Core](https://platformio.org/install/cli) (`pip install platformio`) and Python 3 (used
by the build script that embeds the web page). No Node.js needed.

```sh
pio run                 # first run downloads the ESP32-S3 toolchain and all libraries (several minutes)
pio run -t upload       # flash over the programming USB-C port
pio device monitor      # serial log, 115200 baud
```

If no serial port appears, hold **BOOT**, tap **RST**, release **BOOT**, then upload again. The board uses the
S3's native USB, so no driver is needed.

Later updates can be uploaded from the browser (Update tab) with `.pio/build/seengreat_hub75_s3/firmware.bin`; settings
are kept across updates.

## First-time setup

1. Power up. The panel shows a ten-second test pattern (solid colours, border, gradients, info text), then the clock
   with `SETUP WIFI` / `4.3.2.1` in the bottom half.
2. Connect a phone or laptop to the WiFi network **MatrixClock-XXXX** (open by default). The setup page should open
   automatically; otherwise browse to http://4.3.2.1/.
3. **WiFi tab**: pick your network (Scan), enter the password, Save & connect. The access point goes away once the
   clock is online.
4. **Location & Weather tab**: type your city or ZIP code, press *Find* and pick the match to fill in
   latitude and longitude (the lookup uses Open-Meteo's geocoding service from your browser), choose the time zone, units, and enter a contact e-mail. The NWS API requires a contact in every
   request; alerts stay disabled until one is set.
5. From now on the UI is at http://matrixclock.local/ (or the IP shown in the header).

<img src="docs/ui/setup-wifi.png" width="640" alt="WiFi tab while the clock runs its setup access point">

## Screens

Simulated renderings of the panel (same layout, fonts and colours as the firmware; the LED look is approximate).
Animated GIFs live in `docs/screens/gif/`, still PNGs of the same scenes in `docs/screens/`.

<img src="docs/screens/gif/rotation.gif" width="384" alt="Pages rotating with slide transitions">

*Page rotation with slide transitions, then the forecast screen.*

| | | |
|---|---|---|
| <img src="docs/screens/gif/date.gif" width="256" alt="Date page"> | <img src="docs/screens/gif/temp.gif" width="256" alt="Temperature"> | <img src="docs/screens/gif/cond.gif" width="256" alt="Conditions"> |
| Date page | Temperature | Conditions |
| <img src="docs/screens/gif/rain.gif" width="256" alt="Rain (animated)"> | <img src="docs/screens/gif/snow.gif" width="256" alt="Snow (animated)"> | <img src="docs/screens/gif/thunder.gif" width="256" alt="Thunderstorm (animated)"> |
| Rain (animated) | Snow (animated) | Thunderstorm (animated) |
| <img src="docs/screens/gif/fog.gif" width="256" alt="Fog at night"> | <img src="docs/screens/gif/wind.gif" width="256" alt="Wind"> | <img src="docs/screens/gif/hilo.gif" width="256" alt="High / low"> |
| Fog at night | Wind | High / low |
| <img src="docs/screens/gif/feels.gif" width="256" alt="Feels like"> | <img src="docs/screens/gif/sun.gif" width="256" alt="Sunrise / sunset"> | <img src="docs/screens/gif/forecast.gif" width="256" alt="3-day forecast"> |
| Feels like | Sunrise / sunset | 3-day forecast |
| <img src="docs/screens/gif/hourly.gif" width="256" alt="12-hour graph"> | <img src="docs/screens/gif/tornado.gif" width="256" alt="Tornado warning (new alert)"> | <img src="docs/screens/gif/winter.gif" width="256" alt="Winter storm watch"> |
| 12-hour graph | Tornado warning (new alert) | Winter storm watch |
| <img src="docs/screens/gif/lightning.gif" width="256" alt="Lightning nearby"> | <img src="docs/screens/gif/alarm.gif" width="256" alt="Alarm ringing"> | <img src="docs/screens/gif/timer.gif" width="256" alt="Timer running"> |
| Lightning nearby | Alarm ringing | Timer running |
| <img src="docs/screens/gif/message.gif" width="256" alt="Message"> | <img src="docs/screens/gif/xmas.gif" width="256" alt="Christmas theme"> | <img src="docs/screens/gif/july4.gif" width="256" alt="Fourth of July theme"> |
| Message | Christmas theme | Fourth of July theme |
| <img src="docs/screens/gif/valentine.gif" width="256" alt="Valentine's theme"> | <img src="docs/screens/gif/halloween.gif" width="256" alt="Halloween theme"> | <img src="docs/screens/gif/night.gif" width="256" alt="Night mode"> |
| Valentine's theme | Halloween theme | Night mode |
| <img src="docs/screens/gif/ip.gif" width="256" alt="IP after connecting"> | <img src="docs/screens/gif/ap.gif" width="256" alt="Setup access point"> | <img src="docs/screens/gif/splash.gif" width="256" alt="Splash"> |
| IP after connecting | Setup access point | Splash |
| <img src="docs/screens/gif/test.gif" width="256" alt="Test pattern"> |
| Test pattern |

### Weather alert screens

The banner scrolls the event and headline of every active alert, coloured by the highest severity: red for Extreme and
Severe, orange for Moderate, yellow for Minor. A one-pixel frame flashes for the first minute after a new alert ("new"
below); the clock stays visible throughout.

| | | |
|---|---|---|
| <img src="docs/screens/gif/tornado.gif" width="256" alt="Tornado Warning (Extreme, new)"> | <img src="docs/screens/gif/hurricane.gif" width="256" alt="Hurricane Warning (Extreme, new)"> | <img src="docs/screens/gif/svr.gif" width="256" alt="Severe Thunderstorm Warning (Severe, new)"> |
| Tornado Warning (Extreme, new) | Hurricane Warning (Extreme, new) | Severe Thunderstorm Warning (Severe, new) |
| <img src="docs/screens/gif/ffw.gif" width="256" alt="Flash Flood Warning (Severe, new)"> | <img src="docs/screens/gif/heat.gif" width="256" alt="Excessive Heat Warning (Severe, new)"> | <img src="docs/screens/gif/torwatch.gif" width="256" alt="Tornado Watch (Severe)"> |
| Flash Flood Warning (Severe, new) | Excessive Heat Warning (Severe, new) | Tornado Watch (Severe) |
| <img src="docs/screens/gif/blizzard.gif" width="256" alt="Blizzard Warning (Severe)"> | <img src="docs/screens/gif/windfire.gif" width="256" alt="High Wind + Red Flag Warnings (two alerts)"> | <img src="docs/screens/gif/winter.gif" width="256" alt="Winter Storm Watch + Wind Advisory (Moderate)"> |
| Blizzard Warning (Severe) | High Wind + Red Flag Warnings (two alerts) | Winter Storm Watch + Wind Advisory (Moderate) |
| <img src="docs/screens/gif/wwa.gif" width="256" alt="Winter Weather Advisory (Moderate)"> | <img src="docs/screens/gif/heatadv.gif" width="256" alt="Heat Advisory (Moderate)"> | <img src="docs/screens/gif/fogadv.gif" width="256" alt="Dense Fog Advisory (Minor)"> |
| Winter Weather Advisory (Moderate) | Heat Advisory (Moderate) | Dense Fog Advisory (Minor) |

<img src="docs/screens/tornado_strip.png" width="192" alt="Tornado warning banner scrolling, four frames half a second apart">

*Four frames, half a second apart: the banner scrolls left while the frame flashes.*

## Web interface

Everything is configured from the clock's own page at http://matrixclock.local/. The Status tab shows a live view of
the panel, current conditions, active alerts with acknowledge and test buttons, and system health; the other tabs
hold the settings. Changes apply immediately except panel driver settings, which need a reboot.

<img src="docs/ui/ui-status.png" width="720" alt="Status tab: live view of the panel, current weather, alerts and system health">

| | |
|---|---|
| <img src="docs/ui/ui-weather.png" alt="Location & Weather tab"> | <img src="docs/ui/ui-display.png" alt="Display tab"> |
| Location & Weather: coordinates with city/ZIP search, time zone, units, NWS alert filters, lightning | Display: brightness schedule, night mode, pages, effects, colours |
| <img src="docs/ui/ui-alarms.png" alt="Alarms tab"> | <img src="docs/ui/ui-update.png" alt="Update tab"> |
| Alarms: countdown timer and four alarms with weekdays | Update: automatic updates, settings backup, firmware upload |
| <img src="docs/ui/ui-panel.png" alt="Panel tab"> | <img src="docs/ui/ui-notify.png" alt="Notify tab"> |
| Panel: HUB75 driver settings and the test pattern | Notify: Pushbullet notifications in both directions |
| <img src="docs/ui/ui-audio.png" alt="Audio tab"> | <img src="docs/ui/ui-wifi.png" alt="WiFi tab"> |
| Audio: chime, volume, quiet hours | WiFi: network, hostname, setup AP password, factory reset |

Screenshots are taken from the real page served with sample data; images live in `docs/ui/`.

## What the screen shows

```
+----------------------------------------------------------------+
|  1 2 : 3 4  PM     <- time, 7x11 digits, AM/PM or seconds     |
|  [icon] 72°F        <- bottom half: rotating pages             |
|         H 46%                                                  |
+----------------------------------------------------------------+
```

Bottom-half pages (order and timing configurable): `date` (weekday, month day), `temp` (icon, temperature,
humidity), `cond` (icon and conditions text, scrolls if long), `wind` (direction, speed, gusts), `hilo` (today's
high/low), `feels` (feels-like temperature), `sun` (sunrise and sunset). Every few cycles the whole panel switches to
a full screen: the **forecast** (three columns with weekday, icon and high/low) alternating with the **hourly graph**
(temperature curve and rain-chance bars for the next 12 hours). Pages slide in from the right; when it is raining,
snowing or thundering, matching animation runs behind the weather pages. On holidays the clock changes colours and
adds an effect (snow at Christmas, confetti on New Year and July 4th, hearts on Valentine's Day, and so on).

When an alert is active the bottom half becomes a scrolling **banner** (`EVENT: headline`, all active alerts
in severity order) and a one-pixel frame flashes around the panel for the first minute. The clock stays visible.
In **night mode** the panel dims to the configured level and hides the weather, but an alert banner still shows.

Before the time is known the bottom half shows the connection state (`WIFI...`, `SETUP WIFI` with `4.3.2.1`, or the
IP address once online). After every WiFi connection the IP address is shown for 20 seconds (Display tab).

## Alerts in detail

- Polled every 2 minutes (configurable, minimum 60 s) from
  `https://api.weather.gov/alerts/active?point=lat,lon&status=actual&message_type=alert,update`
  (the endpoint rejects a `limit` parameter; the firmware keeps at most 12 alerts).
- Only alerts at or above the configured minimum severity are shown; events in the ignore list (for example
  `Special Weather Statement`) are hidden. The chime has its own, usually stricter, severity threshold.
- Updates to an alert you already saw inherit its state, so they do not flash or chime again. Cancelled alerts and
  alerts past their end time disappear on their own.
- After three failed polls in a row the banner marks the data as stale with a grey dot in the corner; the last known
  alerts stay on screen.
- **Acknowledge** (web UI or button K2) stops the flashing frame and any repeat chimes.
- Test without waiting for a storm: the Location tab has an *Inject test alert* button (3 minutes, Extreme), and
  `tools/find_nws_test_point.sh` prints coordinates inside a currently active real alert.

## Lightning

Optional, off by default (Location tab). The clock subscribes to the Blitzortung.org volunteer network through the
public MQTT relay used by the Home Assistant integration (`blitzortung.ha.sed.pl`, personal use only, no account) for
the map cells around your coordinates and keeps strikes inside your radius (default 40 km) for a window (default
15 minutes). While strikes are nearby: a bolt icon next to the clock, a bolt flash across the panel on every new
strike, a `LIGHTNING` page with distance, direction and age in the rotation, and a chime on the first strike of a
storm (then at most every five minutes, respecting quiet hours). Thunderstorm conditions from the weather data also
show lightning in the animation even without the network feed.

## Alarms, timer and messages

Alarms tab: up to four alarms with time, weekdays, label and sound. A ringing alarm shows `ALARM` and its label,
flashes an orange frame and repeats the sound every few seconds for up to ten minutes, ignoring quiet hours.
Wheel push (K2) stops it, wheel up/down snoozes for nine minutes; the Status tab has the same buttons. The timer
counts down in the bottom half and rings the same way.

Messages: `POST /api/message` with `{"text": "...", "seconds": 60, "chime": true, "color": "#40C0FF"}` scrolls the
text (0 seconds = until cleared with `POST /api/message/clear` or the wheel push). The Status tab has a form for it.

## Pushbullet

Notify tab: paste an access token from Pushbullet's account settings. The clock registers itself as a device named
"Matrix Clock" and then pushes a note to your phone for new NWS alerts at or above the chosen severity, for the first
lightning strike of a storm (and at most every five minutes after that), and optionally for alarms. With "show pushes"
on it also polls your account and scrolls any push sent to all devices or to the clock: pick "Matrix Clock" in the
phone app, or use IFTTT / Home Assistant / `curl` against the Pushbullet API. Pushes the clock sent itself are ignored.

## Buttons (thumb-wheel switch)

| Key | Short press | Long press (1.5 s) |
|---|---|---|
| K1 | next page (snooze while ringing) | run the panel test pattern |
| K2 | stop a ringing alarm/timer, else clear a message, else acknowledge alerts | play a test chime (ignores quiet hours) |
| K3 | refresh weather and alerts now (snooze while ringing) | reboot |

## REST API

Everything the web UI does goes through JSON endpoints, so the clock can be scripted. The full list with the
configuration schema is in [docs/api.md](docs/api.md). Examples:

```sh
curl -s http://matrixclock.local/api/status | jq .
curl -X POST http://matrixclock.local/api/config -H 'Content-Type: application/json' \
     -d '{"display":{"brightness":40},"audio":{"volume":80}}'
curl -X POST http://matrixclock.local/api/test/alert -H 'Content-Type: application/json' \
     -d '{"event":"Tornado Warning","severity":"Extreme","headline":"Test until 5 PM","minutes":3}'
curl -F 'firmware=@.pio/build/seengreat_hub75_s3/firmware.bin' http://matrixclock.local/update
```

## Troubleshooting

| Symptom | What to try |
|---|---|
| Panel stays dark | Panel tab: driver `FM6126A`, then `ICN2038S`; save and reboot |
| Colours washed out / pastel | driver `FM6124` |
| Ghost columns, smeared pixels | latch blanking 3-4, clock phase off |
| Flicker | minimum refresh 150 Hz or higher, or colour depth 6 |
| Red and blue swapped | *Swap red/blue* |
| Leftmost column missing, stray pixels bottom right, image shifted one pixel | clock phase off (the default) or on |
| Board resets when the panel goes bright | panel power supply too weak; lower *Max brightness cap* |
| WiFi weak while the panel runs | raise TX power on the WiFi tab, route the ribbon cable away from the antenna |
| No chime | Status tab shows whether the ES8311 was found; check volume, quiet hours, speaker connector |
| Keys do nothing | Status tab shows whether the PCA9557 expander was found; `/api/log` prints raw key states |

If a panel setting makes the board reset repeatedly, the firmware restores the panel defaults automatically after
three quick resets. To wipe everything (including WiFi): Factory reset on the WiFi tab, or `pio run -t erase`.

More detail: [docs/hardware.md](docs/hardware.md) (pins, panel notes, test pattern),
[docs/testing.md](docs/testing.md) (bring-up checklist with commands).

## Project layout

```
platformio.ini            build environment `seengreat_hub75_s3` (pioarduino, Arduino core 3.3 / IDF 5.5)
partitions/mwc_16MB.csv   two 3 MB OTA app slots + LittleFS
include/pins.h            every GPIO in one place
include/fonts/            7x11 clock digits
web/index.html            the web UI; tools/build_web.py gzips it into flash at build time
src/main.cpp              boot order and the 30 fps frame loop
src/app.*                 config staging from the web, reboot / factory reset
src/config/               settings struct, JSON load/save/validation (LittleFS /config.json)
src/display/              HUB75 bring-up, off-screen canvas with diff blit, renderer, icons, scroller, test pattern
src/net/                  WiFi + captive portal, network task, Open-Meteo and NWS clients, alert store
src/time/                 NTP, time zone table, PCF85063 RTC driver
src/audio/                ES8311 codec, I2S chime synthesizer
src/io/                   I2C bus with device probing, PCA9557 expander, buttons
src/web/                  async web server, REST API, OTA
docs/                     hardware, API and testing notes
```

Design notes: all network I/O runs in its own FreeRTOS task on core 0 so rendering never stalls on HTTPS; JSON
documents and the alert list live in PSRAM; only changed pixels are pushed to the DMA framebuffer each frame; the
panel clock is fixed at 8 MHz to keep WiFi usable next to the running matrix.

## Credits

[ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA) (panel driver),
[Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library), [ESP32Async ESPAsyncWebServer and
AsyncTCP](https://github.com/ESP32Async), [ArduinoJson](https://arduinojson.org/). The ES8311 register sequence is
ported from Espressif's esp-bsp codec component (Apache-2.0). Weather data by Open-Meteo, alerts by the US National
Weather Service.
