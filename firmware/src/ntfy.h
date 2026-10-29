#pragma once

// ntfy (ntfy.sh or your own server): Goli keeps a connection open to a topic and rings for every
// message posted there, so services in the cloud can reach him without a public address.
// Anything that can make an HTTP POST works: curl, GitHub Actions, Grafana Cloud, Uptime Kuma, phone
// shortcuts. Messages that are JSON in a webhook format Goli knows are read like POST /notify.
namespace ntfy {

void begin();          // starts its own task (call once a topic is set up; again is harmless)
const char *status();  // "off", "listening", or what's wrong

}  // namespace ntfy
