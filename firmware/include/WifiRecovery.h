#pragma once

#include <stdint.h>

// Track a continuous outage, independently of individual association retries.
class WifiRecovery {
 public:
  bool update(bool connected, uint32_t now, uint32_t timeout) {
    if (connected) {
      disconnected_ = false;
      return false;
    }
    if (!disconnected_) {
      disconnected_ = true;
      since_ = now;
    }
    return static_cast<uint32_t>(now - since_) >= timeout;
  }

 private:
  bool disconnected_ = false;
  uint32_t since_ = 0;
};
