#pragma once

// A tiny uptime monitor: Goli fetches each URL from setup every minute. When one stops answering
// (twice in a row) he is alarmed; when it's back, he's happy.
namespace watch {

void poll();          // from the network task
int down();           // how many watched URLs are down right now

}  // namespace watch
