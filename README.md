# Goli desk bot

A printable desk robot of **Goli**, the Goliash mascot: tired, kind, always one more alert away from a coffee.
His whole face is a round AMOLED with an ESP32-S3 behind it, so the eyes, brows and mouth are animated.

![Goli desk bot](docs/hero.jpg)

| Something new | Something on fire | Good news |
|---|---|---|
| ![a deploy: ding](docs/face-alert.jpg) | ![a critical drift: ding-ding](docs/face-critical.jpg) | ![an agent is back: chirp](docs/face-good.jpg) |

| Exploded | Back |
|---|---|
| ![exploded](docs/exploded.jpg) | ![back](docs/back.jpg) |

**[Try him in your browser](https://pipozzz.github.io/goli-bot/sim/)** · **[Install the firmware from your browser](https://pipozzz.github.io/goli-bot/)** · **[3D viewer](https://pipozzz.github.io/goli-bot/viewer.html)** (also `viewer.html` here, with an exploded-assembly slider)

**Status:** designed and checked in CAD, first prints under way. Print the bezel and bracket first (plate 3 of
either 3MF, about 1 h) and test-fit them on the display before you print the body.

## Size

About 108 × 65 × 150 mm with the bell on. Every part fits the Bambu Lab A1 mini (180 mm³).

**Goli mini** is the same robot at 90 %: about 97 × 58 × 135 mm with the bell on (body 109 × 58 × 130 mm
with the arms). The display is the same, so his face is a little bigger for his size. Its files are in
[`stl/mini/`](stl/mini), named `goli_mini_*` (the bezel, bracket and mug are identical to the full-size ones).
It needs a smaller battery: a **803040** LiPo (40 × 30 × 8 mm, about 1000 mAh) instead of the 103450.
The body prints in about 6 h 45 min in one colour (108 g) or 14 h 40 min with the AMS, and the back
plate in 52 min.

## Parts to buy

| Part | Notes |
|---|---|
| [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75) | 466×466 round AMOLED with touch, ESP32-S3, IMU, mic, speaker amp, LiPo charger and RTC. Get the version **without** the case. |
| Speaker, 8 Ω 1 W, about 20×30 mm, MX1.25 plug | Glue it behind the belly card; the two "message lines" are the grille. |
| 3× M2×6 machine screws | Bracket → the board's M2 nuts |
| 3× M2×6 self-tapping screws | Bracket → bezel |
| 3× M2×8 self-tapping screws | Back plate → body |
| USB-C cable with a **right-angle (90°) plug** | The board's port points down inside the face socket, so a straight plug doesn't fit. The plug's head may reach 8 mm from the port and be up to 13 × 7.5 mm; the cable then runs backwards inside him and out through the slot in the back plate. Up- or down-angled both work (USB-C plugs in either way round). A 90° USB-C adapter of the same size works too. |
| Optional: 3.7 V LiPo, 1000 mAh, size **103450** (50 × 34 × 10 mm), MX1.25 plug (Goli mini: **803040**, 40 × 30 × 8 mm) | Lies on a cradle in the base (three rails and stops, printed with the body); a strip of double-sided tape holds it. The board's power button is inside the body, so plan for USB power or leave the board on. On battery the screen goes dark after he has slept for 30 min; touch or move him to wake him. |

## Print

| File | Colour | Orientation | Notes |
|---|---|---|---|
| `goli_body_single.3mf`, or `goli_body_blue.stl` alone | blue | upright, as exported | The quick way: about 8 h, paint the details after. Normal (grid) supports, also inside. Plates 2–4: the other parts (below). |
| `goli_body_multicolor.3mf` (`goli_body_blue.stl` + `goli_belly_cream.stl` + `goli_accents_red.stl`) | blue / cream / red | upright, as exported | One multi-colour object for the AMS lite, about 18 h. Plates 2–4: the other parts (below). |
| `goli_bezel_navy.stl` | navy | face down | No supports. With the bracket on plate 3 of both 3MF files (57 min, 15 g). |
| `goli_bracket_any.stl` | any | as exported | No supports. |
| `goli_backplate_blue.stl` | blue | outer face down | No supports; about 1 h. Plate 2 of both 3MF files. |
| `goli_bell_yellow.stl` | yellow | lip down | No supports. With the mug on plate 4 of both 3MF files, at 0.12 mm (29 min). |
| `goli_mug_white.stl` | white | upright | Optional prop: "coffee #4". |

PLA, 0.2 mm layers (0.12 mm for the bell and mug), 3 walls, 15 % gyroid.

The body needs supports. The arms start as islands beside the body at about 2 cm, and the ceiling inside by
the face is nearly flat. Use **normal** (grid) supports, not on the build plate only: a tree's thin tip
let the arms' first layers go into the air. The roof, the face socket and the screw bosses inside have 45°
undersides. The inner supports come out through the open back.

**The quick way (one colour, about 8 h):** open **`stl/goli_body_single.3mf`** in Bambu Studio and print it
in blue PLA (0.20 mm, 3 walls, 15 % gyroid, normal supports; 130 g). Then paint the belly,
card, badge and cheeks (see *Painting* below). The back plate is on the project's second plate (about
1 h, 23 g): print it after the body, the same blue, no supports. Plate 3 has the face bezel and the display bracket
(navy, about 1 h), plate 4 the bell and the mug (yellow and white, 0.12 mm layers, about 30 min); load that
filament before you print the plate. The multi-colour print below takes 18 h, and 10 of those
hours are the AMS lite changing filament about 280 times.

