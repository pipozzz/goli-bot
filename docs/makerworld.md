# MakerWorld listing (draft)

Ready to paste once there are photos of a printed Goli. Upload `stl/goli_body_multicolor.3mf` as the main
print profile and the other STLs as extra plates.

**Title:** Goli – a desk robot with an AMOLED face that rings when your deploys drift

**Summary (short):** Goli, the tired-but-kind mascot of Goliash, as a desk robot. His whole face is a
1.75" round AMOLED with an ESP32-S3: he blinks, looks around, gets dizzy when shaken, and rings a desk
bell when something needs you.

**Description:**

Goli is the mascot of [Goliash](https://goliash.dev), the tool that watches which versions run where. On
your desk he is a 15 cm robot whose face is a round touch screen:

- He rings a little desk bell for every Goliash alert: a double ding and an alarmed face for critical
  ones (an agent gone quiet, a vulnerability, production drift), a happy chirp for good news.
- Anything else can ring him too: a CI job, a cron script, Home Assistant (`POST /notify`).
- Swipe for an overview, a big clock and his status. Three taps for 25 minutes of focus.
- Tap him, pet him, tilt him, shake him, clap at him.
- Install the firmware from your browser (no tools), set him up from your phone with a QR code, and he
  updates himself over Wi-Fi.

**Print:** Bambu Lab A1 mini or any 180 mm printer. The body is one multi-colour object (blue, cream,
red; AMS lite), about 17 h at 0.2 mm with tree supports from the plate. Bezel, bracket, back plate and
bell are single-colour and print without supports.

**You also need:** Waveshare ESP32-S3-Touch-AMOLED-1.75 (without case), an 8 Ω 1 W speaker with an
MX1.25 plug, 6× M2×6 and 3× M2×8 screws, a USB-C cable. Optional: a 3.7 V LiPo.

**Everything else** (assembly, firmware, web installer, sources): https://github.com/pipozzz/goli-bot

**License:** CC BY-NC-SA 4.0 for the model and artwork; the firmware is MIT.

**Tags:** desk robot, esp32, amoled, multicolor, ams lite, devops, mascot, notification, bambu a1 mini
