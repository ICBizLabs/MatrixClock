# Bring-up and testing checklist

Commands assume the repo root. `pio` is PlatformIO Core (`pip install platformio`).

```sh
pio run                      # build
pio run -t upload            # flash over USB (hold BOOT + tap RST if the port is missing)
pio device monitor           # serial log at 115200
```

1. **Boot** – the monitor shows `MWC x.y.z boot`, the I2C map (`es8311=0x18 pca9557=0x.. rtc=0x51`) and
   `HUB75 ok 64x32 @ NNN Hz`. The panel plays the test pattern for 10 s on the very first boot.
2. **WiFi** – without credentials the AP `MatrixClock-XXXX` appears; connect, open http://4.3.2.1/, enter SSID and
   password on the WiFi tab and save. The panel shows `SETUP WIFI` / `4.3.2.1` until it is online, then the time.
   `ping matrixclock.local` answers.
3. **Config** – `curl -s http://matrixclock.local/api/config | jq .display.brightness`, then
   `curl -X POST http://matrixclock.local/api/config -H 'Content-Type: application/json' -d '{"display":{"brightness":30}}'`
   dims the panel at once and survives a power cycle. `-d '{"location":{"lat":999}}'` returns HTTP 400.
4. **Weather** – `curl -s http://matrixclock.local/api/weather | jq .cur.temp` matches
   `curl -s 'http://api.open-meteo.com/v1/forecast?latitude=LAT&longitude=LON&current=temperature_2m&temperature_unit=fahrenheit'`.
   Bottom pages rotate; the forecast screen appears every third cycle. Unplug the router: the last data stays and
   `/api/status` `net.wx_err` shows the failure.
5. **Alerts** – set the NWS contact on the Location tab, then
   `curl -X POST http://matrixclock.local/api/test/alert -H 'Content-Type: application/json' -d '{"event":"Tornado Warning","severity":"Extreme","headline":"Test until 5 PM","minutes":3}'`
   shows a red scrolling banner with a flashing frame; the chime plays (outside quiet hours); `GET /api/alerts`
   lists it; it disappears after 3 minutes. Injecting the same test twice does not flash again.
   For a live alert run `tools/find_nws_test_point.sh` and point the clock at the printed coordinates.
6. **Audio** – `curl -X POST http://matrixclock.local/api/test/chime -H 'Content-Type: application/json' -d '{"force":true}'`
   plays; with quiet hours covering "now" a test alert stays silent and `/api/log` shows `chime suppressed: quiet hours`.
   **Speech** – with WiFi up, `curl -X POST http://matrixclock.local/api/voice/download`, then watch
   `curl -s http://matrixclock.local/api/status | jq .speech` go `downloading` (with `progress`) → `verifying` → `installed`
   and `/api/log` print `voice: pack v1 installed (134 clips, en_US-ljspeech-medium)`. Then
   `curl -X POST http://matrixclock.local/api/test/say -H 'Content-Type: application/json' -d '{"text":"Tornado Warning"}'`
   speaks (a phrase that is not in the pack returns 404), a test alert plays its chime followed by the event name,
   a 5-second timer (`POST /api/timer {"seconds":5}`) beeps and says "Timer finished" once and then only beeps,
   and `POST '/api/demo?on=1&sound=1'` announces every scenario. Power-cycle: `speech.installed` stays true.
7. **RTC / buttons / OTA** – power-cycle with WiFi unavailable: the time is right immediately and
   `/api/status` `time.source` is `rtc`. K1 short = next page, K1 long = test pattern, K2 short = acknowledge alerts,
   K2 long = test chime, K3 short = refresh data, K3 long = reboot.
   `curl -F 'firmware=@.pio/build/seengreat_hub75_s3/firmware.bin' http://matrixclock.local/update` reboots into the
   new version with settings intact.
