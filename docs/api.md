# REST API

All endpoints answer JSON unless noted. The web UI uses nothing else.

| Method | Path | Description |
|---|---|---|
| GET | `/` | Web UI (gzip, ETag) |
| GET | `/api/status` | time, weather summary, active alerts, WiFi, network fetch status, memory, panel, audio, I2C map |
| GET | `/api/config` | full configuration (passwords masked as `***`) plus `tz_options` |
| POST | `/api/config` | partial configuration merge; body is any subset of the config object. Returns `{ok, applied[], reboot_required[]}` or `{ok:false, error}` with HTTP 400 |
| GET | `/api/weather` | last Open-Meteo result |
| GET | `/api/alerts[?all=1]` | active alerts (filtered by severity/ignore list unless `all=1`) |
| POST | `/api/alerts/ack` | form or query `id=<alert id>` or `id=all` |
| POST | `/api/test/alert` | `{event, severity, headline, minutes}` injects a synthetic alert |
| POST | `/api/test/chime` | `{style?, force}` plays a chime (`force` ignores quiet hours) |
| POST | `/api/test/panel` | shows the test pattern; optional `sec=3..300` (default 10) |
| POST | `/api/message` | `{text, seconds (0 = until cleared), color "#RRGGBB", chime, force}` scrolls a message |
| POST | `/api/message/clear` | remove the message |
| GET | `/api/frame` | current panel content as raw RGB565 little-endian (header `X-Frame-Size: 64x32`) |
| POST | `/api/timer` | `{minutes}` or `{seconds}` starts the countdown timer |
| POST | `/api/timer/cancel` | cancel the timer (also silences a finished one) |
| POST | `/api/alarm/stop`, `/api/alarm/snooze` | control a ringing alarm |
| GET | `/api/config?download=1` | full configuration with passwords, as a downloadable backup |
| POST | `/api/test/push` | send a test Pushbullet notification |
| POST | `/api/update/check` | check the manifest for a newer version now |
| POST | `/api/update/install` | download, verify and install the available version, then reboot |
| POST | `/api/show` | `screen=forecast` or `screen=hourly` shows that full screen now |
| POST | `/api/refresh` | fetch weather and alerts now |
| GET | `/api/wifi/scan[?start=1|?poll=1]` | start / poll an async network scan |
| GET | `/api/log` | text log ring buffer |
| POST | `/api/reboot` | reboot |
| POST | `/api/factory-reset` | form or query `confirm=yes`; erases config and reboots |
| POST | `/update` | multipart firmware upload (`firmware` field); reboots on success |

Configuration keys and defaults:

```json
{ "wifi":     { "ssid": "", "pass": "", "hostname": "matrixclock", "ap_pass": "", "tx_power": 34 },
  "location": { "lat": 39.7392, "lon": -104.9903, "name": "" },
  "time":     { "tz_id": "America/Denver", "tz_posix": "MST7MDT,M3.2.0,M11.1.0", "ntp1": "pool.ntp.org", "ntp2": "time.nist.gov", "use_24h": false, "show_seconds": false },
  "weather":  { "enabled": true, "units": "imperial", "refresh_min": 15, "forecast_days": 3 },
  "alerts":   { "enabled": true, "poll_sec": 120, "user_agent_contact": "", "min_severity": "Moderate", "ignored_events": [], "chime_min_severity": "Severe", "flash_frame_sec": 60, "banner_px_per_s": 20, "tls_verify": false },
  "display":  { "brightness": 128, "gamma": 2.2, "page_sec": 6, "pages": ["date","temp","cond","wind"], "forecast_page": true, "forecast_every_n_cycles": 3, "colon_blink": true, "ip_on_connect_sec": 20,
                "hourly_page": true, "transitions": true, "precip_fx": true, "holiday_themes": true,
                "schedule": { "enabled": true, "follow_sun": false, "sun_offset_min": 30, "day_start": "07:00", "day_level": 160, "night_start": "21:00", "night_level": 40 },
                "night_mode": { "enabled": true, "start": "23:00", "end": "06:00", "level": 8, "hide_bottom": true },
                "colors": { "time": "#FFFFFF", "date": "#80C0FF", "temp": "#FFD060", "text": "#C0C0C0", "hi": "#FF8060", "lo": "#60A0FF" } },
  "panel":    { "width": 64, "height": 32, "chain": 1, "driver": "SHIFTREG", "clkphase": false, "latch_blanking": 2, "i2s_speed_hz": 8000000, "min_refresh_hz": 120, "max_brightness": 255, "color_depth_bits": 8, "double_buffer": false, "swap_rb": false },
  "audio":    { "enabled": true, "volume": 60, "chime": "two_tone", "repeat_min": 0, "quiet": { "enabled": true, "start": "22:00", "end": "07:00" } },
  "pushbullet": { "token": "", "device_iden": "", "notify_alerts": true, "notify_min_severity": "Severe", "notify_lightning": true,
                  "notify_alarms": false, "show_pushes": true, "poll_sec": 60, "show_sec": 60, "chime": true },
  "update":   { "check": true, "auto_install": true, "url": "https://icbizlabs.github.io/MatrixClock/manifest.json", "check_hours": 6 },
  "lightning": { "enabled": false, "server": "blitzortung.ha.sed.pl", "port": 1883, "radius_km": 40, "window_min": 15, "chime": true, "show_bolt": true },
  "alarms":   [ { "enabled": false, "time": "07:00", "days": "1111100", "chime": "triple_beep", "label": "" }, "... up to 4" ] }
```

Pages: `date`, `temp`, `cond`, `wind`, `hilo`, `feels`, `sun`. Alarm `days` is a 7-character string Monday..Sunday (`1` = on). Severities: `Unknown`, `Minor`, `Moderate`, `Severe`,
`Extreme`. Chimes: `none`, `two_tone`, `triple_beep`, `chirp`. Drivers: `SHIFTREG`, `FM6124`, `FM6126A`,
`ICN2038S`, `MBI5124`, `DP3246`. Sending `"tz_id"` without `"tz_posix"` fills the POSIX string from the built-in
US zone table. `wifi.pass` / `wifi.ap_pass` set to `"***"` keep the stored secret.