**Multi-colour body (AMS lite):** open **`stl/goli_body_multicolor.3mf`** in Bambu Studio. It is one object
with three parts already on filaments 1–3 (blue, cream, red), set up for the A1 mini with 0.20 mm layers,
3 walls, 15 % gyroid and normal supports. Map the three filaments to your AMS slots and print:
about 18 h and 163 / 58 / 13 g of blue / cream / red PLA, purging included. Plate 2 is the back plate in
blue (filament 1), about 1 h; plate 3 the bezel and bracket (navy, filament 4), plate 4 the bell and mug
(yellow and white, filaments 5 and 6).
With a single colour, use the quick way above.

### Painting

The belly, the card on it, the red badge and the cheeks all stand out from the body by 0.8-2.8 mm, so
their edges stop the paint by themselves.

You need acrylic paint markers with a fine tip, for example Posca PC-3M (or PC-1MR for the cheeks'
edges): **ivory** or **beige** for the belly and card, **red** for the badge and cheeks.

1. Wipe the print with isopropyl alcohol so the paint sticks; let it dry.
2. Shake the marker and prime it on paper until the paint flows evenly.
3. Paint each area's raised **edge first**, going round it with the tip held flat against the step, then
   fill the middle. Thin coats: two thin ones cover better than one thick one. Wait about 15 minutes
   between coats.
4. Belly: leave the **stitching** (the little dashes sunk into its edge) blue; it then reads as a seam.
   Paint the card on top of the belly in the same cream; its two slots are the speaker grille.
5. Badge: red; the "!" is sunk in, leave it blue or dot it with a dark marker.
6. Cheeks: red, with a light touch: they're the thinnest (0.8 mm).
7. Optional: a light coat of matte clear spray once everything is dry (a day), so it doesn't wear off
   where you pick him up.

A slip of the marker comes off with a cotton bud dipped in water while the paint is wet, or with a
fingernail once it's dry.

To rebuild the 3MF files after changing the model, run `python3 make_3mf.py` (and `python3 make_3mf.py --mini`). It uses Bambu Studio's command line,
so Bambu Studio must be installed.

## Assembly

1. Glue the speaker behind the belly grille, with the plug pointing up.
2. Drop the display into the bezel from behind, glass first. The lip holds the glass by its black border.
3. Screw the bracket to the board's three back nuts (M2×6). The board's USB-C port points **down**, through
   the notch in the bezel. Plug in the right-angle USB-C cable now, its cable pointing backwards.
4. Feed the cable in through the face opening and push the bezel into the face socket from the front: the
   plug's head goes into the pocket under the opening. From inside, screw the bracket to the bezel tube
   (3× M2×6 self-tapping). The body wall is clamped between the bezel and the bracket.
5. Plug in the speaker. With a battery: stick it onto the cradle in the base (label up, cable towards the
   back) and plug it into BAT. Route the USB-C cable down and out through the back plate slot.
