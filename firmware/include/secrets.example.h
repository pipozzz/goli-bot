#pragma once
// Copy to secrets.h (git-ignored) and fill in. Without secrets.h Goli runs offline.

#define WIFI_SSID "my-wifi"
#define WIFI_PASS "my-wifi-password"

// Your Goliash, without a trailing slash.
#define GOLIASH_URL "https://goliash.example.com"
// A read-only token: goliash token create -name goli-bot -role viewer
// (or Settings -> Users -> API tokens)
#define GOLIASH_TOKEN "glsh_api_..."

// Optional: PEM of the CA that signed your Goliash certificate. Leave empty to skip
// certificate checks (fine for a viewer token on your own network).
#define GOLIASH_ROOT_CA ""
