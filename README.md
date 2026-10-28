# Goli desk bot

A printable desk robot of **Goli**, the Goliash mascot: tired, kind, always one more alert away from a coffee.
His whole face is a round AMOLED with an ESP32-S3 behind it, so the eyes, brows and mouth are animated.

![Goli desk bot](docs/hero.jpg)

| Exploded | Back |
|---|---|
| ![exploded](docs/exploded.jpg) | ![back](docs/back.jpg) |

Open `viewer.html` in a browser for an interactive 3D view with an exploded-assembly slider.

**Status:** designed and checked in CAD, **not printed yet**. Print the bezel and bracket first (about 20 min)
and test-fit them on the display before you print the body.

## Size

About 108 × 65 × 150 mm with the bell on. Every part fits the Bambu Lab A1 mini (180 mm³).

## Parts to buy

| Part | Notes |
|---|---|
| [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75) | 466×466 round AMOLED with touch, ESP32-S3, IMU, mic, speaker amp, LiPo charger and RTC. Get the version **without** the case. |
| Speaker, 8 Ω 1 W, about 20×30 mm, MX1.25 plug | Glue it behind the belly card; the two "message lines" are the grille. |
| 3× M2×6 machine screws | Bracket → the board's M2 nuts |
| 3× M2×6 self-tapping screws | Bracket → bezel |
| 3× M2×8 self-tapping screws | Back plate → body |
| USB-C cable | Goes through the slot in the back plate. A right-angle plug fits best. |
| Optional: 3.7 V LiPo (~1000 mAh, MX1.25) | Sits in the base. The board's power button is inside the body, so plan for USB power or leave the board on. |

## Print

| File | Colour | Orientation | Notes |
|---|---|---|---|
| `goli_body_blue.stl` + `goli_belly_cream.stl` + `goli_accents_red.stl` | blue / cream / red | upright, as exported | One multi-colour object (see below). Tree supports, build plate only, for the arm undersides. |
| `goli_bezel_navy.stl` | navy | face down | No supports. |
| `goli_bracket_any.stl` | any | as exported | No supports. |
| `goli_backplate_blue.stl` | blue | outer face down | No supports. |
| `goli_bell_yellow.stl` | yellow | lip down | No supports. |
| `goli_mug_white.stl` | white | upright | Optional prop: "coffee #4". |

PLA, 0.2 mm layers (0.12 mm for the bell and mug), 3 walls, 15 % gyroid.

The inside of the body is designed to print without supports: the roof is a 45° cone, and the face socket
and screw bosses have 45° undersides.

**Multi-colour body (AMS lite):** in Bambu Studio, select the three body files together → *Import* →
answer **Yes** to "Load these files as a single object with multiple parts". Then give each part its filament.
The three files share one origin, so they stay registered. With a single colour, just print the body file.
The belly, card and cheeks stand out in relief either way.

## Assembly

1. Glue the speaker behind the belly grille, with the plug pointing up.
2. Drop the display into the bezel from behind, glass first. The lip holds the glass by its black border.
3. Screw the bracket to the board's three back nuts (M2×6). The board's USB-C port points **down**, through
   the notch in the bezel.
4. Push the bezel into the face socket from the front. From inside, screw the bracket to the bezel tube
   (3× M2×6 self-tapping). The body wall is clamped between the bezel and the bracket.
5. Plug in the speaker (and the battery). Route the USB-C cable down and out through the back plate slot.
6. Close the back plate (3× M2×8). Press the bell onto the antenna peg; add a drop of glue if it is loose.

## Firmware

`firmware/` is a PlatformIO project: Arduino core 3 (via pioarduino), LVGL 8.4, the CO5300 driver from
GFX Library for Arduino, SensorLib for touch and IMU, Espressif's ES8311 codec driver, and ArduinoJson.

![Goli's faces](docs/faces.png)

| Input | Goli |
|---|---|
| nothing happening | tired: heavy lids, worried brows, wavy mouth; blinks and glances around |
| new Goliash notification | **ding** of a desk bell, surprised face, red count badge, title scrolling under the mouth |
| tap the face | boop: surprised, then happy with blushing cheeks; also marks alerts as seen |
| double tap | wink |
| finger resting on / stroking the face | petting: blissful closed eyes |
| tilt him | pupils roll with the tilt |
| shake him | dizzy, pupils spinning, wobbly sound |
| bump or pick him up | surprised |
| 1 min idle → 5 min idle, or lying flat | drowsy → asleep with a yawn (screen dims, "z Z", breathing) |
| hold a finger still for 3 s | turns the face by 90° (saved): use it once after assembly |

The whole face drifts by a few pixels over minutes so the AMOLED does not burn in.

```bash
cd firmware
pio run -t upload        # board in USB-C; hold BOOT while plugging in if upload does not start
pio device monitor
```

`include/config.h` holds brightness, volume, quiet hours, the idle timeouts and the tilt directions. If the pupils roll the wrong way
when you tilt Goli, flip `TILT_ROLL_SIGN` / `TILT_PITCH_SIGN`.

Preview every expression without the board (renders `docs/faces.png`):

```bash
pio run -e snapshot && .pio/build/snapshot/program && python3 tools/sheet.py
```

### Sounds

Goli synthesizes all his sounds (no audio files): a brass desk bell for alerts (a few inharmonic partials
with different decays), plus boop, chirp, wobble and yawn. Hear them in `docs/sounds/`
(re-render with `python3 firmware/tools/sounds_wav.py`). The speaker amp is only on while a sound plays.
From 22:00 to 07:00 (`QUIET_FROM` / `QUIET_TO`) alerts are silent and only the badge appears.

### Goliash alerts

1. Create a read-only token: `goliash token create -name goli-bot -role viewer`
   (or *Settings → Users → API tokens*).
2. `cp firmware/include/secrets.example.h firmware/include/secrets.h` and fill in Wi-Fi, your Goliash URL and
   the token. `secrets.h` is git-ignored.
3. Make sure a notify rule sends what you care about (any channel counts). To keep alerts inside Goliash
   only, use the *In Goliash* channel.

Every 30 s Goli asks `GET /api/v1/notifications/sent?since=…`, the same list the bell in the Goliash UI
shows. Each message newer than the last one he saw rings once. The first poll after power-on only learns
where things stand, so he doesn't ring for old news. TLS: put your CA in `GOLIASH_ROOT_CA`; if you leave it
empty, the certificate is not checked.

## Changing the design

Everything is generated by `goli_bot.py`: the body silhouette is the Bézier path from the Goli artwork, and the
display's dimensions are constants at the top of the file.

```bash
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python goli_bot.py      # writes stl/ (print-oriented) and stl/assembly/ (assembled positions)
.venv/bin/python make_viewer.py   # rebuilds viewer.html
```

Useful knobs: `S` (overall scale), `FACE_TILT` (how far the face looks up), `K` (body depth),
`FIT` (clearance for press fits), and the `PCB_NUTS` / `GLASS_R` / `NUT_DEPTH` constants if you use another display.
The display's mechanical drawing is in `docs/waveshare-amoled-1.75-dimensions.jpg`.