6. Close the back plate (3× M2×8). Press the bell onto the antenna peg; add a drop of glue if it is loose.

## Firmware

`firmware/` is a PlatformIO project: Arduino core 3 (via pioarduino), LVGL 8.4, the CO5300 driver from
GFX Library for Arduino, SensorLib for touch and IMU, Espressif's ES8311 codec driver, and ArduinoJson.

![Goli's faces](docs/faces.png)

| Input | Goli |
|---|---|
| nothing happening | tired: heavy lids, worried brows, wavy mouth; blinks and glances around |
| new alert (Goliash, GitHub, a script...) | **ding** of a desk bell, surprised face, count badge, title scrolling under the mouth |
| something critical (agent gone quiet, vulnerability, production drift...) | **ding-ding**, alarmed face, red badge |
| good news (an agent is back, a drift was resolved) | chirp, happy face, green badge |
| tap the face | boop: surprised, then happy with blushing cheeks; also marks alerts as seen |
| 2 quick taps | wink |
| 3 quick taps | focus mode (a pomodoro) on / off: 25 minutes by default, a blue ring counts down, only critical alerts ring; happy when it's over, then a 5-minute break with a green ring and "back to it?" (both lengths on the web page; 3 taps skip a break) |
| 5 quick taps | setup mode (QR code on the screen) |
| 10 quick taps | demo mode for a booth (any tap stops it) |
| tap, then hold and talk | ask him something: he listens while the finger stays and answers out loud (with API keys, see *Ask Goli*) |
| swipe across the face | latest alerts → Goliash overview → today's meetings → big clock → Goli's day → status |
| knock twice on the desk | snooze: alerts stay silent for 10 minutes |
| a meeting in 10 minutes | an amber ring counts down, its name under his mouth; a ding when it starts |
| during a meeting | alerts stay silent (badge only, critical ones still ring), "in a meeting" under his mouth |
| an alert arrives | he hops, and the badge breathes until you've seen it (faster when critical) |
| nothing happens for a while | every few minutes a glance around, a look at the clock, a silent yawn |
| rain / sun / frost coming | umbrella / sunglasses / scarf |
| below 0 °C / above 28 °C outside | a shiver now and then / a drop of sweat |
| every 15 minutes (web page: never, now and then, always) | the date, the time and the temperature under his mouth for a few seconds |
| you pick him up | the time and the temperature, like a glance at a watch |
| your birthday, December | party hat, Santa hat |
| a Goli next to him rings | he looks that way |
| finger resting on / stroking the face | petting: blissful closed eyes |
| clap | marks alerts as seen (wink), or just startles him |
| loud noise for a while | winces |
| tilt him | pupils roll with the tilt |
| shake him | dizzy, pupils spinning, wobbly sound |
| bump or pick him up | surprised |
| 1 min idle → 5 min idle, or lying flat | drowsy → asleep with a yawn (screen dims, "z Z", breathing, the time under his eyes) |
| hold a finger still for 3 s | turns the face by 90° (saved): use it once after assembly |
| finger on the face while he starts | self-test screen (see *Checking the assembly*) |
| battery low, or charging | battery icon next to the bell |
| production deploy on a Friday after 15:00 | winces: "on a Friday?!" |

The whole face drifts by a few pixels over minutes so the AMOLED does not burn in.

### Pages

![Swipe pages](docs/pages.png)

Swipe across his face for the **latest alerts** (time coloured by level), **Goliash at a glance** (drifting services, updates waiting, updates that fix
exploited vulnerabilities, stale targets; from `GET /api/v1/summary` every 5 minutes; only when Goliash is
set up), **today's meetings**, a **big clock**, **Goli's day** (alerts, good news, how often you petted
him, focus sessions done, and whether yesterday was rough) and **Goli's status** (version, IP, Wi-Fi, connections, battery,
weather). A tap or 30 s goes back to the face.

The time comes from NTP and is kept by the board's RTC, so the clock is right after a restart even
before Wi-Fi is up.

### Ears

The board has two microphones. Goli only measures how loud it is, on the device; nothing is recorded or
sent, and he ignores his own sounds. A clap marks alerts as seen; a noisy room makes him wince.
Turn it off on the web page. The one exception is *Ask Goli* below, and only while your finger holds him.

