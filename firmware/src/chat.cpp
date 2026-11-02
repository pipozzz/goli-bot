#include "chat.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>

#include <algorithm>

#include "mic.h"
#include "settings.h"
#include "sound.h"
#include "text.h"
#include "tls.h"

namespace chat {
namespace {

const int kRate = 16000;
const size_t kMaxRec = kRate * 12;     // 12 s of question
const size_t kMaxSpeech = kRate * 45;  // 45 s of answer
const size_t kPrefix = 1024;           // room for the multipart head and the WAV header before the PCM
const size_t kSuffix = 64;
const int kTurns = 3;                  // remembered questions, for "and what about...?"
const uint32_t kForgetMs = 5 * 60000;  // a new conversation after 5 quiet minutes

uint8_t *upload = nullptr;   // multipart head | PCM the mics fill | multipart tail, in PSRAM
int16_t *speech = nullptr;   // his answer at 16 kHz, in PSRAM
size_t rec_n = 0;
volatile bool busy = false;  // recording or talking to the cloud: one question at a time
SemaphoreHandle_t go = nullptr;

struct Turn {
  String q, a;
};
Turn turns[kTurns];
int turn_n = 0;
uint32_t last_turn = 0;

// What the face should show, for poll() on the main loop
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
bot::Chat phase = bot::Chat::IDLE;
char text[1200] = "";
uint32_t text_ms = 0;
uint32_t version = 0;

void show(bot::Chat p, const char *t = "", uint32_t ms = 0) {
  portENTER_CRITICAL(&lock);
  phase = p;
  snprintf(text, sizeof text, "%s", t);
  text_ms = ms;
  version++;
  portEXIT_CRITICAL(&lock);
}

// A short reason for the face; `who` is "speech" or "Claude"
String problem(const char *who, int code) {
  if (code == 401 || code == 403) return String(who) + " key?";
  if (code == 429) return "too many requests";
  if (code < 0) return "no connection";
  return String(who) + " error " + code;
}

bool begin(HTTPClient &http, NetworkClientSecure &tls, const String &url) {
  trustPublicRoots(tls);
  if (!http.begin(tls, url)) return false;
  http.setTimeout(30000);
  return true;
}

void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = uint8_t(v >> (8 * i));
}

// The recording as words; "" when nothing was understood
bool transcribe(const Settings &cfg, String &out, String &err) {
  int16_t *pcm = (int16_t *)(upload + kPrefix);
  // Lift a quiet recording to about -3 dB: the mics are far from the mouth
  int peak = 1;
  for (size_t i = 0; i < rec_n; i++) peak = std::max(peak, abs(int(pcm[i])));
  if (peak < 23000) {
    const int gain = std::min(8 * 256, 23000 * 256 / peak);
    for (size_t i = 0; i < rec_n; i++) pcm[i] = int16_t(constrain(int(pcm[i]) * gain / 256, -32767, 32767));
  }

  const char *boundary = "goliaskgoliask";
  String head;
  head += String("--") + boundary + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n" + cfg.stt_model + "\r\n";
  head += String("--") + boundary + "\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n" + language() + "\r\n";
  head += String("--") + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"ask.wav\"\r\n"
                                     "Content-Type: audio/wav\r\n\r\n";
  uint8_t wav[44];
  const uint32_t bytes = rec_n * 2;
  memcpy(wav, "RIFF", 4), put32(wav + 4, 36 + bytes), memcpy(wav + 8, "WAVEfmt ", 8);
  put32(wav + 16, 16), put32(wav + 20, 1 | (1 << 16)), put32(wav + 24, kRate), put32(wav + 28, kRate * 2);
  put32(wav + 32, 2 | (16 << 16)), memcpy(wav + 36, "data", 4), put32(wav + 40, bytes);
  if (head.length() + sizeof wav > kPrefix) {
    err = "bad model name";
    return false;
  }
  uint8_t *start = upload + kPrefix - sizeof wav - head.length();
  memcpy(start, head.c_str(), head.length());
  memcpy(upload + kPrefix - sizeof wav, wav, sizeof wav);
  const String tail = String("\r\n--") + boundary + "--\r\n";
  memcpy(upload + kPrefix + bytes, tail.c_str(), tail.length());
  const size_t total = (upload + kPrefix + bytes + tail.length()) - start;

  NetworkClientSecure tls;
  HTTPClient http;
  if (!begin(http, tls, cfg.speech_url + "/audio/transcriptions")) {
    err = "speech address?";
    return false;
  }
  http.addHeader("Authorization", "Bearer " + cfg.speech_key);
  http.addHeader("Content-Type", String("multipart/form-data; boundary=") + boundary);
  const int code = http.POST(start, total);
  const String body = code > 0 ? http.getString() : String();
  http.end();
  if (code != 200) {
    Serial.printf("chat: transcription HTTP %d %s\n", code, body.substring(0, 200).c_str());
    err = problem("speech", code);
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    err = "speech: bad reply";
    return false;
  }
  out = doc["text"] | "";
  out.trim();
  return true;
}

// Claude's answer to `question`, in a few spoken sentences
bool ask(const Settings &cfg, const String &question, String &out, String &err) {
  static const char *names[][2] = {{"en", "English"}, {"sk", "Slovak"}, {"cs", "Czech"}, {"de", "German"}};
  const char *lang = "English";
  for (auto &n : names)
    if (!strcmp(language(), n[0])) lang = n[1];
  char now[40] = "";
  const time_t t = time(nullptr);
  struct tm lt;
  if (t > 1700000000 && localtime_r(&t, &lt)) strftime(now, sizeof now, "%A %d %B %Y, %H:%M", &lt);

  JsonDocument req;
  req["model"] = cfg.ai_model;
  req["max_tokens"] = 1024;
  req["output_config"]["effort"] = "low";  // a quick spoken answer, not an essay
  // Server-side fallback on a declined request, where the model supports the "default" form
  const bool fallback = cfg.ai_model.startsWith("claude-opus-5") || cfg.ai_model == "claude-fable-5-1" ||
                        cfg.ai_model == "claude-sonnet-5-5";
  if (fallback) req["fallbacks"] = "default";
  req["system"] = String(
                      "You are Goli, a small round robot on the user's desk; your face is a round screen. "
                      "Someone just asked you something out loud. Your answer is spoken by a text-to-speech voice "
                      "through a tiny speaker while it scrolls across your face, so answer in ") +
                  lang +
                  ", in one to three short sentences of plain spoken language: no lists, markdown, emoji or "
                  "links. Be warm and a little playful, but get to the point. If you can't know something "
                  "current (news, prices, scores), say so briefly." +
                  (*now ? String(" Where the user is, it is ") + now + "." : String());
  JsonArray msgs = req["messages"].to<JsonArray>();
  for (int i = 0; i < turn_n; i++) {
    JsonObject u = msgs.add<JsonObject>();
    u["role"] = "user", u["content"] = turns[i].q;
    JsonObject a = msgs.add<JsonObject>();
    a["role"] = "assistant", a["content"] = turns[i].a;
  }
  JsonObject u = msgs.add<JsonObject>();
  u["role"] = "user", u["content"] = question;
  String payload;
  serializeJson(req, payload);

  NetworkClientSecure tls;
  HTTPClient http;
  if (!begin(http, tls, "https://api.anthropic.com/v1/messages")) {
    err = "no connection";
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", cfg.ai_key);
  http.addHeader("anthropic-version", "2026-10-21");
  if (fallback) http.addHeader("anthropic-beta", "server-side-fallback-2026-07-01");
  const int code = http.POST(payload);
  const String body = code > 0 ? http.getString() : String();
  http.end();
  if (code != 200) {
    Serial.printf("chat: Claude HTTP %d %s\n", code, body.substring(0, 300).c_str());
    err = problem("Claude", code);
    return false;
  }
  JsonDocument filter;
  filter["stop_reason"] = true;
  filter["content"][0]["type"] = true;
  filter["content"][0]["text"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
    err = "Claude: bad reply";
    return false;
  }
  out = "";
  for (JsonObject b : doc["content"].as<JsonArray>())
    if (!strcmp(b["type"] | "", "text")) out += b["text"] | "";
  out.trim();
  if (!strcmp(doc["stop_reason"] | "", "refusal")) {
    err = "can't help with that";
    return false;
  }
  if (!out.length()) {
    err = "nothing to say";
    return false;
  }
  return true;
}

// The answer as his voice at 16 kHz in `speech`; returns the samples (0: no voice, the text still shows)
size_t speak(const Settings &cfg, const String &answer) {
  JsonDocument req;
  req["model"] = cfg.tts_model;
  req["voice"] = cfg.voice;
  req["input"] = answer;
  req["response_format"] = "pcm";  // 24 kHz, 16-bit, mono
  String payload;
  serializeJson(req, payload);
  NetworkClientSecure tls;
  HTTPClient http;
  if (!begin(http, tls, cfg.speech_url + "/audio/speech")) return 0;
  http.addHeader("Authorization", "Bearer " + cfg.speech_key);
  http.addHeader("Content-Type", "application/json");
  http.useHTTP10(true);  // a plain stream to the end
  const int code = http.POST(payload);
  if (code != 200) {
    Serial.printf("chat: speech HTTP %d\n", code);
    http.end();
    return 0;
  }
  // 24 kHz -> 16 kHz: of every three samples keep the first and the average of the other two
  NetworkClient *stream = http.getStreamPtr();
  uint8_t chunk[1024];
  size_t n = 0;
  int k = 0, held = 0, carry = -1;
  const uint32_t started = millis();
  while (n < kMaxSpeech && millis() - started < 30000) {
    const int avail = stream->available();
    if (!avail) {
      if (!stream->connected()) break;
      delay(5);
      continue;
    }
    const int got = stream->readBytes(chunk, std::min<int>(avail, sizeof chunk));
    for (int i = 0; i < got && n < kMaxSpeech; i++) {
      if (carry < 0) {
        carry = chunk[i];
        continue;
      }
      const int s = int16_t(carry | (chunk[i] << 8));
      carry = -1;
      if (k == 0) speech[n++] = int16_t(s);
      else if (k == 1) held = s;
      else speech[n++] = int16_t((held + s) / 2);
      k = (k + 1) % 3;
    }
  }
  http.end();
  return n;
}

void task(void *) {
  for (;;) {
    xSemaphoreTake(go, portMAX_DELAY);
    const Settings cfg = settings::copy();
    if (millis() - last_turn > kForgetMs) turn_n = 0;
    String question, answer, err;
    show(bot::Chat::THINKING);
    if (!transcribe(cfg, question, err)) {
      show(bot::Chat::FAILED, err.c_str());
    } else if (!question.length()) {
      show(bot::Chat::FAILED, "");  // didn't catch that
    } else if (!ask(cfg, question, answer, err)) {
      show(bot::Chat::FAILED, err.c_str());
    } else {
      Serial.printf("chat: \"%s\" -> \"%s\"\n", question.c_str(), answer.c_str());
      if (turn_n == kTurns) {
        for (int i = 1; i < kTurns; i++) turns[i - 1] = turns[i];
        turn_n--;
      }
      turns[turn_n++] = {question, answer};
      last_turn = millis();
      const size_t samples = speak(cfg, answer);
      // On screen as long as he talks, or a reading pace without a voice
      const uint32_t ms = samples ? samples * 1000 / kRate + 1500 : 3000 + answer.length() * 60;
      show(bot::Chat::SPEAKING, answer.c_str(), ms);
      if (samples) sound::playPcm(speech, samples);
      vTaskDelay(pdMS_TO_TICKS(ms));  // the buffer plays from PSRAM: no new answer over it
    }
    busy = false;
  }
}

}  // namespace

bool begin() {
  upload = (uint8_t *)ps_malloc(kPrefix + kMaxRec * 2 + kSuffix);
  speech = (int16_t *)ps_malloc(kMaxSpeech * sizeof(int16_t));
  if (!upload || !speech) return false;
  go = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(task, "goli_chat", 12288, nullptr, 1, nullptr, 1);
  return true;
}

bool ready() {
  const Settings &cfg = settings::get();
  return go && cfg.ai_key.length() && cfg.speech_key.length() && cfg.speech_url.length();
}

void listen() {
  if (!ready() || busy) return;
  busy = true;
  sound::stop();
  sound::play(Sound::BOOP);  // "go ahead" (not recorded: the mics skip his own sounds)
  mic::record((int16_t *)(upload + kPrefix), kMaxRec);
  show(bot::Chat::LISTENING);
}

void send() {
  if (!busy) return;
  rec_n = mic::stopRecording();
  if (rec_n < kRate / 2) {  // under half a second: nothing said
    busy = false;
    show(bot::Chat::FAILED, "");
    return;
  }
  xSemaphoreGive(go);
}

bool poll(bot::Chat &p, const char *&t, uint32_t &ms) {
  static uint32_t seen = 0;
  static char copy[sizeof text];
  portENTER_CRITICAL(&lock);
  const bool changed = version != seen;
  if (changed) {
    seen = version;
    p = phase;
    memcpy(copy, text, sizeof copy);
    ms = text_ms;
  }
  portEXIT_CRITICAL(&lock);
  t = copy;
  return changed;
}

}  // namespace chat
