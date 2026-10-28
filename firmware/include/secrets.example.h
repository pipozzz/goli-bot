#pragma once
// Optional, for developer builds: copy to secrets.h (git-ignored) to pre-fill the settings.
// Normally you set Goli up from your phone instead (tap his face 5 times).

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