### Ask Goli

Tap him, then keep the finger on him and ask something; let go when you're done (up to 12 s). He boops,
shows *listening...*, then *hmm...*, and answers out loud in his language while the answer scrolls across
his face. He remembers the last three questions for five minutes, so "and tomorrow?" works.

It needs two API keys, set in the *Ask Goli* card on the **Goli** tab of his web page:

- an **Anthropic** key for the answers (Claude, `claude-opus-5-5` by default, at low effort for a quick
  reply; a declined question falls back to another model on Anthropic's side);
- a key for an **OpenAI-compatible speech service** that turns your voice into text and the answer into
  his voice (OpenAI by default: `whisper-1` and `tts-1` with the `alloy` voice; the address and models
  can be changed).

What you said while holding him goes to the speech service as audio and to Anthropic as text; nothing
else is recorded. Both services bill your accounts: a question costs about a cent with the defaults. The keys are
never shown back on the web page (they are in the backup file). Without both keys, tap and hold is
petting, as before. The screen has no accents, so Slovak, Czech and German answers show without them,
but he says them properly.

### His day

- **Language:** Goli's words on his face (greetings, pages, hints) in English, Slovak, Czech or German,
  chosen on the web page. The fonts have no accents, so it's "dobre rano!".
- **Reminders:** "14:00 call mum" once, every day or on weekdays, set on the web page. He rings with the
  title at that minute, even in quiet hours (you asked for it).

- **Mood follows yesterday:** after a day with many alerts (critical ones count three times) his resting
  lids are heavier the next morning. At midnight today becomes yesterday, and it survives a restart.
- **Desk buddy** (on by default): the first time someone is at the desk in the morning he says good
  morning (happy birthday on your birthday) with the day in one line, like "3 meetings, rain later"; after 50 minutes at the desk he suggests a stretch and some
  water; on Friday after 17:00 he tells you to go home. "At the desk" means a touch, or voices and typing
  heard by the microphones in the last few minutes.
- **Knock knock:** two quick knocks on the desk (felt by the IMU) snooze alerts for 10 minutes. The
  threshold is a guess until it meets a real desk.
- **Night:** with *Screen off at night* on, his screen goes dark while he sleeps in quiet hours; a touch
  wakes him.
- **A team of Golis:** Golis on the same network find each other. On each one's web page, pick who sits
  on its left and right; when one rings, its neighbours look its way.

### Demo mode

Tap him 10 times: he loops through his faces and a few made-up Goliash alerts (a deploy, a critical
drift, an agent coming back) with sounds, no Wi-Fi needed. Any tap stops it. Made for a booth.

### Tests

`firmware/test/run.sh` runs host tests without a board (CI runs them too): the calendar reader against
real-world iCalendar cases (time zones, weekly and daily repeats, EXDATE, moved and cancelled instances,
VALARM, free time, folded lines, COUNT/UNTIL), what rings and how loud (Goliash items and the filter,
Alertmanager, Grafana and Uptime Kuma webhooks), and every translation's format string. It needs
ArduinoJson from one firmware build.

`pio run -e behaviour && .pio/build/behaviour/program` (in `firmware/`) runs Goli's behaviour (`bot.cpp`)
through a day with fake time and records what he plays: taps and the wink, alerts and the badge, quiet
hours, focus with its break, the kitchen timer through quiet, meetings, the knock-knock snooze, setup and
demo taps, the time and temperature on his face, falling asleep, and the morning greeting once a day.

### Simulator

**<https://pipozzz.github.io/goli-bot/sim/>** runs Goli's firmware in the browser: `bot.cpp` (every
gesture, mood, alert, focus, demo and page), the face and the sounds, compiled to WebAssembly with LVGL.
The mouse is the finger; buttons stand in for Goliash alerts, the IMU (lean, nod, shake, bump, lay him
down), the microphones and the battery. Only the hardware glue in `main.cpp` is left out.

```bash
pio run -e snapshot -d firmware   # fetches LVGL
sim/build.sh                      # needs Emscripten (brew install emscripten)
python3 -m http.server -d sim/out
```

### Installing from the browser

