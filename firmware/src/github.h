#pragma once

// Your GitHub notifications that need you: a review request, failed CI on your pull requests, a
// mention, an assignment, a security alert. Needs a classic token with the "notifications" scope
// (GitHub's notifications API doesn't take fine-grained tokens).
namespace github {

void poll();           // from the network task
const char *state();   // "off", "ok", or what went wrong

}  // namespace github
