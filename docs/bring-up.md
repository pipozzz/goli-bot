# Bringing up a new board

Goli's firmware was written and tested without the hardware: in the simulator, with host tests and with
rendered screens. This is the order to check things the first time a Waveshare ESP32-S3-Touch-AMOLED-1.75
is on the desk, and what to change when something is off. Do it **before** printing the body.

Keep a serial monitor open the whole time: `pio device monitor` (115200), or *Logs & console* in the web
installer. Every module prints a line when it starts or fails ("touch init failed", "PMU init failed",
"api: http://goli-1a2b.local/ (192.168.1.42)", "calendar: 2 meetings left today", ...).

## 1. Flash and first boot

1. Install from <https://pipozzz.github.io/goli-bot/> (Chrome or Edge), let it erase the board.
2. If no port shows up: hold **BOOT**, press **RESET**, release BOOT, try again.
3. Expected: the setup hotspot screen (QR code, `Goli-XXXX`, a password).

| If you see | Check |
|---|---|
| Nothing at all, backlight-less black | the log for `display init failed`; the QSPI pins in `include/config.h` (`LCD_SDIO0..3`, `LCD_SCLK`, `LCD_CS`, `LCD_RESET`) against the board's schematic |
| Garbage or stripes | `Arduino_CO5300(bus, LCD_RESET, 0, ...)` arguments in `main.cpp` (rotation, IPS, column offset) |
| The screen mirrored or shifted by a few pixels | the `col_offset` arguments of `Arduino_CO5300`; the `rounder()` in `main.cpp` |

## 2. Self-test

Keep a finger on the face while pressing RESET, about a second, then lift it.

| Row | Good | If not |
|---|---|---|
| touch: x, y | the green ring sits under your finger, x grows to the right, y downwards | mirrored or swapped axes: map them in `pollTouch()` in `main.cpp` (the CST9217 may report rotated); nothing at all: `TP_INT`/`TP_RESET`, I2C address `0x5A` |
| tilt | the bubble moves the way you lean the board, upright reads about 0 | wrong direction: flip `TILT_ROLL_SIGN` / `TILT_PITCH_SIGN` in `config.h`; "no IMU": I2C address `0x6B` |
| sound: tap for a ding | a bell from the speaker | "no codec": ES8311 at `0x18`; silent but no error: the amplifier enable `AUDIO_PA_EN` (46) may need the opposite level, or the codec's CE through the TCA9554 expander (see *Hardware notes* below) |
| Wi-Fi | dBm after setup | only after the phone setup below |
| power | battery %, or "USB, no battery" | "no PMU": AXP2101 at `0x34` |
| mic: dB | about -60 quietly, -30 when you talk close | "no ADC": ES7210 at `0x40`; stuck at -90: the ES7210 register sequence in `mic.cpp` |

Hold 3 s to leave.

## 3. Phone setup and the face

1. Scan the QR code, save your Wi-Fi (Goliash is optional).
2. Goli restarts into his face. If he looks sideways, hold a finger still on him for 3 s (turns 90°, saved).
3. Tap: boop. 2 taps: wink. Swipe: the pages. If swipes go the wrong way round, that's the touch mapping
   from step 2.
4. Open `http://goli-xxxx.local/` and pair the browser with the code on his face.

## 4. Things that are guesses until they meet a real desk

| What | Where | How to tune |
|---|---|---|
| Knock-knock snooze | `motion.cpp`, `jerk > 0.22f` and the 150-700 ms gap | knock twice; watch the serial log while you add `Serial.printf` of `jerk`; raise the threshold if taps on the glass or typing trigger it |
| Clap | `mic.cpp`, `floor_db + 25` and `-32 dB` | clap at your usual distance; lower to +20 if it's missed, raise if voices trigger it |
| Noisy room (wince) | `mic.cpp`, `floor_db + 15` for 1.5 s | |
| Someone at the desk | `mic.cpp`, `floor_db + 8` | a quiet office may need +6 |
| Brightness | `BRIGHTNESS_AWAKE` / `BRIGHTNESS_ASLEEP` in `config.h` | AMOLED: keep awake moderate for burn-in |
| Volume | the web page | 70 % by default |

## 5. Sources

On the web page's Home tab, *Right now* shows each source's state: Goliash, calendar, weather, GitHub,
Home Assistant, sites down. *Test an alert* rings him without any source.

## Hardware notes

- Waveshare's own BSP drives the speaker amplifier and the codec's CE pin through the TCA9554 I/O
  expander (`0x20`) on some revisions; their Arduino ES8311 example just sets GPIO46 high, which is what
  Goli does. If there is no sound at all, try driving TCA9554 outputs (their `ESP32_IO_Expander` library).
- The board's PWR button is part of the AXP2101; Goli doesn't use it.
- Never power the board from both USB and the 3V3 header.