No tools needed: open **<https://pipozzz.github.io/goli-bot/>** in Chrome or Edge on a computer, plug the
board in by USB-C and click **Install**. The page is built by GitHub Actions from every commit to `main`.

1. Use a USB-C cable that carries data. Pick the port (*USB JTAG/serial debug unit* or similar).
2. On the first install, let it erase the board. For updates, don't erase: Goli keeps his settings.
3. If no port shows up or it can't connect: hold **BOOT**, press and release **RESET**, release BOOT and try
   again. Press RESET once more when it's done.
4. Choose **Connect to Wi-Fi** when the installer offers it (Goli speaks [Improv](https://www.improv-wifi.com/)
   over USB). Then open the link it shows to add your Goliash address and token on Goli's setup page.
   No Wi-Fi step? Set him up from your phone instead (below).

The page always installs the latest [release](https://github.com/pipozzz/goli-bot/releases).

### Updates over Wi-Fi

Once Goli is on your Wi-Fi he updates himself: a few minutes after starting and then once a day he reads
`ota.json` from the installer site, and if a different release is out he downloads it, says *updating
myself...* and restarts. The download only comes from a server with a trusted certificate (the roots are in
`src/tls_roots.h`), and the ESP32 checks the image before it boots it. Turn it off on the web page.
Builds you make yourself are versioned `dev` and never update themselves.
To not wait for the daily check, open *Updates* on his web page: **Check now** shows the latest release,
and **Update to vX** installs it right away (also on a `dev` build, or with the daily updates off).

Safari and phones can't flash (no Web Serial). Every release also has `goli-firmware.bin`, a merged image for
offset 0x0: `esptool.py --chip esp32s3 write_flash 0x0 goli-firmware.bin`.

### First time on a new board

The firmware hasn't met real hardware yet: **[docs/bring-up.md](docs/bring-up.md)** is the order to check
the display, touch, IMU, sound, microphones and battery, and what to change when something is off.

### Checking the assembly

![Self-test screen](docs/selftest.png)

Keep a finger on his face while he starts (about a second) for the self-test. A ring follows your finger, a
bubble level shows the tilt, every tap rings the bell, and it shows Wi-Fi signal and battery. Red means the
part didn't answer. Hold 3 s to leave. If the ring isn't under your finger, the touch panel is rotated or
mirrored against the display.

### Building it yourself

```bash
cd firmware
pio run -t upload        # board in USB-C; hold BOOT while plugging in if upload does not start
pio device monitor
```

The build also writes `.pio/build/goli/goli-firmware.bin` (`tools/merge_bin.py`).

**Releasing:** push a tag like `v0.2.0`. GitHub Actions builds that version, creates the release with both
images, and publishes the installer and `ota.json` to Pages; every Goli on Wi-Fi picks it up within a day.
Pushes to `main` and pull requests only build and render the faces. To try the installer page
locally, run `web/build_site.sh` and serve `site/` with `python3 -m http.server -d site`; Web Serial works on
`localhost`.

`include/config.h` holds brightness, default volume, timezone, the idle timeouts and the tilt directions. If the pupils roll the wrong way
when you tilt Goli, flip `TILT_ROLL_SIGN` / `TILT_PITCH_SIGN`.

Preview every expression without the board (renders `docs/faces.png`):

```bash
pio run -e snapshot && .pio/build/snapshot/program && python3 tools/sheet.py
```

### Sounds

Goli synthesizes all his sounds (no audio files). Pick one for each kind of alert on the web page:

| Sound | What it is |
|---|---|
| Desk bell, Desk bell twice | a brass desk bell (inharmonic partials with different decays); twice and a tone lower for critical |
| Ship's bell | lower and longer, struck twice like on a ship |
| Marimba, 8-bit, Coin, Water drop | a wooden arpeggio, a chiptune arpeggio, a game coin, a drop |
| Whisper | a soft "pff" and a tick, for an office |
| Goli: "hm?", "ou!", "ehh...", "ya-ay!" | his own voice: formant synthesis, a buzz shaped into vowels, nothing recorded |
| Your own sound | any audio file, uploaded on the web page (first 3 seconds, converted in the browser) |

With **Goli talks** on, he also says "hm?" when booped and "ya-ay!" when happy. Hear everything in
`docs/sounds/` (re-render: the command at the top of `firmware/tools/render_sounds.cpp`) or in the
simulator. The speaker amp is only on while a sound plays. During quiet hours (22:00 to 07:00 by default)
alerts are silent and only the badge appears.

### Setting Goli up (from a phone or a PC)

On first start, Goli has no Wi-Fi yet, so he opens a setup hotspot and shows a QR code. To set him up again
later, **tap his face 5 times** quickly.

| On Goli's screen | On your phone |
|---|---|
| ![setup screen](docs/setup-screen.png) | ![setup page](docs/setup-page.jpg) |

1. Scan the QR code with the phone camera to join `Goli-XXXX`. The password is on the screen and changes
   every time.
2. The setup page opens by itself (captive portal). If it doesn't, scan the second QR code or open `192.168.4.1`.
3. Pick your Wi-Fi, enter your Goliash address and API token, and set volume and quiet hours. Then tap **Save**.
   Goli restarts and joins your Wi-Fi.

A PC works the same way: join the `Goli-XXXX` Wi-Fi and open `192.168.4.1`. Everything is stored in Goli's flash.
Saved passwords and the token are never shown back on the page; leave a field empty to keep the old value.
If the Wi-Fi or the token stops working, a hint appears at the bottom of his face.

Why not Bluetooth: setting a device up over Bluetooth from a web page (Web Bluetooth) doesn't work on iPhone.
The hotspot works on every phone and computer without an app.

### Managing Goli from a browser

Once Goli is on your Wi-Fi, open **`http://goli-xxxx.local/`** (the status page shows the name and IP).
The first time, click *Show me a code*: Goli shows a 6-digit code on his face, and typing it pairs the
browser (5 wrong tries and the code is gone, and every further round makes you wait twice as long). Only someone standing next to Goli can pair. From then on
everything is there:

- **Home:** connections, his mood, a **kitchen timer** (orange ring, a ding at the end) and a **quiet**
  switch, **alerts today by hour** (a small chart), **his face live** (a snapshot every 2 seconds), **reminders**, today's meetings, the
  latest alerts; boop him, start focus or the demo, test alerts.
- **Sounds:** a sound per kind of alert with a play button, Goli talks, volume, quiet hours, your own sound.
- **Alerts:** Goliash (optional), which alerts ring, GitHub, URLs to keep an eye on, the key for scripts.
- **Day:** calendar, weather, desk buddy, birthday, the microphones, the team.
- **Goli:** Ask Goli (API keys), Wi-Fi, Home Assistant, language, night, updates, **install a firmware file** (`goli-app.bin`
  from a release, or your own build: no USB needed once he's assembled), **backup and restore** (every
  setting and reminder, secrets included, in one JSON file), restart, setup mode, forget this browser.

Secrets (Wi-Fi password, tokens, API keys, the calendar address) are never shown back. The setup hotspot (tap 5
times) still works for the first setup or a new Wi-Fi.

### Sources

Everything is optional; Goli is happy with none of them, and Goliash is just one of them.

| Source | What Goli does |
|---|---|
| **Goliash** | notifications by severity, the overview page (see below) |
| **Calendar**: secret iCal addresses (Google, Outlook, iCloud, Nextcloud), up to 4, separated by spaces | today's meetings on a page, an amber countdown ring 10 minutes before each, a ding when it starts. Daily and weekly repeats (with skipped dates) are expanded; monthly ones and moved single instances are not. Times are taken in Goli's time zone. Checked every 10 minutes. |
| **Weather**: a location (*Use my location* on the web page) | from [Open-Meteo](https://open-meteo.com/) every 30 minutes: an umbrella with rain falling when rain is likely in the next 8 hours, sunglasses on a sunny 20 °C+ day, a scarf at 2 °C or snow; the temperature (°C or °F) with the time on his face and in the morning greeting |
| **GitHub**: a classic token with the `notifications` scope | review requests and mentions ring; failed CI on your pull requests and security alerts ring as critical. Checked every minute. |
| **Keep an eye on**: up to 6 URLs | fetched every minute; two failures in a row is critical ("shop.example.com is down"), and he's happy when it's back |
| **ntfy**: a topic on ntfy.sh or your own server | every message posted to it rings; JSON in a webhook format he knows is read like `POST /notify` |
| **MQTT**: Home Assistant's broker, or a cloud one (TLS) | `goli/goli-xxxx/notify` takes a title or webhook JSON |
| **Anything else**: `POST /notify` | see *Ring Goli from anything* |

HTTPS to these services is checked against the Mozilla root certificates built into ESP-IDF; the watched
URLs only check that a site answers, so their certificates are not checked.

### Goliash alerts

Create a read-only token for the web page (or the setup page): `goliash token create -name goli-bot -role viewer`
(or *Settings → Users → API tokens*). Make sure a notify rule sends what you care about; any channel counts.
To keep alerts inside Goliash only, use the *In Goliash* channel.

Every 30 s Goli asks `GET /api/v1/notifications/sent?since=…`, the same list the bell in the Goliash UI
shows. Each message newer than the last one he saw rings once. The first poll after power-on only learns
where things stand, so he doesn't ring for old news. If you leave the certificate field empty, the
Goliash certificate is not checked.

**Which alerts ring** is up to you on the web page: everything or only critical ones, only production,
only some services (comma-separated). Items without an environment or service, like an agent going
quiet, always pass the production and service filters.

**Instant alerts with a webhook:** if your Goliash can reach Goli on your network, add a *Webhook*
channel in Goliash with the URL `http://goli-xxxx.local/notify` (or his IP; the status page shows both)
and Goli's key as the secret, point a notify rule at it, and tick *Goliash sends alerts to me* on the web page. Goli checks the
`X-Goliash-Signature` on every request and stops polling for notifications. He still asks for the summary
every 5 minutes, which also tells him the token works. (Goliash in Docker may not resolve `.local`
names: use the IP then, and give Goli a fixed address in your router.)

### Home Assistant

Fill in your MQTT broker on the web page (*Goli* tab; the Mosquitto add-on works). Goli announces
himself with MQTT discovery and shows up as a device:

| Entity | What it is |
|---|---|
| Unseen alerts, Mood, Alerts today | sensors |
| Focus, Someone at the desk | binary sensors (an occupancy sensor for automations) |
| Boop, Focus on/off, Demo on/off | buttons |
| Timer | a number (minutes): a kitchen timer with an orange ring |
| Quiet | a switch: alerts only as a badge until it's off |
| In a meeting | a binary sensor from the calendar |
| Ring Goli | a notify entity: `notify.send_message` with a message rings him |

For a level, publish `{"title": "Washing machine done", "level": "good"}` to `goli/goli-xxxx/notify`.
Goli marks himself offline in Home Assistant when he loses the broker.

### From the cloud: Goli has no public address

Webhooks (`POST /notify`) only reach Goli from inside your network: a Goliash, Grafana or Uptime Kuma
running at home, a script on your laptop. Everything Goli **asks for** works from anywhere, because he
connects out: Goliash (polling, the default), GitHub, calendars, weather, updates. For services in the
cloud to **push** to him, give them something both sides connect to:

**ntfy** (simplest): pick a long random topic, e.g. `https://ntfy.sh/goli-7f3k9q2xw`, put it on Goli's web
page (*Alerts* → *ntfy*), and post to it from anywhere:

```bash
curl -d "deploy finished" -H "Tags: tada" https://ntfy.sh/goli-7f3k9q2xw          # good news
curl -d "prod is down" -H "Priority: urgent" https://ntfy.sh/goli-7f3k9q2xw       # critical
```

Priority 5 (`urgent`) or a `rotating_light`, `warning`, `x` tag rings as critical; `white_check_mark`,
`tada`, `+1` as good news. Grafana Cloud, Uptime Kuma or Alertmanager can use the topic as their webhook
URL: the JSON they post becomes the message, and Goli reads it like `POST /notify`. On ntfy.sh anyone who
knows the topic can post to it, so keep it secret, or run your own ntfy with an access token.

**MQTT**: a broker in the cloud (HiveMQ Cloud and EMQX Cloud have free plans) with **TLS** ticked on the
web page, port 8883. Publish a title or webhook JSON to `goli/goli-xxxx/notify`. From GitHub Actions:

```yaml
- if: failure()
  run: |
    sudo apt-get install -y mosquitto-clients
    mosquitto_pub -h ${{ secrets.MQTT_HOST }} -p 8883 --capath /etc/ssl/certs \
      -u ${{ secrets.MQTT_USER }} -P ${{ secrets.MQTT_PASS }} -t goli/goli-xxxx/notify \
      -m '{"title":"${{ github.repository }}: build failed","level":"critical"}'
```

Or from ntfy in a GitHub Actions step: `curl -d "build failed" -H "Priority: urgent" https://ntfy.sh/<topic>`.

### Ring Goli from anything

Goli listens on your network: `POST /notify` with a JSON `title` and an optional `level`
(`info`, `critical` or `good`), and his key from the web page (Alerts tab).

```bash
curl -H "Authorization: Bearer <key>" -H "Content-Type: application/json" \
  -d '{"title":"nightly build failed","level":"critical"}' http://goli-xxxx.local/notify
```

He answers `202` with `ringing`, or `filtered out` when *only critical* is on and it isn't; without the
JSON content type he answers `415`. `GET /status`
returns his version, Wi-Fi signal and Goliash connection, without secrets.

From GitHub Actions on a self-hosted runner in the same network:

```yaml
- if: failure()
  run: |
    curl -fsS -H "Authorization: Bearer ${{ secrets.GOLI_KEY }}" -H "Content-Type: application/json" \
      -d '{"title":"${{ github.repository }}: ${{ github.workflow }} failed","level":"critical"}' \
      http://goli-xxxx.local/notify
```

From Home Assistant (`configuration.yaml`), then call `rest_command.goli` from any automation:

```yaml
rest_command:
  goli:
    url: http://goli-xxxx.local/notify
    method: post
    headers:
      Authorization: !secret goli_bearer   # "Bearer <key>"
    content_type: application/json
    payload: '{"title": "{{ title }}", "level": "{{ level | default(''info'') }}"}'
```

**Monitoring tools, as they are:** Alertmanager (Prometheus), Grafana alerting and Uptime Kuma can post
straight to `http://goli-xxxx.local/notify`. Goli understands their webhook formats: firing alerts ring
(critical when the `severity` label is critical), resolved ones and monitors coming back up are good
news, a monitor going down is critical. Give the key as a Bearer header, or in the address
(`/notify?key=...`) for tools that can't set headers; the address then contains the key, so keep it
inside your network.

Titles show in ASCII; common accented letters are folded (č → c).

Developers can pre-fill the settings at build time: copy `firmware/include/secrets.example.h` to `secrets.h`
(git-ignored). It is only used while nothing has been saved on the device.

## Changing the design

Everything is generated by `goli_bot.py`: the body silhouette is the Bézier path from the Goli artwork, and the
display's dimensions are constants at the top of the file.

```bash
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python goli_bot.py      # writes stl/ (print-oriented) and stl/assembly/ (assembled positions)
.venv/bin/python goli_bot.py --mini  # Goli mini: stl/mini/
.venv/bin/python make_viewer.py   # rebuilds viewer.html
```

Useful knobs: `S` (overall scale), `FACE_TILT` (how far the face looks up), `K` (body depth),
`FIT` (clearance for press fits), and the `PCB_NUTS` / `GLASS_R` / `NUT_DEPTH` constants if you use another display.
The display's mechanical drawing is in `docs/waveshare-amoled-1.75-dimensions.jpg`.

## License

| What | License |
|---|---|
| Firmware (`firmware/`), web installer (`web/`), CI and the Python tools | [MIT](LICENSE) |
| 3D model (`stl/`, `goli_bot.py` and what it generates, `viewer.html`), the Goli character, renders and artwork in `docs/` | [CC BY-NC-SA 4.0](LICENSE-MODEL.txt) |
| ES8311 codec driver (`firmware/lib/es8311/`) | Apache-2.0, © Espressif Systems |
| Clock digits (`firmware/src/font_clock.c`, rendered from Montserrat) | SIL Open Font License 1.1 |
| ES7210 register sequence in `firmware/src/mic.cpp` | follows Espressif's esp_codec_dev driver, Apache-2.0 |

Print, remix and share Goli for non-commercial use: credit "Goli desk bot by Peter Gonda" with a link to this
repository and share remixes under the same license. Selling prints or remixes needs written permission.
